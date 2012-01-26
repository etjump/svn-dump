#include "g_local.h"
#include "../sqlite/sqlite3.h"

char bigTextBuffer[100000];

/*
What is needed:	
disable save?
BUGS:
AFTER 110B
no greeting -> lil lagspike on connect?
adminhelp
callvote kick

!setlevel x 2
!setlevel x 0
!rename x y
!setlevel y 2
-> finger y = user (x)
*/

// I don't think anyone needs over 64 admin levels.

admin_level_t *g_admin_levels[MAX_ADMIN_LEVELS];

// 2048 users for now. If there's a reason to waste any more
// memory I'll edit this.

admin_user_t *g_admin_users[MAX_USERS];

// 512 bans, should be more than enough

admin_ban_t *g_admin_bans[MAX_BANS];

struct g_admin_cmd {
	const char *keyword;
	qboolean (* const handler)(gentity_t *ent, int skiparg);
	char flag;
	const char *function;
};

static const struct g_admin_cmd g_admin_cmds[] = {
	{"levadd",		G_admin_add_level,	'E',	"Add a new level"},
	{"admintest",	G_admin_admintest,	'a',	"displays your current admin level"},
	{"ban",			G_admin_ban,		'b',	"bans target player"},
	{"cancelvote",  G_admin_cancelvote, 'C',	"cancels the current vote in progress"},
	{"levedit",		G_admin_edit_level, 'E',	"used to edit level properties"},
	{"nogoto",		G_admin_disable_goto,'T',	"target can not use goto or call"},
	{"nosave",		G_admin_disable_save,'T',	"target can not use save or load"},
	{"finger",		G_admin_finger,		'f',	"displays target's admin level"},
	{"help",		G_admin_help,		'h',	"displays info about commands"},
	{"listcmds",	G_admin_help,		'h',	"displays info about commands"},
	{"kick",		G_admin_kick,		'k',	"kicks target player"},
	{"levinfo",		G_admin_levinfo,	'E',	"prints info about level"},
	{"levlist",		G_admin_levlist,	'E',	"prints all current levels"},
	{"listbans",	G_admin_listbans,	'L',	"lists all current bans"},
	{"map",			G_admin_map,		'M',	"changes map"},
	{"mute",		G_admin_mute,		'm',	"mutes target player"},
	{"passvote",	G_admin_passvote,	'P',	"passes the current vote in progress"},
	{"putteam",		G_admin_putteam,	'p',	"puts target to a team"},
	{"readconfig",	G_admin_readconfig,	'G',	"reads admin config"},
	{"rename",		G_admin_rename,		'R',	"renames the target player"},
	{"restart",		G_admin_restart,	'r',	"restarts the map"},
	{"rmsaves",		G_admin_remove_saves,'T',	"clear saved positions of target"},
//	{"setlevel",	G_admin_setlevel,	's',	"sets target level"},
	{"spec",		G_admin_spec,		'S',	"spectate target"},
	{"unban",		G_admin_unban,		'b',	"unbans #"},
	{"unmute",		G_admin_unmute,		'm',	"unmutes target player"},
	{"",			NULL,				'\0',	""}
};
// Prints on both chat & console.
void G_admin_chat_print(char *string) {
	AP(va("chat \"%s", string));
	G_Printf("%s\n", string);
}
// pm + console
void G_admin_personal_info_print(gentity_t *ent, char *string) {
	if(ent)	CP(va("chat \"%s\"", string));
	else G_Printf("%s\n", string);
}

void G_admin_print(gentity_t *ent, char *m)
{

	if(ent) CP(va("print \"%s\"", m));
	else {
		char m2[MAX_STRING_CHARS];
		DecolorString(m, m2);
		G_Printf(m2);
	}
}

void G_shrubbot_buffer_begin()
{
	bigTextBuffer[0] = '\0';
}

void G_shrubbot_buffer_end(gentity_t *ent)
{
	ASP(bigTextBuffer);
}

void G_admin_buffer_print(gentity_t *ent, char *string) {
	if(!ent) {
		char string2[MAX_STRING_CHARS];
		DecolorString(string, string2);

		if(strlen(string2) + strlen(bigTextBuffer) > 239) {
			ASP(bigTextBuffer);
			bigTextBuffer[0] = '\0';
		}
		Q_strcat(bigTextBuffer, sizeof(bigTextBuffer), string2);
	} else {
		if(strlen(string) + strlen(bigTextBuffer) >= 1009) {
			ASP(bigTextBuffer);
			bigTextBuffer[0] = '\0';
		}
		Q_strcat(bigTextBuffer, sizeof(bigTextBuffer), string);
	}
}
// returns true if caller is higher, if equal returns true if equal aswell
qboolean G_admin_higher(gentity_t *ent, gentity_t *target, qboolean equal) {
	if(!ent) return qtrue;
	if(!target) return qfalse;

	if(equal) {
		if(target->client->sess.uinfo.level <= ent->client->sess.uinfo.level) {
			return qtrue;
		} else {
			return qfalse;
		}
	} else {
		if(target->client->sess.uinfo.level < ent->client->sess.uinfo.level) {
			return qtrue;
		} else {
			return qfalse;
		}
	}
}

int userCount = 0;
int levelCount = 0;
int banCount = 0;

void G_admin_cleanup()
{
	int i = 0;
	userCount = 0;
	levelCount = 0;
	banCount = 0;

	for(i=0; g_admin_levels[i]; i++) {
		free(g_admin_levels[i]);
		g_admin_levels[i] = NULL;
	}
	for(i=0; g_admin_users[i]; i++) {
		free(g_admin_users[i]);
		g_admin_users[i] = NULL;
	}
}
/*
SQLite version of the database. Need to write functions for readconfig, setlevel,
addlevel, editlevel, levelinfo.
*/

// Used to check if there's anything in a table

int countRows(void *pData, int numFields, char **pFields, char **pColNames) {
	int *pRowNumber = (int*)pData;
	(*pRowNumber)++;
	return 0;
}

/*
=============================
G_admin_default_database
Creates an empty database 
=============================
*/

