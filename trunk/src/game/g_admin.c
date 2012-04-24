#include "g_local.h"

char bigTextBuffer[100000];

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
	const char *syntax;
};

struct g_admin_additional_flag {
    char flag;
    const char *data;
};

static const struct g_admin_additional_flag g_admin_additional_flags[] = {
    {AF_IMMUNITY, "Immunity to vote kick and mute."},
    {AF_NONAMECHANGELIMIT, "Unlimited name changes."},
    {AF_NOVOTELIMIT, "Unlimited vote limit."},
    {AF_SILENTCOMMANDS, "Silent admin commands."},
    {0, "\0"},
};

static const struct g_admin_cmd g_admin_cmds[] = {
	{"8ball",		G_admin_8ball,		'8',	"Magic 8 Ball!", "Syntax: !8ball <question>"},
	{"admintest",	G_admin_admintest,	'a',	"Displays your current admin level.", "Syntax: !admintest"},
	{"ban",			G_admin_ban,		'b',	"Bans target user.", "Syntax: !ban <name> <time> <reason>"},
	{"cancelvote",  G_admin_cancelvote, 'C',	"Cancels current vote in progress.", "Syntax: !cancelvote"},
	{"editcmds",	G_admin_editcommands,'A',	"Edits command permissions of target admin level.", "Syntax: !editcmds <level> <+cmd|-cmd> <+cmd2|-cmd>..."},
	{"finger",		G_admin_finger,		'f',	"Displays target's admin level.", "Syntax: !finger <target>"},
	{"help",		G_admin_help,		'h',	"Prints useful information about commands.", "Syntax: !help <command>"},
	{"kick",		G_admin_kick,		'k',	"Kicks target.", "Syntax: !kick <player>"},
	{"levadd",		G_admin_levadd,		'A',	"Adds admin level to admin level database.", "Syntax: !levadd <level>"},
	{"levedit",		G_admin_levedit,	'A',	"Edits admin level.", "Syntax: !levedit <level> <name|gtext|cmds> <third parameter>"},
	{"levinfo",		G_admin_levinfo,	'A',	"Prints information about admin levels.", "Syntax: !levinfo or !levinfo <level>"},
	{"listbans",	G_admin_listbans,	'L',	"Lists all current bans.", "Syntax: !listbans"},
	{"listcmds",	G_admin_help,		'h',	"Prints useful information about commands.", "Syntax: !help <command>"},
	{"listflags",	G_admin_listflags,	'A',	"Prints of list of all admin command flags.", "Syntax: !listflags"},
	{"listmaps",	G_admin_listmaps,	'a',	"Prints a list of all maps on the server.", "Syntax: !listmaps"},
	{"listplayers",	G_admin_listplayers,'l',	"Displays admin level information about all players on the server.", "Syntax: !listplayers"},
	{"listusers",	G_admin_listusers,	'A',	"Prints a list of all users in the admin database.", "Syntax: !listusers"},
	{"map",			G_admin_map,		'M',	"Changes map.", "Syntax: !map <mapname>"},
	{"mute",		G_admin_mute,		'm',	"Mutes target.", "Syntax: !mute <target>"},
#ifndef EDITION999
	{"noclip",		G_admin_noclip,		'N',	"Toggles noclip for target player.", "Syntax: !noclip"},
#endif
#ifdef EDITION999
	{"noclip",		G_admin_noclip,		AF_ADMINBYPASS, "Toggles noclip for target player.", "Syntax: !noclip <target>"},
#endif
	{"nogoto",		G_admin_disable_goto,'T',	"Disables target's use of goto.", "Syntax: !nogoto <target>"},
	{"nosave",		G_admin_disable_save,'T',	"Disables target's use of call.", "Syntax: !nosave <target>"},
	{"passvote",	G_admin_passvote,	'P',	"Passes the current vote in progress.","Syntax: !passvote}"},
	{"putteam",		G_admin_putteam,	'p',	"Puts target to a team.", "Syntax: !putteam target <b|r|s>"},
	{"readconfig",	G_admin_readconfig,	'G',	"Reads admin config.", "Syntax: !readconfig"},
	{"removelevel", G_admin_removelevel,'A',	"Removes admin level from database.", "Syntax: !removelevel <level>"},
	{"removeuser",	G_admin_removeuser,	'A',	"Remove admin user from database.", "Syntax: !removeuser <username>"},
	{"rename",		G_admin_rename,		'R',	"Renames target.", "Syntax: !rename <target> <newname>"},
	{"restart",		G_admin_restart,	'r',	"Restarts the map.", "Syntax: !restart"},
	{"rmsaves",		G_admin_remove_saves,'T',	"Clears target's saved positions.", "Syntax: !rmsaves <target>"},
	{"setlevel",	G_admin_setlevel,	's',	"Sets target level.", "Syntax: !setlevel <target> <level>"},
	{"spec",		G_admin_spec,		'S',	"Spectates target.", "Syntax: !spec <target>"},
	{"unban",		G_admin_unban,		'b',	"Unbans ban ID.", "Syntax: !unban <number>"},
	{"unmute",		G_admin_unmute,		'm',	"Unmutes target player.", "Syntax: !unmute <target>"},
	{"\0",			NULL,				'\0',	"", ""}
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

/* 
Some utility chat print functions with proper naming.
Probably useless as I forget to use them but they're
here anyway! Yay!
*/
// chat to ent
#define ChatP(ent, x) ChatPrint(ent, x)
// chat to all
#define ChatPA(x) ChatPrintAll(x)
// cpm to ent
#define CPMP(ent, x) CPMPrint(ent, x)
// cpm to all
#define CPMPA(x) CPMPrintAll(x)
// cp to ent
#define CPP(ent, x) CPPrint(ent, x)
// cp to all
#define CPPA(x) CPPrintAll(x)

void ChatPrint(gentity_t *ent, char *message) {
    if(ent) CP(va("chat \"%s\"", message));
    else G_Printf("%s\n", message);
}

void ChatPrintAll(char *message) {
    AP(va("chat \"%s\"", message));
    G_Printf("%s\n", message);
}

void CPMPrint(gentity_t *ent, char *message) {
    if(ent) CP(va("cpm \"%s\n\"", message));
    else G_Printf("%s\n", message);
}

void CPMPrintAll(char *message) {
    AP(va("cpm \"%s\n\"", message));
    G_Printf("%s\n", message);
}

void CPPrint(gentity_t *ent, char *message) {
    if(ent)CP(va("cp \"%s\n\"", message));
    else G_Printf("%s\n", message);
}

void CPPrintAll(char *message) {
    AP(va("cp \"%s\n\"", message));
    G_Printf("%s\n", message);
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
	trap_FS_Write("commands = *\n", 13, f);
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
		trap_FS_Write("username = ", 11, f);
		G_admin_writeconfig_string(g_admin_users[i]->username, f);
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
        trap_FS_Write("hardware = ", 11, f);
        G_admin_writeconfig_string(g_admin_bans[i]->hardware, f);
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
			} else if (!Q_stricmp(t, "username")) {
				G_admin_readconfig_string(&data,
					temp_user->username, sizeof(temp_user->username));
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
            else if(!Q_stricmp(t, "hardware")) {
                G_admin_readconfig_string(&data,
                    temp_ban->hardware, sizeof(temp_ban->hardware));
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
			*temp_user->username = '\0';
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
	// Help, admintest, spec for everyone.
	if(flag == 'h' || flag == 'a' || flag == 'S') {
		return qtrue;
	}

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

char *CompleteAdminCommand(char *src) {
	int i = 0;
	int matchcount = 0;
	int index = -1;
	static char line[MAX_STRING_CHARS];

	for(i = 0; i < strlen(src); i++) {
		src[i] = tolower(src[i]);
	}

	for( i = 0; g_admin_cmds[i].keyword[0]; i++) {
		if(!Q_strncmp(src, g_admin_cmds[i].keyword, strlen(src))) {
			index = i;
			++matchcount;
		}
	}

	if(matchcount == 1) {
		Q_strncpyz(line, g_admin_cmds[index].keyword, sizeof(line));
	} else {
		return "\0";
	}

	return line;
}

qboolean G_admin_cmd_check(gentity_t *ent) {
	int i;
	char command[MAX_CMD_LEN];
	char *completedCommand;
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

	completedCommand = CompleteAdminCommand(cmd);

	for(i = 0; g_admin_cmds[i].keyword[0]; i++) {
		if(Q_stricmp(completedCommand, g_admin_cmds[i].keyword)) {
			continue;
		}
		else if(G_admin_permission(ent, g_admin_cmds[i].flag)) {
			g_admin_cmds[i].handler(ent, skip);
			if(g_logCommands.integer) {
				if(ent)
                    G_ALog("Command: \\!%s\\%s\\%s\\", g_admin_cmds[i].keyword, ent->client->sess.uinfo.username, ent->client->sess.uinfo.ip);
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

admin_t setlevel_temp;

// Send client a password + username request.

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
		AIP(ent, "^3setlevel: ^7you can't set the level of a fellow admin");
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
			setlevel_temp.level = level;
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
	setlevel_temp.level = 0;
	setlevel_temp.password[0] = '\0';
	setlevel_temp.username[0] = '\0';
}
// Part of setlevel
// Client sends "register_client" to server with two args,
// password & username.
void G_admin_register_client(gentity_t *ent) {
	int i ;
	admin_user_t *a;
	qboolean updated = qfalse;
	char arg[MAX_TOKEN_CHARS];
	char *username;
	// Do not allow player to register
	// if it's not done with setlevel

	if(!ent->client->sess.allowRegister) {
		return;
	}

	ent->client->sess.allowRegister = qfalse;

	// Syntax register_client <password> <username>

	if(trap_Argc() != 3) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

	Q_strncpyz(arg, G_SHA1(arg), sizeof(arg));

	username = ConcatArgs(2);

	// Give user a level + a "finger" name

	for(i = 0; g_admin_users[i]; i++) {
		// Username does not match with pw
		if( Q_strncmp(arg, g_admin_users[i]->password, sizeof(g_admin_users[i]->password)) &&
			!Q_strncmp(username, g_admin_users[i]->username, sizeof(g_admin_users[i]->username))) {
				AIP(ent, "^3adminsystem:^7 username is in use. Please choose another one.");
				G_clear_temp_admin();
				return;
		}

		if( !Q_strncmp(arg, g_admin_users[i]->password, sizeof(g_admin_users[i]->password)) &&
			!Q_strncmp(username, g_admin_users[i]->username, sizeof(g_admin_users[i]->username))) {

			ent->client->sess.uinfo.level = setlevel_temp.level;
			Q_strncpyz(ent->client->sess.uinfo.username, g_admin_users[i]->username, sizeof(ent->client->sess.uinfo.username));
			Q_strncpyz(ent->client->sess.uinfo.password, g_admin_users[i]->password, sizeof(ent->client->sess.uinfo.password));

			Q_strncpyz(ent->client->sess.uinfo.name,
				ent->client->pers.netname, sizeof(ent->client->sess.uinfo.name));

			g_admin_users[i]->level = setlevel_temp.level;
			Q_strncpyz(g_admin_users[i]->password, arg, sizeof(g_admin_users[i]->password));

			updated = qtrue;
		}
	}

	if(!updated) {
		if(i == MAX_USERS) {
			return;
		}
		a = malloc(sizeof(admin_user_t));
		a->level = setlevel_temp.level;
		Q_strncpyz(a->name, ent->client->pers.netname, sizeof(a->name));
		Q_strncpyz(a->password, arg, sizeof(a->password));	
		Q_strncpyz(a->username, username, sizeof(a->username));
		ent->client->sess.uinfo.level = setlevel_temp.level;
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
	char *username = 0;
	char *value = 0;

	if(trap_Argc() != 3) {
		return;
	}
	// Don't want people to change password too many times.
	ent->client->sess.password_change_count++;
	
	trap_Argv(1, arg, sizeof(arg));

	trap_GetUserinfo(ent->client->ps.clientNum, userinfo, sizeof(userinfo));

	value = Info_ValueForKey(userinfo, "ip");

	Q_strncpyz(password, G_SHA1(arg), sizeof(password));
	Q_strncpyz(ent->client->sess.uinfo.password, G_SHA1(arg), sizeof(ent->client->sess.uinfo.password));
	
	username = ConcatArgs(2);

	Q_strncpyz(ent->client->sess.uinfo.username, username, sizeof(ent->client->sess.uinfo.username));

	for(i = 0; g_admin_users[i]; i++) {
		if( !Q_stricmp(g_admin_users[i]->password, password) &&
			!Q_strncmp(g_admin_users[i]->username, username, sizeof(g_admin_users[i]->username))) {
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
		ent->client->sess.uinfo.name, value);
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
		int id = level.sortedClients[i];
		trap_SendServerCommand(id, "identify_self");
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
		if( !Q_stricmp(ent->client->sess.uinfo.password, g_admin_users[i]->password) &&
			!Q_stricmp(ent->client->sess.uinfo.username, g_admin_users[i]->username)) {
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
    char *ip;
	char userinfo[MAX_INFO_STRING];

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
			AIP(ent, "^3mute: ^7you cannot mute a fellow admin");
			return qfalse;
		}
		if(target == ent) {
			AIP(ent, "^3mute: ^7you cannot mute yourself");
			return qfalse;
		}
	}

	if(target->client->sess.muted) {
		AIP(ent, "^3mute:^7 target is already muted");
		return qfalse;
	}
    
	target->client->sess.muted = qtrue;
					
	trap_GetUserinfo( target->client->ps.clientNum, userinfo, sizeof( userinfo ) );
	ip = Info_ValueForKey (userinfo, "ip");

    G_AddIpMute(ip);

	CPx(pids[0], "print \"^5You've been muted\n\"" );

	ACP(va("%s ^7has been muted", target->client->pers.netname));

	return qtrue;
}

qboolean G_admin_unmute(gentity_t *ent, int skiparg) {
	int pids[MAX_CLIENTS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;
	char name[MAX_TOKEN_CHARS];
    char *ip;
	char userinfo[MAX_INFO_STRING];

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

    trap_GetUserinfo( target->client->ps.clientNum, userinfo, sizeof( userinfo ) );
	ip = Info_ValueForKey (userinfo, "ip");

    G_RemoveIPMute(ip);

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
    trap_SendServerCommand(pids[0], va("name %s", newname));
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
			AIP(ent, "^3putteam:^7 you can't putteam a fellow admin");
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

        int cmdcount = 0;
		AIP(ent, "^3!help: ^7check console for more information.");
		ABP_begin();
		for(i = 0; g_admin_cmds[i].keyword[0]; i++) {
			if(G_admin_permission(ent, g_admin_cmds[i].flag)) {
				if((cmdcount != 0 ) && (cmdcount % 3 == 0)) {
					ABP(ent, "\n");
				}
				ABP(ent, va("%-21s ", g_admin_cmds[i].keyword));
                ++cmdcount;
			}
		}
		ABP(ent, "\n");
		ABP_end();
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
				AIP(ent, va("%s", g_admin_cmds[i].syntax));
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
	char *ip = NULL, *hardware = NULL;
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
			AIP(ent, "^3ban: ^7you can't ban a fellow admin");
			return qfalse;
		}

		if(ent == target) {
			AIP(ent, "^3ban: ^7you can't ban yourself");
			return qfalse;
		}
	}

	trap_GetUserinfo(pids[0], userinfo, sizeof(userinfo));
	ip = Info_ValueForKey(userinfo, "ip");

    hardware = Info_ValueForKey(userinfo, "hwinfo");

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

    Q_strncpyz(b->hardware, hardware, sizeof(b->hardware));

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

    if(ent && target->client->sess.uinfo.level >= ent->client->sess.uinfo.level) {
        AIP(ent, "^3!nosave: ^7can't remove fellow admin's saves.");
        return qfalse;
    }

	saves[0] = target->client->sess.allies_save_pos;
	saves[1] = target->client->sess.axis_save_pos;

	for (i = 0; i < 2; i++)
		for (j = 0; j < MAX_SAVE_POSITIONS; j++)
			saves[i][j].isValid = qfalse;

	AIP(ent, va("^3adminsystem: ^7%s^7's ^7saves were removed.", target->client->pers.netname) );
    AIP(target, va("^3adminsystem: ^7%s ^7your saves were removed.", target->client->pers.netname) );
	
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

    if(ent && target->client->sess.uinfo.level >= ent->client->sess.uinfo.level) {
        AIP(ent, "^3!nosave: ^7can't disable fellow admin's goto & call");
        return qfalse;
    }

	if(target->client->sess.goto_allowed) {
		target->client->sess.goto_allowed = qfalse;
		AIP(target, va("^3adminsystem: ^7%s^7 you are not allowed to use goto.", target->client->pers.netname));
		AIP(ent, va("^3adminsystem: ^7%s^7 is not allowed to use goto.", target->client->pers.netname));
	} else {
		target->client->sess.goto_allowed = qtrue;
        AIP(target, va("^3adminsystem:^7 %s^7 you are now allowed to use goto.", target->client->pers.netname));
		AIP(ent, va("^3adminsystem: ^7%s^7 is now allowed to use goto.", target->client->pers.netname));
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

    if(ent && target->client->sess.uinfo.level >= ent->client->sess.uinfo.level) {
        AIP(ent, "^3!nosave: ^7can't disable fellow admin's save & load");
        return qfalse;
    }

	if(target->client->sess.save_allowed) {
		target->client->sess.save_allowed = qfalse;
        AIP(target, va("^3adminsystem:^7 %s^7 you are not allowed to save your position.", target->client->pers.netname));
		AIP(ent, va("^3adminsystem:^7 %s^7 is not allowed to save their position.", target->client->pers.netname));
	} else {
		target->client->sess.save_allowed = qtrue;
		AIP(target, va("^3adminsystem:^7 %s^7 you are now allowed to save your position.", target->client->pers.netname));
		AIP(ent, va("^3adminsystem:^7 %s^7 is now allowed to save their position.", target->client->pers.netname));
	}
	return qtrue;
}

// !spec <player>
// flag: S

qboolean G_admin_spec(gentity_t *ent, int skiparg) {
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(!ent) {
		return qfalse;
	}

	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage:^7 !spec <player>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));
	
	if(!(target = getPlayerForName(name, err, sizeof(err)))) {
		AIP(ent, va("^3!spec: ^7%s", err));
		return qfalse;
	}

	if(target->client->sess.sessionTeam == TEAM_SPECTATOR) {
		AIP(ent, "^3!spec:^7 you can't spectate a spectator.");
		return qfalse;
	}

#ifdef EDITION999
    if(!G_AllowFollow(ent, target)) {
        if(!G_admin_permission(ent, AF_ADMINBYPASS)) {
            AIP(ent, va("^3!spec: %s ^7is locked from spectators.", target->client->pers.netname));
			return qfalse;
        } else {
            AIP(target, va("^3adminsystem:^7 %s ^7is spectating you.", ent->client->pers.netname));
        }
    }
#else

	if(!G_AllowFollow(ent, target)) {
		AIP(ent, va("^3!spec: %s ^7is locked from spectators.", target->client->pers.netname));
		return qfalse;
	}

#endif

	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		SetTeam( ent, "spectator", qfalse, -1, -1, qfalse );
	}

	ent->client->sess.spectatorState = SPECTATOR_FOLLOW;
	ent->client->sess.spectatorClient = target->client->ps.clientNum;
	return qtrue;
}

qboolean G_admin_listplayers(gentity_t *ent, int skiparg) {

	int i = 0;

	AIP(ent, "^3!listplayers:^7 check console for more information.");
	if(ent) {
		CP("print \"ID : Player                        Level\n\"");
		CP("print \"----------------------------------------\n\"");
	} else {
		G_Printf("ID : Player                        Level\n\"");
		G_Printf("----------------------------------------\n\"");
	}

	for(i = 0; i < level.numConnectedClients; i++) {
		int idnum = level.sortedClients[i];
		gclient_t *cl = &level.clients[idnum];
		char name[MAX_NETNAME];
		char *clean = NULL;

		Q_strncpyz(name, cl->pers.netname, sizeof(name));
		name[26] = 0;
		clean = Q_CleanStr(name);


		if(ent) {
			CP(va("print \"%2d : %-26s^7%7d\n\"", idnum, clean, cl->sess.uinfo.level));
		} else {
			SanitizeString(cl->pers.netname, name, qfalse);
			name[26] = 0;
			G_Printf("%2d : %-26s  %7d\n\"", idnum, name, cl->sess.uinfo.level);
		}
	}
	return qtrue;
}

int adminComparator( const void *first, const void *second ) {
	admin_level_t* a = *(admin_level_t**)first;
	admin_level_t* b = *(admin_level_t**)second;

	return a->level - b->level;
}

qboolean G_admin_levedit( gentity_t *ent, int skiparg ) {

	char arg1[MAX_TOKEN_CHARS] = "\0";
	char arg2[MAX_TOKEN_CHARS] = "\0";
	char *arg3 = NULL;
	int level = -1;
	int i = 0;
	qboolean found = qfalse;
	admin_level_t *lev = NULL;

	// !levedit arg1 arg2 arg3
	
	if(Q_SayArgc() < 4 + skiparg) {
		AIP(ent, "^3usage:^7 !levedit <level> <name|gtext|cmds> <additional parameters.>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg1, sizeof(arg1));
	Q_SayArgv(2 + skiparg, arg2, sizeof(arg2));
	arg3 = Q_SayConcatArgs(3 + skiparg);

	level = atoi(arg1);

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			lev = g_admin_levels[i];
			found = qtrue;
		}
	}

	if(!found) {
		AIP(ent, "^3!levedit: ^7level not found.");
		return qfalse;
	}
	
	if(ent && level > ent->client->sess.uinfo.level) {
		AIP(ent, "^3!levedit: ^7you can't edit levels higher than yours.");
		return qfalse;
	}
	
	if(!Q_stricmp(arg2, "name")) {
		Q_strncpyz(lev->name, arg3, sizeof(lev->name));
	} else if (!Q_stricmp(arg2, "cmds")) {
		if(arg3[0] == '+') {
			Q_strcat(lev->commands, sizeof(lev->commands), (arg3+1));
		} else if (arg3[0] == '-') {
			for(i = 0; i < strlen(arg3); i++) {
				RemoveAllChars(arg3[i], lev->commands);
			}
		} else {
			Q_strncpyz(lev->commands, arg3, sizeof(lev->commands));
		}
		RemoveDuplicates(lev->commands);
		SortString(lev->commands);
	} else if (!Q_stricmp(arg2, "gtext")) {
		Q_strncpyz(lev->greeting, arg3, sizeof(lev->greeting));
	} else {
		AIP(ent, "^3usage:^7 !levedit <level> <name|gtext|cmds> <additional parameters.>");
		return qfalse;
	}
	G_admin_writeconfig();
	return qtrue;
}

qboolean G_admin_levadd( gentity_t *ent, int skiparg ) {
	char arg1[MAX_TOKEN_CHARS];
	int level = -1;
	int i = 0;
	admin_level_t *temp;
	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage: ^7!levadd <level>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg1, sizeof(arg1));
	level = atoi(arg1);

	if(ent && level > ent->client->sess.uinfo.level) {
		AIP(ent, "^3!levadd: ^7you cannot add levels higher than yours.");
		return qfalse;
	}

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			AIP(ent, "^3!levadd:^7 level already exists.");
			return qtrue;
		}
	}
	if(i == MAX_ADMIN_LEVELS) {
		AIP(ent, "^3Database error:^7 too many levels on database.");
		return qfalse;
	}

	temp = (admin_level_t*)malloc(sizeof(admin_level_t));

	temp->level = level;
	temp->commands[0] = 0;
	temp->greeting[0] = 0;
	temp->name[0] = 0;

	g_admin_levels[i++] = temp;
	qsort(g_admin_levels, i, sizeof(admin_level_t*), adminComparator);
	G_admin_writeconfig();
	return qtrue;
}
// Just for fun :D
static const char *m8BallResponses[] = {
    "It is certain",
    "It is decidedly so",
    "Without a doubt",
    "Yes - definitely",
    "You may rely on it",
     
    "As I see it, yes",
    "Most likely",
    "Outlook good",
    "Signs point to yes",
    "Yes",
     
    "Reply hazy, try again",
    "Ask again later",
    "Better not tell you now",
    "Cannot predict now",
    "Concentrate and ask again",
     
    "Don't count on it",
    "My reply is no",
    "My sources say no",
    "Outlook not so good",
    "Very doubtful"
};


qboolean G_admin_8ball(gentity_t *ent, int skiparg) {
	int random = 0;
	char color[3];
	
#define DELAY_8BALL 3000 // in ms

    if(ent && ent->client->last8BallTime + DELAY_8BALL > level.time) {
        AIP(ent, va("^3!8ball: ^7you must wait %i seconds before using !8ball again.", (ent->client->last8BallTime + DELAY_8BALL - level.time)/1000));
        return qfalse;
    }
    
    if(Q_SayArgc() < 2 + skiparg) {
		AIP(ent, "^3usage:^7 !8ball <question>");
		return qfalse;
	}

	random = rand() % 20;

	if(random < 10) {
        ChatPA(va("^3Magic 8 Ball: ^2%s.", m8BallResponses[random]));
	} else if (random < 15) {
		ChatPA(va("^3Magic 8 Ball: ^3%s.", m8BallResponses[random]));
	} else {
		ChatPA(va("^3Magic 8 Ball: ^1%s.", m8BallResponses[random]));
    }	
    if(ent) ent->client->last8BallTime = level.time;
	return qtrue;
}

qboolean G_admin_listflags( gentity_t *ent, int skiparg ) {
	int i = 0;

	if(ent) {
		AIP(ent, "^3!listflags: ^7check console for more information.");
	}
    ABP_begin();
	for(i = 0; g_admin_cmds[i].keyword[0]; i++) {
        ABP(ent, va("%-30s %c\n", g_admin_cmds[i].keyword, g_admin_cmds[i].flag));
	}
    ABP(ent, "\n");
    ABP(ent, "Additional flags: \n");
    for(i = 0; g_admin_additional_flags[i].data[0]; i++) {
        ABP(ent, va("%-30s %c\n", g_admin_additional_flags[i].data, g_admin_additional_flags[i].flag));
    }
    ABP(ent, "\n");
    ABP_end();
	return qtrue;
}

qboolean G_admin_levinfo( gentity_t *ent, int skiparg ) {
	if(Q_SayArgc() > 2 + skiparg) {
		AIP(ent, "^3usage: ^7!levinfo or !levinfo <level>");
		return qtrue;
	}
	if(Q_SayArgc() == 1 + skiparg) {
		int i = 0;
		ASP("Levels: ");
		for( i = 0; g_admin_levels[i]; i++) {
			if(i == 0)
				ASP(va("%d", g_admin_levels[i]->level));
			else
				ASP(va(", %d",g_admin_levels[i]->level));
		}
		ASP("\n");
	} else if (Q_SayArgc() > 1 + skiparg) {

		int i = 0;
		int level = -1;
		qboolean found = qfalse;
		char arg1[MAX_TOKEN_CHARS] = "\0";
		Q_SayArgv(1 + skiparg, arg1, sizeof(arg1));
		
		level = atoi(arg1);

		for(i = 0; g_admin_levels[i]; i++) {
			if(level == g_admin_levels[i]->level) {
				found = qtrue;
				break;
			}
		}

		if(!found) {
			AIP(ent, "^3!levinfo: ^7level not found.");
			return qfalse;
		}

		if(ent) {
			AIP(ent, "^3!levinfo: ^7check console for more information.");
			CP("print \"^5[LEVEL]\n\"");
			CP(va("print \"^5level    = ^7%d\n\"", g_admin_levels[i]->level));
			CP(va("print \"^5name     = ^7%s\n\"", g_admin_levels[i]->name));
			CP(va("print \"^5commands = ^7%s\n\"", g_admin_levels[i]->commands));
			CP(va("print \"^5greeting = ^7%s\n\"", g_admin_levels[i]->greeting));
		} else {
			G_Printf("^5[LEVEL]\n");
			G_Printf("^5level     = ^7%d\n", g_admin_levels[i]->level);
			G_Printf("^5name      = ^7%s\n", g_admin_levels[i]->name);
			G_Printf("^5commands  = ^7%s\n", g_admin_levels[i]->commands);
			G_Printf("^5greeting  =^7 %s\n", g_admin_levels[i]->greeting);
		}		
	}
	return qtrue;
}
// !editcmds 0 +admintest +kick -ban (example)
qboolean G_admin_editcommands(gentity_t *ent, int skiparg) {

	char	levelstr[MAX_TOKEN_CHARS];
	// array to store all commands into
	char	addcmdflags[MAX_COMMANDS];
	// array to store all commands to be removed
	char	rmcmdflags[MAX_COMMANDS];
	char	commands[MAX_COMMANDS][MAX_CMD_LEN];
	int		commandCount = 0;
	int		addflagCount = 0;
	int		rmflagCount = 0;
	int		i = 0;
	int		level = -1;
	int		levelIndex = -1;

	if(Q_SayArgc() < 3 + skiparg) {
		AIP(ent, "^3usage: ^7!editcmds <level> <+command|-command> <+another|-another> ...");
		return qfalse;
	}
	// Let's copy all args to an array
	for( i = 2 + skiparg; i < Q_SayArgc(); i++) {
		char arg[MAX_TOKEN_CHARS];
		// Just in case.. should never happen tho.
		if(i > MAX_COMMANDS) {
			AIP(ent, "^1ERROR:^7 too many parameters.");
			return qfalse;
		}
		Q_SayArgv(1 + skiparg, levelstr, sizeof(levelstr));
		level = atoi(levelstr);
		Q_SayArgv(i, arg, sizeof(arg));
		Q_strncpyz(commands[commandCount++], arg, sizeof(commands[0]));
	}

	if(ent && level > ent->client->sess.uinfo.level) {
		AIP(ent, "^3!editcommands: ^7you can't edit levels higher than yours.");
		return qfalse;
	}

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			levelIndex = i;
			break;
		}
	}

	if(levelIndex < 0) {
		AIP(ent, "^3!editcmds:^7 level not found.");
		return qfalse;
	}
	
	for( i = 0; i < commandCount; i++) {
		char sign = '+';
		qboolean signexists = qfalse;
		int j = 0;
		if(commands[i][0] == '+') {
			signexists = qtrue;
			sign = '+';
		} else if (commands[i][0] == '-') {
			signexists = qtrue;
			sign = '-';
		} else {
			signexists = qfalse;
			sign = '+';
		}

		for(j = 0; g_admin_cmds[j].keyword[0]; j++) {
			if(signexists) {
				if(sign == '+') {
					if(!Q_stricmp(g_admin_cmds[j].keyword, &commands[i][1])) {
						addcmdflags[addflagCount++] = g_admin_cmds[j].flag;
						break;
					} 
				} else {
					if(!Q_stricmp(g_admin_cmds[j].keyword, &commands[i][1])) {
						rmcmdflags[rmflagCount++] = g_admin_cmds[j].flag;
						break;
					}
				}
			} else {
				if(!Q_stricmp(g_admin_cmds[j].keyword, commands[i])) {
					addcmdflags[addflagCount++] = g_admin_cmds[j].flag;
					break;
				}
			}
		}
		addcmdflags[addflagCount] = 0;
		rmcmdflags[rmflagCount] = 0;
	}

	for(i = 0; rmcmdflags[i]; i++) {
		RemoveAllChars(rmcmdflags[i], g_admin_levels[levelIndex]->commands);
	}

	Q_strcat(g_admin_levels[levelIndex]->commands, sizeof(g_admin_levels[levelIndex]->commands), addcmdflags);

	RemoveDuplicates(g_admin_levels[levelIndex]->commands);
	SortString(g_admin_levels[levelIndex]->commands);
	G_admin_writeconfig();
	return qtrue;
}

