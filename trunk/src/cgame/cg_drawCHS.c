/*
 * Crosshair stats HUD inspired by DeFRaG one.
 */

#include "cg_local.h"

// TODO make font size configurable via cvars, see CG_DrawSpeed2()
#define CHSCHAR_SIZEX	0.2
#define CHSCHAR_SIZEY	0.2

static playerState_t *CG_CHS_GetPlayerState(void)
{
	if (cg.snap->ps.clientNum != cg.clientNum) {
		return &cg.snap->ps;
	} else {
		// return predictedPlayerState if not spectating
		return &cg.predictedPlayerState;
	}
}

static void CG_CHS_PlayerStateSpeed(char *buf, int size)
{
	playerState_t *ps = CG_CHS_GetPlayerState();
	Com_sprintf(buf, size, "%.0f", VectorLength(ps->velocity));
}

static void CG_CHS_PlayerStateHealth(char *buf, int size)
{
	playerState_t *ps = CG_CHS_GetPlayerState();
	Com_sprintf(buf, size, "%d", ps->stats[STAT_HEALTH]);
}

static void CG_CHS_PlayerStateAmmo(char *buf, int size)
{
	// TODO
}

typedef struct {
	void (*fun)(char *, int);
	const char *name;	// used as a CHS2 prefix
	const char *desc;	// used to display info about the stat
} stat_t;

/*
 * Arrays of all statistics.
 * Note: keep them synchronized with DeFRaG CHS numbers (if possible).
 */
static stat_t stats[] = {
/*   0 */	{NULL},
/*   1 */	{CG_CHS_PlayerStateSpeed, "Speed", "player speed" },
/*   2 */	{CG_CHS_PlayerStateHealth, "Health", "player health"},
/*   3 */	{NULL},	// armor
/*   4 */	{CG_CHS_PlayerStateAmmo, "Ammo", "player ammo for currently selected weapon"},
/*   5 */	{NULL},	// health/armor
/*   6 */	{NULL},	// health/armor/ammo
/*   7 */	{NULL},	// empty
/*   8 */	{NULL},	// empty
/*   9 */	{NULL},	// empty

			{NULL}
};

static void CG_CHS_GetName(char *buf, int size, int statNum)
{
	if (!buf || size <= 0) {
		// no can do
		return;
	}

	// always terminate the buffer
	buf[0] = '\0';

	if (statNum < 0 || statNum >= sizeof(stats) / sizeof(stats[0])
			|| !stats[statNum].fun || !stats[statNum].name) {
		return;
	}

	// the stat is valid and has a name, fill the buffer
	Com_sprintf(buf, size, "%s: ", stats[statNum].name);
}

static void CG_CHS_GetValue(char *buf, int size, int statNum)
{
	if (!buf || size <= 0) {
		// no can do
		return;
	}

	// always terminate the buffer
	buf[0] = '\0';

	if (statNum < 0 || statNum >= sizeof(stats) / sizeof(stats[0])
			|| !stats[statNum].fun) {
		return;
	}

	// the stat is valid, fill the buffer
	stats[statNum].fun(buf, size);
}

// XXX isn't similar enum defined somewhere else already?
typedef enum {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT
} align_t;

static void CG_CHS_DrawSingleInfo(int x, int y, int stat, qboolean drawName, align_t align)
{
	char buf[1024];
	int l = 0;
	int x_off, y_off;

	if (drawName) {
		CG_CHS_GetName(buf, sizeof(buf), stat);
		l = strlen(buf);
	}
	CG_CHS_GetValue(buf + l, sizeof(buf) - l, stat);

	x_off = CG_Text_Width_Ext(buf, CHSCHAR_SIZEX, 0, &cgs.media.limboFont1);
	y_off = CG_Text_Height_Ext(buf, CHSCHAR_SIZEY, 0, &cgs.media.limboFont1) / 2;
	switch (align) {
		case ALIGN_LEFT:
			x_off = 0;
			break;
		case ALIGN_CENTER:
			x_off /= 2;
			break;
		case ALIGN_RIGHT:
			break;
	}
	CG_Text_Paint_Ext(x - x_off, y + y_off, CHSCHAR_SIZEX, CHSCHAR_SIZEY,
			colorWhite, buf, 0, 0, 0, &cgs.media.limboFont1);
}

void CG_DrawCHS(void)
{
	int x, y;

	// CHS1
	if (cg_drawCHS1.integer) {
		x = (SCREEN_WIDTH / 2) - 1;
		y = (SCREEN_HEIGHT / 2) - 1;
		/*      1
		 *   8     2
		 * 7    x    3
		 *   6     4
		 *      5
		 */
		CG_CHS_DrawSingleInfo(x,      y - 20, cg_CHS1Info1.integer, qfalse, ALIGN_CENTER);
		CG_CHS_DrawSingleInfo(x + 10, y - 10, cg_CHS1Info2.integer, qfalse, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x + 20, y,      cg_CHS1Info3.integer, qfalse, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x + 10, y + 10, cg_CHS1Info4.integer, qfalse, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x,      y + 20, cg_CHS1Info5.integer, qfalse, ALIGN_CENTER);
		CG_CHS_DrawSingleInfo(x - 10, y + 10, cg_CHS1Info6.integer, qfalse, ALIGN_RIGHT);
		CG_CHS_DrawSingleInfo(x - 20, y,      cg_CHS1Info7.integer, qfalse, ALIGN_RIGHT);
		CG_CHS_DrawSingleInfo(x - 10, y - 10, cg_CHS1Info8.integer, qfalse, ALIGN_RIGHT);
	}

	// CHS2
	if (cg_drawCHS2.integer) {
		x = 30;
		y = (SCREEN_HEIGHT / 2) + 40;
		CG_CHS_DrawSingleInfo(x, y +  0, cg_CHS2Info1.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 10, cg_CHS2Info2.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 20, cg_CHS2Info3.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 30, cg_CHS2Info4.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 40, cg_CHS2Info5.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 50, cg_CHS2Info6.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 60, cg_CHS2Info7.integer, qtrue, ALIGN_LEFT);
		CG_CHS_DrawSingleInfo(x, y + 70, cg_CHS2Info8.integer, qtrue, ALIGN_LEFT);
	}
}

void CG_InfoCHS_f(void)
{
	int i;
	for (i = 0; i < sizeof(stats) / sizeof(stats[0]); i++) {
		if (!stats[i].fun) {
			continue;
		}

		CG_Printf("% 3d: %s\n", i, stats[i].desc);
	}
}