qboolean G_admin_default_database() {
	int retval = -1;
	sqlite3 *userDatabase = NULL;
	char *errorMessage = NULL;
	char filename[MAX_TOKEN_CHARS];

	Q_strncpyz(filename, va("etjump/%s", g_admin.string), sizeof(filename));

	// Let's open the database
	retval = sqlite3_open(filename, &userDatabase);
	// Failed to open database
	if(retval != SQLITE_OK) {
		G_Printf("Database error: failed to open database.\n");
		return qfalse;
	}

	// Let's create tables for bans, levels & users.

	sqlite3_exec(userDatabase, "create table if not exists bans(\
							   name varchar(36),\
							   ip varchar(18),\
							   guid varchar(32),\
							   reason varchar(256),\
							   made varchar(50),\
							   expires INTEGER,\
							   banner varchar(36));", 
							   0,
							   0,
							   &errorMessage);
	// Let's check if there was any problems creating the table
	if(errorMessage) {
		G_Printf("Database error: %s", errorMessage);
		// Zero: should we back up the admin db in this case
		return qfalse;
	}

	sqlite3_exec(userDatabase, "create table if not exists levels(\
							   level INTEGER,\
							   name VARCHAR(36),\
							   commands VARCHAR(36),\
							   greeting VARCHAR(256));",
							   0,
							   0,
							   &errorMessage);

	if(errorMessage) {
		G_Printf("Database error: %s", errorMessage);
		// Zero: should we back up the admin db in this case
		return qfalse;
	}

	sqlite3_exec(userDatabase, "CREATE TABLE IF NOT EXISTS users(\
							   level INTEGER,\
							   name VARCHAR(36),\
							   password VARCHAR(40),\
							   guid VARCHAR(32));",
							   0,
							   0,
							   &errorMessage);

	if(errorMessage) {
		G_Printf("Database error: %s", errorMessage);
		// Zero: should we back up the admin db in this case
		return qfalse;
	}
	G_Printf("Database error: couldn't find a user database. Creating a new one.\n");
	sqlite3_close(userDatabase);
	// We do not need to create anything else.
	return qtrue;
}

/*
Readconfig sqlite callback functions
*/

// Used to count users/levels/bans on readconfig

//////////////////////////////////////////////////
// levelsCallback
// Gets all the admin levels from the config
//////////////////////////////////////////////////

// Return != 0 if a fail occurs.
int levelsCallback(void *pData, int numFields, char **pFields, char **pColNames) {
	int *pRowNumber = (int*)pData; // Used to pass row count to original function
	admin_level_t *temp_level = NULL;

	if(levelCount > MAX_ADMIN_LEVELS) {
		G_Printf("User database: max level count exceeded.\n");
		return 1;
	}

	temp_level = (admin_level_t*)malloc(sizeof(admin_level_t));
	// Just to be sure nothing bad happens
	temp_level->level = 0;
	*temp_level->greeting = '\0';
	*temp_level->name = '\0';
	*temp_level->commands = '\0';

	// There are four fields for levels: level, name, commands, greeting
	if(numFields != 4) {
		G_Printf("Database error: invalid field count on row %i", pRowNumber);
		free(temp_level);
		return 1;
	}

	// Increase row count so calling function knows it too.
	(*pRowNumber)++;

	temp_level->level = atoi(pFields[0]);
	// FIXME: These should use Q_strncpy.
	Q_strncpyz(temp_level->name, pFields[1], sizeof(temp_level->name));
	Q_strncpyz(temp_level->commands, pFields[2], sizeof(temp_level->commands));
	Q_strncpyz(temp_level->greeting, pFields[3], sizeof(temp_level->greeting));
	// Add the level to level array.
	g_admin_levels[levelCount++] = temp_level;

	return 0; // SUCCESS!
}

//////////////////////////////////////////////////
// usersCallback
// Gets all the admin users from the config
//////////////////////////////////////////////////

int usersCallback(void *pData, int numFields, char **pFields, char **pColNames) {
	int *pRowNumber = (int*)pData; // Used to pass row count to original function
	admin_user_t *temp_user = NULL;

	if(userCount > MAX_USERS) {
		G_Printf("User database: max user count exceeded.\n");
		return 1;
	}

	temp_user = (admin_user_t*)malloc(sizeof(admin_user_t));
	// Just to be sure nothing bad happens
	temp_user->level = 0;
	*temp_user->name = '\0';
	*temp_user->password = '\0';

	// There are three fields for users: level, name, password
	if(numFields != 3) {
		G_Printf("Database error: invalid field count on row %i", pRowNumber);
		free(temp_user);
		return 1;
	}

	// Increase row count so calling function knows it too.
	(*pRowNumber)++;

	temp_user->level = atoi(pFields[0]);
	// FIXME: These should use Q_strncpy.
	Q_strncpyz(temp_user->name, pFields[1], sizeof(temp_user->name));
	Q_strncpyz(temp_user->password, pFields[2], sizeof(temp_user->password));
	// Add the level to level array.
	g_admin_users[userCount++] = temp_user;

	return 0; // SUCCESS!
}

//////////////////////////////////////////////////
// bansCallback
// Gets all the bans from the config
//////////////////////////////////////////////////

int bansCallback(void *pData, int numFields, char **pFields, char **pColNames) {
	int *pRowNumber = (int*)pData; // Used to pass row count to original function
	admin_ban_t *temp_ban = NULL;

	if(banCount > MAX_BANS) {
		G_Printf("User database: max ban count exceeded.\n");
		return 1;
	}

	temp_ban = (admin_ban_t*)malloc(sizeof(admin_ban_t));
	// Just to be sure nothing bad happens
	*temp_ban->name = '\0';
	*temp_ban->ip = '\0';
	*temp_ban->reason = '\0';
	*temp_ban->made = '\0';
	temp_ban->expires = 0;
	*temp_ban->banner = '\0';

	// There are six fields for bans: name, ip, reason, made, expires, banner
	if(numFields != 6) {
		G_Printf("Database error: invalid field count on row %i", pRowNumber);
		free(temp_ban);
		return 1;
	}

	// Increase row count so calling function knows it too.
	(*pRowNumber)++;

	Q_strncpyz(temp_ban->name, pFields[0], sizeof(temp_ban->name));
	Q_strncpyz(temp_ban->ip, pFields[1], sizeof(temp_ban->ip));
	Q_strncpyz(temp_ban->reason, pFields[2], sizeof(temp_ban->reason));
	Q_strncpyz(temp_ban->made, pFields[3], sizeof(temp_ban->made));
	temp_ban->expires = atoi(pFields[4]);
	Q_strncpyz(temp_ban->banner, pFields[5], sizeof(temp_ban->banner));
	// Add the bans to ban array.
	g_admin_bans[banCount++] = temp_ban;

	return 0; // SUCCESS!
}


qboolean G_admin_readconfig(gentity_t *ent, int skiparg) {
	int retval[3];
	sqlite3 *userDatabase;
	int bans = 0;
	int levels = 0;
	int users = 0;
	char *errorMessage = NULL;
	char filename[MAX_TOKEN_CHARS];

	Q_strncpyz(filename, va("etjump/%s", g_admin.string), sizeof(filename));

	retval[0] = sqlite3_open(filename, &userDatabase);

	// Failed to open database
	if(retval[0] != SQLITE_OK) {
		G_Printf("Database error: failed to open database.\n");
		return qfalse;
	}

	G_admin_cleanup();

	retval[0] = sqlite3_exec(userDatabase, "select * from bans", bansCallback, &bans, &errorMessage);
	retval[1] = sqlite3_exec(userDatabase, "select * from levels", levelsCallback, &levels, &errorMessage);
	retval[2] = sqlite3_exec(userDatabase, "select * from users", usersCallback, &users, &errorMessage);

	sqlite3_close(userDatabase);

	// Nothing could be found
	if(!bans && !levels && !users) {
		G_admin_default_database();
		return qfalse;
	}

	AIP(ent, va("^3readconfig: ^7loaded %d levels, %d users and %d bans", levelCount, userCount, banCount));

	G_admin_identify_all();

	return qtrue;
}

