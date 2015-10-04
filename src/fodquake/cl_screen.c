/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai
Copyright (C) 2005-2007, 2009-2011 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "quakedef.h"
#include "keys.h"
#include "image.h"
#include "menu.h"
#include "netqw.h"
#include "sbar.h"

#include "teamplay.h"
#include "utils.h"
#include "modules.h"
#include "strl.h"

#include "config.h"

#include "server_browser.h"
#include "context_sensitive_tab.h"
#include "lua.h"

#ifdef GLQUAKE
#include "gl_local.h"
#else
#include "r_local.h"
#endif

#ifdef GLQUAKE
int				glx, gly, glwidth, glheight;
#endif

#if USE_PNG
#define			DEFAULT_SSHOT_FORMAT		"png"
#else
#ifdef GLQUAKE
#define			DEFAULT_SSHOT_FORMAT		"tga"
#else
#define			DEFAULT_SSHOT_FORMAT		"pcx"
#endif
#endif

char *COM_FileExtension (char *in);


extern byte	current_pal[768];	// Tonik


// only the refresh window will be updated unless these variables are flagged 
int				scr_copytop;
int				scr_copyeverything;

float			scr_con_current;
float			scr_conlines;           // lines of console to display

static float con_cursorspeed = 4;

#define		MAXCMDLINE	256

unsigned int scr_clearnotifylines;

float			oldscreensize, oldfov, oldsbar;
cvar_t			scr_viewsize = {"viewsize", "100", CVAR_ARCHIVE};
cvar_t			scr_fov = {"fov", "90", CVAR_ARCHIVE};	// 10 - 140
cvar_t			scr_consize = {"scr_consize", "0.75"};
cvar_t			scr_conspeed = {"scr_conspeed", "1000"};
cvar_t			scr_centertime = {"scr_centertime", "2"};
cvar_t			scr_showram = {"showram", "1"};
cvar_t			scr_showturtle = {"showturtle", "0"};
cvar_t			scr_showpause = {"showpause", "1"};
cvar_t			scr_printspeed = {"scr_printspeed", "8"};
qboolean OnChange_scr_allowsnap(cvar_t *, char *);
cvar_t			scr_allowsnap = {"scr_allowsnap", "0", 0, OnChange_scr_allowsnap};

cvar_t			scr_clock = {"cl_clock", "0"};
cvar_t			scr_clock_x = {"cl_clock_x", "0"};
cvar_t			scr_clock_y = {"cl_clock_y", "-1"};

cvar_t			scr_gameclock = {"cl_gameclock", "0"};
cvar_t			scr_gameclock_x = {"cl_gameclock_x", "0"};
cvar_t			scr_gameclock_y = {"cl_gameclock_y", "-3"};

cvar_t			scr_democlock = {"cl_democlock", "0"};
cvar_t			scr_democlock_x = {"cl_democlock_x", "0"};
cvar_t			scr_democlock_y = {"cl_democlock_y", "-2"};

cvar_t			show_speed = {"show_speed", "0"};
cvar_t			show_speed_x = {"show_speed_x", "-1"};
cvar_t			show_speed_y = {"show_speed_y", "1"};

cvar_t			show_fps = {"show_fps", "0"};
cvar_t			show_fps_x = {"show_fps_x", "-5"};
cvar_t			show_fps_y = {"show_fps_y", "-1"};

cvar_t			show_framestddev = {"show_framestddev", "0"};
cvar_t			show_framestddev_x = {"show_framestddev_x", "-5"};
cvar_t			show_framestddev_y = {"show_framestddev_y", "-2"};

cvar_t			scr_sshot_format		= {"sshot_format", DEFAULT_SSHOT_FORMAT};
cvar_t			scr_sshot_dir			= {"sshot_dir", ""};

#ifdef GLQUAKE
cvar_t			gl_triplebuffer = {"gl_triplebuffer", "1", CVAR_ARCHIVE};
#endif

cvar_t			scr_autoid		= {"scr_autoid", "0"};
cvar_t			scr_coloredText = {"scr_coloredText", "1"};

qboolean		scr_initialized;                // ready to draw

static struct Picture *pausepic;
static struct Picture *loadingpic;

static struct Picture *rampic;
static struct Picture *netpic;
static struct Picture *turtlepic;

int				scr_fullupdate;

int				clearconsole;
static int				clearnotify;

viddef_t		vid;                            // global video state

vrect_t			scr_vrect;

qboolean		scr_skipupdate;

qboolean		scr_drawloading;
qboolean		scr_disabled_for_loading;
float			scr_disabled_time;

qboolean		block_drawing;


static int scr_autosshot_countdown = 0;
char auto_matchname[2 * MAX_OSPATH];

static void SCR_CheckAutoScreenshot(void);


qboolean OnChange_scr_allowsnap(cvar_t *var, char *s) {
	return (cls.state >= ca_connected && cbuf_current == cbuf_svc);
}

/**************************** CENTER PRINTING ********************************/

char		scr_centerstring[1024];
float		scr_centertime_start;   // for slow victory printing
float		scr_centertime_off;
int			scr_center_lines;
int			scr_erase_lines;
int			scr_erase_center;

//Called for important messages that should stay in the center of the screen for a few moments
void SCR_CenterPrint(char *str)
{
	Q_strncpyz (scr_centerstring, str, sizeof(scr_centerstring));
	scr_centertime_off = scr_centertime.value;
	scr_centertime_start = cl.time;

	// count the number of lines for centering
	scr_center_lines = 1;
	while (*str)
	{
		if (*str == '\n')
			scr_center_lines++;
		str++;
	}
}

void SCR_DrawCenterString(void)
{
	char *start;
	int l, j, x, y, remaining;

	// the finale prints the characters one at a time
	remaining = cl.intermission ? scr_printspeed.value * (cl.time - scr_centertime_start) : 9999;

	scr_erase_center = 0;
	start = scr_centerstring;

	y = (scr_center_lines <= 4) ? vid.conheight * 0.35 : 48;

	while (1)
	{
		// scan the width of the line
		for (l = 0; l < 40; l++)
		{
			if (start[l] == '\n' || !start[l])
				break;
		}

		x = (vid.conwidth - l * 8) / 2;

		for (j = 0; j < l; j++, x += 8)
		{
			Draw_Character (x, y, start[j]);        
			if (!remaining--)
				return;
		}

		y += 8;

		while (*start && *start != '\n')
			start++;

		if (!*start)
			break;

		start++;                // skip the \n
	}
}

