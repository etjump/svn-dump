#include "g_admin.h"
#include "g_local.h"

// I don't think anyone needs over 64 admin levels.

admin_level_t *g_admin_levels[MAX_ADMIN_LEVELS];

// 2048 users for now. If there's a reason to waste any more
// memory I'll edit this.

admin_user_t *g_admin_users[MAX_USERS];

struct g_admin_cmd {
	const char *keyword;
	qboolean (* const handler)(gentity_t *ent, int skiparg);
	char flag;
	const char *function;
};

static const struct g_admin_cmd g_admin_cmds[] = {
	{"setlevel", G_admin_setlevel, 's', ""},
	{"admintest", G_admin_admintest, 'a', ""},
	{"kick", G_admin_kick, 'k', ""},
	{"finger", G_admin_finger, 'f', ""},
	{"", NULL, '\0', ""}
};

void G_admin_chat_print(char *string) {
	AP(va("chat \"%s", string));
	G_Printf("%s\n", string);
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
	trap_FS_Write("greeting = hi\n", 14, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 1\n", 10, f);
	trap_FS_Write("name = ET Admin I\n", 18, f);
	trap_FS_Write("commands = ak\n", 14, f);
	trap_FS_Write("greeting = hi\n", 14, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 2\n", 10, f);
	trap_FS_Write("name = ET Admin II\n", 19, f);
	trap_FS_Write("commands = ak\n", 14, f);
	trap_FS_Write("greeting = hi\n", 14, f);
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
	
	if(!*g_admin.string) return;

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


qboolean G_admin_readconfig(gentity_t *ent) {
	admin_level_t *temp_level = NULL;
	admin_user_t  *temp_user = NULL;
	int lc = 0, uc = 0;
	fileHandle_t f;
	int len;
	char *data, *data2;
	char *t;
	qboolean level_open, user_open;

	if(!*g_admin.string) return qfalse;

	len = trap_FS_FOpenFile(g_admin.string, &f, FS_READ);

	if(len < 0) {
		G_Printf(va("adminsystem: could not open %s.\n", g_admin.string));
		if(ent) CP(va("print \"adminsystem: could not open %s.\n\"", g_admin.string));
		G_admin_writeconfig_default();
		return qfalse;
	}

	data = (char*)malloc(len+1);
	data2 = data;
	trap_FS_Read(data, len, f);
	*(data + len) = '\0';

	G_admin_cleanup();

	t = COM_Parse(&data);
	level_open = user_open = qfalse;

	while(*t) {
		// New block found
		if( !Q_stricmp(t, "[level]") ||
			!Q_stricmp(t, "[user]") ) {
			if(level_open) g_admin_levels[lc++] = temp_level;
			else if(user_open) g_admin_users[uc++] = temp_user;
			level_open = user_open = qfalse;
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
		t = COM_Parse(&data);
	}

	if(level_open) g_admin_levels[lc++] = temp_level;
	if(user_open) g_admin_users[uc++] = temp_user;

	free(data2);
	G_Printf(va("^7readconfig: loaded %d levels and %d users\n", lc, uc));
	if(ent) CP(va("print \"^7readconfig: loaded %d levels and %d users\"\n", lc, uc));

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
				}
			*flags++;
			}
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
			return qtrue;
		}
		else {
			ACP(va("%s: permission denied", g_admin_cmds[i].keyword));
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
	char arg[MAX_TOKEN_CHARS];
	int clientNum;

	gentity_t *target;

	if(Q_SayArgc() != (3+skiparg)) {
		if(ent) CP(va("print \"usage: setlevel <user> <level>\n\""));
		else G_Printf(va("usage: setlevel <user> <level>\n"));
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, arg, sizeof(arg));

	if ((clientNum = ClientNumberFromString(ent, arg)) == -1) {
		ACP("setlevel: client is not on server.");
		return qfalse;
	}

	target = g_entities + clientNum;
	// Allows client to use the register cmd
	target->client->sess.allowRegister = qtrue;

	Q_SayArgv(2 + skiparg, arg, sizeof(arg));

	level = atoi(arg);

	if(ent) {
		if(level > G_admin_get_level(ent)) {
			ACP("^7setlevel: you may not setlevel higher than your current level.");
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
		ACP("setlevel: level not found.");
		return qfalse;
	}
	trap_SendServerCommand(clientNum, "server_setlevel");
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
	char level[MAX_TOKEN_CHARS];
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

	// Give user a level + a "finger" name

	for(i = 0; g_admin_users[i]; i++) {
		if(!Q_stricmp(arg, g_admin_users[i]->password)) {

			ent->client->sess.uinfo.level = temp.level;

			Q_strncpyz(ent->client->sess.uinfo.name,
				ent->client->pers.netname, sizeof(ent->client->sess.uinfo.name));

			g_admin_users[i]->level = temp.level;
			Q_strncpyz(g_admin_users[i]->password, ent->client->pers.netname, sizeof(g_admin_users[i]->password));

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

	AP(va("chat \"^7setlevel: %s^7 is now a level %d user\"", ent->client->pers.netname, ent->client->sess.uinfo.level));

	G_admin_writeconfig();

	G_clear_temp_admin();
}

qboolean G_admin_admintest(gentity_t *ent, int skiparg) {
	int level = ent->client->sess.uinfo.level, i;
	char name[MAX_NETNAME];
	if(!ent) return qtrue;

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == level) {
			Q_strncpyz(name, g_admin_levels[i]->name, sizeof(name));
			break;
		}
	}

	ACP(va("^7admintest: %s^7 is a level %d user (%s^7)", ent->client->pers.netname, level , name));
	return qtrue;
}

void G_admin_login(gentity_t *ent) {
	int i;
	qboolean found = qfalse;
	char arg[MAX_TOKEN_CHARS];
	char password[PASSWORD_LEN+1];
	
	if(trap_Argc() != 2) {
		return;
	}

	trap_Argv(1, arg, sizeof(arg));

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
		for(i = 0; g_admin_users[i]; i++) {
			if(g_admin_levels[i]->level == 0) {
				Q_strncpyz(ent->client->sess.uinfo.name, g_admin_users[i]->name, sizeof(ent->client->sess.uinfo.name));
				found = qtrue;
				break;
			}
		}
		if(!found) {
			Q_strncpyz(ent->client->sess.uinfo.name, "ET Jumper", sizeof(ent->client->sess.uinfo.name));
		}
	}
}
///////////////////
// Kick
// Usage: !kick <player> <reason> <timeout> - timeout/reason not necessary

qboolean G_admin_kick(gentity_t *ent, int skiparg) {
	int timeout = 0;
	int clientNum;
	char name[MAX_TOKEN_CHARS];
	char reason[MAX_TOKEN_CHARS] = "Player kicked!";
	char length[MAX_TOKEN_CHARS];
	gentity_t *target;

	if(Q_SayArgc() - skiparg < 2 || Q_SayArgc() - skiparg > 4) {
		ACP("usage: kick <player> <reason> <timeout>");
		return qfalse;
	}

	Q_SayArgv(1 + skiparg, name, sizeof(name));

	if ((clientNum = ClientNumberFromString(ent, name)) == -1) {
		ACP("adminsystem: target not found.");
		return qfalse;
	}

	target = g_entities + clientNum;
	if(ent) {
		if(target->client->sess.uinfo.level > ent->client->sess.uinfo.level) {
			ACP("adminsystem: you can't kick a higher level player!");
			return qfalse;
		} else if(target == ent) {
			ACP("adminsystem: you can't kick yourself!");
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

	trap_DropClient(clientNum, reason, timeout);
	
}

// Handles setlevel on connect &
// greeting

void G_admin_identify(gentity_t *ent) {
	int clientNum;
	clientNum = g_entities - ent;

	trap_SendServerCommand(clientNum, "identify_self");
}

void G_admin_identify_all() {
	int i;
	for(i = 0; i < level.numConnectedClients; i++) {
		trap_SendServerCommand(i, "identify_self");
	}
}

void G_admin_greeting(gentity_t *ent) {
	char *greeting;
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

qboolean G_admin_finger(gentity_t *ent, int skipargs) {
	int clientNum;
	int i;
	char name[MAX_TOKEN_CHARS];
	char levelname[MAX_NETNAME];
	qboolean found = qfalse;
	gentity_t *target;

	if(Q_SayArgc() - skipargs != 2) {
		ACP("usage: !finger <name>");
		return qfalse;
	}

	if ((clientNum = ClientNumberFromString(ent, name)) == -1)
		return qfalse;

	target = g_entities + clientNum;

	for(i = 0; g_admin_levels[i]; i++) {
		if(g_admin_levels[i]->level == target->client->sess.uinfo.level) {
			Q_strncpyz(levelname, g_admin_levels[i]->name, sizeof(levelname));
			break;
		}
	}

	ACP(va("^7finger: %s ^7(%s^7) is a level %d user (%s^7)", target->client->pers.netname,
		target->client->sess.uinfo.name, target->client->sess.uinfo.level, levelname));
	return qtrue;
}