qboolean G_admin_listmaps(gentity_t *ent, int skiparg) {
	int i = 0;

	if(ent) {
		if(ent->client->sess.lastListmapsTime != 0 && level.time - ent->client->sess.lastListmapsTime < 60000) {
			AIP(ent, "^3!listmaps:^7 you must wait atleast 60 seconds before using !listmaps again.");
			return qfalse;
		}
	}

	AIP(ent, "^3!listmaps:^7 check console for more information.");
	ABP_begin();
	for(i = level.mapCount - 1; i != 0; i--) {
		if(i % 3 == 0) {
			ABP(ent, "\n");
		}
		ABP(ent, va("%-27s ", g_maplist[i]));
	}
	ABP(ent, "\n");
	ABP_end();
	if(ent)
		ent->client->sess.lastListmapsTime = level.time;
	return qtrue;
}

qboolean G_admin_removeuser( gentity_t *ent, int skiparg ) {
	int i = 0;
	int levindex = -1;
	qboolean found = qfalse;
	char arg[MAX_TOKEN_CHARS] = "\0";
	if(Q_SayArgc() != 2 + skiparg) {
		AIP(ent, "^3usage: ^7!removeuser <username>");
		return qfalse;
	}
	
	Q_SayArgv(1 + skiparg, arg, sizeof(arg));
	
	for(i = 0; g_admin_users[i]; i++) {

		if(!Q_stricmp(g_admin_users[i]->username, arg)) {
			found = qtrue;
			levindex = i;
			break;
		}
	}
	
    if(!found) {
		AIP(ent, "^3!removeuser:^7 player not found.");
		return qfalse;
	}
	
	g_admin_users[levindex]->level = 0;

	for(i = 0; i < level.numConnectedClients; i++) {
		gentity_t *target = NULL;
		int id = level.sortedClients[i];
		if(!Q_stricmp((target = g_entities + id)->client->sess.uinfo.username,
			g_admin_users[levindex]->username)) {
				target->client->sess.uinfo.level = 0;
		}
	}
	AIP(ent, va("^3!removeuser: ^7removed user %s^7.", g_admin_users[levindex]->username));
	G_admin_writeconfig();
	return qtrue;
}