// Used to add a single level (int)

qboolean G_admin_add_level(gentity_t *ent, int skiparg) {
	sqlite3 *userDatabase;
	char *errorMessage;
	int rows = 0;
	admin_level_t *temp;
	int returnValue = 0;
	char query[MAX_STRING_CHARS];
	char filename[MAX_STRING_CHARS];
	char levelarg[MAX_TOKEN_CHARS];
	int level = 0;

	Q_strncpyz(filename, va("etjump/%s", g_admin.string), sizeof(filename));

	returnValue = sqlite3_open(filename, &userDatabase);

	if(returnValue != SQLITE_OK) {
		G_Printf("Database error: failed to open database.\n");
		return qfalse;
	}

	if(Q_SayArgc() - skiparg != 2) {
		AIP(ent, "^3usage:^7 !addlevel <level>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, levelarg, sizeof(levelarg));
	level = atoi(levelarg);

	if(level < 0) {
		return qtrue;
	}
	
	// Let's check if we can find the level already.
	Com_sprintf(query, sizeof(query), "select * from levels where level = '%i'", level);

	returnValue = sqlite3_exec(userDatabase, query, countRows, &rows, &errorMessage);

	if(rows) {
		AIP(ent, va("^3addLevel: ^7level %i exists already.", level));
		// Exists already
		return qtrue;		
	}

	temp = (admin_level_t*) malloc(sizeof(admin_level_t));

	temp->level = 0;
	*temp->name = '\0';
	*temp->commands = '\0';
	*temp->greeting = '\0';

	Com_sprintf(query, sizeof(query), "insert into levels values('%i', '%s', '%s', '%s')",
		level,
		'\0',
		'\0',
		'\0');

	returnValue = sqlite3_exec(userDatabase, query, 0, 0, &errorMessage);
	if(errorMessage) {
		AIP(ent, "Database error: failed to add level.");
		free(temp);
		return qfalse;
	}
	sqlite3_close(userDatabase);

	temp->level = level;

	g_admin_levels[levelCount++] = temp;
	return qtrue;
}

// !editlevel <level> <param> <commands|greeting|name>
// params: -name, -gtext, -cmds

#define LEVEDIT_NAME 0
#define	LEVEDIT_COMMANDS 1
#define LEVEDIT_GREETING 2

qboolean G_admin_edit_level( gentity_t *ent, int skiparg ) {
	sqlite3 *userDatabase = NULL;
	char *errorMessage = NULL;
	int rows = 0, i = 0;
	admin_level_t *temp = NULL;
	int returnValue = 0;
	char query[MAX_STRING_CHARS];
	char filename[MAX_STRING_CHARS];
	char arg1[MAX_TOKEN_CHARS] = "\0";
	char arg2[MAX_TOKEN_CHARS] = "\0";
	char *arg3 = NULL;
	int level = 0;
	unsigned type = -1;

	Q_strncpyz(filename, va("etjump/%s", g_admin.string), sizeof(filename));

	returnValue = sqlite3_open(filename, &userDatabase);

	if(returnValue != SQLITE_OK) {
		G_Printf("Database error: failed to open user database.\n");
		return qfalse;
	}

	if(Q_SayArgc() < 4 + skiparg) {
		AIP(ent, "^3usage: ^7!editlevel <level> <param> <commands|greeting|name>");
		return qfalse;
	}

	Q_SayArgv(1, arg1, sizeof(arg1)); // level
	Q_SayArgv(2, arg2, sizeof(arg2)); // param
	arg3 = Q_SayConcatArgs(3+skiparg);


	level = atoi(arg1);
	if(level < 0) {
		AIP(ent, "^3editlevel: ^7invalid level specified.");
		return qtrue;
	}

	temp = (admin_level_t*) malloc(sizeof(admin_level_t));
	temp->level = 0;
	*temp->name = '\0';
	*temp->commands = '\0';
	*temp->greeting = '\0';

	if(!Q_strncmp(arg2, "-name", 5)) {
		type = LEVEDIT_NAME;
		Q_strncpyz(temp->name, arg3, sizeof(temp->name));
		Com_sprintf(query, sizeof(query), "update levels set name='%s' where level='%i'",
										  temp->name,
										  level);
	} else if (!Q_strncmp(arg2, "-gtext", 6)) {
		type = LEVEDIT_GREETING;
		Q_strncpyz(temp->greeting, arg3, sizeof(temp->greeting));
		Com_sprintf(query, sizeof(query), "update levels\
										  set greeting='%s'\
										  where level='%i'",
										  temp->greeting,
										  level);
	} else if (!Q_strncmp(arg2, "-cmds", 5)) {
		type = LEVEDIT_COMMANDS;
		Q_strncpyz(temp->commands, arg3, sizeof(temp->commands));
		Com_sprintf(query, sizeof(query), "update levels\
										  set commands='%s'\
										  where level='%i'",
										  temp->commands,
										  level);
	} else {
		free(temp);
		AIP(ent, "^3editlevel: ^7invalid parameter specified");
		return qfalse;
	}

	returnValue = sqlite3_exec(userDatabase, query, 0, 0, &errorMessage);

	if(errorMessage) {
		free(temp);
		G_Printf("Database error: %s", errorMessage);
		return qfalse;
	}

	// Let's copy the new data to the array aswell.
	for(i = 0; i < levelCount; i++) {
		if(g_admin_levels[i]->level == level) {
			if(type == LEVEDIT_COMMANDS) {
				Q_strncpyz(g_admin_levels[i]->commands, temp->commands, sizeof(g_admin_levels[i]->commands));
			} else if(type == LEVEDIT_GREETING) {
				Q_strncpyz(g_admin_levels[i]->greeting, temp->greeting, sizeof(g_admin_levels[i]->greeting));
			} else if(type == LEVEDIT_NAME) {
				Q_strncpyz(g_admin_levels[i]->name, temp->name, sizeof(g_admin_levels[i]->name));
			}
		}
	}
	free(temp);
	return qtrue;
}

// !levinfo 

qboolean G_admin_levinfo( gentity_t *ent, int skiparg ) {
	sqlite3 *userDatabase = NULL;
	char *errorMessage = NULL;
	int returnValue = 0;
	char filename[MAX_STRING_CHARS];
	char arg1[MAX_TOKEN_CHARS];
	int level = 0, i = 0;
	qboolean found = qfalse;

	Q_strncpyz(filename, va("etjump/%s", g_admin.string), sizeof(filename));

	returnValue = sqlite3_open(filename, &userDatabase);

	if(returnValue != SQLITE_OK) {
		G_Printf("Database error: failed to open user database.\n");
		return qfalse;
	}

	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage: ^7!levinfo <level>");
		return qfalse;
	}

	Q_SayArgv(1, arg1, sizeof(arg1)); // level

	level = atoi(arg1);

	if(level < 0) {
		AIP(ent, "^3levinfo:^7 invalid level specified.");
		return qfalse;
	}

	for(i = 0; i < levelCount; i++) {
		if(g_admin_levels[i]->level == level) {
			found = qtrue;
			break;
		}
	}

	if(found) {
		if(ent) {
			CP(va("print \"LEVEL: %i\nNAME: %s\nCOMMANDS: %s\nGREETING: %s\n", 
				g_admin_levels[i]->level,
				g_admin_levels[i]->name,
				g_admin_levels[i]->commands,
				g_admin_levels[i]->greeting));
		} else {
			G_Printf("LEVEL: %i\nNAME: %s\nCOMMANDS: %s\nGREETING: %s\n", 
				g_admin_levels[i]->level,
				g_admin_levels[i]->name,
				g_admin_levels[i]->commands,
				g_admin_levels[i]->greeting);
		}
	} else {
		AIP(ent, "^3levinfo: ^7can't find level specified.");
		return qfalse;
	}
	return qtrue;
}
//FIXME
qboolean G_admin_levlist(gentity_t *ent, int skiparg) {
	int i = 0;
	if(ent) {
		CP("print \"LEVELS\n\"");
	}
	else {
		G_Printf("LEVELS\n");
	}


	for(i = 0; i < levelCount; i++) {
		if(ent) {
			CP(va("print \"%i\n\"", g_admin_levels[i]->level));
		} else {
			G_Printf("%i\n", g_admin_levels[i]->level);
		}
	}
	return qtrue;
}

//////////////////////////////////////////
// Setlevel command and stuff related to it

// Temporary admindata used in setlevel
admin_t admindata;

// !Setlevel <client> <level>

qboolean G_admin_setlevel( gentity_t *ent, int skiparg ) {
	int level = 0;
	int i = 0;
	int pids[MAX_CLIENTS];
	qboolean found = qfalse;
	char err[MAX_STRING_CHARS];
	char arg[MAX_TOKEN_CHARS];

	gentity_t *target;

	if(Q_SayArgc() != 3 + skiparg) {
		AIP(ent,(va("^3usage: ^7setlevel <user> <level>")));
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg, sizeof(arg));

	if(ClientNumbersFromString(arg, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!setlevel: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(ent != target && !G_admin_higher(ent, target, qtrue)) {
		AIP(ent, "^3setlevel: ^7you cannot set the level of a fellow admin");
		return qfalse;
	}

	// Allows client to use the register cmd
	target->client->sess.allowRegister = qtrue;

	Q_SayArgv(2 + skiparg, arg, sizeof(arg));

	level = atoi(arg);

	if(ent) {
		if(level > G_admin_get_level(ent)) {
			AIP(ent, "^3setlevel: ^7you may not setlevel higher than your current level.");
			return qfalse;
		}
	}

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			admindata.level = level;
			found = qtrue;
			break;
		}
	}

	if(!found) {
		AIP(ent, "^3setlevel:^7 level not found.");
		return qfalse;
	}

	trap_SendServerCommand(pids[0], "server_setlevel");
	return qtrue;
}




// Returns pointer to a player if found a single matching player.
// else returns a nullpointer & error.
// FIXME: error does not work properly for some reason.
// G_MatchOnePlayer does work properly but in the function calling
// this one the error will be 3 chars long no matter what.
// Feen: (For Nolla) Fixed?
gentity_t *getPlayerForName(char *name, char *err, int size) {
	int pids[MAX_CLIENTS];
	gentity_t *player;

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, size);
		return NULL;
	}

	player = g_entities + pids[0];
	return player;
}


