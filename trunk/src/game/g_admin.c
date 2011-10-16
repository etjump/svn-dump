#include "g_admin.h"
#include "g_local.h"
#include "g_sha1.h"

// I don't think anyone needs over 64 admin levels.

admin_level_t *g_admin_levels[MAX_ADMIN_LEVELS];

// 2048 users for now. If there's a reason to waste any more
// memory I'll edit this.

admin_user_t *g_admin_users[MAX_USERS];

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
	trap_FS_Write("commands = a\n", 10, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 1\n", 10, f);
	trap_FS_Write("name = ET Admin I\n", 18, f);
	trap_FS_Write("commands = ak\n", 12, f);
	trap_FS_Write("\n", 1, f);

	trap_FS_Write("[level]\n", 8, f);
	trap_FS_Write("level = 2\n", 10, f);
	trap_FS_Write("name = ET Admin II\n", 19, f);
	trap_FS_Write("commands = ak\n", 12, f);
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
	if(ent) CP(va("^7readconfig: loaded %d levels and %d users\n", lc, uc));

	if(lc == 0) {
		G_admin_writeconfig_default();
	}
	return qtrue;
}