qboolean G_admin_listusers(gentity_t *ent, int skiparg) {
	int i = 0;
	ABP_begin();
	ABP(ent, "^5List of admin users: \n");
	for( i = 0; g_admin_users[i]; i++) {
		if(i != 0 && i % 4 == 0) {
			ABP(ent, "\n");
		}
		if(g_admin_users[i]->level != 0)
			ABP(ent, va("^7%-36s ", g_admin_users[i]->username));
	}
	ABP(ent, "\n");
	ABP_end();
	return qtrue;
}

qboolean G_admin_removelevel( gentity_t *ent, int skiparg ) {
	int i = 0;
	int lvl = -1;
	int levelcount = 0;
	int levindex = -1;
	qboolean found = qfalse;
	char arg[MAX_TOKEN_CHARS];

	if(Q_SayArgc() != 2 + skiparg ) {
		AIP(ent, "^3usage: ^7!removelevel <level>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg, sizeof(arg));

	lvl = atoi(arg);

	if(ent && lvl > ent->client->sess.uinfo.level) {
		AIP(ent, "^3!editcommands: ^7you can't edit levels higher than yours.");
		return qfalse;
	}

	for(levelcount = 0; g_admin_levels[levelcount]; levelcount++) {
		;
	}
	
	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == lvl && lvl != 0) {
			found = qtrue;
			levindex = i;
			break;
		}
	}
	
	if(!found) {
		AIP(ent, "^3!removelevel: ^7level not found.");
		return qfalse;
	}
	
	
	free(g_admin_levels[levindex]);
	g_admin_levels[levindex] = NULL;
	levelcount--;
	
	if(levindex != levelcount) {
		g_admin_levels[levindex] = g_admin_levels[levelcount];
		g_admin_levels[levelcount] = NULL;

		qsort(g_admin_levels, levelcount, sizeof(admin_level_t*), adminComparator);
	}
	
	for(i = 0; g_admin_users[i]; i++) {
		if(g_admin_users[i]->level == lvl) {
			g_admin_users[i]->level = 0;
		}
	}

	for(i = 0; i < level.numConnectedClients; i++) {
		gentity_t *target = NULL;
		int id = level.sortedClients[i];
		if((target = g_entities + id)->client->sess.uinfo.level == lvl) {
			target->client->sess.uinfo.level = 0;
		}
	}
	
	AIP(ent, va("^3!removelevel: ^7level %d removed.", lvl));
	G_admin_writeconfig();
	
	return qtrue;
}