void SCR_CheckDrawCenterString(void)
{
	scr_copytop = 1;
	if (scr_center_lines > scr_erase_lines)
		scr_erase_lines = scr_center_lines;

	scr_centertime_off -= cls.frametime;

	if (scr_centertime_off <= 0 && !cl.intermission)
		return;
	if (key_dest != key_game)
		return;

	SCR_DrawCenterString ();
}

void SCR_EraseCenterString(void)
{
	int y;

	if (scr_erase_center++ > vid.numpages)
	{
		scr_erase_lines = 0;
		return;
	}

	y = (scr_center_lines <= 4) ? vid.conheight * 0.35 : 48;

	scr_copytop = 1;
	Draw_TileClear (0, y, vid.conwidth, min(8 * scr_erase_lines, vid.conheight - y - 1));
}

/************************************ FOV ************************************/

static void CalcFov(float fov, float *fov_x, float *fov_y, float width, float height)
{
	float t;
	float fovx;
	float fovy;

	if (fov < 10)
		fov = 10;
	else if (fov > 140)
		fov = 140;

	if (width / 4 < height /3)
	{
		fovx = fov;
		t = width / tan(fovx / 360 * M_PI);
		fovy = atan (height / t) * 360 / M_PI;
	}
	else
	{
		fovx = fov;
		t = 4.0 / tan(fovx / 360 * M_PI);
		fovy = atan (3.0 / t) * 360 / M_PI;
		t = height / tan(fovy / 360 * M_PI);
		fovx = atan (width / t) * 360 / M_PI;
	}

	if (fovx < 10 || fovx > 140)
	{
		if (fovx < 10)
			fovx = 10;
		else if (fovx > 140)
			fovx = 140;

		t = width / tan(fovx / 360 * M_PI);
		fovy = atan (height / t) * 360 / M_PI;
	}

	if (fovy < 10 || fovy > 140)
	{
		if (fovy < 10)
			fovy = 10;
		else if (fovy > 140)
			fovy = 140;

		t = height / tan(fovy / 360 * M_PI);
		fovx = atan (width / t) * 360 / M_PI;
	}

	if (fovx < 1 || fovx > 179 || fovy < 1 || fovy > 179)
		Sys_Error ("CalcFov: Bad fov (%f, %f)", fovx, fovy);

	*fov_x = fovx;
	*fov_y = fovy;
}

//Must be called whenever vid changes
static void SCR_CalcRefdef (void)
{
	float size;
#ifdef GLQUAKE
	int h;
	qboolean full = false;
#else
	vrect_t vrect;
#endif

	scr_fullupdate = 0;             // force a background redraw
	vid.recalc_refdef = 0;

	// force the status bar to redraw
	Sbar_Changed ();
	
	// bound viewsize
	if (scr_viewsize.value < 30)
		Cvar_Set (&scr_viewsize, "30");
	if (scr_viewsize.value > 120)
		Cvar_Set (&scr_viewsize, "120");

	// bound field of view
	if (scr_fov.value < 10)
		Cvar_Set (&scr_fov, "10");
	if (scr_fov.value > 140)
		Cvar_Set (&scr_fov, "140");

	// intermission is always full screen   
	size = cl.intermission ? 120 : scr_viewsize.value;

	if (size >= 120)
		sb_lines = 0;           // no status bar at all
	else if (size >= 110)
		sb_lines = 24;          // no inventory
	else
		sb_lines = 24 + 16 + 8;

#ifdef GLQUAKE

	if (scr_viewsize.value >= 100.0)
	{
		full = true;
		size = 100.0;
	}
	else
	{
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		sb_lines = 0;
	}
	size /= 100.0;

	if (!cl_sbar.value && full)
		h = vid.conheight;
	else
		h = vid.conheight - sb_lines;

	r_refdef.vrect.width = vid.conwidth * size;
	if (r_refdef.vrect.width < 96)
	{
		size = 96.0 / r_refdef.vrect.width;
		r_refdef.vrect.width = 96;      // min for icons
	}

	r_refdef.vrect.height = vid.conheight * size;
	if (cl_sbar.value || !full)
	{
  		if (r_refdef.vrect.height > vid.conheight - sb_lines)
  			r_refdef.vrect.height = vid.conheight - sb_lines;
	}
	else if (r_refdef.vrect.height > vid.conheight)
	{
			r_refdef.vrect.height = vid.conheight;
	}
	r_refdef.vrect.x = (vid.conwidth - r_refdef.vrect.width) / 2;
	if (full)
		r_refdef.vrect.y = 0;
	else 
		r_refdef.vrect.y = (h - r_refdef.vrect.height) / 2;

	CalcFov(scr_fov.value, &r_refdef.fov_x, &r_refdef.fov_y, r_refdef.vrect.width, r_refdef.vrect.height);

	scr_vrect = r_refdef.vrect;

	Draw_SizeChanged();
#else

	CalcFov(scr_fov.value, &r_refdef.fov_x, &r_refdef.fov_y, r_refdef.vrect.width, r_refdef.vrect.height);

#warning No idea of the below lines are correct.
	// these calculations mirror those in R_Init() for r_refdef, but take noaccount of water warping
	vrect.x = 0;
	vrect.y = 0;
	vrect.width = vid.displaywidth;
	vrect.height = vid.displayheight;

	R_SetVrect (&vrect, &scr_vrect, sb_lines);

	// guard against going from one mode to another that's less than half the vertical resolution
	scr_con_current = min(scr_con_current, vid.conheight);

	// notify the refresh of the change
	R_ViewChanged (&vrect, sb_lines, vid.aspect);

#endif
}

//Keybinding command
void SCR_SizeUp_f(void)
{
	Cvar_SetValue (&scr_viewsize, scr_viewsize.value + 10);
	vid.recalc_refdef = 1;
}

//Keybinding command
void SCR_SizeDown_f(void)
{
	Cvar_SetValue (&scr_viewsize,scr_viewsize.value - 10);
	vid.recalc_refdef = 1;
}

/********************************** ELEMENTS **********************************/

void SCR_DrawRam(void)
{
	if (!scr_showram.value)
		return;

	if (!r_cache_thrash)
		return;

	Draw_DrawPicture(rampic, scr_vrect.x + 32, scr_vrect.y, 32, 32);
}

void SCR_DrawTurtle(void)
{
	static int count;
	
	if (!scr_showturtle.value)
		return;

	if (cls.frametime < 0.1)
	{
		count = 0;
		return;
	}

	count++;
	if (count < 3)
		return;

	Draw_DrawPicture(turtlepic, scr_vrect.x, scr_vrect.y, 32, 32);
}