qboolean G_admin_permission(gentity_t *ent, char flag) {
	int i;
	char *flags;

	if(!ent) return qtrue;

	for(i = 0; g_admin_levels[i]; i++) {
		if(ent->client->sess.uinfo.level == 
		   g_admin_levels[i]->level) {
			   flags = g_admin_levels[i]->commands;
			while(*flags) {
				if(*flags == flag)
					return qtrue;
				else if(*flags == '-') {
					while(*flags++) {
						if(*flags == flag)
							return qfalse;
						else if(*flags == '+')
							break;
					}
				}
				else if(*flags == '*') {
					while(*flags++) {
						if(*flags == flag)
							return qfalse;
					}
					return qtrue;
				}
			*flags++;
			}
		}
	}
	return qfalse;
}

qboolean G_admin_ban_check(char *userinfo, char *reason)
{
	char *ip;
	int i;
	time_t t;
	int seconds = 0; // Dens: Perm is default

	if(!time(&t)) return qfalse;
	t = t - ADMIN_BAN_EXPIRE_OFFSET;
	if(!*userinfo) return qfalse;
	ip = Info_ValueForKey(userinfo, "ip");
	if(!*ip) return qfalse;
	for(i=0; g_admin_bans[i]; i++) {
		// 0 is for perm ban
		if(g_admin_bans[i]->expires != 0 &&
			(g_admin_bans[i]->expires - t) < 1)
			continue;
		// Dens: lets find out seconds now, we need that later
		if(g_admin_bans[i]->expires != 0){
			seconds = g_admin_bans[i]->expires - t;
		}
		if(strstr(ip, g_admin_bans[i]->ip)) {
			// Dens: check if there is a reason, than check if the ban expires
			if(*g_admin_bans[i]->reason) {
				if(seconds == 0){
					Com_sprintf(
						reason,
						MAX_STRING_CHARS,
						"Reason: %s\nExpires: NEVER.\n",
						g_admin_bans[i]->reason
					);
				}else{
					Com_sprintf(
					reason,
					MAX_STRING_CHARS,
					"Reason: %s\nExpires in: %i seconds.\n",
					g_admin_bans[i]->reason,
					seconds
					);
				}
			}else{
				if(seconds == 0){
					Com_sprintf(
						reason,
						MAX_STRING_CHARS,
						"Expires: NEVER.\n"
					);
				}else{
					Com_sprintf(
					reason,
					MAX_STRING_CHARS,
					"Expires in: %i seconds.\n",
					seconds
					);
				}
			}
			return qtrue;
		}
	}
	return qfalse;
}

