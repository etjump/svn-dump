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

#define MAX_COMMANDS 128
#define MAX_CMD_LEN 20
#define MAX_ADMIN_LEVELS 64
#define MAX_USERS 2048
#define MAX_ADMIN_NAME_LEN 36
#define PASSWORD_LEN 40

typedef struct {
	int level;
	char name[MAX_ADMIN_NAME_LEN];
	char commands[MAX_COMMANDS];
} admin_level_t;

typedef struct {
	int level;
	char name[MAX_ADMIN_NAME_LEN];
	char password[PASSWORD_LEN+1];
} admin_user_t;

void G_admin_chat_print(char *string);

#define ACP(x) G_admin_chat_print(x);

#endif