void SCR_DrawNet (void)
{
	if (cls.demoplayback)
		return;

#ifdef NETQW
	if (!cls.netqw)
		return;

	if (NetQW_GetTimeSinceLastPacketFromServer(cls.netqw) < 500000)
		return;
#else
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged < UPDATE_BACKUP-1)
		return;
#endif

	Draw_DrawPicture(netpic, scr_vrect.x + 64, scr_vrect.y, 32, 32);
}

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.conwidth - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.conheight - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

void SCR_DrawFPS (void)
{
	double t;
	int x, y;
	char str[80];
	static float lastfps;
	static double lastframetime;
	static int last_fps_count;
	extern int fps_count;

	if (!show_fps.value)
		return;

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 1.0)
	{
		lastfps = (fps_count - last_fps_count) / (t - lastframetime);
		last_fps_count = fps_count;
		lastframetime = t;
	}

	snprintf(str, sizeof(str), "%3.1f%s", lastfps + 0.05, show_fps.value == 2 ? " FPS" : "");
	x = ELEMENT_X_COORD(show_fps);
	y = ELEMENT_Y_COORD(show_fps);
	Draw_String (x, y, str);
}

void SCR_DrawFrameStdDev (void)
{
	double t;
	int x, y;
	char str[80];
	static double lastframestddev;
	static double framedevcum;
	static double lastframetime;
	static int last_fps_count;
	extern int fps_count;

	if (!show_framestddev.value)
		return;

	framedevcum+= (cls.framedev*1000)*(cls.framedev*1000);

	t = Sys_DoubleTime();
	if ((t - lastframetime) >= 1.0)
	{
		lastframestddev = sqrt(framedevcum/(fps_count-last_fps_count));
		last_fps_count = fps_count;
		framedevcum = 0;
		lastframetime = t;
	}

	snprintf(str, sizeof(str), "%3.1f%s", lastframestddev, show_framestddev.value == 2 ? " frame stddev" : "");
	x = ELEMENT_X_COORD(show_framestddev);
	y = ELEMENT_Y_COORD(show_framestddev);
	Draw_String (x, y, str);
}

void SCR_DrawSpeed (void)
{
	double t;
	int x, y, mynum;
	char str[80];
	vec3_t vel;
	float speed;
	static float maxspeed = 0, display_speed = -1;
	static double lastframetime = 0;
	static int lastmynum = -1;

	if (!show_speed.value)
		return;

	t = Sys_DoubleTime();

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1)
		mynum = cl.playernum;

	if (mynum != lastmynum)
	{
		lastmynum = mynum;
		lastframetime = t;
		display_speed = -1;
		maxspeed = 0;
	}

	if (!cl.spectator || cls.demoplayback || mynum == cl.playernum)
		VectorCopy (cl.simvel, vel);
	else
		VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[mynum].velocity, vel);

	vel[2] = 0;
	speed = VectorLength(vel);

	maxspeed = max(maxspeed, speed);

	if (display_speed >= 0)
	{
		snprintf(str, sizeof(str), "%3d%s", (int) display_speed, show_speed.value == 2 ? " SPD" : "");
		x = ELEMENT_X_COORD(show_speed);
		y = ELEMENT_Y_COORD(show_speed);
		Draw_String (x, y, str);
	}

	if (t - lastframetime >= 0.1)
	{
		lastframetime = t;
		display_speed = maxspeed;
		maxspeed = 0;
	}
}

void SCR_DrawClock (void)
{
	int x, y;
	time_t t;
	struct tm *ptm;
	char str[80];

	if (!scr_clock.value)
		return;

	if (scr_clock.value == 2)
	{
		time (&t);
		if ((ptm = localtime (&t)))
		{
			strftime (str, sizeof(str) - 1, "%H:%M:%S", ptm);
		}
		else
		{
			strcpy (str, "#bad date#");
		}
	}
	else
	{
		float time = (cl.servertime_works) ? cl.servertime : cls.realtime;
		Q_strncpyz (str, SecondsToHourString((int) time), sizeof(str));
	}

	x = ELEMENT_X_COORD(scr_clock);
	y = ELEMENT_Y_COORD(scr_clock);
	Draw_String (x, y, str);
}

void SCR_DrawGameClock (void)
{
	int x, y;
	char str[80], *s;
	float timelimit;

	if (!scr_gameclock.value)
		return;

	if (scr_gameclock.value == 2 || scr_gameclock.value == 4) 
		timelimit = 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit"));
	else
		timelimit = 0;

	if (cl.countdown || cl.standby)
		Q_strncpyz (str, SecondsToHourString(timelimit), sizeof(str));
	else
		Q_strncpyz (str, SecondsToHourString((int) abs(timelimit - cl.gametime)), sizeof(str));

	if ((scr_gameclock.value == 3 || scr_gameclock.value == 4) && (s = strstr(str, ":")))
		s++;		// or just use SecondsToMinutesString() ...
	else
		s = str;

	x = ELEMENT_X_COORD(scr_gameclock);
	y = ELEMENT_Y_COORD(scr_gameclock);
	Draw_String (x, y, s);
}

void SCR_DrawDemoClock (void)
{
	int x, y;
	char str[80];

	if (!cls.demoplayback || !scr_democlock.value)
		return;

	if (scr_democlock.value == 2)
		Q_strncpyz (str, SecondsToHourString((int) (cls.demotime)), sizeof(str));
	else
		Q_strncpyz (str, SecondsToHourString((int) (cls.demotime - demostarttime)), sizeof(str));

	x = ELEMENT_X_COORD(scr_democlock);
	y = ELEMENT_Y_COORD(scr_democlock);
	Draw_String (x, y, str);
}

