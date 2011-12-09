#ifndef G_ADMIN_H
#define G_ADMIN_H

/////////////////////////////////////////////////////////////////////////////
// A new admin system since punkbuster dropped et support
// This system uses user passwords instead of guids to
// recognize people. Passwords will be hashed in Sha1
// algorithm to protect them from evil scriptkiddies.
//
// There will be admin levels. Each level has commands.
// Every command will have a unique flag that represents
// it. 
//
// User info (admin level & user data) will be stored in
// a file. (g_admin.string.dat)
//
// User info format will be something like this:
//
// [level] // Tells the parser that a new level block starts here
// level = 0 // Which level it is
// name = Uber Pollo // level name.
// commands = kbuma // Command flags. k = kick etc. Needs docs
// ^ These can be in any order, as long as they're after [level]
// it will be fine. 
//
// [user] // Tells the parser that a new user block starts here
// name = user name
// level = user level
// password = user password // This will be hashed for security purposes.
/////////////////////////////////////////////////////////////////////////////

// admin flags for variety of things
#define AF_IMMUNITY '1'
#define AF_NONAMECHANGELIMIT '2'
#define AF_NOVOTELIMIT '3'
#define AF_SILENTCOMMANDS '/'

#define MAX_COMMANDS 128
#define MAX_CMD_LEN 20
#define MAX_ADMIN_LEVELS 64
#define MAX_USERS 2048
#define MAX_ADMIN_NAME_LEN 36
#define PASSWORD_LEN 40
#define MAX_STRING_CHARS 1024
#define MAX_BANS 512

// Offset for ban, don't ask.
#define ADMIN_BAN_EXPIRE_OFFSET 946490400
#define ADMIN_MAX_SHOWBANS 30

typedef struct {
	int level;
	char name[MAX_ADMIN_NAME_LEN];
	char commands[MAX_COMMANDS];
	char greeting[MAX_STRING_CHARS];
} admin_level_t;

typedef struct {
	int level;
	char name[MAX_ADMIN_NAME_LEN];
	char password[PASSWORD_LEN+1];
} admin_user_t;

typedef struct {
	char name[MAX_NAME_LENGTH];
	char ip[18];
	char reason[MAX_STRING_CHARS];
	char made[50];
	int expires;
	char banner[MAX_NAME_LENGTH];
} admin_ban_t;

void G_admin_chat_print(char *string);

// Zero: very informative names for the macros.

#define ACP(x) G_admin_chat_print(x); // admin chat print
#define AIP(ent, msg) G_admin_personal_info_print(ent, msg); // Admin info print (if ent -> priv else g_printf)
#define ASP(x) G_admin_print(ent, x) 
#define ABP(ent, msg) G_admin_buffer_print(ent, msg);
#define ABP_begin() G_shrubbot_buffer_begin()
#define ABP_end() G_shrubbot_buffer_end(ent)

#define EDITION999

#ifdef EDITION999
#define AF_ADMINBYPASS 'Z'
#endif // EDITION999

#endif // G_ADMIN_H