qboolean G_admin_cmd_check(gentity_t *ent) {
	int i;
	char command[MAX_CMD_LEN];
	char *cmd;
	int skip = 0;

	if(!*g_admin.string) return qfalse;

	command[0] = '\0';

	Q_SayArgv(0, command, sizeof(command));
	if(!Q_stricmp(command, "say") || !Q_stricmp(command, "enc_say")) {
		skip = 1;
		Q_SayArgv(1, command, sizeof(command));
	}

	if(!*command) return qfalse;

	// Store cmd without ! in cmd
	if(command[0] == '!') cmd = &command[1];
	else if(!ent) cmd = &command[0];
	else return qfalse;

	for(i = 0; g_admin_cmds[i].keyword[0]; i++) {
		if(Q_stricmp(cmd, g_admin_cmds[i].keyword)) {
			continue;
		}
		else if(G_admin_permission(ent, g_admin_cmds[i].flag)) {
			g_admin_cmds[i].handler(ent, skip);
			if(g_logCommands.integer) {
				if(ent)
					G_ALog("Command: \\!%s\\%s\\%s\\", g_admin_cmds[i].keyword, ent->client->pers.netname, ent->client->sess.uinfo.ip);
				else 
					G_ALog("Command: \\!%s\\console\\", g_admin_cmds[i].keyword);
			}
			return qtrue;
		}
		else {
			AIP(ent, va("^3%s: ^7permission denied", g_admin_cmds[i].keyword));
			return qfalse;
		}
	}
	return qfalse;
}

int G_admin_get_level(gentity_t *ent) {
	return ent->client->sess.uinfo.level;
}

qboolean G_admin_admintest(gentity_t *ent, int skiparg) {
	int level, i;
	char name[MAX_NETNAME];
	if(!ent) return qtrue;
	level = ent->client->sess.uinfo.level;

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			Q_strncpyz(name, g_admin_levels[i]->name, sizeof(name));
			break;
		}
	}

	ACP(va("^3admintest: ^7%s^7 is a level %d user (%s^7)", ent->client->pers.netname, level , name));
	return qtrue;
}

void G_admin_login(gentity_t *ent) {
	int i;
	qboolean found = qfalse;
	char arg[MAX_TOKEN_CHARS];
	char password[PASSWORD_LEN+1];
	char userinfo[MAX_INFO_STRING];
	char *value;

	if(trap_Argc() != 2) {
		return;
	}
	// Don't want people to change password too many times.
	ent->client->sess.password_change_count++;
	
	trap_Argv(1, arg, sizeof(arg));

	trap_GetUserinfo(ent->client->ps.clientNum, userinfo, sizeof(userinfo));

	value = Info_ValueForKey(userinfo, "ip");

	Q_strncpyz(password, G_SHA1(arg), sizeof(password));
	Q_strncpyz(ent->client->sess.uinfo.password, G_SHA1(arg), sizeof(ent->client->sess.uinfo.password));

	for(i = 0; g_admin_users[i]; i++) {
		if(!Q_stricmp(g_admin_users[i]->password, password)) {
			ent->client->sess.uinfo.level = g_admin_users[i]->level;
			Q_strncpyz(ent->client->sess.uinfo.name, g_admin_users[i]->name, sizeof(ent->client->sess.uinfo.name));
			found = qtrue;
			break;
		}
	}
	if(!found) {
		ent->client->sess.uinfo.level = 0;
		Q_strncpyz(ent->client->sess.uinfo.name, ent->client->pers.netname, sizeof(ent->client->sess.uinfo.name));
	}
	G_ALog("Login: \\user\\%s\\username\\%s\\ip\\%s\\", ent->client->pers.netname,
		ent->client->sess.uinfo.name, ent->client->sess.uinfo.ip);
}
///////////////////
// Kick
// Usage: !kick <player> <reason> <timeout> - timeout/reason not necessary

qboolean G_admin_kick(gentity_t *ent, int skiparg) {
	int timeout = 0;
	int pids[MAX_CLIENTS];
	char err[MAX_STRING_CHARS];
	char name[MAX_TOKEN_CHARS];
	char reason[MAX_TOKEN_CHARS] = "Player kicked!";
	char length[MAX_TOKEN_CHARS];
	gentity_t *target;

	if(Q_SayArgc() - skiparg < 2 || Q_SayArgc() - skiparg > 4) {
		AIP(ent, "^3usage: ^7kick <player> <reason> <timeout>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!kick: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(ent) {
		if(ent != target && !G_admin_higher(ent, target, qfalse)) {
			AIP(ent, "^3kick:^7 you can't kick a fellow admin");
			return qfalse;
		} else if(target == ent) {
			AIP(ent, "^3kick:^7 you can't kick yourself");
			return qfalse;
		}
	}

	if(Q_SayArgc() -skiparg >= 3) {
		Q_SayArgv(2 + skiparg, reason, sizeof(reason));
	}

	if(Q_SayArgc() -skiparg == 4) {
		Q_SayArgv(3 + skiparg, length, sizeof(length));
		timeout = atoi(length);
	}

	trap_DropClient(pids[0], reason, timeout);
	return qtrue;
}

// Handles setlevel on connect &
// greeting

void G_admin_identify(gentity_t *ent) {
	int clientNum;
	clientNum = ent->client->ps.clientNum;

	trap_SendServerCommand(clientNum, "identify_self");
}

void G_admin_identify_all() {
	int i;
	for(i = 0; i < level.numConnectedClients; i++) {
		trap_SendServerCommand(i, "identify_self");
	}
}

void G_admin_greeting(gentity_t *ent) {
	char *greeting = "";
	qboolean broadcast = qfalse;
	int level = 0;
	int i;

	if( !ent ) {
		return;
	}

	if( !ent->client->sess.need_greeting ) {
		return;
	}

	for(i = 0; g_admin_users[i]; i++) {
		if(!Q_stricmp(ent->client->sess.uinfo.password, g_admin_users[i]->password)) {
			level = g_admin_users[i]->level;
		}
	}

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			if(*g_admin_levels[i]->greeting)
				greeting = Q_StrReplace( g_admin_levels[i]->greeting, "[n]", ent->client->pers.netname);
			break;
		}
	}

	if(*greeting) {
		broadcast = qtrue;
	}

	if(broadcast) {
		ent->client->sess.need_greeting = qfalse;
		ACP(va("%s", greeting));
	}
}

qboolean G_admin_finger(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	int i;
	char err[MAX_STRING_CHARS];
	char name[MAX_TOKEN_CHARS];
	char levelname[MAX_NETNAME];
	gentity_t *target;

	if(Q_SayArgc() - skiparg != 2) {
		AIP(ent, "^3usage:^7 !finger <name>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!finger: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	// Just in case

	if(!target) return qfalse;

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == target->client->sess.uinfo.level) {
			Q_strncpyz(levelname, g_admin_levels[i]->name, sizeof(levelname));
			break;
		}
	}

	ACP(va("^3finger:^7 %s ^7(%s^7) is a level %d user (%s^7)", target->client->pers.netname,
		target->client->sess.uinfo.name, target->client->sess.uinfo.level, levelname));
	return qtrue;
}