void SCR_DrawPause (void)
{
	if (!scr_showpause.value)               // turn off for screenshots
		return;

	if (!cl.paused)
		return;

	if (pausepic == 0)
		pausepic = Draw_LoadPicture("gfx/pause.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	Draw_DrawPicture(pausepic, (vid.conwidth - 128) / 2, (vid.conheight - 48 - 24) / 2, 128, 24);
}

void SCR_DrawLoading (void)
{
	if (!scr_drawloading)
		return;

	if (loadingpic == 0)
		loadingpic = Draw_LoadPicture("gfx/loading.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	Draw_DrawPicture(loadingpic, (vid.conwidth - 144) / 2, (vid.conheight - 48 - 24) / 2, 144, 24);
}



void SCR_BeginLoadingPlaque (void)
{
	if (cls.state != ca_active)
		return;

	if (key_dest == key_console)
		return;

	// redraw with no console and the loading plaque
	scr_fullupdate = 0;
	Sbar_Changed ();
	scr_drawloading = true;
	SCR_UpdateScreen ();
	scr_drawloading = false;

	scr_disabled_for_loading = true;
	scr_disabled_time = cls.realtime;
	scr_fullupdate = 0;
}

void SCR_EndLoadingPlaque(void)
{
	if (!scr_disabled_for_loading)
		return;

	scr_disabled_for_loading = false;
	scr_fullupdate = 0;
}

/********************************** CONSOLE **********************************/

void SCR_SetUpToDrawConsole(void)
{
	Con_CheckResize(vid.conwidth);

	// decide on the height of the console
	if (SCR_NEED_CONSOLE_BACKGROUND)
	{
		scr_conlines = vid.conheight;		// full screen
		scr_con_current = scr_conlines;
	}
	else if (key_dest == key_console)
	{
		scr_conlines = vid.conheight * scr_consize.value;
		scr_conlines = bound(30, scr_conlines, vid.conheight);
	}
	else
	{
		scr_conlines = 0;				// none visible
	}

	if (scr_conlines < scr_con_current)
	{
		scr_con_current -= scr_conspeed.value * cls.trueframetime * vid.conheight / 320;
		scr_con_current = max(scr_con_current, scr_conlines);
	}
	else if (scr_conlines > scr_con_current)
	{
		scr_con_current += scr_conspeed.value * cls.trueframetime * vid.conheight / 320;
		scr_con_current = min(scr_con_current, scr_conlines);
	}

	if (clearconsole++ < vid.numpages)
	{
#ifndef GLQUAKE
		scr_copytop = 1;
		Draw_TileClear (0, (int) scr_con_current, vid.conwidth, vid.conheight - (int) scr_con_current);
#endif
		Sbar_Changed ();
	}
	else if (clearnotify++ < vid.numpages)
	{
#ifndef GLQUAKE
		scr_copytop = 1;
		Draw_TileClear(0, 0, vid.conwidth, scr_clearnotifylines * 8);
#endif
	}
	else
	{
		scr_clearnotifylines = 0;
	}

#ifndef GLQUAKE
	{
		extern cvar_t scr_conalpha;

		if (!scr_conalpha.value && scr_con_current)
		{

			Draw_TileClear(0, 0, vid.conwidth, scr_con_current);
		}
	}
#endif
}

static void SCR_DrawDownload(unsigned int vislines)
{
	int i, j, x, n;
	char *text;
	char dlbar[1024];
	unsigned int linewidth;
	unsigned int textlength;
	unsigned int dotlength;
	unsigned int barlength;

	linewidth = (vid.conwidth >> 3) - 2;

	// draw the download bar
	// figure out width
	if ((text = strrchr(cls.downloadname, '/')) != NULL)
		text++;
	else
		text = cls.downloadname;

	x = linewidth - ((linewidth * 7) / 40);
	i = linewidth/3;

	if (x - i < 11)
		return;

	textlength = strlen(text);

	if (strlen(text) > i)
	{
		barlength = x - i - 11;
		dotlength = 3;
		textlength = i;
	}
	else
	{
		barlength = x - strlen(text) - 8;
		dotlength = 0;
	}

	if (textlength + dotlength + 2 + 1 + barlength + 1 + 5 + 1 > sizeof(dlbar))
		return;

	memcpy(dlbar, text, textlength);
	if (dotlength)
		memcpy(dlbar + textlength, "...", 3);

	i = textlength + dotlength;

	memcpy(dlbar + i, ": \x80", 3);
	i += 3;

	// where's the dot go?
	n = barlength * cls.downloadpercent / 100;

	for (j = 0; j < barlength; j++)
		if (j == n)
			dlbar[i++] = '\x83';
		else
			dlbar[i++] = '\x81';
	dlbar[i++] = '\x82';

	sprintf(dlbar + i, " %02d%%", cls.downloadpercent);

	// draw it
	Draw_String(8, vislines - 22 + 8, dlbar);
}

static void SCR_DrawMessageMode(unsigned int y)
{
	char temp[MAXCMDLINE + 1];
	char *s;
	int len;
	int skip;

	clearnotify = 0;
	scr_copytop = 1;

	if (chat_team)
	{
		Draw_String(8, y, "say_team:");
		skip = 11;
	}
	else
	{
		Draw_String(8, y, "say:");
		skip = 5;
	}

	// FIXME: clean this up
	strlcpy(temp, chat_buffer, sizeof(temp));
	s = temp;

	// add the cursor frame
	if ((int) (curtime * con_cursorspeed) & 1)
	{
		if (chat_linepos == strlen(s))
			s[chat_linepos+1] = '\0';

		s[chat_linepos] = 11;
	}

	// prestep if horizontally scrolling
	if (chat_linepos + skip >= (vid.conwidth >> 3))
		s += 1 + chat_linepos + skip - (vid.conwidth >> 3);

	len = strlen(s);
	if (len+skip > (vid.conwidth>>3))
		len = (vid.conwidth>>3) - skip;

	if (len > 0)
		Draw_String_Length(skip<<3, y, s, len);
}

void SCR_DrawConsole(void)
{
	unsigned int lines;

	if (scr_con_current)
	{
		scr_copyeverything = 1;
		Con_DrawConsole(scr_con_current);
		Context_Sensitive_Tab_Completion_Draw();
		Context_Sensitive_Tab_Completion_Notification(false);
		clearconsole = 0;

		if (cls.download)
			SCR_DrawDownload(scr_con_current);
	}
	else
	{
		if (key_dest == key_game || key_dest == key_message)
		{
			lines = Con_DrawNotify();      // only draw notify in game

			if (key_dest == key_message)
			{
				SCR_DrawMessageMode(lines * 8);
				lines++;
			}

			if (lines)
			{
				clearnotify = 0;

				if (lines > scr_clearnotifylines)
					scr_clearnotifylines = lines;
			}
		}
	}
}

static void SCR_BeginChat(unsigned int is_chat_team)
{
	if (cls.state != ca_active)
		return;

	chat_team = is_chat_team;
	key_dest = key_message;
	chat_buffer[0] = 0;
	chat_linepos = 0;
}

static void Con_MessageMode_f(void)
{
	SCR_BeginChat(false);
}

static void Con_MessageMode2_f (void)
{
	SCR_BeginChat(true);
}

/*********************************** AUTOID ***********************************/

#ifdef GLQUAKE


int qglProject (float objx, float objy, float objz, float *model, float *proj, int *view, float* winx, float* winy, float* winz) {
	float in[4], out[4];
	int i;

	in[0] = objx; in[1] = objy; in[2] = objz; in[3] = 1.0;

	
	for (i = 0; i < 4; i++)
		out[i] = in[0] * model[0 * 4 + i] + in[1] * model[1 * 4 + i] + in[2] * model[2 * 4 + i] + in[3] * model[3 * 4 + i];

	
	for (i = 0; i < 4; i++)
		in[i] =	out[0] * proj[0 * 4 + i] + out[1] * proj[1 * 4 + i] + out[2] * proj[2 * 4 + i] + out[3] * proj[3 * 4 + i];

	if (!in[3])
		return 0;	

	VectorScale(in, 1 / in[3], in);

	
	*winx = view[0] + (1 + in[0]) * view[2] / 2;
	*winy = view[1] + (1 + in[1]) * view[3] / 2;
	*winz = (1 + in[2]) / 2;

	return 1;
}

typedef struct player_autoid_s {
	float x, y;
	player_info_t *player;
} autoid_player_t;

static autoid_player_t autoids[MAX_CLIENTS];
static int autoid_count;

#define ISDEAD(i) ((i) >= 41 && (i) <= 102)

void SCR_SetupAutoID (void) {
	int j, view[4], tracknum = -1;
	float model[16], project[16], winz, *origin;
	player_state_t *state;
	player_info_t *info;
	item_vis_t visitem;
	autoid_player_t *id;

	autoid_count = 0;

	if (!scr_autoid.value)
		return;

	if (cls.state != ca_active || !cl.validsequence)
		return;

	if (!cls.demoplayback && !cl.spectator)
		return;

	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	glGetFloatv(GL_PROJECTION_MATRIX, project);
	glGetIntegerv(GL_VIEWPORT, view);

	if (cl.spectator)
		tracknum = Cam_TrackNum();

	
	VectorCopy(vpn, visitem.forward);
	VectorCopy(vright, visitem.right);
	VectorCopy(vup, visitem.up);
	VectorCopy(r_origin, visitem.vieworg);

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) {
		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator)
			continue;

		if (
			(state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame)) ||
			state->modelindex == cl_modelindices[mi_h_player]
		)
			continue;

		if (R_CullSphere(state->origin, 0))
			continue;

		
		VectorCopy (state->origin, visitem.entorg);
		visitem.entorg[2] += 27;
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = 25;

		if (!TP_IsItemVisible(&visitem))
			continue;

		id = &autoids[autoid_count];
		id->player = info;
		origin = state->origin;
		if (qglProject(origin[0], origin[1], origin[2] + 28, model, project, view, &id->x, &id->y, &winz))
			autoid_count++;
	}
}

