/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// sbar.c -- status bar code

#include <string.h>

#include "quakedef.h"
#include "sbar.h"
#include "teamplay.h"
#ifdef NETQW
#include "netqw.h"
#endif
#ifndef CLIENTONLY
#include "server.h"
#endif

#include "utils.h"

#include "strl.h"

static int sb_inited;

int sb_updates;		// if >= vid.numpages, no update needed

#define STAT_MINUS		10	// num frame for '-' stats digit
struct Picture *sb_nums[2][11];
struct Picture *sb_colon, *sb_slash;
struct Picture *sb_ibar;
struct Picture *sb_sbar;
struct Picture *sb_scorebar;

struct Picture *sb_weapons[7][8]; // 0 is active, 1 is owned, 2-5 are flashes
struct Picture *sb_ammo[4];
struct Picture *sb_sigil[4];
struct Picture *sb_armor[3];
struct Picture *sb_armor_invuln;
struct Picture *sb_items[32];

struct Picture *sb_faces[5][2]; // 0 is dead, 1-4 are alive
                                // 0 is static, 1 is temporary animation
struct Picture *sb_face_invis;
struct Picture *sb_face_quad;
struct Picture *sb_face_invuln;
struct Picture *sb_face_invis_invuln;

struct Picture *rankingpic;

struct Picture *completepic;
struct Picture *interpic;
struct Picture *finalepic;

qboolean	sb_showscores;
qboolean	sb_showteamscores;

int			sb_lines;			// scan lines to draw

static int Sbar_TeamOverlay(void);

static int	sbar_xofs;

cvar_t	scr_centerSbar = {"scr_centerSbar", "0"};

cvar_t	scr_compactHud = {"scr_compactHud", "0"};
cvar_t	scr_compactHudAlign = {"scr_compactHudAlign", "0"};

cvar_t	scr_drawHFrags = {"scr_drawHFrags", "1"};
cvar_t	scr_drawVFrags = {"scr_drawVFrags", "1"};

cvar_t	scr_scoreboard_teamsort = {"scr_scoreboard_teamsort", "1"};
cvar_t	scr_scoreboard_forcecolors = {"scr_scoreboard_forcecolors", "1"};
cvar_t	scr_scoreboard_showfrags = {"scr_scoreboard_showfrags", "1"};
cvar_t	scr_scoreboard_drawtitle = {"scr_scoreboard_drawtitle", "1"};
cvar_t	scr_scoreboard_borderless = {"scr_scoreboard_borderless", "0"};
cvar_t scr_scoreboard_titleseperator = { "scr_scoreboard_titleseperator", "0" };

cvar_t	scr_scoreboard_fillalpha = {"scr_scoreboard_fillalpha", "0.7"};
cvar_t	scr_scoreboard_fillcolored = {"scr_scoreboard_fillcolored", "2"};

/********************************** CONTROL **********************************/

//Tab key down
void Sbar_ShowTeamScores (void)
{
	if (sb_showteamscores)
		return;

	sb_showteamscores = true;
	sb_updates = 0;
}

//Tab key up
void Sbar_DontShowTeamScores (void)
{
	sb_showteamscores = false;
	sb_updates = 0;
}

//Tab key down
void Sbar_ShowScores (void)
{
	if (sb_showscores)
		return;

	sb_showscores = true;
	sb_updates = 0;
}

//Tab key up
void Sbar_DontShowScores (void)
{
	sb_showscores = false;
	sb_updates = 0;
}

void Sbar_Changed (void)
{
	sb_updates = 0;	// update next frame
}

/************************************ INIT ************************************/

void Sbar_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register (&scr_centerSbar);

	Cvar_Register (&scr_compactHud);
	Cvar_Register (&scr_compactHudAlign);

	Cvar_Register (&scr_drawHFrags);
	Cvar_Register (&scr_drawVFrags);
	Cvar_Register (&scr_scoreboard_teamsort);
	Cvar_Register (&scr_scoreboard_forcecolors);
	Cvar_Register (&scr_scoreboard_showfrags);
	Cvar_Register (&scr_scoreboard_drawtitle);
	Cvar_Register (&scr_scoreboard_borderless);
	Cvar_Register (&scr_scoreboard_titleseperator);
	Cvar_Register (&scr_scoreboard_fillalpha);
	Cvar_Register (&scr_scoreboard_fillcolored);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("+showscores", Sbar_ShowScores);
	Cmd_AddCommand ("-showscores", Sbar_DontShowScores);

	Cmd_AddCommand ("+showteamscores", Sbar_ShowTeamScores);
	Cmd_AddCommand ("-showteamscores", Sbar_DontShowTeamScores);
}

