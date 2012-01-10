#include "g_local.h"

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
	{"admintest",	G_admin_admintest,	'a',	"displays your current admin level"},
	{"ban",			G_admin_ban,		'b',	"bans target player"},
	{"cancelvote",  G_admin_cancelvote, 'C',	"cancels the current vote in progress"},
	{"nogoto",		G_admin_disable_goto,'T',	"target can not use goto or call"},
	{"nosave",		G_admin_disable_save,'T',	"target can not use save or load"},
	{"finger",		G_admin_finger,		'f',	"displays target's admin level"},
	{"help",		G_admin_help,		'h',	"displays info about commands"},
	{"listcmds",	G_admin_help,		'h',	"displays info about commands"},
	{"kick",		G_admin_kick,		'k',	"kicks target player"},
	{"listbans",	G_admin_listbans,	'L',	"lists all current bans"},
	{"map",			G_admin_map,		'M',	"changes map"},
	{"mute",		G_admin_mute,		'm',	"mutes target player"},
	{"passvote",	G_admin_passvote,	'P',	"passes the current vote in progress"},
	{"putteam",		G_admin_putteam,	'p',	"puts target to a team"},
	{"readconfig",	G_admin_readconfig,	'G',	"reads admin config"},
	{"rename",		G_admin_rename,		'R',	"renames the target player"},
	{"restart",		G_admin_restart,	'r',	"restarts the map"},
	{"rmsaves",		G_admin_remove_saves,'T',	"clear saved positions of target"},
	{"setlevel",	G_admin_setlevel,	's',	"sets target level"},
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