void SCR_DrawAutoID (void) {
	int i, x, y;

	if (!scr_autoid.value || (!cls.demoplayback && !cl.spectator))
		return;

	for (i = 0; i < autoid_count; i++) {
		x =  autoids[i].x * vid.conwidth / glwidth;
		y =  (glheight - autoids[i].y) * vid.conheight / glheight;
		Draw_String(x - strlen(autoids[i].player->name) * 4, y - 8, autoids[i].player->name);
	}
}

#endif

/********************************* TILE CLEAR *********************************/

#ifdef GLQUAKE

void SCR_TileClear (void) {
	if (cls.state != ca_active && cl.intermission) {
		Draw_TileClear (0, 0, vid.conwidth, vid.conheight);
		return;
	}

	if (r_refdef.vrect.x > 0) {
		// left
		Draw_TileClear (0, 0, r_refdef.vrect.x, vid.conheight - sb_lines);
		// right
		Draw_TileClear (r_refdef.vrect.x + r_refdef.vrect.width, 0, 
			vid.conwidth - (r_refdef.vrect.x + r_refdef.vrect.width), vid.conheight - sb_lines);
	}
	if (r_refdef.vrect.y > 0) {
		// top
		Draw_TileClear (r_refdef.vrect.x, 0, r_refdef.vrect.width, r_refdef.vrect.y);
	}
	if (r_refdef.vrect.y + r_refdef.vrect.height < vid.conheight - sb_lines) {
		// bottom
		Draw_TileClear (r_refdef.vrect.x,
			r_refdef.vrect.y + r_refdef.vrect.height, 
			r_refdef.vrect.width, 
			vid.conheight - sb_lines - (r_refdef.vrect.height + r_refdef.vrect.y));
	}
}

#else

void SCR_TileClear(void)
{
	if (scr_fullupdate++ < vid.numpages)
	{
		// clear the entire screen
		scr_copyeverything = 1;
		Draw_TileClear (0, 0, vid.conwidth, vid.conheight);
		Sbar_Changed ();
	}
	else
	{
		if (scr_viewsize.value < 100)
		{
			static const char str[11] = "xxxxxxxxxx";
			// clear background for counters
			if (show_speed.value)
				Draw_TileClear(ELEMENT_X_COORD(show_speed), ELEMENT_Y_COORD(show_speed), 10 * 8, 8);
			if (show_fps.value)
				Draw_TileClear(ELEMENT_X_COORD(show_fps), ELEMENT_Y_COORD(show_fps), 10 * 8, 8);
			if (show_framestddev.value)
				Draw_TileClear(ELEMENT_X_COORD(show_framestddev), ELEMENT_Y_COORD(show_framestddev), 19 * 8, 8);
			if (scr_clock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
			if (scr_gameclock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_gameclock), ELEMENT_Y_COORD(scr_gameclock), 10 * 8, 8);
			if (scr_democlock.value)
				Draw_TileClear(ELEMENT_X_COORD(scr_clock), ELEMENT_Y_COORD(scr_clock), 10 * 8, 8);
		}
	}
}

#endif

void SCR_DrawElements(void) {
	if (scr_drawloading) {
		SCR_DrawLoading ();
		Sbar_Draw ();
	} else {
		if (cl.intermission == 1) {
			Sbar_IntermissionOverlay ();
			Con_ClearNotify ();
		} else if (cl.intermission == 2) {
			Sbar_FinaleOverlay ();
			SCR_CheckDrawCenterString ();
			Con_ClearNotify ();
		}

		if (cls.state == ca_active) {
			SCR_DrawRam ();
			SCR_DrawNet ();
			SCR_DrawTurtle ();
			SCR_DrawPause ();
#ifdef GLQUAKE
			SCR_DrawAutoID ();
#endif
			if (!cl.intermission) {
				if (key_dest != key_menu)
					Draw_Crosshair ();
				SCR_CheckDrawCenterString ();
				SCR_DrawSpeed ();
				SCR_DrawClock ();
				SCR_DrawGameClock ();
				SCR_DrawDemoClock ();
				SCR_DrawFPS ();
				SCR_DrawFrameStdDev ();
				Sbar_Draw ();
			}
		}

		if (!scr_autosshot_countdown) {
			SCR_DrawConsole ();	
			M_Draw ();
		}
	}
}