qboolean G_admin_mute(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	gentity_t *target;
	char err[MAX_STRING_CHARS];
	char name[MAX_TOKEN_CHARS];

	if(Q_SayArgc() - skiparg != 2) {
		AIP(ent, "^3usage:^7 !mute <name>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!mute: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(ent) {
		if(ent != target && !G_admin_higher(ent, target, qfalse)) {
			AIP(ent, "^3mute: ^7You cannot mute a fellow admin");
			return qfalse;
		}
		if(target == ent) {
			AIP(ent, "^3mute: ^7You cannot mute yourself");
			return qfalse;
		}
	}

	if(target->client->sess.muted) {
		AIP(ent, "^3mute:^7 target is already muted");
		return qfalse;
	}

	target->client->sess.muted = qtrue;

	CPx(pids[0], "print \"^5You've been muted\n\"" );

	ACP(va("%s ^7has been muted", target->client->pers.netname));

	return qtrue;
}

qboolean G_admin_unmute(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;
	char name[MAX_TOKEN_CHARS];

	if(Q_SayArgc() - skiparg != 2) {
		AIP(ent, "^3usage:^7 !unmute <name>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!unmute: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(!target->client->sess.muted) {
		AIP(ent, "^3unmute: ^7target is not muted");
		return qfalse;
	}

	target->client->sess.muted = qfalse;

	CPx(pids[0], "print \"^5You've been unmuted\n\"" );

	ACP(va("%s ^7has been unmuted", target->client->pers.netname));

	return qtrue;
}

qboolean G_admin_passvote(gentity_t *ent, int skiparg) {
	if(level.voteInfo.voteTime) {
		level.voteInfo.voteNo = 0;
		level.voteInfo.voteYes = level.numConnectedClients;
		ACP("^3passvote:^7 vote has been passed");
	}
	else {
		ACP("^3passvote:^7 no vote in progress");
	}
	return qtrue;
}

qboolean G_admin_cancelvote(gentity_t *ent, int skiparg) {
	if(level.voteInfo.voteTime) {
		level.voteInfo.voteYes = 0;
		level.voteInfo.voteNo = level.numConnectedClients;
		ACP("^3cancelvote:^7 vote has been canceled");
	} else {
		ACP("^3cancelvote:^7 no vote in progress");
	}
	return qtrue;
}

// FIXME: renames but the clientside cvar still stays the same.

qboolean G_admin_rename(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	char *newname, *oldname;
	char arg[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	char userinfo[MAX_INFO_STRING];
	gentity_t *target;
	if(Q_SayArgc() < 3+skiparg) {
		AIP(ent, "^3usage: ^7!rename <name> <newname>");
		return qfalse;
	}

	Q_SayArgv(1+skiparg, arg, sizeof(arg));

	newname = Q_SayConcatArgs(2+skiparg);

	if(ClientNumbersFromString(arg, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!rename: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(ent) {
		if(ent != target && !G_admin_higher(ent, target, qfalse)) {
			AIP(ent, "^3rename: ^7you cannot rename a fellow admin");
			return qfalse;
		}
	}

	trap_GetUserinfo( pids[0], userinfo, sizeof( userinfo ) );

	oldname = Info_ValueForKey( userinfo, "name" );

	AP(va("chat \"^7rename: %s^7 has been renamed to %s\" -1",
		oldname,
		newname));
	Info_SetValueForKey( userinfo, "name", newname);
	trap_SetUserinfo( pids[0], userinfo );
	target->client->sess.nameChangeCount--;
	ClientUserinfoChanged(pids[0]);
	return qtrue;
}

// !putteam <name> <team>

qboolean G_admin_putteam(gentity_t *ent, int skiparg) {
	char arg[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	int pids[MAX_CLIENTS];
	gentity_t *target;

	if(Q_SayArgc() != 3 + skiparg) {
		AIP(ent, "^3usage:^7 !putteam <name> <team>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg, sizeof(arg));

	if(ClientNumbersFromString(arg, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3!putteam: ^7%s", err));
		return qfalse;
	}

	target = g_entities + pids[0];

	if(ent) {
		if(!G_admin_higher(ent, target, qtrue)) {
			AIP(ent, "^3putteam:^7 you cannot putteam a fellow admin");
			return qfalse;
		}
	}

	Q_SayArgv(2 + skiparg, arg, sizeof(arg));

	target->client->sess.lastTeamSwitch = level.time;

	SetTeam(target, arg, qfalse, -1, -1, qtrue);

	return qtrue;
}

qboolean G_admin_help(gentity_t *ent, int skiparg) {
	int i;

	if(Q_SayArgc() == 1 + skiparg) {

		int j = 0;
		int count = 0;
		char *str = "";

		for(i = 0; g_admin_cmds[i].keyword[0]; i++) {

			if(G_admin_permission(ent, g_admin_cmds[i].flag)) {
				if(j == 6) {
					str = va( "%s\n^3%-12s", str, g_admin_cmds[i].keyword);
					j = 0;
				} else {
					str = va( "%s\n^3%-12s", str, g_admin_cmds[i].keyword);
				}
				j++;
				count++;
			}
		}
		AIP(ent, "^3help: ^7check console for more information");
		ABP_begin();
		ASP(str);
		if(ent) CP("print \"\n");
		else G_Printf("\n");
	}
	else {
		char parameter[20];

		Q_SayArgv(1+skiparg, parameter, sizeof(parameter));

		for(i = 0; g_admin_cmds[i].keyword[0]; i++) {
			if(!Q_stricmp(parameter, g_admin_cmds[i].keyword)) {
				if(!G_admin_permission(ent, g_admin_cmds[i].flag)) {
					AIP(ent, "^3help: ^7permission denied");
					return qfalse;
				}
				AIP(ent, va("^3%s:^7 %s", g_admin_cmds[i].keyword, g_admin_cmds[i].function));
				return qtrue;
			} else {
				continue;
			}
		}
		AIP(ent, "^3help: ^7unknown command");
		return qfalse;
	}
	return qtrue;
}

qboolean G_admin_restart(gentity_t *ent, int skiparg) {
	Svcmd_ResetMatch_f(qfalse, qtrue);
	return qtrue;
}

/*
======================
G_admin_ban
Only uses ip to ban at the moment
======================
*/

qboolean G_admin_ban(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	int seconds;
	char name[MAX_NAME_LENGTH], secs[8];
	char *reason, err[MAX_STRING_CHARS];
	char userinfo[MAX_INFO_STRING];
	char *ip;
	char tmp[MAX_NAME_LENGTH];
	int i;
	admin_ban_t *b = NULL;
	time_t t;
	struct tm *lt;
	gentity_t *target;
	int minargc;
	char duration[MAX_STRING_CHARS];
	int modifier = 1;

	if(!time(&t)) return qfalse;
	
	minargc = 4+skiparg;

	if(Q_SayArgc() < minargc) {
		AIP(ent, "^3usage: ^7!ban <name> <time> <reason>");
		return qfalse;
	}
	
	Q_SayArgv(1+skiparg, name, sizeof(name));
	Q_SayArgv(2+skiparg, secs, sizeof(secs));

	if(secs[0]) {
		int lastchar = strlen(secs) - 1;

		if(secs[lastchar] == 'w')
			modifier = 60*60*24*7;
		else if(secs[lastchar] == 'd')
			modifier = 60*60*24;
		else if(secs[lastchar] == 'h')
			modifier = 60*60;
		else if(secs[lastchar] == 'm')
			modifier = 60;

		if (modifier != 1)
			secs[lastchar] = '\0';
	}

	seconds = atoi(secs);
	if(seconds > 0)
		seconds *= modifier;

	if(seconds <= 0) {
		seconds = 0;
	}

	reason = Q_SayConcatArgs(3+skiparg);

	if(ClientNumbersFromString(name, pids) != 1) {
		G_MatchOnePlayer(pids, err, sizeof(err));
		AIP(ent, va("^3ban: ^7%s", err));
		return qfalse;
	}

	target = &g_entities[pids[0]];
	if(ent) {
		if(ent != target && !G_admin_higher(ent, target, qfalse)) {
			AIP(ent, "^3ban: ^7you cannot ban a fellow admin");
			return qfalse;
		}

		if(ent == target) {
			AIP(ent, "^3ban: ^7You cannot ban yourself");
			return qfalse;
		}
	}

	trap_GetUserinfo(pids[0], userinfo, sizeof(userinfo));
	ip = Info_ValueForKey(userinfo, "ip");

	b = malloc(sizeof(admin_ban_t));

	if(!b)
		return qfalse;

	Q_strncpyz(b->name,
		target->client->pers.netname,
		sizeof(b->name));
	if(ent)
		Q_strncpyz(b->banner,
			ent->client->pers.netname,
			sizeof(b->banner));
	else {
		Q_strncpyz(b->banner,
			"console",
			sizeof(b->banner));
	}

	for(i=0; *ip; ip++) {
		if(i >= sizeof(tmp) || *ip == ':') break;
		tmp[i++] = *ip;
	}

	tmp[i] = '\0';
	Q_strncpyz(b->ip, tmp, sizeof(b->ip));

	lt = localtime(&t);
	strftime(b->made, sizeof(b->made), "%m/%d/%y %H:%M:%S", lt);

	if(!seconds)
		b->expires = 0;
	else
		b->expires = t - ADMIN_BAN_EXPIRE_OFFSET + seconds;
	if(!*reason) {
		Q_strncpyz(b->reason,
			reason,
			sizeof(b->reason));
	}
	else {
		Q_strncpyz(b->reason, reason, sizeof(b->reason));
	}

	for(i = 0; g_admin_bans[i]; i++) {
		if(i == MAX_BANS) {
			AIP(ent, "^3ban: ^7too many bans");
			free(b);
			return qfalse;
		}
	}

	g_admin_bans[i] = b;

	AIP(ent, va("%s^7 has been banned", target->client->pers.netname));
// FIXME	G_admin_writeconfig();

	if(seconds) {
		Com_sprintf(duration,
			sizeof(duration),
			"for %i seconds",
			seconds);
	}
	else {
		Q_strncpyz(duration, "PERMANENTLY", sizeof(duration));
	}

	trap_DropClient(pids[0],
		va("You have been banned %s, Reason: %s",
		duration,
		(*reason) ? reason : "banned by admin"),
		0);
	return qtrue;
}
// Usage: unban <id>
qboolean G_admin_unban(gentity_t *ent, int skiparg) {
	int bnum;
	char bs[5];
	time_t t;

	if(!time(&t)) return qfalse;
	if(Q_SayArgc() < 2+skiparg) {
		AIP(ent, "^3usage: ^7!unban [ban #]");
		return qfalse;
	}
	Q_SayArgv(1+skiparg, bs, sizeof(bs));
	bnum = atoi(bs);
	if(bnum < 1) {
		AIP(ent, "^3unban: ^7invalid ban #");
		return qfalse;
	}
	if(!g_admin_bans[bnum-1]) {
		AIP(ent, "^3unban: ^7invalid ban #");
		return qfalse;
	}
	g_admin_bans[bnum-1]->expires = t - ADMIN_BAN_EXPIRE_OFFSET;
	AIP(ent, va("^3unban: ^7ban #%d removed", bnum));
//FIXME	G_admin_writeconfig();
	return qtrue;
}

void G_admin_duration(int secs, char *duration, int dursize)
{

	if(secs > (60*60*24*365*50) || secs < 0) {
		Q_strncpyz(duration, "PERMANENT", dursize);
	}
	else if(secs > (60*60*24*365*2)) {
		Com_sprintf(duration, dursize, "%d years",
			(secs / (60*60*24*365)));
	}
	else if(secs > (60*60*24*365)) {
		Q_strncpyz(duration, "1 year", dursize);
	}
	else if(secs > (60*60*24*30*2)) {
		Com_sprintf(duration, dursize, "%i months",
			(secs / (60*60*24*30)));
	}
	else if(secs > (60*60*24*30)) {
		Q_strncpyz(duration, "1 month", dursize);
	}
	else if(secs > (60*60*24*2)) {
		Com_sprintf(duration, dursize, "%i days",
			(secs / (60*60*24)));
	}
	else if(secs > (60*60*24)) {
		Q_strncpyz(duration, "1 day", dursize);
	}
	else if(secs > (60*60*2)) {
		Com_sprintf(duration, dursize, "%i hours",
			(secs / (60*60)));
	}
	else if(secs > (60*60)) {
		Q_strncpyz(duration, "1 hour", dursize);
	}
	else if(secs > (60*2)) {
		Com_sprintf(duration, dursize, "%i mins",
			(secs / 60));
	}
	else if(secs > 60) {
		Q_strncpyz(duration, "1 minute", dursize);
	}
	else {
		Com_sprintf(duration, dursize, "%i secs", secs);
	}
}

qboolean G_admin_listbans(gentity_t *ent, int skiparg)
{
	int i, found = 0;
	time_t t;
	char duration[MAX_STRING_CHARS];
	char fmt[MAX_STRING_CHARS];
	int max_name = 1, max_banner = 1;
	int secs;
	int start = 0;
	char skip[11];
	char date[11];
	char *made;
	int j;
	char n1[MAX_NAME_LENGTH] = {""};
	char n2[MAX_NAME_LENGTH] = {""};
	char tmp[MAX_NAME_LENGTH];
	int spacesName;
	int spacesBanner;

	if(!time(&t))
		return qfalse;
	t = t - ADMIN_BAN_EXPIRE_OFFSET;

	for(i=0; g_admin_bans[i]; i++) {
		if(g_admin_bans[i]->expires != 0 &&
			(g_admin_bans[i]->expires - t) < 1) {

			continue;
		}
		found++;
	}

	if(Q_SayArgc() < 3+skiparg) {
		Q_SayArgv(1+skiparg, skip, sizeof(skip));
		start = atoi(skip);
		// tjw: !showbans 1 means start with ban 0
		if(start > 0)
			start -= 1;
		else if(start < 0)
			start = found + start;
	}

	// tjw: sanity check
	if(start >= MAX_BANS || start < 0)
		start = 0;

	for(i=start;
		(g_admin_bans[i] && (i-start) < ADMIN_MAX_SHOWBANS);
		i++) {

		DecolorString(g_admin_bans[i]->name, n1);
		DecolorString(g_admin_bans[i]->banner, n2);
		if(strlen(n1) > max_name) {
			max_name = strlen(n1);
		}
		if(strlen(n2) > max_banner) {
			max_banner = strlen(n2);
		}
	}

	if(start > found) {
		AIP(ent, va("^3listbans: ^7there are only %d active bans", found));
		return qfalse;
	}
	ABP_begin();
	for(i=start;
		(g_admin_bans[i] && (i-start) < ADMIN_MAX_SHOWBANS);
		i++) {

		if(g_admin_bans[i]->expires != 0 &&
			(g_admin_bans[i]->expires - t) < 1) {

			continue;
		}

		// tjw: only print out the the date part of made
		date[0] = '\0';
		made = g_admin_bans[i]->made;
		for(j=0; *made; j++) {
			if((j+1) >= sizeof(date))
				break;
			if(*made == ' ')
				break;
			date[j] = *made;
			date[j+1] = '\0';
			made++;
		}

		secs = (g_admin_bans[i]->expires - t);
		G_admin_duration(secs, duration, sizeof(duration));

		DecolorString(g_admin_bans[i]->name, tmp);
		spacesName = max_name - strlen(tmp);
		DecolorString(g_admin_bans[i]->banner, tmp);
		spacesBanner = max_banner - strlen(tmp);

		Com_sprintf(fmt, sizeof(fmt),

			"^F%%4i^7 %%-%is^7 ^F%%-10s^7 %%-%is^7 ^F%%-9s^7 %%s\n",
			spacesName + strlen(g_admin_bans[i]->name),
			spacesBanner + strlen(g_admin_bans[i]->banner));

		ABP(ent, va(fmt,
			(i+1),
			g_admin_bans[i]->name,
			date,
			g_admin_bans[i]->banner,
			duration,
			g_admin_bans[i]->reason
			));
	}

	ABP(ent, va("^3listbans: ^7listing bans %d - %d of %d\n",
		(start) ? (start + 1) : 0,
		((start + ADMIN_MAX_SHOWBANS) > found) ?
			found : (start + ADMIN_MAX_SHOWBANS),
		found));
	if((start + ADMIN_MAX_SHOWBANS) < found) {
		ABP(ent, va("          type !listbans %d to see more\n",
			(start + ADMIN_MAX_SHOWBANS + 1)));
	}
	ABP_end();
	return qtrue;
}

qboolean G_admin_map(gentity_t *ent, int skiparg) {
	char map[MAX_TOKEN_CHARS];

	if(Q_SayArgc() != 2+skiparg) {
		AIP(ent, "^3usage: ^7!map <map>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, map, sizeof(map));

	trap_SendConsoleCommand(EXEC_APPEND, va("map %s", map));
	return qtrue;
}

// !rmsaves <player>
qboolean G_admin_remove_saves(gentity_t *ent, int skiparg) {
	int i, j;
	gentity_t *target;
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	save_position_t	*saves[2];
	if(Q_SayArgc() != 2+skiparg) {
		AIP(ent, "^3usage:^7 !rmsaves <player>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));
	if(!(target = getPlayerForName(name, err, sizeof(err)))) {
		AIP(ent, va("^3!rmsaves: ^7%s", err));
		return qfalse;
	}

	saves[0] = target->client->sess.allies_save_pos;
	saves[1] = target->client->sess.axis_save_pos;

	for (i = 0; i < 2; i++)
		for (j = 0; j < MAX_SAVE_POSITIONS; j++)
			saves[i][j].isValid = qfalse;

	AIP(target, "^3admin: ^7your saves were removed.");
	
	return qtrue;
}
// !nogoto <player>
qboolean G_admin_disable_goto(gentity_t *ent, int skiparg) {
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage:^7 !nogoto <player>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));
	
	if(!(target = getPlayerForName(name, err, sizeof(err)))) {
		AIP(ent, va("^3!nogoto: ^7%s", err));
		return qfalse;
	}

	if(target->client->sess.goto_allowed) {
		target->client->sess.goto_allowed = qfalse;
		AIP(target, "^3admin:^7 you are not allowed to use goto.");
	} else {
		target->client->sess.goto_allowed = qtrue;
		AIP(target, "^3admin:^7 you are now allowed to use goto.");
	}
	return qtrue;
}
// !nosave <player>
qboolean G_admin_disable_save(gentity_t *ent, int skiparg) {
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage:^7 !nosave <player>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));
	
	if(!(target = getPlayerForName(name, err, sizeof(err)))) {
		AIP(ent, va("^3!nosave: ^7%s", err));
		return qfalse;
	}

	if(target->client->sess.save_allowed) {
		target->client->sess.save_allowed = qfalse;
		AIP(target, "^3admin:^7 you are not allowed to save position.");
	} else {
		target->client->sess.save_allowed = qtrue;
		AIP(target, "^3admin:^7 you are now allowed to save position.");
	}
	return qtrue;
}

// !spec <player>
// flag: S

qboolean G_admin_spec(gentity_t *ent, int skiparg) {
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage:^7 !spec <player>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));
	
	if(!(target = getPlayerForName(name, err, sizeof(err)))) {
		AIP(ent, va("^3!spec: ^7%s", err));
		return qfalse;
	}

	if(!G_AllowFollow(ent, target)) {
		AIP(ent, va("^3!spec: %s ^7is locked from spectators.", target->client->pers.netname));
		return qfalse;
	}

	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator", qfalse, -1, -1, qfalse );
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = target->client->ps.clientNum;
	return qtrue;
}



#ifdef EDITION999

#endif