#ifdef EDITION999

qboolean G_admin_noclip(gentity_t *ent, int skiparg) {
	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(Q_SayArgc() > 2 + skiparg) {
		AIP(ent, "^3usage:^7 !noclip <player>");
		return qfalse;
	}

	if(Q_SayArgc() == 1 + skiparg) {
		if(!ent) {
			return qfalse;
		}
		target = ent;
	} else {
		Q_SayArgv(1 + skiparg, name, sizeof(name));
	
		if(!(target = getPlayerForName(name, err, sizeof(err)))) {
			AIP(ent, va("^3!noclip: ^7%s", err));
			return qfalse;
		}
	}

	if(target->client->noclip) {
		target->client->noclip = qfalse;
	} else {
		target->client->noclip = qtrue;
	}
	return qtrue;
}

#else

qboolean G_admin_noclip(gentity_t *ent, int skiparg) {

	char name[MAX_TOKEN_CHARS];
	char err[MAX_STRING_CHARS];
	gentity_t *target;

	if(level.noNoclip) {
		AIP(ent, "^3!noclip:^7 noclip is disabled on this map.");
		return qfalse;
	}

	if(Q_SayArgc() > 2 + skiparg) {
		AIP(ent, "^3usage:^7 !noclip <player>");
		return qfalse;
	}

	if(Q_SayArgc() == 1 + skiparg) {
		if(!ent) {
			return qfalse;
		}
		target = ent;
	} else {
		Q_SayArgv(1 + skiparg, name, sizeof(name));
	
		if(!(target = getPlayerForName(name, err, sizeof(err)))) {
			AIP(ent, va("^3!noclip: ^7%s", err));
			return qfalse;
		}
	}

    if(ent && target->client->sess.uinfo.level >= ent->client->sess.uinfo.level) {
        AIP(ent, "^3!noclip:^7 can't noclip fellow admins.");
        return qfalse;
    }

	if(ent->client->noclip) {
		ent->client->noclip = qfalse;
	} else {
		ent->client->noclip = qtrue;
	}
}

#endif