/******************************* UPDATE SCREEN *******************************/

#ifdef GLQUAKE

//This is called every frame, and can also be called explicitly to flush text to the screen.
//WARNING: be very careful calling this from elsewhere, because the refresh needs almost the entire 256k of stack space!
void SCR_UpdateScreen (void) {
	if (!scr_initialized)
		return;                         // not initialized yet

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20)
			scr_disabled_for_loading = false;
		else
			return;
	}

#ifdef _WIN32
#if 0
	{	// don't suck up any cpu if minimized
		extern int Minimized;

		if (Minimized)
			return;
	}
#endif
#endif

	vid.numpages = 2 + gl_triplebuffer.value;

	scr_copytop = 0;
	scr_copyeverything = 0;

	// check for vid changes
	if (oldfov != scr_fov.value) {
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}

	if (oldscreensize != scr_viewsize.value) {
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (oldsbar != cl_sbar.value) {
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef)
		SCR_CalcRefdef ();

	if ((v_contrast.value > 1 && !VID_HWGammaSupported()) || gl_clear.value)
		Sbar_Changed ();

	// do 3D refresh drawing, and then update the screen
	VID_BeginFrame();

	glwidth = VID_GetWidth();
	glheight = VID_GetHeight();

	SCR_SetUpToDrawConsole ();

	V_RenderView ();

	SCR_SetupAutoID ();	

	GL_Set2D ();

	R_PolyBlend ();

	// draw any areas not covered by the refresh
	SCR_TileClear ();

	if (r_netgraph.value)
		R_NetGraph ();

	SCR_DrawElements();

	SB_Frame();

	Lua_Frame_2D();

	R_BrightenScreen ();

	V_UpdatePalette(false);

	SCR_CheckAutoScreenshot();	

	VID_Update(0);
}

#else

void SCR_UpdateScreen (void) {
	vrect_t vrect;

	if (!scr_initialized)
		return;                         // not initialized yet

	if (scr_skipupdate || block_drawing)
		return;

	if (scr_disabled_for_loading) {
		if (cls.realtime - scr_disabled_time > 20)
			scr_disabled_for_loading = false;
		else
			return;
	}

#ifdef _WIN32
	{	// don't suck up any cpu if minimized
		extern int Minimized;

		if (Minimized)
			return;
	}
#endif

	scr_copytop = 0;
	scr_copyeverything = 0;

	// check for vid changes
	if (oldfov != scr_fov.value) {
		oldfov = scr_fov.value;
		vid.recalc_refdef = true;
	}
	
	if (oldscreensize != scr_viewsize.value) {
		oldscreensize = scr_viewsize.value;
		vid.recalc_refdef = true;
	}

	if (oldsbar != cl_sbar.value) {
		oldsbar = cl_sbar.value;
		vid.recalc_refdef = true;
	}

	if (vid.recalc_refdef) {
		// something changed, so reorder the screen
		SCR_CalcRefdef ();
	}

	// do 3D refresh drawing, and then update the screen
	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	SCR_TileClear ();
	SCR_SetUpToDrawConsole ();
	SCR_EraseCenterString ();

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time

	VID_LockBuffer ();
	V_RenderView ();
	VID_UnlockBuffer ();

	D_EnableBackBufferAccess ();	// of all overlay stuff if drawing directly

	SCR_DrawElements();

	SB_Frame();

	Lua_Frame_2D();

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time
	V_UpdatePalette(false);

	// update one of three areas
	if (scr_copyeverything) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.displaywidth;
		vrect.height = vid.displayheight;
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else if (scr_copytop) {
		vrect.x = 0;
		vrect.y = 0;
		vrect.width = vid.displaywidth;
		vrect.height = vid.displayheight - sb_lines;
#warning This needs translation from con space to display space
		vrect.pnext = 0;

		VID_Update (&vrect);
	} else {
		vrect.x = scr_vrect.x;
		vrect.y = scr_vrect.y;
		vrect.width = scr_vrect.width;
		vrect.height = scr_vrect.height;
		vrect.pnext = 0;

		VID_Update (&vrect);
	}

	SCR_CheckAutoScreenshot();
}

#endif

void SCR_UpdateWholeScreen (void) {
	scr_fullupdate = 0;
	SCR_UpdateScreen ();
}

/******************************** SCREENSHOTS ********************************/

#define SSHOT_FAILED		-1
#define SSHOT_FAILED_QUIET	-2		//failed but don't print an error message
#define SSHOT_SUCCESS		0

typedef enum image_format_s {IMAGE_PCX, IMAGE_TGA, IMAGE_JPEG, IMAGE_PNG} image_format_t;

static char *SShot_ExtForFormat(int format) {
	switch (format) {
	case IMAGE_PCX: return ".pcx";
	case IMAGE_TGA: return ".tga";
	case IMAGE_JPEG: return ".jpg";
	case IMAGE_PNG: return ".png";
	}
	Sys_Error("SShot_ExtForFormat: unknown format");
	return "err";
}

static image_format_t SShot_FormatForName(char *name) {
	char *ext;
	
	ext = COM_FileExtension(name);

#ifdef GLQUAKE
	if (!Q_strcasecmp(ext, "tga"))
		return IMAGE_TGA;
#else
	if (!Q_strcasecmp(ext, "pcx"))
		return IMAGE_PCX;
#endif

#if USE_PNG
	else if (!Q_strcasecmp(ext, "png"))
		return IMAGE_PNG;
#endif

#if USE_JPEG
	else if (!Q_strcasecmp(ext, "jpg"))
		return IMAGE_JPEG;
#endif

#ifdef GLQUAKE
	else if (!Q_strcasecmp(scr_sshot_format.string, "tga"))
		return IMAGE_TGA;
#else
	else if (!Q_strcasecmp(scr_sshot_format.string, "pcx"))
		return IMAGE_PCX;
#endif

#if USE_PNG
	else if (!Q_strcasecmp(scr_sshot_format.string, "png"))
		return IMAGE_PNG;
#endif

#if USE_JPEG
	else if (!Q_strcasecmp(scr_sshot_format.string, "jpg") || !Q_strcasecmp(scr_sshot_format.string, "jpeg"))
		return IMAGE_JPEG;
#endif
	else
#if USE_PNG
		return IMAGE_PNG;
#else
#ifdef GLQUAKE
		return IMAGE_TGA;
#else
		return IMAGE_PCX;
#endif
#endif
}