void Sbar_Init (void)
{
	int i;

	for (i = 0; i < 10; i++)
	{
		sb_nums[0][i] = Draw_LoadPicture(va("wad:num_%i",i), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_nums[1][i] = Draw_LoadPicture(va("wad:anum_%i",i), DRAW_LOADPICTURE_DUMMYFALLBACK);
	}

	sb_nums[0][10] = Draw_LoadPicture("wad:num_minus", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_nums[1][10] = Draw_LoadPicture("wad:anum_minus", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_colon = Draw_LoadPicture("wad:num_colon", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_slash = Draw_LoadPicture("wad:num_slash", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_weapons[0][0] = Draw_LoadPicture("wad:inv_shotgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][1] = Draw_LoadPicture("wad:inv_sshotgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][2] = Draw_LoadPicture("wad:inv_nailgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][3] = Draw_LoadPicture("wad:inv_snailgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][4] = Draw_LoadPicture("wad:inv_rlaunch", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][5] = Draw_LoadPicture("wad:inv_srlaunch", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[0][6] = Draw_LoadPicture("wad:inv_lightng", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_weapons[1][0] = Draw_LoadPicture("wad:inv2_shotgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][1] = Draw_LoadPicture("wad:inv2_sshotgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][2] = Draw_LoadPicture("wad:inv2_nailgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][3] = Draw_LoadPicture("wad:inv2_snailgun", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][4] = Draw_LoadPicture("wad:inv2_rlaunch", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][5] = Draw_LoadPicture("wad:inv2_srlaunch", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_weapons[1][6] = Draw_LoadPicture("wad:inv2_lightng", DRAW_LOADPICTURE_DUMMYFALLBACK);

	for (i = 0; i < 5; i++)
	{
		sb_weapons[2 + i][0] = Draw_LoadPicture(va("wad:inva%i_shotgun", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][1] = Draw_LoadPicture(va("wad:inva%i_sshotgun", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][2] = Draw_LoadPicture(va("wad:inva%i_nailgun", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][3] = Draw_LoadPicture(va("wad:inva%i_snailgun", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][4] = Draw_LoadPicture(va("wad:inva%i_rlaunch", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][5] = Draw_LoadPicture(va("wad:inva%i_srlaunch", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
		sb_weapons[2 + i][6] = Draw_LoadPicture(va("wad:inva%i_lightng", i + 1), DRAW_LOADPICTURE_DUMMYFALLBACK);
	}

	sb_ammo[0] = Draw_LoadPicture("wad:sb_shells", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_ammo[1] = Draw_LoadPicture("wad:sb_nails", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_ammo[2] = Draw_LoadPicture("wad:sb_rocket", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_ammo[3] = Draw_LoadPicture("wad:sb_cells", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_armor[0] = Draw_LoadPicture("wad:sb_armor1", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_armor[1] = Draw_LoadPicture("wad:sb_armor2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_armor[2] = Draw_LoadPicture("wad:sb_armor3", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_armor_invuln = Draw_LoadPicture("wad:disc", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_items[0] = Draw_LoadPicture("wad:sb_key1", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_items[1] = Draw_LoadPicture("wad:sb_key2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_items[2] = Draw_LoadPicture("wad:sb_invis", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_items[3] = Draw_LoadPicture("wad:sb_invuln", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_items[4] = Draw_LoadPicture("wad:sb_suit", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_items[5] = Draw_LoadPicture("wad:sb_quad", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_sigil[0] = Draw_LoadPicture("wad:sb_sigil1", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_sigil[1] = Draw_LoadPicture("wad:sb_sigil2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_sigil[2] = Draw_LoadPicture("wad:sb_sigil3", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_sigil[3] = Draw_LoadPicture("wad:sb_sigil4", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_faces[4][0] = Draw_LoadPicture("wad:face1", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[4][1] = Draw_LoadPicture("wad:face_p1", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[3][0] = Draw_LoadPicture("wad:face2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[3][1] = Draw_LoadPicture("wad:face_p2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[2][0] = Draw_LoadPicture("wad:face3", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[2][1] = Draw_LoadPicture("wad:face_p3", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[1][0] = Draw_LoadPicture("wad:face4", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[1][1] = Draw_LoadPicture("wad:face_p4", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[0][0] = Draw_LoadPicture("wad:face5", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_faces[0][1] = Draw_LoadPicture("wad:face_p5", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_face_invis = Draw_LoadPicture("wad:face_invis", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_face_invuln = Draw_LoadPicture("wad:face_invul2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_face_invis_invuln = Draw_LoadPicture("wad:face_inv2", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_face_quad = Draw_LoadPicture("wad:face_quad", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_sbar = Draw_LoadPicture("wad:sbar", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_ibar = Draw_LoadPicture("wad:ibar", DRAW_LOADPICTURE_DUMMYFALLBACK);
	sb_scorebar = Draw_LoadPicture("wad:scorebar", DRAW_LOADPICTURE_DUMMYFALLBACK);

	rankingpic = Draw_LoadPicture("gfx/ranking.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sb_inited = 1;
}

void Sbar_Shutdown(void)
{
	int i, j;

	if (!sb_inited)
		return;

	sb_inited = 0;

	if (finalepic)
	{
		Draw_FreePicture(finalepic);
		finalepic = 0;
	}

	if (completepic)
	{
		Draw_FreePicture(completepic);
		completepic = 0;
	}

	if (interpic)
	{
		Draw_FreePicture(interpic);
		interpic = 0;
	}

	Draw_FreePicture(rankingpic);

	for (i=0;i<11;i++)
	{
		Draw_FreePicture(sb_nums[0][i]);
		Draw_FreePicture(sb_nums[1][i]);
	}

	Draw_FreePicture(sb_colon);
	Draw_FreePicture(sb_slash);

	for(i=0;i<7;i++)
	{
		for(j=0;j<7;j++)
		{
			Draw_FreePicture(sb_weapons[i][j]);
		}
	}

	for(i=0;i<4;i++)
	{
		Draw_FreePicture(sb_ammo[i]);
	}

	Draw_FreePicture(sb_armor_invuln);

	for(i=0;i<3;i++)
	{
		Draw_FreePicture(sb_armor[i]);
	}
	for(i=0;i<6;i++)
	{
		Draw_FreePicture(sb_items[i]);
	}

	for(i=0;i<4;i++)
	{
		Draw_FreePicture(sb_sigil[i]);
	}

	for (i=0;i<5;i++)
	{
		Draw_FreePicture(sb_faces[i][0]);
		Draw_FreePicture(sb_faces[i][1]);
	}

	Draw_FreePicture(sb_face_invis);
	Draw_FreePicture(sb_face_invuln);
	Draw_FreePicture(sb_face_invis_invuln);
	Draw_FreePicture(sb_face_quad);

	Draw_FreePicture(sb_sbar);
	Draw_FreePicture(sb_ibar);
	Draw_FreePicture(sb_scorebar);
}


/****************************** SUPPORT ROUTINES ******************************/

// drawing routines are relative to the status bar location

static void Sbar_DrawPic(int x, int y, struct Picture *pic, int width, int height)
{
	Draw_DrawPicture(pic, x + sbar_xofs, y + (vid.conheight - SBAR_HEIGHT), width, height);
}

//JACK: Draws a portion of the picture in the status bar.
static void Sbar_DrawSubPicNormalised(int x, int y, int width, int height, struct Picture *pic, float srcx, float srcy, float srcwidth, float srcheight)
{
	Draw_DrawSubPicture(pic, srcx, srcy, srcwidth, srcheight, x, y + (vid.conheight - SBAR_HEIGHT), width, height);
}

static void Sbar_DrawTransPic(int x, int y, struct Picture *pic, int width, int height)
{
	Draw_DrawPicture(pic, x + sbar_xofs, y + (vid.conheight - SBAR_HEIGHT), width, height);
}

//Draws one solid graphics character
static void Sbar_DrawCharacter(int x, int y, int num)
{
	Draw_Character(x + 4 + sbar_xofs, y + vid.conheight - SBAR_HEIGHT, num);
}

void Sbar_DrawString(int x, int y, char *str)
{
	Draw_String(x + sbar_xofs, y + vid.conheight - SBAR_HEIGHT, str);
}

static void Sbar_DrawAltString(int x, int y, char *str)
{
	Draw_Alt_String(x + sbar_xofs, y + vid.conheight - SBAR_HEIGHT, str);
}

static int Sbar_itoa(int num, char *buf)
{
	char *str;
	int pow10, dig;

	str = buf;

	if (num < 0)
	{
		*str++ = '-';
		num = -num;
	}

	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
		;

	do
	{
		pow10 /= 10;
		dig = num / pow10;
		*str++ = '0' + dig;
		num -= dig * pow10;
	} while (pow10 != 1);

	*str = 0;

	return str - buf;
}

static void Sbar_DrawNum(int x, int y, int num, int digits, int color)
{
	char str[12], *ptr;
	int l, frame;

	l = Sbar_itoa(num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Sbar_DrawTransPic(x, y, sb_nums[color][frame], 24, 24);
		x += 24;
		ptr++;
	}
}

static int Sbar_ColorForMap (int m)
{
	m = bound(0, m, 13);

	return 16 * m + 8;
}

/********************************* FRAG SORT *********************************/

int		fragsort[MAX_CLIENTS];
int		scoreboardlines;

#define SCR_TEAM_T_MAXTEAMSIZE	(16 + 1)

typedef struct
{
	char team[SCR_TEAM_T_MAXTEAMSIZE];
	int frags;
	int players;
	int plow, phigh, ptotal;
	int topcolor, bottomcolor;
	qboolean myteam;
} team_t;
team_t teams[MAX_CLIENTS];

int teamsort[MAX_CLIENTS];
int scoreboardteams;

static __inline int Sbar_PlayerNum(void)
{
	int mynum;

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1)
		mynum = cl.playernum;

	return mynum;
}


static __inline qboolean Sbar_IsSpectator(int mynum)
{
	return (mynum == cl.playernum) ? cl.spectator : cl.players[mynum].spectator;
}

static void Sbar_SortFrags(qboolean spec)
{
	int i, j, k;
	static int lastframecount = 0;

	if (!spec && lastframecount && lastframecount == cls.framecount)
		return;


	lastframecount = spec ? 0 : cls.framecount;

	// sort by frags
	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && (spec || !cl.players[i].spectator))
		{
			fragsort[scoreboardlines] = i;
			scoreboardlines++;
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;
		}
	}

	for (i = 0; i < scoreboardlines; i++)
	{
		for (j = 0; j < scoreboardlines - 1 - i; j++)
		{
			if (cl.players[fragsort[j]].frags < cl.players[fragsort[j + 1]].frags)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}

static void Sbar_SortTeams (void)
{
	int i, j, k, mynum, playertoteam[MAX_CLIENTS];
	player_info_t *s;
	char t[SCR_TEAM_T_MAXTEAMSIZE];
	static int lastframecount = 0;

	if (!cl.teamplay)
		return;

	if (lastframecount && lastframecount == cls.framecount)
		return;
	lastframecount = cls.framecount;

	scoreboardteams = 0;

	for (i = 0; i < MAX_CLIENTS; i++)
		playertoteam[i] = -1;

	// sort the teams
	memset(teams, 0, sizeof(teams));
	for (i = 0; i < MAX_CLIENTS; i++)
		teams[i].plow = 999;

	mynum = Sbar_PlayerNum();

	for (i = 0; i < MAX_CLIENTS; i++)
	{
		s = &cl.players[i];
		if (!s->name[0] || s->spectator)
			continue;

		// find his team in the list
		Q_strncpyz (t, s->team, sizeof(t));
		if (!t[0])
			continue; // not on team

		for (j = 0; j < scoreboardteams; j++)
		{
			if (!strcmp(teams[j].team, t))
			{
				playertoteam[i] = j;

				teams[j].frags += s->frags;
				teams[j].players++;
				if (!cl.teamfortress && i == mynum)
				{
					teams[j].topcolor = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
					teams[j].bottomcolor = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
				}
				break;
			}
		}
		if (j == scoreboardteams) // must add him
		{
			j = scoreboardteams++;
			playertoteam[i] = j;

			teams[j].frags = s->frags;
			teams[j].players = 1;
			if (cl.teamfortress)
			{
				teams[j].topcolor = teams[j].bottomcolor = Utils_TF_TeamToColor(t);
			}
			else
			{
				teams[j].bottomcolor = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
				teams[j].topcolor = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			}
			Q_strncpyz(teams[j].team, t, sizeof(teams[j].team));

			if (!Sbar_IsSpectator(mynum) && !strncmp(cl.players[mynum].team, t, sizeof(t) - 1))
				teams[j].myteam = true;
			else
				teams[j].myteam = false;
		}
		teams[j].plow = min(s->ping, teams[j].plow);
		teams[j].phigh = max(s->ping, teams[j].phigh);
		teams[j].ptotal += s->ping;
	}

	//prepare for sort
	for (i = 0; i < scoreboardteams; i++)
		teamsort[i] = i;

	// good 'ol bubble sort
	for (i = 0; i < scoreboardteams - 1; i++)
	{
		for (j = i + 1; j < scoreboardteams; j++)
		{
			if (teams[teamsort[i]].frags < teams[teamsort[j]].frags)
			{
				k = teamsort[i];
				teamsort[i] = teamsort[j];
				teamsort[j] = k;
			}
		}
	}


	if (cl.teamfortress)
	{
		for (i = 0; i < scoreboardteams; i++)
		{
			float frags = (float) teams[i].frags / teams[i].players;

			if (frags != (int) frags)
				return;

			for (j = 0; j < MAX_CLIENTS; j++)
			{
				s = &cl.players[j];
				if (!s->name[0] || s->spectator || playertoteam[j] != i)
					continue;

				if (s->frags != frags)
					return;
			}
		}


		for (i = 0; i < scoreboardteams; i++)
			teams[i].frags /= teams[i].players;
	}
}



static int Sbar_SortTeamsAndFrags_Compare(int a, int b)
{
	int team_comp, frag_comp;
	char *team_one, *team_two;
	player_info_t *p1, *p2;

	p1 = &cl.players[a];
	p2 = &cl.players[b];

	if (p1->spectator)
		return p2->spectator ? Q_strcasecmp(p1->name, p2->name) : 1;
	else if (p2->spectator)
		return -1;

	team_one = cl.teamfortress ? Utils_TF_ColorToTeam(p1->real_bottomcolor) : p1->team;
	team_two = cl.teamfortress ? Utils_TF_ColorToTeam(p2->real_bottomcolor) : p2->team;

	if ((team_comp = strcmp(team_one, team_two)))
	{
		int i;
		team_t *t1 = NULL, *t2 = NULL;

		for (i = 0; i < scoreboardteams; i++)
		{
			if (!strcmp(team_one, teams[i].team))
			{
				t1 = &teams[i];
				break;
			}
		}
		for (i = 0; i < scoreboardteams; i++)
		{
			if (!strcmp(team_two, teams[i].team))
			{
				t2 = &teams[i];
				break;
			}
		}
		return (t1 && t2 && t1->frags != t2->frags) ? (t2->frags - t1->frags) : team_comp;
	}
	else
	{
		return (frag_comp = p2->frags - p1->frags) ? frag_comp : Q_strcasecmp(p1->name, p2->name);
	}
}

static void Sbar_SortTeamsAndFrags(qboolean specs)
{
    int i, j, k;
	qboolean real_teamplay;

	real_teamplay = cl.teamplay && (TP_CountPlayers() > 2);

	if (!real_teamplay || !scr_scoreboard_teamsort.value)
	{
		Sbar_SortFrags(specs);
		return;
	}

	scoreboardlines = 0;
	for (i = 0; i < MAX_CLIENTS; i++)
	{
		if (cl.players[i].name[0] && (specs || !cl.players[i].spectator))
		{
			fragsort[scoreboardlines++] = i;
			if (cl.players[i].spectator)
				cl.players[i].frags = -999;
		}
	}

	Sbar_SortTeams();

	for (i = 0; i < scoreboardlines; i++)
	{
		for (j = 0; j < scoreboardlines - 1 - i ; j++)
		{
			if (Sbar_SortTeamsAndFrags_Compare(fragsort[j], fragsort[j + 1]) > 0)
			{
				k = fragsort[j];
				fragsort[j] = fragsort[j + 1];
				fragsort[j + 1] = k;
			}
		}
	}
}



/********************************* INVENTORY *********************************/

static void Sbar_DrawInventory (void)
{
	int i, flashon;
	char num[6];
	float time;
	qboolean headsup, hudswap;
	extern int cl_preselectedweapon;

	headsup = !(cl_sbar.value || scr_viewsize.value < 100);
	hudswap = cl_hudswap.value; // Get that nasty float out :)

	if (!headsup)
		Sbar_DrawPic(0, -24, sb_ibar, 320, 24);

	// weapons
	for (i = 0; i < 7; i++)
	{
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i) )
		{
			time = cl.item_gettime[i];
			flashon = (int)((cl.time - time) * 10);
			if (flashon < 0)
				flashon = 0;
			if (flashon >= 10)
				flashon = (cl_preselectedweapon == i + 2 || (!cl_preselectedweapon && cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i) ) ) ? 1 : 0;
			else
				flashon = (flashon % 5) + 2;

			if (headsup)
			{
				if (i || vid.conheight > 200)
				{
					if (i == 6)
						Sbar_DrawSubPicNormalised((hudswap ? 0 : vid.conwidth - 24), -68 - (7 - i) * 16, 24, 16, sb_weapons[flashon][i], 0, 0, 0.5, 1);
					else
						Sbar_DrawPic((hudswap ? 0 : vid.conwidth - 24) - sbar_xofs, -68 - (7 - i) * 16, sb_weapons[flashon][i], 24, 16);
				}
			}
			else
			{
				Sbar_DrawPic(i * 24 - sbar_xofs, -16, sb_weapons[flashon][i], i==6?48:24, 16);
			}

			if (flashon > 1)
				sb_updates = 0;		// force update to remove flash
		}
	}

	// ammo counts
	for (i = 0; i < 4; i++)
	{
		snprintf(num, sizeof(num), "%3d", cl.stats[STAT_SHELLS + i]);
		if (num[0] != ' ')
			num[0] = 18 + num[0] - '0';
		if (num[1] != ' ')
			num[1] = 18 + num[1] - '0';
		if (num[2] != ' ')
			num[2] = 18 + num[2] - '0';

		if (headsup)
		{
			Sbar_DrawSubPicNormalised(hudswap ? 0 : vid.conwidth - 42, -24 - (4 - i) * 11, 42, 11, sb_ibar, ((double)(3 + (i * 48)))/320, 0, 42.0/320, 11.0/24);
			Draw_String(hudswap ? 7: vid.conwidth - 35, vid.conheight - SBAR_HEIGHT - 24 - (4 - i) * 11, num);
		}
		else
		{
			Sbar_DrawString((6 * i + 1) * 8 - 2 + 4, -24, num);
		}
	}

	flashon = 0;
	// items
	for (i = 0; i < 6; i++)
	{
		if (cl.stats[STAT_ITEMS] & (1 << (17 + i)))
		{
			time = cl.item_gettime[17 + i];
			if (time &&	time > cl.time - 2 && flashon)
				// flash frame
				sb_updates = 0;
			else
				Sbar_DrawPic(192 + i * 16, -16, sb_items[i], 16, 16);

			if (time &&	time > cl.time - 2)
				sb_updates = 0;
		}
	}

	// sigils
	for (i = 0; i < 4; i++)
	{
		if (cl.stats[STAT_ITEMS] & (1 << (28 + i)))
		{
			time = cl.item_gettime[28 + i];
			if (time &&	time > cl.time - 2 && flashon )
				// flash frame
				sb_updates = 0;
			else
				Sbar_DrawPic(320 - 32 + i * 8, -16, sb_sigil[i], 8, 16);

			if (time &&	time > cl.time - 2)
				sb_updates = 0;
		}
	}
}

/************************************ HUD ************************************/

static void Sbar_DrawFrags_DrawCell(int x, int y, int topcolor, int bottomcolor, int frags, int brackets)
{
	char num[6];

	// draw background
	Draw_Fill (sbar_xofs + x * 8 + 10, y, 28, 4, Sbar_ColorForMap (topcolor));
	Draw_Fill (sbar_xofs + x * 8 + 10, y + 4, 28, 3, Sbar_ColorForMap (bottomcolor));

	// draw number
	if (brackets)
	{
		snprintf(num, sizeof(num), "%c%3i%c", 1?16:' ', frags, 17);

		Sbar_DrawString(x * 8 + 4, -24, num);
		Sbar_DrawCharacter (x * 8 + 0, -24, 16);
	}
	else
	{
		snprintf(num, sizeof(num), "%3i", frags);

		Sbar_DrawString((x + 1) * 8 + 4, -24, num);
	}
}

static void Sbar_DrawFrags (void)
{
	int i, k, l, top, bottom, x, y, mynum, myteam = -1;
	player_info_t *s;
	team_t *tm;
	qboolean drawn_self = false, drawn_self_team = false;

	if (!scr_drawHFrags.value)
		return;

	x = 23;
	y = vid.conheight - SBAR_HEIGHT - 23;

	mynum = Sbar_PlayerNum();

	if (!cl.teamplay || scr_drawHFrags.value == 1)
	{

		Sbar_SortFrags (false);
		l = min(scoreboardlines, 4);

		for (i = 0; i < l; i++)
		{
			if (i + 1 == l && !drawn_self && !Sbar_IsSpectator(mynum))
				k = mynum;
			else
				k = fragsort[i];

			s = &cl.players[k];

			if (!s->name[0] || s->spectator)
				continue;

			top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
			top = bound(0, top, 13);
			bottom = bound(0, bottom, 13);

			Sbar_DrawFrags_DrawCell(x, y, top, bottom, s->frags, k == mynum);

			if (k == mynum)
				drawn_self = true;

			x += 4;
		}
	}
	else
	{
		Sbar_SortTeams();

		for (i = 0; i < scoreboardteams; i++)
		{
			if (teams[i].myteam)
			{
				myteam = i;
				break;
			}
		}

		l = min(scoreboardteams, 4);
		for (i = 0; i < l; i++)
		{
			if (myteam != -1 && i + 1 == l && !drawn_self_team)
				k = myteam;
			else
				k = teamsort[i];

			tm = &teams[k];
			Sbar_DrawFrags_DrawCell(x, y, tm->topcolor, tm->bottomcolor, tm->frags, tm->myteam);

			if (k == myteam)
				drawn_self_team = true;

			x += 4;
		}

		if (scr_drawHFrags.value != 3 && l <= 2 && !Sbar_IsSpectator(mynum))
		{

			x += 4 * (3 - l);

			s = &cl.players[mynum];

			top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
			bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;
			top = bound(0, top, 13);
			bottom = bound(0, bottom, 13);

			Sbar_DrawFrags_DrawCell(x, y, top, bottom, s->frags, 1);
		}
	}
}

static void Sbar_DrawFace (void)
{
	int f, anim;

	if ( (cl.stats[STAT_ITEMS] & (IT_INVISIBILITY | IT_INVULNERABILITY) )
	 == (IT_INVISIBILITY | IT_INVULNERABILITY) )
{
		Sbar_DrawPic(112, 0, sb_face_invis_invuln, 24, 24);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_QUAD)
	{
		Sbar_DrawPic(112, 0, sb_face_quad, 24, 24);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		Sbar_DrawPic(112, 0, sb_face_invis, 24, 24);
		return;
	}
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		Sbar_DrawPic(112, 0, sb_face_invuln, 24, 24);
		return;
	}

	f = cl.stats[STAT_HEALTH] / 20;
	f = bound (0, f, 4);

	if (cl.time <= cl.faceanimtime)
	{
		anim = 1;
		sb_updates = 0;		// make sure the anim gets drawn over
	}
	else
	{
		anim = 0;
	}
	Sbar_DrawPic(112, 0, sb_faces[f][anim], 24, 24);
}

static void Sbar_DrawNormal (void)
{
	if (cl_sbar.value || scr_viewsize.value < 100)
		Sbar_DrawPic(0, 0, sb_sbar, 320, 24);

	// armor
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		Sbar_DrawNum(24, 0, 666, 3, 1);
		Sbar_DrawPic(0, 0, sb_armor_invuln, 24, 24);
	}
	else
	{
		Sbar_DrawNum (24, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
		if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
			Sbar_DrawPic(0, 0, sb_armor[2], 24, 24);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
			Sbar_DrawPic(0, 0, sb_armor[1], 24, 24);
		else if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
			Sbar_DrawPic(0, 0, sb_armor[0], 24, 24);
	}

	// face
	Sbar_DrawFace ();

	// health
	Sbar_DrawNum (136, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	// ammo icon
	if (cl.stats[STAT_ITEMS] & IT_SHELLS)
		Sbar_DrawPic(224, 0, sb_ammo[0], 24, 24);
	else if (cl.stats[STAT_ITEMS] & IT_NAILS)
		Sbar_DrawPic(224, 0, sb_ammo[1], 24, 24);
	else if (cl.stats[STAT_ITEMS] & IT_ROCKETS)
		Sbar_DrawPic(224, 0, sb_ammo[2], 24, 24);
	else if (cl.stats[STAT_ITEMS] & IT_CELLS)
		Sbar_DrawPic(224, 0, sb_ammo[3], 24, 24);

	Sbar_DrawNum (248, 0, cl.stats[STAT_AMMO], 3, cl.stats[STAT_AMMO] <= 10);
}


static void Sbar_DrawCompact(void)
{
	int i, align, old_sbar_xofs;
	static char *weapons[7] = {"sg", "bs", "ng", "sn", "gl", "rl", "lg"};
	char str[4];
	extern int cl_preselectedweapon;

	if (cl_sbar.value || scr_viewsize.value < 100)
		Sbar_DrawPic(0, 0, sb_sbar, 320, 24);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.conwidth - 306) >> 1: 0;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);

	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);

	Draw_BeginTextRendering();

	align = scr_compactHudAlign.value ? 1 : 0;
	for (i = 0; i < 4; i++)
	{
		snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
		if (cl.stats[STAT_SHELLS + i] < 5)
			Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 174 + 32 * i, 3, str);
		else
			Sbar_DrawString(align * 8 * (3 - strlen(str)) + 174 + 32 * i, 3, str);
	}
	for (i = 0; i < 7; i++)
	{
		if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << i))
		{
			if (cl_preselectedweapon == i + 2)
				Sbar_DrawAltString(166 + 5 * 4 * i, 14, weapons[i]);
			else if (!cl_preselectedweapon && cl.stats[STAT_ACTIVEWEAPON] == (IT_SHOTGUN << i))
				Sbar_DrawAltString(166 + 5 * 4 * i, 14, weapons[i]);
			else
				Sbar_DrawString(166 + 5 * 4 * i, 14, weapons[i]);
		}
	}
	sbar_xofs = old_sbar_xofs;

	Draw_EndTextRendering();
}

static void Sbar_DrawCompact_TF(void)
{
	int i, align, old_sbar_xofs;
	char str[4];

	if (cl_sbar.value || scr_viewsize.value < 100)
		Sbar_DrawPic(0, 0, sb_sbar, 320, 24);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.conwidth - 222) >> 1: 0;

	align = scr_compactHudAlign.value ? 1 : 0;
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
	for (i = 0; i < 4; i++)
	{
			snprintf(str, sizeof(str), "%d", cl.stats[STAT_SHELLS + i]);
			if (cl.stats[STAT_SHELLS + i] < 5)
				Sbar_DrawAltString(align * 8 * (3 - strlen(str)) + 166 + 32 * (i % 2), i >= 2 ? 14 : 3, str);
			else
				Sbar_DrawString(align * 8 * (3 - strlen(str)) + 166 + 32 * (i % 2), i >= 2 ? 14 : 3, str);
	}
	sbar_xofs = old_sbar_xofs;
}

static void Sbar_DrawCompact_Bare (void)
{
	int old_sbar_xofs;

	if (cl_sbar.value || scr_viewsize.value < 100)
		Sbar_DrawPic(0, 0, sb_sbar, 320, 24);

	old_sbar_xofs = sbar_xofs;
	sbar_xofs = scr_centerSbar.value ? (vid.conwidth - 158) >> 1: 0;

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		Sbar_DrawNum (2, 0, 666, 3, 1);
	else
		Sbar_DrawNum (2, 0, cl.stats[STAT_ARMOR], 3, cl.stats[STAT_ARMOR] <= 25);
	Sbar_DrawNum (86, 0, cl.stats[STAT_HEALTH], 3, cl.stats[STAT_HEALTH] <= 25);
	sbar_xofs = old_sbar_xofs;
}



/******************************** SCOREBOARD ********************************/

static void Sbar_SoloScoreboard (void)
{
	char str[80];
	double time;
	int minutes, seconds, tens, units;

	sb_updates = 0;         // because time display changes every second

	Sbar_DrawPic(0, 0, sb_scorebar, 320, 24);

	if (cl.gametype == GAME_COOP)
	{
		snprintf(str, sizeof(str), "Monsters:%3i /%3i", cl.stats[STAT_MONSTERS], cl.stats[STAT_TOTALMONSTERS]);
		Sbar_DrawString (8, 4, str);

		snprintf(str, sizeof(str), "Secrets :%3i /%3i", cl.stats[STAT_SECRETS], cl.stats[STAT_TOTALSECRETS]);
		Sbar_DrawString (8, 12, str);
	}

	// time
	if (cl.servertime_works)
	{
		time = cl.servertime;	// good, we know real time spent on level
	}
	else
	{
		time = cls.demoplayback ? cls.demotime : cls.realtime;
		if (cl.gametype == GAME_COOP)
			time -= cl.players[cl.playernum].entertime;
	}

	minutes = time / 60;
	seconds = time - 60 * minutes;
	tens = seconds / 10;
	units = seconds - 10 * tens;
	snprintf(str, sizeof(str), "Time :%3i:%i%i", minutes, tens, units);
	Sbar_DrawString (184, 4, str);

	if (cl.gametype == GAME_COOP)
	{
		// draw level name
		int l = strlen (cl.levelname);
		if (l < 22 && !strstr(cl.levelname, "\n"))
			Sbar_DrawString (232 - l*4, 12, cl.levelname);
	}
}

#define SCOREBOARD_LASTROW		(vid.conheight - 34)

#define	RANK_WIDTH_BASICSTATS	(11 * 8)
#define	RANK_WIDTH_TEAMSTATS	(4 * 8)
#define	RANK_WIDTH_TCHSTATS		(5 * 8)
#define	RANK_WIDTH_CAPSTATS		(5 * 8)

#define RANK_WIDTH_DM				(-8 + 168 + (MAX_SCOREBOARDNAME * 8))
#define RANK_WIDTH_TEAM				(-8 + 208 + (MAX_SCOREBOARDNAME * 8))

#define SCOREBOARD_ALPHA			(0.5 * bound(0, scr_scoreboard_fillalpha.value, 1))
#define SCOREBOARD_HEADINGALPHA		(bound(0, scr_scoreboard_fillalpha.value, 1))

static void Sbar_DeathmatchOverlay()
{
	int stats_basic, stats_team, stats_touches, stats_caps, playerstats[7];
	int scoreboardsize, colors_thickness, statswidth, stats_xoffset;
	int i, k, top, bottom, x, y, xofs, total, minutes, p, skip = 10;
	int rank_width, leftover, startx, tempx, mynum;
	char num[12], scorerow[64], team[5], name[MAX_SCOREBOARDNAME];
	player_info_t *s;
	int start;
	int topy;
	int midy;

#ifndef CLIENTONLY
	// FIXME
	// check number of connections instead?
	if (cl.gametype == GAME_COOP && com_serveractive && !coop.value)
		return;
#endif

	if (cl.teamplay && (sb_showteamscores || cl.intermission))
		start = Sbar_TeamOverlay();
	else
		start = 0;

	if (cls.state == ca_active && !cls.demoplayback)
	{
		// request new ping times every two seconds
		if (cls.realtime - cl.last_ping_request >= 2)
		{
			char buf[7];

			cl.last_ping_request = cls.realtime;

#ifdef NETQW
			buf[0] = clc_stringcmd;
			strcpy(buf + 1, "pings");
			NetQW_AppendReliableBuffer(cls.netqw, buf, 7);
#else
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
#endif
		}
	}

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	mynum = Sbar_PlayerNum();

	rank_width = (cl.teamplay ? RANK_WIDTH_TEAM : RANK_WIDTH_DM);

	statswidth = 0;
	stats_basic = stats_team = stats_touches = stats_caps = 0;
	if (scr_scoreboard_showfrags.value && Stats_IsActive())
	{
		if (rank_width + statswidth + RANK_WIDTH_BASICSTATS < vid.conwidth - 16)
		{
			statswidth += RANK_WIDTH_BASICSTATS;
			stats_basic++;
			if (cl.teamplay)
			{
				if (rank_width + statswidth + RANK_WIDTH_TEAMSTATS < vid.conwidth - 16)
				{
					if ((cl.teamfortress || cl.teamplay != 3)
					 && (!cl.teamfortress || (cl.teamplay & 21) != 21))
					{
						statswidth += RANK_WIDTH_TEAMSTATS;
						stats_team++;
					}
				}
				if (Stats_IsFlagsParsed())
				{
					if (rank_width + statswidth + RANK_WIDTH_TCHSTATS < vid.conwidth - 16)
					{
						statswidth += RANK_WIDTH_TCHSTATS;
						stats_touches++;
					}
					if (rank_width + statswidth + RANK_WIDTH_CAPSTATS < vid.conwidth - 16)
					{
						statswidth += RANK_WIDTH_CAPSTATS;
						stats_caps++;
					}
				}
			}
		}
	}

	rank_width += statswidth;

	leftover = rank_width;
	rank_width = bound(0, rank_width, vid.conwidth - 16);
	leftover = max(0, leftover - rank_width);
	xofs = ((vid.conwidth - rank_width) >> 1);

	if (!start)
	{
		if (scr_scoreboard_drawtitle.value)
		{
			Draw_DrawPicture(rankingpic, xofs + (rank_width - 168) / 2, 0, 168, 24);
			start = 36;
		}
		else
		{
			start = 12;
		}
	}

	y = start;

	topy = y - 9;

	Draw_AlphaFill (xofs, y - 8, rank_width, 9+(scr_scoreboard_titleseperator.value?8:0), 1, SCOREBOARD_HEADINGALPHA);	//Draw heading row

	Draw_String(xofs + 1, y - 8, cl.teamplay ? " ping pl time frags team name" : " ping pl time frags name");

	if (statswidth)
	{
		char temp[32] = {0};

		if (stats_team)
			strcat(temp, "kills tks dths");
		else
			strcat(temp, "kills dths");
		if (stats_touches)
			strcat(temp, " tchs");
		if (stats_caps)
			strcat(temp, " caps");

		stats_xoffset = (cl.teamplay ? 41 * 8 : 36 * 8);
		Draw_String(xofs + 1 + stats_xoffset, y - 8, temp);
	}

	if (scr_scoreboard_titleseperator.value)
	{
		if (cl.teamplay)
			Draw_String ( xofs + 1 , y, " \x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");
		else
			Draw_String ( xofs + 1 , y, " \x1d\x1e\x1e\x1f \x1d\x1f \x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");

		y+= 8;
	}

	midy = y + 1;

	y += 2;												//dont go over the black border, move the rest down

	startx = x = xofs + 8;

	tempx = xofs + rank_width - MAX_SCOREBOARDNAME * 8 - (48 + statswidth) + leftover;

	Sbar_SortTeamsAndFrags (true);	// scores

	for (i = scoreboardsize = 0; i < scoreboardlines; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];

		if (s->name[0])
			scoreboardsize++;
	}

	if (y + (scoreboardsize - 1) * skip > SCOREBOARD_LASTROW)
	{
		colors_thickness = 3;
		skip = 8;
	}
	else
	{
		colors_thickness = 4;
	}

	for (i = 0; i < scoreboardlines && y <= SCOREBOARD_LASTROW; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0])
			continue;

		//render the main background transparencies behind players row
		top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
		bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;

		if (!cl.teamplay
		 || s->spectator
		 || !scr_scoreboard_fillcolored.value
		 || (scr_scoreboard_fillcolored.value == 2 && !scr_scoreboard_teamsort.value))
		{
			Draw_AlphaFill (xofs, y, rank_width, skip, 2, SCOREBOARD_ALPHA);
		}
		else
		{
			Q_strncpyz (team, s->team, sizeof(team));
			if (k == mynum)
			{
				float alpha;
				alpha = 1.7 * SCOREBOARD_ALPHA;
				alpha = min(alpha, 0.75);
				Draw_AlphaFill (xofs, y, rank_width, skip, Sbar_ColorForMap(bottom), alpha);
			}
			else
			{
				Draw_AlphaFill (xofs, y, rank_width, skip, Sbar_ColorForMap(bottom), SCOREBOARD_ALPHA);
			}
		}

		// draw ping
		p = s->ping;
		if (p < 0 || p > 999)
			p = 999;

		snprintf(num, sizeof(num), "&cAAD%4i", p);
		Draw_ColoredString(x, y, num, 0);
		x += 32; // move it forward, ready to print next column

		// draw pl
		p = s->pl;

		if (p > 25)
		{
			snprintf(num, sizeof(num), "&cD33%3i", p);
			Draw_ColoredString (x, y, num, 1);
		}
		else
		{
			snprintf(num, sizeof(num), "&cDB0%3i", p);
			Draw_ColoredString (x, y, num, 0);
		}

		x += 32;

		if (s->spectator)
		{
			Draw_ColoredString (x, y, "&cF20s&cF50p&cF80e&c883c&cA85t&c668a&c55At&c33Bo&c22Dr", 0);

			x += cl.teamplay ? 128 : 88; // move across to print the name

			Q_strncpyz(name, s->name, sizeof(name));
			if (leftover > 0)
			{
				int truncate = (leftover / 8) + (leftover % 8 ? 1 : 0);

				truncate = bound(0, truncate, sizeof(name) - 1);
				name[sizeof(name) - 1 - truncate] = 0;
			}

			Draw_String (x, y, name);	// draw name

			y += skip;
			x = startx;
		}
		else
		{
			// draw time
			total = (cl.intermission ? cl.completed_time : cls.demoplayback ? cls.demotime : cls.realtime) - s->entertime;
			minutes = (int) total / 60;

			// print the shirt/pants colour bars
			Draw_Fill (cl.teamplay ? tempx - 40 : tempx, y + 4 - colors_thickness, 40, colors_thickness, Sbar_ColorForMap (top));
			Draw_Fill (cl.teamplay ? tempx - 40 : tempx, y + 4, 40, 4, Sbar_ColorForMap (bottom));

			// name
			Q_strncpyz(name, s->name, sizeof(name));
			if (leftover > 0)
			{
				int truncate = (leftover / 8) + (leftover % 8 ? 1 : 0);

				truncate = bound(0, truncate, sizeof(name) - 1);
				name[sizeof(name) - 1 - truncate] = 0;
			}

			// team
			if (cl.teamplay)
			{
				Q_strncpyz  (team, s->team, sizeof(team));
				snprintf(scorerow, sizeof(scorerow), " %3i %c%3i%c %-4s %s", minutes, k==mynum?16:' ', s->frags, k==mynum?17:' ', team, name);
			}
			else
			{
				snprintf(scorerow, sizeof(scorerow), " %3i %c%3i%c %s", minutes, k==mynum?16:' ', s->frags, k==mynum?17:' ', name);
			}

			Draw_String (x, y, scorerow);

			if (statswidth)
			{
				Stats_GetBasicStats(s - cl.players, playerstats);
				if (stats_touches || stats_caps)
					Stats_GetFlagStats(s - cl.players, playerstats + 4);

				scorerow[0] = 0;

				if (stats_team)
					snprintf(scorerow, sizeof(scorerow), " &c0B4%3i  &cF60%3i &cF00%3i ", playerstats[0], playerstats[2], playerstats[1]);
				else
					snprintf(scorerow, sizeof(scorerow), " &c0B4%3i  &cF00%3i ", playerstats[0], playerstats[1]);

				if (stats_touches)
					strcat (scorerow, va("  &cFD0%2i ", playerstats[4]));
				if (stats_caps)
					strcat (scorerow, va("  &c24F%2i ", playerstats[6]));

				Draw_ColoredString(x + stats_xoffset - 9 * 8, y, scorerow, 0);
			}

			y += skip;
			x = startx;
		}
	}

	if (!scr_scoreboard_borderless.value)
	{
		Draw_Fill (xofs, topy, rank_width, 1, 0);  //Border - Top
		Draw_Fill (xofs, midy, rank_width, 1, 0);  //Border - Top (under header)
		Draw_Fill (xofs, y - 1, rank_width, 1, 0); //Border - Bottom

		Draw_Fill (xofs - 1, topy, 1, y - topy, 0);                   //Border - Left
		Draw_Fill (xofs - 1 + rank_width + 1, topy, 1, y - topy, 0);  //Border - Right
	}
}

static int Sbar_TeamOverlay(void)
{
	int i, k, x, y, xofs, plow, phigh, pavg, rank_width, skip = 10;
	char num[12], team[5];
	team_t *tm;
	int topy;

#ifndef CLIENTONLY
	// FIXME
	// check number of connections instead?
	if (cl.gametype == GAME_COOP && com_serveractive && !coop.value)
		return 0;
#endif

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	rank_width = cl.teamplay ? RANK_WIDTH_TEAM : RANK_WIDTH_DM;
	rank_width = bound(0, rank_width, vid.conwidth - 16);

	xofs = (vid.conwidth - rank_width) >> 1;

	if (scr_scoreboard_drawtitle.value)
	{
		Draw_DrawPicture(rankingpic, xofs + (rank_width - 168) / 2, 0, 168, 24);
		y = 26;
	}
	else
	{
		y = 2;
	}

	topy = y + 1;

	Draw_AlphaFill (xofs, y + 2, rank_width, skip, 1, SCOREBOARD_HEADINGALPHA);	//draw heading row
	y += 2;																		//dont go over the black border, move the rest down

	x = xofs + rank_width * 0.1 + 8;

	Draw_String(x, y, "low/avg/high team total players");

	y += 10;

	Sbar_SortTeams();		// sort the teams

	for (i = 0; i < scoreboardteams && y <= SCOREBOARD_LASTROW; i++)
	{
		k = teamsort[i];
		tm = teams + k;

		Draw_AlphaFill (xofs, y, rank_width, skip, 2, SCOREBOARD_ALPHA);

		// draw pings
		plow = tm->plow;
		if (plow < 0 || plow > 999)
			plow = 999;

		phigh = tm->phigh;
		if (phigh < 0 || phigh > 999)
			phigh = 999;

		pavg = tm->players ? tm->ptotal / tm->players : 999;
		if (pavg < 0 || pavg > 999)
			pavg = 999;

		snprintf(num, sizeof(num), "%3i/%3i/%3i", plow, pavg, phigh);
		Draw_String (x, y, num);

		// draw team
		Q_strncpyz (team, tm->team, sizeof(team));
		Draw_String (x + 104, y, team);

		// draw total
		snprintf(num, sizeof(num), "%5i", tm->frags);
		Draw_String (x + 104 + 40, y, num);

		// draw players
		snprintf(num, sizeof(num), "%5i", tm->players);
		Draw_String (x + 104 + 88, y, num);

		if (tm->myteam)
		{
			Draw_Character (x + 104 - 8, y, 16);
			Draw_Character (x + 104 + 32, y, 17);
		}

		y += skip;
	}

	if (!scr_scoreboard_borderless.value)
	{
		Draw_Fill(xofs, topy, rank_width, 1, 0);                       //Border - Top
		Draw_Fill(xofs, topy + 10, rank_width, 1, 0);                    //Border - Top (under header)
		Draw_Fill(xofs, y - 1, rank_width, 1, 0);                      //Border - Bottom

		Draw_Fill(xofs - 1, topy, 1, y - topy, 0);                     //Border - Left
		Draw_Fill(xofs - 1 + rank_width + 1, topy, 1, y - topy, 0);    //Border - Right
	}

	y += 14;

	return y;
}


static void Sbar_MiniDeathmatchOverlay (void)
{
	int i, k, top, bottom, x, y, mynum, numlines;
	char num[6 + 1], name[16 + 1], team[4 + 1];
	player_info_t *s;
	team_t *tm;

	if (!scr_drawVFrags.value)
		return;

	if (!sb_lines)
		return; // not enuff room

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	mynum = Sbar_PlayerNum();

	x = 324;


	if (vid.conwidth < 640 && cl.teamplay && scr_drawVFrags.value == 2)
		goto drawteams;

	// scores
	Sbar_SortFrags (false);

	if (!scoreboardlines)
		return; // no one there?

	// draw the text
	y = vid.conheight - sb_lines - 1;
	numlines = sb_lines / 8;
	if (numlines < 3)
		return; // not enough room

	// find us
	for (i = 0; i < scoreboardlines; i++)
		if (fragsort[i] == mynum)
			break;

	if (i == scoreboardlines)	// we're not there, we are probably a spectator, just display top
		i = 0;
	else						// figure out start
		i = i - numlines / 2;

	i = bound(0, i, scoreboardlines - numlines);

	for ( ; i < scoreboardlines && y < vid.conheight - 8 + 1; i++)
	{
		k = fragsort[i];
		s = &cl.players[k];

		if (!s->name[0])
			continue;

		top = scr_scoreboard_forcecolors.value ? s->topcolor : s->real_topcolor;
		bottom = scr_scoreboard_forcecolors.value ? s->bottomcolor : s->real_bottomcolor;

		Draw_Fill (x, y + 1, 40, 3, Sbar_ColorForMap (top));
		Draw_Fill (x, y + 4, 40, 4, Sbar_ColorForMap (bottom));

		// draw number
		snprintf(num, sizeof(num), "%c%3i%c", k==mynum?16:' ', s->frags, k==mynum?17:' ');

		Draw_String(x, y, num);

		// team
		if (cl.teamplay)
		{
			Q_strncpyz (team, s->team, sizeof(team));
			Draw_String (x + 48, y, team);
		}

		// draw name
		Q_strncpyz (name, s->name, sizeof(name));
		if (cl.teamplay)
			Draw_String (x + 48 + 40, y, name);
		else
			Draw_String (x + 48, y, name);

		y += 8;
	}

	// draw teams if room
	if (vid.conwidth < 640 || !cl.teamplay)
		return;

	Sbar_SortTeams();
	if (!scoreboardteams)
		return;

	// draw seperator
	x += 208;
	for (y = vid.conheight - sb_lines; y < vid.conheight - 6; y += 2)
		Draw_Character(x, y, 14);

	x += 16;

drawteams:
	Sbar_SortTeams();

	y = vid.conheight - sb_lines;
	for (i = 0; i < scoreboardteams && y < vid.conheight - 8 + 1; i++)
	{
		k = teamsort[i];
		tm = teams + k;

		//draw name
		Q_strncpyz (team, tm->team, sizeof(team));
		Draw_String (x, y, team);

		// draw total
		Draw_Fill (x + 40, y + 1, 48, 3, Sbar_ColorForMap (tm->topcolor));
		Draw_Fill (x + 40, y + 4, 48, 4, Sbar_ColorForMap (tm->bottomcolor));
		snprintf(num, sizeof(num), "%4i", tm->frags);
		Draw_Character (x + 40 + 8 , y, num[0]);
		Draw_Character (x + 40 + 16, y, num[1]);
		Draw_Character (x + 40 + 24, y, num[2]);
		Draw_Character (x + 40 + 32, y, num[3]);

		if (tm->myteam)
		{
			Draw_Character (x + 40, y, 16);
			Draw_Character (x + 40 + 40, y, 17);
		}

		y += 8;
	}
}

static void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color, int rightadjustify)
{
	char str[12], *ptr;
	int l, frame;

	l = Sbar_itoa (num, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (rightadjustify && l < digits)
		x += (digits - l) * 24;

	while (*ptr)
	{
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';
		Draw_DrawPicture(sb_nums[color][frame], x, y, 24, 24);
		x += 24;
		ptr++;
	}
}

void Sbar_IntermissionOverlay(void)
{
	float time;
	int	dig, num, xofs;

	scr_copyeverything = 1;
	scr_fullupdate = 0;

	if (cl.gametype == GAME_DEATHMATCH)
	{
		Sbar_DeathmatchOverlay();

		return;
	}

	xofs = (vid.conwidth - 320 ) >> 1;

	if (completepic == 0)
		completepic = Draw_LoadPicture("gfx/complete.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	Draw_DrawPicture(completepic, xofs + 64, 24, 192, 24);

	if (interpic == 0)
		interpic = Draw_LoadPicture("gfx/inter.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	Draw_DrawPicture(interpic, xofs, 56, 160, 144);

	if (cl.servertime_works)
		time = cl.solo_completed_time;	// we know the exact time
	else
		time = cl.completed_time - cl.players[cl.playernum].entertime;	// use an estimate

	//time
	dig = time / 60;
	dig = max(dig, 0);
	Sbar_IntermissionNumber (xofs + 152, 64, dig, 3, 0, true);
	num = time - dig * 60;
	Draw_DrawPicture(sb_colon, xofs + 224, 64, 24, 24);
	Draw_DrawPicture(sb_nums[0][num / 10], xofs + 248, 64, 24, 24);
	Draw_DrawPicture(sb_nums[0][num % 10], xofs + 272, 64, 24, 24);

	//secrets
	Sbar_IntermissionNumber (xofs + 152, 104, cl.stats[STAT_SECRETS], 3, 0, true);
	Draw_DrawPicture(sb_slash, xofs + 224, 104, 24, 24);
	Sbar_IntermissionNumber (xofs + 248, 104, cl.stats[STAT_TOTALSECRETS], 3, 0, false);

	//monsters
	Sbar_IntermissionNumber (xofs + 152, 144, cl.stats[STAT_MONSTERS], 3, 0, true);
	Draw_DrawPicture(sb_slash, xofs + 224, 144, 24, 24);
	Sbar_IntermissionNumber (xofs + 248, 144, cl.stats[STAT_TOTALMONSTERS], 3, 0, false);
}

void Sbar_FinaleOverlay(void)
{
	scr_copyeverything = 1;

	if (finalepic == 0)
		finalepic = Draw_LoadPicture("gfx/finale.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	Draw_DrawPicture(finalepic, (vid.conwidth - 288) / 2, 16, 288, 24);
}

/********************************* INTERFACE *********************************/

void Sbar_Draw(void)
{
	qboolean headsup;
	char st[64];

	headsup = !(cl_sbar.value || scr_viewsize.value < 100);
	if (sb_updates >= vid.numpages && !headsup)
		return;

	scr_copyeverything = 1;

	sb_updates++;

	if (scr_centerSbar.value)
		sbar_xofs = (vid.conwidth - 320) >> 1;
	else
		sbar_xofs = 0;

	// top line
	if (sb_lines > 24)
	{
		if (!cl.spectator || autocam == CAM_TRACK)
			Sbar_DrawInventory();

		if (cl.gametype == GAME_DEATHMATCH && (!headsup || vid.conwidth < 512))
			Sbar_DrawFrags();
	}

	// main area
	if (sb_lines > 0)
	{
		if (cl.spectator)
		{
			if (autocam != CAM_TRACK)
			{
				Sbar_DrawPic(0, 0, sb_scorebar, 320, 24);
				Sbar_DrawString(160 - 7 * 8,4, "SPECTATOR MODE");
				Sbar_DrawString(160 - 14 * 8 + 4, 12, "Press [ATTACK] for AutoCamera");
			}
			else
			{
				if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
					Sbar_SoloScoreboard ();

				else if (scr_compactHud.value == 3)
					Sbar_DrawCompact_Bare();
				else if (scr_compactHud.value == 2)
					Sbar_DrawCompact_TF();
				else if (scr_compactHud.value == 1)
					Sbar_DrawCompact();
				else
					Sbar_DrawNormal();

				snprintf(st, sizeof(st), "Tracking %-.13s", cl.players[spec_track].name);
				if (!cls.demoplayback)
					strlcat(st, ", [JUMP] for next", sizeof(st));

				Sbar_DrawString(0, -8, st);
			}
		}
		else if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
			Sbar_SoloScoreboard();
		else if (scr_compactHud.value == 3)
			Sbar_DrawCompact_Bare();
		else if (scr_compactHud.value == 2)
			Sbar_DrawCompact_TF();
		else if (scr_compactHud.value == 1)
			Sbar_DrawCompact();
		else
			Sbar_DrawNormal();
	}

	// main screen deathmatch rankings
	// if we're dead show team scores in team games
	if ((cl.stats[STAT_HEALTH] <= 0 && !cl.spectator) || sb_showscores || sb_showteamscores)
	{
		Sbar_DeathmatchOverlay();
	}

#ifdef GLQUAKE
	if (sb_showscores || sb_showteamscores || cl.stats[STAT_HEALTH] <= 0)
		sb_updates = 0;

	// clear unused areas in GL
	if (vid.conwidth > 320 && !headsup)
	{
		if (scr_centerSbar.value)	// left
			Draw_TileClear (0, vid.conheight - sb_lines, sbar_xofs, sb_lines);

		Draw_TileClear (320 + sbar_xofs, vid.conheight - sb_lines, vid.conwidth - (320 + sbar_xofs), sb_lines);	// right
	}
	if (!headsup && cl.spectator && autocam != CAM_TRACK && sb_lines > SBAR_HEIGHT)
		Draw_TileClear (sbar_xofs, vid.conheight - sb_lines, 320, sb_lines - SBAR_HEIGHT);
#endif

	if (vid.conwidth >= 512 && sb_lines > 0 && cl.gametype == GAME_DEATHMATCH && !scr_centerSbar.value)
		Sbar_MiniDeathmatchOverlay ();
}