// This will print a default config in the mod
// folder when executed. If the mod can't find 
// any admin.cfg it will use this to create a
// new one.
void G_admin_writeconfig_default() {
	int len;
	fileHandle_t f;

	if(!*g_admin.string) return;

	len = trap_FS_FOpenFile(g_admin.string, &f, FS_WRITE);

	// New admin levels.

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 0\n", 10, f);
	trap_FS_Write("name = ET Jumper\n", 17, f);
	trap_FS_Write("commands = a\n", 13, f);
	trap_FS_Write("greeting = Welcome ET Jumper [n]\n", 33, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 1\n", 10, f);
	trap_FS_Write("name = ET Admin I\n", 18, f);
	trap_FS_Write("commands = ak\n", 14, f);
	trap_FS_Write("greeting = Welcome ET Admin I [n]\n", 34, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 2\n", 10, f);
	trap_FS_Write("name = ET Admin II\n", 19, f);
	trap_FS_Write("commands = ak\n", 14, f);
	trap_FS_Write("greeting = Welcome ET Admin II [n]\n", 35, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_FCloseFile(f);
	
}

// writeconfig help-function straight
// from etpub

void G_admin_writeconfig_int(int val, fileHandle_t f) {
	char buf[32];

	Com_sprintf(buf, 32, "%d", val);
	//sprintf(buf, "%d", v);
	if(buf[0]) trap_FS_Write(buf, strlen(buf), f);
	trap_FS_Write("\n", 1, f);
}

void G_admin_writeconfig_string(char *s, fileHandle_t f) {
	char buf[MAX_STRING_CHARS];

	buf[0] = '\0';
	if(s[0]) {
		//Q_strcat(buf, sizeof(buf), s);
		Q_strncpyz(buf, s, sizeof(buf));
		trap_FS_Write(buf, strlen(buf), f);
	}
	trap_FS_Write("\n", 1, f);
}

void G_admin_writeconfig() {
	fileHandle_t f;
	int len, i;
	time_t t;
	
	if(!*g_admin.string) return;

	time(&t);
	t = t - ADMIN_BAN_EXPIRE_OFFSET;

	len = trap_FS_FOpenFile(g_admin.string, &f, FS_WRITE);
	if(len < 0) {
		G_Printf(va("adminsystem: could not open %s\n", g_admin.string));
		return;
	}

	for(i = 0; g_admin_levels[i]; i++) {
		trap_FS_Write("[level]\n", 8, f);
		trap_FS_Write("level    = ", 11, f);
		G_admin_writeconfig_int(g_admin_levels[i]->level, f);
		trap_FS_Write("name     = ", 11, f);
		G_admin_writeconfig_string(g_admin_levels[i]->name, f);
		trap_FS_Write("commands = ", 11, f);
		G_admin_writeconfig_string(g_admin_levels[i]->commands, f);
		trap_FS_Write("greeting = ", 11, f);
		G_admin_writeconfig_string(g_admin_levels[i]->greeting, f);
		trap_FS_Write("\n", 1, f);
	}
	for(i = 0; g_admin_users[i]; i++) {
		if(g_admin_users[i]->level == 0) continue;

		trap_FS_Write("[user]\n", 7, f);
		trap_FS_Write("name     = ", 11, f);
		G_admin_writeconfig_string(g_admin_users[i]->name, f);
		trap_FS_Write("password = ", 11, f);
		G_admin_writeconfig_string(g_admin_users[i]->password, f);
		trap_FS_Write("level    = ", 11, f);
		G_admin_writeconfig_int(g_admin_users[i]->level, f);
		trap_FS_Write("\n", 1, f);
	}
	for(i = 0; g_admin_bans[i]; i++) {
		if(g_admin_bans[i]->expires != 0 &&
			(g_admin_bans[i]->expires - t) < 1) continue;

		trap_FS_Write("[ban]\n", 6, f);
		trap_FS_Write("name     = ", 11, f);
		G_admin_writeconfig_string(g_admin_bans[i]->name, f);
		trap_FS_Write("ip       = ", 11, f);
		G_admin_writeconfig_string(g_admin_bans[i]->ip, f);
		trap_FS_Write("reason   = ", 11, f);
		G_admin_writeconfig_string(g_admin_bans[i]->reason, f);
		trap_FS_Write("made     = ", 11, f);
		G_admin_writeconfig_string(g_admin_bans[i]->made, f);
		trap_FS_Write("expires  = ", 11, f);
		G_admin_writeconfig_int(g_admin_bans[i]->expires, f);
		trap_FS_Write("banner   = ", 11, f);
		G_admin_writeconfig_string(g_admin_bans[i]->banner, f);
		trap_FS_Write("\n", 1, f);
	}
	trap_FS_FCloseFile(f);
}

void G_admin_cleanup()
{
	int i = 0;

	for(i=0; g_admin_levels[i]; i++) {
		free(g_admin_levels[i]);
		g_admin_levels[i] = NULL;
	}
	for(i=0; g_admin_users[i]; i++) {
		free(g_admin_users[i]);
		g_admin_users[i] = NULL;
	}
}

void G_admin_readconfig_string(char **cnf, char *s, int size)
{
	char *t;

	//COM_MatchToken(cnf, "=");
	t = COM_ParseExt(cnf, qfalse);
	if(!strcmp(t, "=")) {
		t = COM_ParseExt(cnf, qfalse);
	}
	else {
		G_Printf("readconfig: warning missing = before "
			"\"%s\" on line %d\n",
			t,
			COM_GetCurrentParseLine());
	}
	s[0] = '\0';
	while(t[0]) {
		if((s[0] == '\0' && strlen(t) <= size) ||
			(strlen(t)+strlen(s) < size)) {

			Q_strcat(s, size, t);
			Q_strcat(s, size, " ");
		}
		t = COM_ParseExt(cnf, qfalse);
	}
	// trim the trailing space
	if(strlen(s) > 0 && s[strlen(s)-1] == ' ')
		s[strlen(s)-1] = '\0';
}

void G_admin_readconfig_int(char **cnf, int *v)
{
	char *t;

	//COM_MatchToken(cnf, "=");
	t = COM_ParseExt(cnf, qfalse);
	if(!strcmp(t, "=")) {
		t = COM_ParseExt(cnf, qfalse);
	}
	else {
		G_Printf("readconfig: warning missing = before "
			"\"%s\" on line %d\n",
			t,
			COM_GetCurrentParseLine());
	}
	*v = atoi(t);
}

void G_admin_readconfig_float(char **cnf, float *v)
{
	char *t;

	//COM_MatchToken(cnf, "=");
	t = COM_ParseExt(cnf, qfalse);
	if(!strcmp(t, "=")) {
		t = COM_ParseExt(cnf, qfalse);
	}
	else {
		G_Printf("readconfig: warning missing = before "
			"\"%s\" on line %d\n",
			t,
			COM_GetCurrentParseLine());
	}
	*v = atof(t);
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


qboolean G_admin_readconfig(gentity_t *ent, int skiparg) {
	admin_level_t *temp_level = NULL;
	admin_user_t  *temp_user = NULL;
	admin_ban_t	  *temp_ban = NULL;
	int lc = 0, uc = 0, bc = 0;
	fileHandle_t f;
	int len;
	char *data, *data2;
	char *t;
	qboolean level_open, user_open, ban_open;

	if(!*g_admin.string) return qfalse;

	len = trap_FS_FOpenFile(g_admin.string, &f, FS_READ);

	if(len < 0) {
		AIP(ent, va("^3adminsystem: ^7could not open %s.", g_admin.string));
		G_admin_writeconfig_default();
		return qfalse;
	}

	data = (char*)malloc(len+1);
	data2 = data;
	trap_FS_Read(data, len, f);
	*(data + len) = '\0';

	G_admin_cleanup();

	t = COM_Parse(&data);
	level_open = user_open = ban_open = qfalse;

	while(*t) {
		// New block found
		if( !Q_stricmp(t, "[level]") ||
			!Q_stricmp(t, "[user]")  ||
			!Q_stricmp(t, "[ban]") ) {
			if(level_open) g_admin_levels[lc++] = temp_level;
			else if(user_open) g_admin_users[uc++] = temp_user;
			else if(ban_open) g_admin_bans[bc++] = temp_ban;
			level_open = user_open = ban_open = qfalse;
		}

		// if level block open -> parse level info
		if(level_open) {
			if(!Q_stricmp(t, "level")) {
				G_admin_readconfig_int(&data,
					&temp_level->level);
			}
			
			else if(!Q_stricmp(t, "name")) {
				G_admin_readconfig_string(&data,
					temp_level->name, sizeof(temp_level->name));
			} 
			
			else if(!Q_stricmp(t, "commands")) {
				G_admin_readconfig_string(&data,
					temp_level->commands, sizeof(temp_level->commands));
			}

			else if(!Q_stricmp(t, "greeting")) {
				G_admin_readconfig_string(&data,
					temp_level->greeting, sizeof(temp_level->greeting));
			}

			else {
				G_Printf(va("^7readconfig: ^7[level] parse error near "
					"%s on line %d",
					t,
					COM_GetCurrentParseLine()));
				if(ent) {
					CP(va("^7readconfig: ^7[level] parse error near "
					"%s on line %d",
					t,
					COM_GetCurrentParseLine()));
				}
			}
		}

		else if(user_open) {
			if(!Q_stricmp(t, "name")) {
				G_admin_readconfig_string(&data, 
					temp_user->name, sizeof(temp_user->name));

			} else if (!Q_stricmp(t, "password")) {
				G_admin_readconfig_string(&data,
					temp_user->password, sizeof(temp_user->password));
			} else if (!Q_stricmp(t, "level")) {
				G_admin_readconfig_int(&data, 
					&temp_user->level);
			}
		}

		else if(ban_open) {
			if(!Q_stricmp(t, "name")) {
				G_admin_readconfig_string(&data,
					temp_ban->name, sizeof(temp_ban->name));
			}
			else if(!Q_stricmp(t, "ip")) {
				G_admin_readconfig_string(&data,
					temp_ban->ip, sizeof(temp_ban->ip));
			}
			else if(!Q_stricmp(t, "reason")) {
				G_admin_readconfig_string(&data,
					temp_ban->reason, sizeof(temp_ban->reason));
			}
			else if(!Q_stricmp(t, "made")) {
				G_admin_readconfig_string(&data,
					temp_ban->made, sizeof(temp_ban->made));
			}
			else if(!Q_stricmp(t, "expires")) {
				G_admin_readconfig_int(&data, &temp_ban->expires);
			}
			else if(!Q_stricmp(t, "banner")) {
				G_admin_readconfig_string(&data,
					temp_ban->banner, sizeof(temp_ban->banner));
			}
			else {
				AIP(ent, va("^3readconfig: ^7[ban] parse error near "
					"%s on line %d",
					t,
					COM_GetCurrentParseLine()));
			}
		}

		if(!Q_stricmp(t, "[level]")) {
			if(lc >= MAX_ADMIN_LEVELS) {
				G_Printf("adminsystem: too many admin levels.\n");
				return qfalse;
			}
			temp_level = malloc(sizeof(admin_level_t));

			temp_level->level = 0;
			*temp_level->name = '\0';
			*temp_level->commands = '\0';
			*temp_level->greeting = '\0';
			level_open = qtrue;
		}
		else if(!Q_stricmp(t, "[user]")) {
			if(uc >= MAX_USERS) {
				G_Printf("adminsystem: too many users.\n");
				return qfalse;
			}
			temp_user = malloc(sizeof(admin_user_t));

			temp_user->level = 0;
			*temp_user->name = '\0';
			*temp_user->password = '\0';
			user_open = qtrue;
		}
		else if(!Q_stricmp(t, "[ban]")) {
			if(bc >= MAX_BANS) return qfalse;
			temp_ban = malloc(sizeof(admin_ban_t));
			*temp_ban->name = '\0';
			*temp_ban->ip = '\0';
			*temp_ban->made = '\0';
			temp_ban->expires = 0;
			*temp_ban->reason = '\0';
			*temp_ban->banner = '\0';
			ban_open = qtrue;
		}
		t = COM_Parse(&data);
	}

	if(level_open) g_admin_levels[lc++] = temp_level;
	if(user_open) g_admin_users[uc++] = temp_user;
	if(ban_open) g_admin_bans[bc++] = temp_ban;

	free(data2);
	AIP(ent, va("^3readconfig: ^7loaded %d levels, %d users and %d bans", lc, uc, bc));

	G_admin_identify_all();

	if(lc == 0) {
		G_admin_writeconfig_default();
	}
	trap_FS_FCloseFile(f);
	return qtrue;
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

//////////////////////////////////////////
// Setlevel command and stuff related to it

admin_t temp;

qboolean G_admin_setlevel(gentity_t *ent, int skiparg) {
	int level = 0, i;
	qboolean found = qfalse;
	char err[MAX_STRING_CHARS];
	char arg[MAX_TOKEN_CHARS];
	int pids[MAX_CLIENTS];

	gentity_t *target;

	if(Q_SayArgc() != (3+skiparg)) {
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
			temp.level = level;
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

void G_clear_temp_admin() {
	temp.level = 0;
	temp.password[0] = '\0';
}
// part of setlevel cmd
void G_admin_register_client(gentity_t *ent) {
	int i ;
	admin_user_t *a;
	qboolean updated = qfalse;
	char arg[MAX_TOKEN_CHARS];
	// Do not allow player to register
	// if it's not done with setlevel

	if(!ent->client->sess.allowRegister) {
		return;
	}

	ent->client->sess.allowRegister = qfalse;

	// Syntax register_client <password>
	// if no password -> no register can be done

	if(trap_Argc() != 2) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	Q_strncpyz(arg, G_SHA1(arg), sizeof(arg));

	// Give user a level + a "finger" name

	for(i = 0; g_admin_users[i]; i++) {
		if(!Q_stricmp(arg, g_admin_users[i]->password)) {

			ent->client->sess.uinfo.level = temp.level;

			Q_strncpyz(ent->client->sess.uinfo.name,
				ent->client->pers.netname, sizeof(ent->client->sess.uinfo.name));

			g_admin_users[i]->level = temp.level;
			Q_strncpyz(g_admin_users[i]->password, arg, sizeof(g_admin_users[i]->password));

			updated = qtrue;
		}
	}

	if(!updated) {
		if(i == MAX_USERS) {
			return;
		}
		a = malloc(sizeof(admin_user_t));
		a->level = temp.level;
		Q_strncpyz(a->name, ent->client->pers.netname, sizeof(a->name));
		Q_strncpyz(a->password, arg, sizeof(a->password));	
		ent->client->sess.uinfo.level = temp.level;
		Q_strncpyz(ent->client->sess.uinfo.name, ent->client->pers.netname, sizeof(ent->client->sess.uinfo.name));
		g_admin_users[i] = a;
	}

	AP(va("chat \"^3setlevel:^7 %s^7 is now a level %d user\"", ent->client->pers.netname, ent->client->sess.uinfo.level));

	G_admin_writeconfig();

	G_clear_temp_admin();
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
	G_admin_writeconfig();

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
	G_admin_writeconfig();
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