
#include "g_local.h"

qboolean G_Admin_Readconfig() {
	char *data, *s;
	int len, i;
	fileHandle_t f;

	len = trap_FS_FOpenFile("admins.dat", &f, FS_READ);
	if(len > 0) {
		data = (char*)malloc(len+1);
		trap_FS_Read(data, len, f);
		trap_FS_FCloseFile(f);
		data[len] = '\0';
		for(i = 0; i < MAX_ADMINS; i++) {
			s = Info_ValueForKey(data, "password");
			if(!strlen(s)) {
				break;
			}
			Q_strncpyz(level.adminPasswords[i], s, MAX_PASSWORD_LEN);
			Info_RemoveKey(data, "password");
		}
		free(s);
		G_Printf("adminsystem: loading admin passwords... Loading done.\n");
		return qtrue;
	} else return qfalse;
}