#ifdef GLQUAKE

extern unsigned short ramps[3][256];
//applies hwgamma to RGB data
static void applyHWGamma(byte *buffer, int size) {
	int i;

	if (VID_HWGammaSupported()) {
		for (i = 0; i < size; i += 3) {
			buffer[i + 0] = ramps[0][buffer[i + 0]] >> 8;
			buffer[i + 1] = ramps[1][buffer[i + 1]] >> 8;
			buffer[i + 2] = ramps[2][buffer[i + 2]] >> 8;
		}
	}
}

int SCR_Screenshot(char *name) {
	int i, temp, buffersize;
	int success = SSHOT_FAILED;
	byte *buffer;
	image_format_t format;

	name = (*name == '/') ? name + 1 : name;
	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));
	buffersize = glwidth * glheight * 3;

	buffer = Q_Malloc (buffersize);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, buffer); 

#if USE_PNG
	if (format == IMAGE_PNG) {
		if (QLib_isModuleLoaded(qlib_libpng)) {
			applyHWGamma(buffer, buffersize);
			success = Image_WritePNG(name, image_png_compression_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;
		} else {
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
	}
#endif

#if USE_JPEG
	if (format == IMAGE_JPEG) {
		if (QLib_isModuleLoaded(qlib_libjpeg)) {
			applyHWGamma(buffer, buffersize);
			success = Image_WriteJPEG(name, image_jpeg_quality_level.value,
					buffer + buffersize - 3 * glwidth, -glwidth, glheight)
				? SSHOT_SUCCESS : SSHOT_FAILED;;
		} else {
			Com_Printf("Can't take a JPEG screenshot without libjpeg.");
			if (SShot_FormatForName("noext") == IMAGE_JPEG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");
			success = SSHOT_FAILED_QUIET;
		}
	}
#endif

	if (format == IMAGE_TGA) {
		// swap rgb to bgr
		for (i = 0; i < buffersize; i += 3)	{
			temp = buffer[i];
			buffer[i] = buffer[i + 2];
			buffer[i + 2] = temp;
		}
		applyHWGamma(buffer, buffersize);
		success = Image_WriteTGA(name, buffer, glwidth, glheight)
					? SSHOT_SUCCESS : SSHOT_FAILED;
	}

	free(buffer);
	return success;
}

#else

int SCR_Screenshot(char *name) {
	int success;
	image_format_t format;

	name = (*name == '/') ? name + 1 : name;
	format = SShot_FormatForName(name);
	COM_ForceExtension (name, SShot_ExtForFormat(format));

	D_EnableBackBufferAccess ();	// enable direct drawing of console to back buffer

	if (format == IMAGE_PCX)
	{
		success = Image_WritePCX (name, vid.buffer, vid.displaywidth, vid.displayheight, vid.rowbytes, current_pal) ? SSHOT_SUCCESS : SSHOT_FAILED;
	}
#if USE_PNG
	else if (format == IMAGE_PNG)
	{
		if (QLib_isModuleLoaded(qlib_libpng))
		{
			success = Image_WritePNGPLTE(name, image_png_compression_level.value, vid.buffer, vid.displaywidth, vid.displayheight, vid.rowbytes, current_pal) ? SSHOT_SUCCESS : SSHOT_FAILED;
		}
		else 
		{
			Com_Printf("Can't take a PNG screenshot without libpng.");
			if (SShot_FormatForName("noext") == IMAGE_PNG)
				Com_Printf(" Try changing \"%s\" to another image format.", scr_sshot_format.name);
			Com_Printf("\n");

			success = SSHOT_FAILED_QUIET;
		}
	}
#endif
	else
	{
		Com_Printf("Unsupported screenshot format.\n");
		success = SSHOT_FAILED_QUIET;
	}

	D_DisableBackBufferAccess ();	// for adapters that can't stay mapped in for linear writes all the time

	return success;
}

#endif

void SCR_ScreenShot_f (void) {
	char name[MAX_OSPATH], ext[4], *filename, *sshot_dir;
	int i, success;
	FILE *f;

	ext[0] = 0;
	sshot_dir = scr_sshot_dir.string[0] ? scr_sshot_dir.string : cls.gamedirfile;

	if (Cmd_Argc() == 2) {
		Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	} else if (Cmd_Argc() == 1) {
		// find a file name to save it to

#ifdef GLQUAKE
		if (Q_strcasecmp(scr_sshot_format.string, "tga") == 0)
			strcpy(ext, "tga");
#else
		if (Q_strcasecmp(scr_sshot_format.string, "pcx") == 0)
			strcpy(ext, "pcx");
#endif

#if USE_PNG
		if (!Q_strcasecmp(scr_sshot_format.string, "png"))
			Q_strncpyz(ext, "png", 4);
#endif
#if USE_JPEG
		if (!Q_strcasecmp(scr_sshot_format.string, "jpeg") || !Q_strcasecmp(scr_sshot_format.string, "jpg"))
			Q_strncpyz(ext, "jpg", 4);
#endif
		if (!ext[0])
			Q_strncpyz(ext, DEFAULT_SSHOT_FORMAT, 4);

		for (i = 0; i < 999; i++) {
			snprintf(name, sizeof(name), "fodquake%03i.%s", i, ext);
			if (!(f = fopen (va("%s/%s/%s", com_basedir, sshot_dir, name), "rb")))
				break;  // file doesn't exist
			fclose(f);
		}
		if (i == 1000) {
			Com_Printf ("Error: Cannot create more than 1000 screenshots\n");
			return;
		}
	} else {
		Com_Printf("Usage: %s [filename]\n", Cmd_Argv(0));
		return;
	}

	
	for (filename = name; *filename == '/' || *filename == '\\'; filename++)
		;

	success = SCR_Screenshot(va("%s/%s", sshot_dir, filename));
	if (success != SSHOT_FAILED_QUIET)
		Com_Printf ("%s %s\n", success == SSHOT_SUCCESS ? "Wrote" : "Couldn't write", name);
}

void SCR_RSShot_f (void) { 
	int success = SSHOT_FAILED;
	char *filename;
#ifdef GLQUAKE
	int width, height;
	byte *base, *pixels;
#endif

	if (CL_IsUploading())
		return;		// already one pending

	if (cls.state < ca_onserver)
		return;		// gotta be connected

	if (!scr_allowsnap.value) { 
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd); 
		SZ_Print (&cls.netchan.message, "snap\n"); 
		Com_Printf ("Refusing remote screen shot request.\n"); 
		return; 
	}

	Com_Printf ("Remote screenshot requested.\n");

	filename = "fodquake/temp/__rsshot__";

#ifdef GLQUAKE

	width = 400; height = 300;
	base = Q_Malloc ((width * height + glwidth * glheight) * 3);
	pixels = base + glwidth * glheight * 3;

	glReadPixels (glx, gly, glwidth, glheight, GL_RGB, GL_UNSIGNED_BYTE, base);
	Image_Resample (base, glwidth, glheight, pixels, width, height, 3, 0);
#if USE_JPEG
	if (QLib_isModuleLoaded(qlib_libjpeg)) {
		success = Image_WriteJPEG (filename, 70, pixels + 3 * width * (height - 1), -width, height)
			? SSHOT_SUCCESS : SSHOT_FAILED;
		goto sshot_taken;
	}
#endif
	success = Image_WriteTGA (filename, pixels, width, height)
		? SSHOT_SUCCESS : SSHOT_FAILED;
	goto sshot_taken;

sshot_taken:
	free(base);

#else		//GLQUAKE

	D_EnableBackBufferAccess ();

#if USE_PNG
	if (QLib_isModuleLoaded(qlib_libpng)) {
		success = Image_WritePNGPLTE(filename, 9, vid.buffer, vid.displaywidth, vid.displayheight, vid.rowbytes, current_pal)
			? SSHOT_SUCCESS : SSHOT_FAILED;
		goto sshot_taken;
	}
#endif

	success = Image_WritePCX (filename, vid.buffer, vid.displaywidth, vid.displayheight, vid.rowbytes, current_pal)
		? SSHOT_SUCCESS : SSHOT_FAILED;
	goto sshot_taken;

sshot_taken:

	D_DisableBackBufferAccess();

#endif		//GLQUAKE

	if (success == SSHOT_SUCCESS) {
		Com_Printf ("Sending screenshot to server...\n");
		CL_StartUpload(filename);
	}

	remove(va("%s/%s", com_basedir, filename));
}

static void SCR_CheckAutoScreenshot(void) {
	char *filename, savedname[2 * MAX_OSPATH], *sshot_dir, *fullsavedname, *ext;
	char *exts[5] = {"pcx", "tga", "png", "jpg", NULL};
	int num;

	if (!scr_autosshot_countdown || --scr_autosshot_countdown)
		return;


	if (!cl.intermission)
		return;

	for (filename = auto_matchname; *filename == '/' || *filename == '\\'; filename++)
		;

	sshot_dir = scr_sshot_dir.string[0] ? scr_sshot_dir.string : cls.gamedirfile;
	ext = SShot_ExtForFormat(SShot_FormatForName(filename));

	fullsavedname = va("%s/%s", sshot_dir, auto_matchname);
	if ((num = Util_Extend_Filename(fullsavedname, exts)) == -1) {
		Com_Printf("Error: no available filenames\n");
		return;
	}

	snprintf(savedname, sizeof(savedname), "%s_%03i%s", auto_matchname, num, ext);
	fullsavedname = va("%s/%s", sshot_dir, savedname);

#ifdef GLQUAKE
	glFinish();
#endif

	if ((SCR_Screenshot(fullsavedname)) == SSHOT_SUCCESS)
		Com_Printf("Match scoreboard saved to %s\n", savedname);
}

void SCR_AutoScreenshot(char *matchname) {
	if (cl.intermission == 1) {
		scr_autosshot_countdown = vid.numpages;
		Q_strncpyz(auto_matchname, matchname, sizeof(auto_matchname));
	}
}

/************************************ INIT ************************************/

void SCR_CvarInit (void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register (&scr_fov);
	Cvar_Register (&scr_viewsize);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_consize);
	Cvar_Register (&scr_conspeed);
	Cvar_Register (&scr_printspeed);

#ifdef GLQUAKE
#warning Huh? Here?
	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&gl_triplebuffer);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_showram);
	Cvar_Register (&scr_showturtle);
	Cvar_Register (&scr_showpause);
	Cvar_Register (&scr_centertime);

	Cvar_Register (&scr_clock_x);
	Cvar_Register (&scr_clock_y);
	Cvar_Register (&scr_clock);

	Cvar_Register (&scr_gameclock_x);
	Cvar_Register (&scr_gameclock_y);
	Cvar_Register (&scr_gameclock);

	Cvar_Register (&scr_democlock_x);
	Cvar_Register (&scr_democlock_y);
	Cvar_Register (&scr_democlock);

	Cvar_Register (&show_speed);
	Cvar_Register (&show_speed_x);
	Cvar_Register (&show_speed_y);

	Cvar_Register (&show_fps);
	Cvar_Register (&show_fps_x);
	Cvar_Register (&show_fps_y);

	Cvar_Register (&show_framestddev);
	Cvar_Register (&show_framestddev_x);
	Cvar_Register (&show_framestddev_y);

	Cvar_Register (&scr_autoid);
	Cvar_Register (&scr_coloredText);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREENSHOTS);
	Cvar_Register (&scr_allowsnap);
	Cvar_Register (&scr_sshot_format);
	Cvar_Register (&scr_sshot_dir);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("screenshot", SCR_ScreenShot_f);
	Cmd_AddCommand ("sizeup", SCR_SizeUp_f);
	Cmd_AddCommand ("sizedown", SCR_SizeDown_f);
	Cmd_AddCommand ("messagemode", Con_MessageMode_f);
	Cmd_AddCommand ("messagemode2", Con_MessageMode2_f);
}

void SCR_Init(void)
{
	rampic = Draw_LoadPicture("wad:ram", DRAW_LOADPICTURE_DUMMYFALLBACK);
	netpic = Draw_LoadPicture("wad:net", DRAW_LOADPICTURE_DUMMYFALLBACK);
	turtlepic = Draw_LoadPicture("wad:turtle", DRAW_LOADPICTURE_DUMMYFALLBACK);

	scr_initialized = true;
}

void SCR_Shutdown()
{
	if (pausepic)
	{
		Draw_FreePicture(pausepic);
		pausepic = 0;
	}

	if (loadingpic)
	{
		Draw_FreePicture(loadingpic);
		loadingpic = 0;
	}

	Draw_FreePicture(turtlepic);
	Draw_FreePicture(netpic);
	Draw_FreePicture(rampic);

	scr_initialized = false;
}

