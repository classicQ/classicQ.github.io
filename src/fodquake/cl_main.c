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
// cl_main.c  -- client main loop

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "quakedef.h"
#include "sys_io.h"
#include "filesystem.h"
#include "cdaudio.h"
#include "input.h"
#include "keys.h"
#include "menu.h"
#include "sbar.h"
#include "skin.h"
#include "sound.h"
#include "version.h"
#include "teamplay.h"
#include "image.h"
#include "wad.h"

#include "movie.h"
#include "logging.h"
#include "ignore.h"
#include "fmod.h"
#include "fchecks.h"
#include "modules.h"
#include "config_manager.h"
#include "mp3_player.h"
#include "huffman.h"
#include "config.h"
#include "sleep.h"
#include "mouse.h"
#ifdef NETQW
#include "netqw.h"
#endif
#include "strl.h"
#include "ruleset.h"
#include "tokenize_string.h"
#include "context_sensitive_tab.h"
#include "lua.h"

#ifndef GLQUAKE
#include "d_local.h"
#endif

#ifdef FOD_PPC
int altivec_available;
#endif

int movementkey;

static qboolean net_maxfps_callback(cvar_t *var, char *string);
static qboolean net_lag_callback(cvar_t *var, char *string);
static qboolean net_lag_ezcheat_callback(cvar_t *var, char *string);

#include "pmove.h"

static qboolean cl_imitate_client_callback(cvar_t *var, char *string);
static qboolean cl_imitate_os_callback(cvar_t *var, char *string);

static void R_Draw_Flat(float x_lower_limit, float x_upper_limit, float y_lower_limit, float y_upper_limit, float z_lower_limit, float z_upper_limit, float r, float g, float b, qboolean unset);
static qboolean r_drawflat_enable_callback(cvar_t *var, char *string);
static qboolean r_drawflat_walls_callback(cvar_t *var, char *string);
static qboolean r_drawflat_floors_ceilings_callback(cvar_t *var, char *string);
static qboolean r_drawflat_slopes_callback(cvar_t *var, char *string);

cvar_t	rcon_password = {"rcon_password", ""};
cvar_t	rcon_address = {"rcon_address", ""};

static cvar_t con_clearnotify = {"con_clearnotify","1"};

cvar_t	cl_timeout = {"cl_timeout", "60"};

cvar_t	cl_shownet = {"cl_shownet", "0"};	// can be 0, 1, or 2

cvar_t	cl_sbar		= {"cl_sbar", "0", CVAR_ARCHIVE};
cvar_t	cl_hudswap	= {"cl_hudswap", "0", CVAR_ARCHIVE};
cvar_t	cl_maxfps	= {"cl_maxfps", "0", CVAR_ARCHIVE};

cvar_t	cl_predictPlayers = {"cl_predictPlayers", "1"};
cvar_t	cl_solidPlayers = {"cl_solidPlayers", "1"};

cvar_t  localid = {"localid", ""};

static qboolean allowremotecmd = true;

cvar_t	cl_deadbodyfilter = {"cl_deadbodyFilter", "0"};
cvar_t	cl_gibfilter = {"cl_gibFilter", "0"};
cvar_t	cl_muzzleflash = {"cl_muzzleflash", "1"};
cvar_t	cl_rocket2grenade = {"cl_r2g", "0"};
cvar_t	cl_demospeed = {"cl_demospeed", "1"};
cvar_t	cl_staticsounds = {"cl_staticSounds", "1"};
cvar_t	cl_trueLightning = {"cl_trueLightning", "0"};
cvar_t	cl_parseWhiteText = {"cl_parseWhiteText", "1"};
cvar_t	cl_filterdrawviewmodel = {"cl_filterdrawviewmodel", "0"};
cvar_t	cl_oldPL = {"cl_oldPL", "0"};
cvar_t	cl_demoPingInterval = {"cl_demoPingInterval", "5"};
cvar_t	cl_chatsound = {"cl_chatsound", "1"};
cvar_t	cl_confirmquit = {"cl_confirmquit", "1"};
cvar_t	default_fov = {"default_fov", "0"};
cvar_t	qizmo_dir = {"qizmo_dir", "qizmo"};

cvar_t cl_floodprot			= {"cl_floodprot", "0"};
cvar_t cl_fp_messages		= {"cl_fp_messages", "4"};
cvar_t cl_fp_persecond		= {"cl_fp_persecond", "4"};
cvar_t cl_cmdline			= {"cl_cmdline", "", CVAR_ROM};
cvar_t cl_useproxy			= {"cl_useproxy", "0"};

cvar_t net_maxfps = { "net_maxfps", "0", 0, net_maxfps_callback };
static cvar_t net_lag = { "net_lag", "0", 0, net_lag_callback };
static cvar_t net_lag_ezcheat = { "net_lag_ezcheat", "0", 0, net_lag_ezcheat_callback };

cvar_t cl_imitate_client = { "cl_imitate_client", "none", 0, cl_imitate_client_callback };
cvar_t cl_imitate_os = { "cl_imitate_os", "none", 0, cl_imitate_os_callback };

cvar_t cl_model_bobbing		= {"cl_model_bobbing", "1"};
cvar_t cl_nolerp			= {"cl_nolerp", "1"};

cvar_t r_rocketlight			= {"r_rocketLight", "1"};
cvar_t r_rocketlightcolor		= {"r_rocketLightColor", "0"};
cvar_t r_explosionlightcolor	= {"r_explosionLightColor", "0"};
cvar_t r_explosionlight			= {"r_explosionLight", "1"};
cvar_t r_explosiontype			= {"r_explosionType", "0"};
cvar_t r_flagcolor				= {"r_flagColor", "0"};
cvar_t r_lightflicker			= {"r_lightflicker", "1"};
cvar_t r_rockettrail			= {"r_rocketTrail", "1"};
cvar_t r_grenadetrail			= {"r_grenadeTrail", "1"};
cvar_t r_powerupglow			= {"r_powerupGlow", "1"};
cvar_t r_drawflat_enable		= {"r_drawflat_enable", "0", 0, r_drawflat_enable_callback };
cvar_t r_drawflat_walls = {"r_drawflat_walls", "off", 0, r_drawflat_walls_callback};
cvar_t r_drawflat_floors_ceilings = {"r_drawflat_floors_ceilings", "off", 0, r_drawflat_floors_ceilings_callback};
cvar_t r_drawflat_slopes= {"r_drawflat_slopes", "off", 0, r_drawflat_slopes_callback};

// info mirrors
cvar_t	password = {"password", "", CVAR_USERINFO};
cvar_t	spectator = {"spectator", "", CVAR_USERINFO};
cvar_t	name = {"name", "player", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	team = {"team", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	topcolor = {"topcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	bottomcolor = {"bottomcolor","0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	skin = {"skin", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	rate = {"rate", "30000", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	msg = {"msg", "1", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	noaim = {"noaim", "0", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	w_switch = {"w_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};
cvar_t	b_switch = {"b_switch", "", CVAR_ARCHIVE|CVAR_USERINFO};

int imitatedclientnum;
int imitatedosnum;

clientPersistent_t	cls;
clientState_t		cl;

centity_t		cl_entities[CL_MAX_EDICTS];
efrag_t			cl_efrags[MAX_EFRAGS];
lightstyle_t	cl_lightstyle[MAX_LIGHTSTYLES];
dlight_t		cl_dlights[MAX_DLIGHTS];
unsigned int cl_dlight_active[MAX_DLIGHTS/32];

// draw flat globals
static float draw_flat[3][3];

// refresh list
#ifdef GLQUAKE
visentlist_t	cl_firstpassents, cl_visents, cl_alphaents;
#else
visentlist_t	cl_visents, cl_visbents;
#endif

double		connect_time = 0;		// for connection retransmits

qboolean	host_skipframe;			// used in demo playback

byte		*host_basepal;
byte		*host_colormap;

int			fps_count;

static int cl_vidinitialised;

struct netaddr cl_net_from;
sizebuf_t cl_net_message;
static byte cl_net_message_buffer[MAX_MSGLEN*2];

static qboolean net_maxfps_callback(cvar_t *var, char *string)
{
	float maxfps;
	float value;

#ifdef NETQW
	value = Q_atof(string);

	maxfps = cl.maxfps;
	if (maxfps == 0)
		maxfps = 72;

	if (value && value < maxfps)
		maxfps = value;

	if (maxfps < 30)
		maxfps = 30;

	if (cls.netqw)
		NetQW_SetFPS(cls.netqw, maxfps);
#endif

	return false;
}

static qboolean net_lag_callback(cvar_t *var, char *string)
{
	float value;

#ifdef NETQW
	value = Q_atof(string);
	if (cls.netqw)
		NetQW_SetLag(cls.netqw, value*1000);
#endif

	return false;
}

static qboolean net_lag_ezcheat_callback(cvar_t *var, char *string)
{
	float value;

#ifdef NETQW
	value = Q_atof(string);
	if (cls.netqw)
		NetQW_SetLagEzcheat(cls.netqw, value!=0);
#endif

	return false;
}

static void CL_InitClientVersionInfo(void);

enum
{
	CLIENT_FODQUAKE,
	CLIENT_EZQUAKE_1144,
	CLIENT_EZQUAKE_1517,
	CLIENT_EZQUAKE_1_8_2,
	CLIENT_FUHQUAKE_0_31_675
};

static char *validclientnames[] =
{
	"none",
	"ezquake-1144",
	"ezquake-1517",
	"ezquake-1.8.2",
	"FuhQuake-0.31-675"
};

char *fversion_clientnames[] =
{
	"Fodquake version "FODQUAKE_VERSION,
	"ezQuake version 1144",
	"ezQuake version 1517",
	"ezQuake 1.8.2 stable (build 2029)",
	"FuhQuake version 0.31 (build 675)"
};

char *validosnames[] =
{
	"none",
	"Cygwin",
	"Linux",
	"MorphOS",
	"Win32"
};

char *fversion_osnames[] =
{
	QW_PLATFORM,
	"Cygwin",
	"Linux",
	"MorphOS",
	"Win32"
};

static int Point_On_Surface(vec3_t *points, int count, vec3_t point)
{
	vec3_t temp3, temp4, p, q;
	int i;

	VectorSubtract(points[count-1], point, temp3);
	VectorSubtract(points[0], point, temp4);

	CrossProduct(temp3, temp4, p);

	for (i=0;i<count-1;i++)
	{
		VectorSubtract(points[i], point, temp3);
		VectorSubtract(points[i+1], point, temp4);
		CrossProduct(temp3, temp4, q);
		if (DotProduct(p,q) < 0)
			return 0;
	}

	return 1;
}

static qboolean r_drawflat_enable_callback(cvar_t *var, char *string)
{
#ifndef GLQUAKE
	model_t *model;
	unsigned int i;

	model = cl.worldmodel;

	if (model)
	{
		for (i=0;i<model->numsurfaces;i++)
		{
			D_UncacheSurface(&model->surfaces[i]);
		}
	}
#endif

	return false;
}

static qboolean r_drawflat_walls_callback(cvar_t *var, char *string)
{
	struct tokenized_string *ts;
	float r, g, b;

	ts = Tokenize_String(string);
	if (!ts)
		return true;

	if (ts->count != 1 && ts->count != 3)
	{
		Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
		Tokenize_String_Delete(ts);
		return true;
	}

	if (ts->count == 1)
	{
		if (strcmp(ts->tokens[0], "off") != 0)
		{
			Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
			Tokenize_String_Delete(ts);
			return true;
		}
		R_Draw_Flat(-1, 1, -1, 1, 0, 0, 0, 0, 0, true);
		Tokenize_String_Delete(ts);
		return false;
	}

	if (ts->count == 3)
	{
		r = bound(0, atof(ts->tokens[0]), 1);
		g = bound(0, atof(ts->tokens[1]), 1);
		b = bound(0, atof(ts->tokens[2]), 1);

		draw_flat[0][0] = r;
		draw_flat[0][1] = g;
		draw_flat[0][2] = b;

		R_Draw_Flat(-1, 1, -1, 1, 0, 0, r, g, b, false);
		Tokenize_String_Delete(ts);
		return false;
	}

	return true;
}

static qboolean r_drawflat_floors_ceilings_callback(cvar_t *var, char *string)
{
	struct tokenized_string *ts;
	float r, g, b;

	ts = Tokenize_String(string);
	if (!ts)
		return true;

	if (ts->count != 1 && ts->count != 3)
	{
		Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
		Tokenize_String_Delete(ts);
		return true;
	}

	if (ts->count == 1)
	{
		if (strcmp(ts->tokens[0], "off") != 0)
		{
			Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
			Tokenize_String_Delete(ts);
			return true;
		}
		R_Draw_Flat(-1, 1, -1, 1, -1, -1, 0, 0, 0, true);
		R_Draw_Flat(-1, 1, -1, 1, 1, 1, 0, 0, 0, true);
		Tokenize_String_Delete(ts);
		return false;
	}

	if (ts->count == 3)
	{
		r = bound(0, atof(ts->tokens[0]), 1);
		g = bound(0, atof(ts->tokens[1]), 1);
		b = bound(0, atof(ts->tokens[2]), 1);

		draw_flat[1][0] = r;
		draw_flat[1][1] = g;
		draw_flat[1][2] = b;

		R_Draw_Flat(-1, 1, -1, 1, -1, -1, r, g, b, false);
		R_Draw_Flat(-1, 1, -1, 1, 1, 1, r, g, b, false);
		Tokenize_String_Delete(ts);
		return false;
	}

	return true;
}

static qboolean r_drawflat_slopes_callback(cvar_t *var, char *string)
{
	struct tokenized_string *ts;
	float r, g, b;

	ts = Tokenize_String(string);
	if (!ts)
		return true;

	if (ts->count != 1 && ts->count != 3)
	{
		Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
		Tokenize_String_Delete(ts);
		return true;
	}

	if (ts->count == 1)
	{
		if (strcmp(ts->tokens[0], "off") != 0)
		{
			Com_Printf("usage: %s [r] [g] [b] or off, colors should be in the range of 0 to 1.\n", var->name);
			Tokenize_String_Delete(ts);
			return true;
		}
		R_Draw_Flat(-1, 1, -1, 1, -0.99999f, -0.00001f, 0, 0, 0, true);
		R_Draw_Flat(-1, 1, -1, 1, 0.00001f, 0.99999f, 0, 0, 0, true);
		Tokenize_String_Delete(ts);
		return false;
	}

	if (ts->count == 3)
	{
		r = bound(0, atof(ts->tokens[0]), 1);
		g = bound(0, atof(ts->tokens[1]), 1);
		b = bound(0, atof(ts->tokens[2]), 1);

		draw_flat[2][0] = r;
		draw_flat[2][1] = g;
		draw_flat[2][2] = b;

		R_Draw_Flat(-1, 1, -1, 1, -0.99999f, -0.00001f, r, g, b, false);
		R_Draw_Flat(-1, 1, -1, 1, 0.00001f, 0.99999f, r, g, b, false);
		Tokenize_String_Delete(ts);
		return false;
	}

	return true;
}

static void R_DrawFlat_UpdateSurface(model_t *model, msurface_t *surface)
{
#ifdef GLQUAKE
	unsigned int i;

	if (model->vertcolours)
	{
		if (surface->polys)
		{
			for(i=surface->polys->firstindex;i<surface->polys->firstindex+surface->polys->numverts;i++)
			{
				model->vertcolours[i*3+0] = surface->color[0];
				model->vertcolours[i*3+1] = surface->color[1];
				model->vertcolours[i*3+2] = surface->color[2];
			}

			model->surface_colours_dirty = 1;
		}
	}
#else
	surface->palcolor = V_LookUpColour(surface->color[0], surface->color[1], surface->color[2]);
	if (r_drawflat_enable.value == 1)
		D_UncacheSurface(surface);
#endif
}

static void R_DrawFlatShoot_f(void)
{
	model_t *model;
	vec3_t	vec, vec1;
	vec3_t *points;
	pmtrace_t trace;
	mleaf_t	*leaf;
	msurface_t *surface;
	unsigned int marksurface;
	mvertex_t *vertex;
	int e, x, i;
	float color[3];

	if (Cmd_Argc() != 4)
	{
		Com_Printf("Usage: r_drawflat_shoot [r] [g] [b], colors should be in range of 0 to 1\n");
		return;
	}

	if (!cl.worldmodel)
	{
		Com_Printf("No map loaded, can't set surface color\n");
		return;
	}

	color[0] = bound(0, atof(Cmd_Argv(1)), 1);
	color[1] = bound(0, atof(Cmd_Argv(2)), 1);
	color[2] = bound(0, atof(Cmd_Argv(3)), 1);

	model = cl.worldmodel;
	AngleVectors(r_refdef.viewangles, vec1, NULL, NULL);

	VectorMA(r_refdef.vieworg, 10000.0f, vec1, vec);

	trace = PM_TraceLine (r_refdef.vieworg, vec);

	leaf = Mod_PointInLeaf(trace.endpos, model);

	if (!leaf)
	{
		Com_Printf("no leaf returned\n");
		return;
	}

	for (i=0, marksurface = leaf->firstmarksurfacenum; i<leaf->nummarksurfaces; i++, marksurface++)
	{
		surface = model->surfaces + model->marksurfaces[marksurface];

		points = calloc(surface->numedges , sizeof(vec3_t));
		if (points == NULL)
		{
			Com_Printf("alloc error\n");
			return;
		}
		for (x=0;x<surface->numedges;x++)
		{
			e = model->surfedges[surface->firstedge + x];
			if (abs(e) >= model->numedges)
				return;

			if (e >= 0)
				vertex = &model->vertexes[model->edges[e].v[0]];
			else
				vertex = &model->vertexes[model->edges[-e].v[1]];
			points[x][0] = vertex->position[0];
			points[x][1] = vertex->position[1];
			points[x][2] = vertex->position[2];
		}

		if (Point_On_Surface(points, surface->numedges, trace.endpos) && trace.draw_plane == surface->plane )
		{
			surface->is_drawflat = 1;
			surface->color[0] = color[0];
			surface->color[1] = color[1];
			surface->color[2] = color[2];

			R_DrawFlat_UpdateSurface(model, surface);
		}
		free(points);
	}
	return;
}

static void R_DrawFlatShootUnset_f(void)
{
	model_t *model;
	vec3_t	vec, vec1;
	vec3_t *points;
	pmtrace_t trace;
	mleaf_t	*leaf;
	msurface_t *surface;
	unsigned int marksurface;
	mvertex_t *vertex;
	int e, x, i;

	if (!cl.worldmodel)
	{
		Com_Printf("No map loaded, can't set surface color\n");
		return;
	}

	model = cl.worldmodel;
	AngleVectors(r_refdef.viewangles, vec1, NULL, NULL);

	VectorMA(r_refdef.vieworg, 10000.0f, vec1, vec);

	trace = PM_TraceLine (r_refdef.vieworg, vec);

	leaf = Mod_PointInLeaf(trace.endpos, model);

	if (!leaf)
	{
		Com_Printf("no leaf returned\n");
		return;
	}

	for (i=0, marksurface = leaf->firstmarksurfacenum; i<leaf->nummarksurfaces; i++, marksurface++)
	{
		surface = model->surfaces + model->marksurfaces[marksurface];

		points = calloc(surface->numedges , sizeof(vec3_t));
		if (points == NULL)
		{
			Com_Printf("alloc error\n");
			return;
		}
		for (x=0;x<surface->numedges;x++)
		{
			e = model->surfedges[surface->firstedge + x];
			if (abs(e) >= model->numedges)
				return;

			if (e >= 0)
				vertex = &model->vertexes[model->edges[e].v[0]];
			else
				vertex = &model->vertexes[model->edges[-e].v[1]];
			points[x][0] = vertex->position[0];
			points[x][1] = vertex->position[1];
			points[x][2] = vertex->position[2];
		}

		if (Point_On_Surface(points, surface->numedges, trace.endpos) && trace.draw_plane == surface->plane )
		{
			surface->is_drawflat = 0;

			R_DrawFlat_UpdateSurface(model, surface);
		}
		free(points);
	}
	return;
}


static void R_Draw_Flat(float x_lower_limit, float x_upper_limit, float y_lower_limit, float y_upper_limit, float z_lower_limit, float z_upper_limit, float r, float g, float b, qboolean unset)
{
	model_t *model;
	vec3_t	vec;
	int i;

	if (!cl.worldmodel)
		return;

	model = cl.worldmodel;

	for (i=0;i<model->numsurfaces;i++)
	{
		VectorCopy(model->surfaces[i].plane->normal, vec);
		VectorNormalize(vec);
		if (vec[2] <= z_upper_limit && vec[2] >= z_lower_limit
		 && vec[1] <= y_upper_limit && vec[1] >= y_lower_limit
		 && vec[0] <= x_upper_limit && vec[0] >= x_lower_limit)
		{
			if (unset == true)
			{
				model->surfaces[i].is_drawflat = 0;
			}
			else
			{
				model->surfaces[i].is_drawflat = 1;
				model->surfaces[i].color[0] = r;
				model->surfaces[i].color[1] = g;
				model->surfaces[i].color[2] = b;
			}

			R_DrawFlat_UpdateSurface(model, &model->surfaces[i]);
		}
	}

	return;
}

static void R_DrawFlat_f(void)
{
	float limit[6], color[3];

	if (Cmd_Argc() != 10)
	{
		Com_Printf("Usage: r_drawflat [x_lower_limit] [x_upper_limit] [y_lower_limit] [y_upper_limit] [z_lower_limit] [z_upper_limit] [r] [g] [b], limits go from -1 to 1, colors should be in the range of 0 to 1.\n");
		return;
	}

	limit[0] = atof(Cmd_Argv(1));
	limit[1] = atof(Cmd_Argv(2));
	limit[2] = atof(Cmd_Argv(3));
	limit[3] = atof(Cmd_Argv(4));
	limit[4] = atof(Cmd_Argv(5));
	limit[5] = atof(Cmd_Argv(6));
	color[0] = bound(0, atof(Cmd_Argv(7)), 1);
	color[1] = bound(0, atof(Cmd_Argv(8)), 1);
	color[2] = bound(0, atof(Cmd_Argv(9)), 1);

	R_Draw_Flat(limit[0], limit[1], limit[2], limit[3], limit[4], limit[5], color[0], color[1], color[2], true);
}

static void R_DrawFlat_Set_f(void)
{
	model_t *model;
	float r, g, b;
	int surface;

	if (Cmd_Argc() != 5)
	{
		Com_Printf("Usage: r_drawflat_set [surface] [r] [g] [b], colors should be in range of 0 to 1\n");
		return;
	}

	if (!cl.worldmodel)
	{
		Com_Printf("No map loaded, can't set surface color\n");
		return;
	}

	model = cl.worldmodel;

	surface = atoi(Cmd_Argv(1));
	r = bound(0, atof(Cmd_Argv(2)), 1);
	g = bound(0, atof(Cmd_Argv(3)), 1);
	b = bound(0, atof(Cmd_Argv(4)), 1);

	if (surface < 0 || surface >= model->numsurfaces)
	{
		Com_Printf("Surface out of range.\n");
		return;
	}

	model->surfaces[surface].is_drawflat = 1;
	model->surfaces[surface].color[0] = r;
	model->surfaces[surface].color[1] = g;
	model->surfaces[surface].color[2] = b;

	R_DrawFlat_UpdateSurface(model, &model->surfaces[surface]);
}

static void R_DrawFlat_Unset_f(void)
{
	model_t *model;
	int surface;

	if (!cl.worldmodel)
		return;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: r_drawflat_unset [surface number]\n");
		return;
	}

	model = cl.worldmodel;

	surface = atoi(Cmd_Argv(1));

	if (surface < 0 || surface >= model->numsurfaces)
	{
		Com_Printf("Surface out of range.\n");
		return;
	}

	model->surfaces[surface].is_drawflat = 0;
}



static void R_DrawFlat_Write_Config(void)
{
	FILE *f;
	char *file;
	model_t *model;
	int i;

	if (!cl.worldmodel)
		return;

	model = cl.worldmodel;

	file = va("%s/qw/%s.dfcfg", com_basedir, mapname.string);

	f = fopen(file, "r");
	if (f)
	{
		fclose(f);
		if (Cmd_Argc() != 2)
		{
			Com_Printf("File \"%s\" exists. Please use \"r_drawflat_writeconfig overwrite\" to overwrite the old file.\n", mapname.string);
			return;
		}
		else
		{
			if (strcmp(Cmd_Argv(1), "overwrite") != 0)
			{
				Com_Printf("If you really want to overwrite please spell overwrite right.\n");
				return;
			}
		}
	}

	f = fopen(file, "wb");

	if (!f)
		return;

	for (i=0;i<model->numsurfaces;i++)
	{
		if(model->surfaces[i].is_drawflat == 1)
		{
			fprintf(f,"r_drawflat_set %i %f %f %f\n",i ,model->surfaces[i].color[0], model->surfaces[i].color[1], model->surfaces[i].color[2]);
		}
	}
	fclose(f);
}

void R_DrawFlat_NewMap (void)
{
	if (r_drawflat_enable.value != 1)
		return;

	if (strcmp(r_drawflat_walls.string, "off") != 0)
	{
		R_Draw_Flat(-1, 1, -1, 1, 0, 0, draw_flat[0][0], draw_flat[0][1], draw_flat[0][2], false);
	}

	if (strcmp(r_drawflat_floors_ceilings.string, "off") != 0)
	{
		R_Draw_Flat(-1, 1, -1, 1, -1, -1, draw_flat[1][0], draw_flat[1][1], draw_flat[1][2], false);
		R_Draw_Flat(-1, 1, -1, 1, 1, 1, draw_flat[1][0], draw_flat[1][1], draw_flat[1][2], false);
	}

	if (strcmp(r_drawflat_slopes.string, "off") != 0)
	{
		R_Draw_Flat(-1, 1, -1, 1, -0.99999f, -0.00001f, draw_flat[2][0], draw_flat[2][1], draw_flat[2][2], false);
		R_Draw_Flat(-1, 1, -1, 1, 0.00001f, 0.99999f, draw_flat[2][0], draw_flat[2][1], draw_flat[2][2], false);
	}

	Cbuf_AddText(va("exec %s.dfcfg\n", mapname.string));
}


#define NUMVALIDCLIENTNAMES (sizeof(validclientnames)/sizeof(*validclientnames))
#define NUMVALIDOSNAMES (sizeof(validosnames)/sizeof(*validosnames))

static qboolean cl_imitate_client_callback(cvar_t *var, char *string)
{
	int i;

	for(i=0;i<NUMVALIDCLIENTNAMES;i++)
	{
		if (Q_strcasecmp(validclientnames[i], string) == 0)
			break;
	}

	if (i == NUMVALIDCLIENTNAMES)
	{
		Com_Printf("Client name \"%s\" is invalid. Valid client names are:\n", string);
		for(i=0;i<NUMVALIDCLIENTNAMES;i++)
			Com_Printf(" - %s\n", validclientnames[i]);

		return true;
	}

	if (Q_strcasecmp(string, validclientnames[imitatedclientnum]) == 0)
		return false;

	imitatedclientnum = i;
	CL_InitClientVersionInfo();
	Com_Printf("Client information will be updated after (re)connecting to a server.\n");

	return false;
}

static qboolean cl_imitate_os_callback(cvar_t *var, char *string)
{
	int i;

	for(i=0;i<NUMVALIDOSNAMES;i++)
	{
		if (Q_strcasecmp(validosnames[i], string) == 0)
			break;
	}

	if (i == NUMVALIDOSNAMES)
	{
		Com_Printf("OS name \"%s\" is invalid. Valid OS names are:\n", string);
		for(i=0;i<NUMVALIDOSNAMES;i++)
			Com_Printf(" - %s\n", validosnames[i]);

		return true;
	}

	imitatedosnum = i;

	return false;
}

char emodel_name[] = "emodel";
char pmodel_name[] = "pmodel";

//============================================================================

const char *CL_Macro_ConnectionType(void)
{
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.spectator ? "spectator" : "player";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));

	return macrobuf;
}

const char *CL_Macro_Demoplayback(void)
{
	char *s;
	static char macrobuf[16];

	s = cls.mvdplayback ? "mvdplayback" : cls.demoplayback ? "qwdplayback" : "0";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));

	return macrobuf;
}

const char *CL_Macro_Serverstatus(void)
{
	char *s;
	static char macrobuf[16];

	s = (cls.state < ca_connected) ? "disconnected" : cl.standby ? "standby" : "normal";
	Q_strncpyz(macrobuf, s, sizeof(macrobuf));

	return macrobuf;
}

int CL_ClientState(void)
{
	return cls.state;
}

void CL_MakeActive(void)
{
	cls.state = ca_active;
	if (cls.demoplayback)
	{
		host_skipframe = true;
		demostarttime = cls.demotime;
	}

	if (!cls.demoplayback)
		VID_SetCaption (va("Fodquake: %s", cls.servername));

	Con_ClearNotify();
	TP_ExecTrigger("f_spawn");

	Ignore_PostNewMap();

	if (key_dest == key_game)
		VID_SetMouseGrab(1);
}

//Cvar system calls this when a CVAR_USERINFO cvar changes
void CL_UserinfoChanged(char *key, char *string)
{
	char *s;

	s = TP_ParseFunChars (string, false);

	if (strcmp(s, Info_ValueForKey (cls.userinfo, key)))
	{
		Info_SetValueForKey (cls.userinfo, key, s, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
#ifdef NETQW
			if (cls.netqw)
			{
				unsigned int i;
				char buf[1400];

				i = snprintf(buf, sizeof(buf), "%csetinfo \"%s\" \"%s\"\n", clc_stringcmd, key, s);
				if (i < sizeof(buf))
				{
					NetQW_AppendReliableBuffer(cls.netqw, buf, i+1);
				}
			}
#else
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, va("setinfo \"%s\" \"%s\"\n", key, s));
#endif
		}
	}
}

#ifndef NETQW
//called by CL_Connect_f and CL_CheckResend
static void CL_SendConnectPacket(void)
{
	char data[2048];
	int i;

	if (cls.state != ca_disconnected)
		return;

	Ruleset_Activate();

	connect_time = cls.realtime;	// for retransmit requests
	cls.qport = Cvar_VariableValue("qport");

	i = Q_snprintfz(data, sizeof(data), "\xff\xff\xff\xff" "connect %i %i %i \"%s\"\n", PROTOCOL_VERSION, cls.qport, cls.challenge, cls.userinfo);

	i++;

	if (i < sizeof(data) && cls.netchan.huffcontext)
		i+= Q_snprintfz(data+i, sizeof(data)-i, "0x%08x 0x%08x\n", QW_PROTOEXT_HUFF, cls.hufftablecrc);

	if (i < sizeof(data) && cls.ftexsupported)
		i+= Q_snprintfz(data+i, sizeof(data)-i, "0x%08x 0x%08x\n", QW_PROTOEXT_FTEX, cls.ftexsupported);

	if (i < sizeof(data))
	{
		if (data[i-1] == 0)
			i--;

		NET_SendPacket(NS_CLIENT, i+1, data, &cls.server_adr);
		return;
	}

	Com_Printf("Connection packet size overflow\n");
	return;
}

//Resend a connect message if the last one has timed out
static void CL_CheckForResend (void)
{
	char data[2048];

	if (cls.state == ca_disconnected && com_serveractive)
	{
		// if the local server is running and we are not, then connect
		Q_strncpyz (cls.servername, "local", sizeof(cls.servername));
		NET_StringToAdr(0, "local", &cls.server_adr);
		CL_SendConnectPacket ();	// we don't need a challenge on the local server
		// FIXME: cls.state = ca_connecting so that we don't send the packet twice?
		return;
	}

	if (cls.state != ca_disconnected || !connect_time)
		return;
	if (cls.realtime - connect_time < 5.0)
		return;

	connect_time = cls.realtime;	// for retransmit requests

	Com_Printf("Connecting to %s...\n", cls.servername);
	Q_snprintfz(data, sizeof(data), "\xff\xff\xff\xff" "getchallenge\n");
	NET_SendPacket(NS_CLIENT, strlen(data), data, &cls.server_adr);
}
#endif

static void CL_BeginServerConnect(void)
{
	connect_time = 0;

#ifdef NETQW
	Ruleset_Activate();

	cls.netqw = NetQW_Create(cls.servername, cls.userinfo, rand()&0xffff, FTEX_SUPPORTED);
	if (cls.netqw)
	{
		Com_Printf("Connecting to %s...\n", cls.servername);
		NetQW_SetLag(cls.netqw, net_lag.value*1000);
		NetQW_SetLagEzcheat(cls.netqw, net_lag_ezcheat.value!=0);
	}
	else
		Com_Printf("Unable to create connection\n");
#endif

#if 1
#warning This is currently needed for /packet to work.
	if (!NET_StringToAdr(0, cls.servername, &cls.server_adr))
	{
		Com_Printf("Bad server address\n");
		return;
	}

	if (!NET_OpenSocket(NS_CLIENT, cls.server_adr.type))
	{
		Com_Printf("Unable to create socket\n");
		return;
	}
#endif

#ifndef NETQW
	connect_time = -999;	// CL_CheckForResend() will fire immediately
	CL_CheckForResend();
#endif
}

void CL_Connect_f(void)
{
	qboolean proxy;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <server>\n", Cmd_Argv(0));
		return;
	}

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (proxy)
	{
		Cbuf_AddText(va("say ,connect %s", Cmd_Argv(1)));
	}
	else
	{
		Host_EndGame();
		Q_strncpyz(cls.servername, Cmd_Argv (1), sizeof(cls.servername));
		CL_BeginServerConnect();
	}
}




qboolean CL_ConnectedToProxy(void)
{
	cmd_alias_t *alias = NULL;
	char **s, *qizmo_aliases[] = {	"ezcomp", "ezcomp2", "ezcomp3", "f_sens", "f_fps", "f_tj", "f_ta", NULL};

	if (cls.state < ca_active)
		return false;

	for (s = qizmo_aliases; *s; s++)
	{
		if (!(alias = Cmd_FindAlias(*s)) || !(alias->flags & ALIAS_SERVER))
			return false;
	}

	return true;
}

void CL_Join_f(void)
{
	qboolean proxy;

	if (cls.demoplayback)
	{
		Com_Printf("Playing a demo, can't join.\n");
		return;
	}

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2)
	{
		Com_Printf ("Usage: %s [server]\n", Cmd_Argv(0));
		return;
	}

	Cvar_Set(&spectator, "");

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else if (cl.z_ext & Z_EXT_JOIN_OBSERVE)
		Cbuf_AddText("cmd join\n");
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

void CL_Observe_f(void)
{
	qboolean proxy;

	if (cls.demoplayback)
	{
		Com_Printf("Playing a demo, can't observe.\n");
		return;
	}

	proxy = cl_useproxy.value && CL_ConnectedToProxy();

	if (Cmd_Argc() > 2)
	{
		Com_Printf("Usage: %s [server]\n", Cmd_Argv(0));
		return;
	}

	if (!spectator.string[0] || !strcmp(spectator.string, "0"))
		Cvar_SetValue(&spectator, 1);

	if (Cmd_Argc() == 2)
		Cbuf_AddText(va("%s %s\n", proxy ? "say ,connect" : "connect", Cmd_Argv(1)));
	else if (cl.z_ext & Z_EXT_JOIN_OBSERVE)
		Cbuf_AddText("cmd observe\n");
	else
		Cbuf_AddText(va("%s\n", proxy ? "say ,reconnect" : "reconnect"));
}

void CL_QTVPlay_f()
{
	char buf[512];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <stream>\n", Cmd_Argv(0));
		return;
	}

	snprintf(buf, sizeof(buf), "alias f_qtv \"say .qtv %s\"; connect $sb_qtv_proxy\n", Cmd_Argv(1));
	Cbuf_AddText(buf);
}

void CL_DNS_f()
{
	char address[128];
	struct netaddr addr;
	qboolean r;
	const char *cname;

	if (Cmd_Argc() != 2)
	{
		Com_Printf("Usage: %s <address>\n", Cmd_Argv(0));
		return;
	}

	strlcpy(address, Cmd_Argv(1), sizeof(address));

#if 0
	if ((s = strchr(address, ':')))
		*s = 0;
#endif

	r = NET_StringToAdr(0, address, &addr);
	if (!r)
	{
		Com_Printf("Couldn't look up \"%s\"\n", address);
	}
	else
	{
		Com_Printf("IP: %s\n", NET_BaseAdrToString(&addr));
		cname = NET_GetHostnameForAddress(&addr);
		if (cname)
			Com_Printf("Hostname: %s\n", cname);
	}
}


void CL_ClearState(void)
{
	int i;
	extern float scr_centertime_off;

	S_StopAllSounds(true);

	Com_DPrintf("Clearing memory\n");

	CL_ClearTEnts();
	CL_ClearScene();

	CL_FreeStatics();

	// wipe the entire cl structure
	memset(&cl, 0, sizeof(cl));

	SZ_Clear(&cls.netchan.message);

	// clear other arrays
	memset(cl_efrags, 0, sizeof(cl_efrags));
	memset(cl_dlight_active, 0, sizeof(cl_dlight_active));
	memset(cl_lightstyle, 0, sizeof(cl_lightstyle));
	memset(cl_entities, 0, sizeof(cl_entities));

	// make sure no centerprint messages are left from previous level
	scr_centertime_off = 0;

	// allocate the efrags and chain together into a free list
	cl.free_efrags = cl_efrags;
	for (i = 0; i < MAX_EFRAGS - 1; i++)
		cl.free_efrags[i].entnext = &cl.free_efrags[i + 1];

	cl.free_efrags[i].entnext = NULL;
}

//Sends a disconnect message to the server
//This is also called on Host_Error, so it shouldn't cause any errors
void CL_Disconnect(void)
{
	byte final[10];

	connect_time = 0;
	cl.teamfortress = false;
	cls.ftexsupported = 0;
	cls.fte2supported = 0;

	VID_SetCaption("Fodquake");

	// stop sounds (especially looping!)
	S_StopAllSounds(true);

	MT_Disconnect();

	if (cls.demorecording && cls.state != ca_disconnected)
		CL_Stop_f();

	if (cls.demoplayback)
	{
		CL_StopPlayback();
	}
	else if (cls.state != ca_disconnected)
	{
		final[0] = clc_stringcmd;
		strcpy((char *)(final + 1), "drop");
		Netchan_Transmit(&cls.netchan, 6, final);
		Netchan_Transmit(&cls.netchan, 6, final);
		Netchan_Transmit(&cls.netchan, 6, final);
	}

	memset(&cls.netchan, 0, sizeof(cls.netchan));
	memset(&cls.server_adr, 0, sizeof(cls.server_adr));
	cls.state = ca_disconnected;
	VID_SetMouseGrab(0);
	connect_time = 0;

#ifdef NETQW
	if (cls.netqw)
	{
		NetQW_Delete(cls.netqw);
		cls.netqw = 0;
	}
#endif

	Cam_Reset();

	if (cls.download)
	{
		fclose(cls.download);
		cls.download = NULL;
	}

	CL_StopUpload();
	DeleteServerAliases();

	/* Restore the rate if it has been overridden by the server */
	if (strcmp(Info_ValueForKey(cls.userinfo, "rate"), rate.string) != 0)
	{
		CL_UserinfoChanged("rate", rate.string);
	}
}

void CL_Disconnect_f(void)
{
	cl.intermission = 0;
	Host_EndGame();
}

//The server is changing levels
void CL_Reconnect_f(void)
{
	if (cls.download)  // don't change when downloading
		return;

	S_StopAllSounds (true);

	if (cls.state == ca_connected)
	{
		char buf[5];

		Com_Printf ("reconnecting...\n");
#ifdef NETQW

		if (cls.netqw)
		{
			buf[0] = clc_stringcmd;
			strcpy(buf+1, "new");
			NetQW_AppendReliableBuffer(cls.netqw, buf, 5);
		}
#else
		MSG_WriteChar(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, "new");
#endif
		return;
	}

	if (!*cls.servername)
	{
		Com_Printf("No server to reconnect to.\n");
		return;
	}

	Host_EndGame();
	CL_BeginServerConnect();
}

//Responses to broadcasts, etc
void CL_ConnectionlessPacket(void)
{
	int c;
	char *s, cmdtext[2048];

	MSG_BeginReading(&cl_net_message);
	MSG_ReadLong();        // skip the -1

	c = MSG_ReadByte();

	if (msg_badread)
		return;			// runt packet

	switch(c)
	{
#ifndef NETQW
	case S2C_CHALLENGE:
		if (!NET_CompareAdr(&cl_net_from, &cls.server_adr))
			return;

		Com_Printf("%s: challenge\n", NET_AdrToString(&cl_net_from));
		cls.challenge = atoi(MSG_ReadString());

		while(1)
		{
			unsigned int extension;
			unsigned int value;

			extension = MSG_ReadLong();
			value = MSG_ReadLong();
			if (msg_badread)
				break;

			if (extension == QW_PROTOEXT_HUFF)
			{
				Com_Printf("Server supports Huffman compression\n");
				cls.hufftablecrc = value;

				cls.netchan.huffcontext = Huff_Init(cls.hufftablecrc);
				if (!cls.netchan.huffcontext)
					Com_Printf("Unknown Huffman table\n");
			}
			else if (extension == QW_PROTOEXT_FTEX)
			{
				cls.ftexsupported = FTEX_SUPPORTED&value;

				Com_Printf("FTE extensions enabled: %08x\n", cls.ftexsupported);
			}
#if 0
			else
			{
				printf("Unknown extension %08x %08x\n", extension, value);
			}
#endif
		}
		CL_SendConnectPacket();

		break;

	case S2C_CONNECTION:
		if (!NET_CompareAdr(&cl_net_from, &cls.server_adr))
			return;

		if (!com_serveractive || developer.value)
			Com_Printf("%s: connection\n", NET_AdrToString(&cl_net_from));

		if (cls.state >= ca_connected)
		{
			if (!cls.demoplayback)
				Com_Printf("Dup connect received.  Ignored.\n");
			break;
		}

		Netchan_Setup(NS_CLIENT, &cls.netchan, cl_net_from, cls.qport);
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");
		cls.state = ca_connected;
		if (!com_serveractive || developer.value)
			Com_Printf("Connected.\n");

		allowremotecmd = false; // localid required now for remote cmds

		break;
#endif

#warning This is b0rken in so many ways.
	case A2C_CLIENT_COMMAND:	// remote command from gui front end
		Com_Printf ("%s: client command\n", NET_AdrToString(&cl_net_from));

		if (!NET_IsLocalAddress(&cl_net_from))
		{
			Com_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
#ifdef _WIN32
#if 0
		ShowWindow (mainwindow, SW_RESTORE);
		SetForegroundWindow (mainwindow);
#endif
#endif
		s = MSG_ReadString();
		Q_strncpyz(cmdtext, s, sizeof(cmdtext));
		s = MSG_ReadString();

		while (*s && isspace(*s))
			s++;

		while (*s && isspace(s[strlen(s) - 1]))
			s[strlen(s) - 1] = 0;

		if (!allowremotecmd && (!*localid.string || strcmp(localid.string, s)))
		{
			if (!*localid.string)
			{
				Com_Printf("===========================\n");
				Com_Printf("Command packet received from local host, but no "
					"localid has been set.  You may need to upgrade your server "
					"browser.\n");
				Com_Printf("===========================\n");
			}
			else
			{
				Com_Printf("===========================\n");
				Com_Printf("Invalid localid on command packet received from local host. "
					"\n|%s| != |%s|\n"
					"You may need to reload your server browser and FuhQuake.\n",
					s, localid.string);
				Com_Printf("===========================\n");
				Cvar_Set(&localid, "");
			}
		}
		else
		{
			Cbuf_AddText(cmdtext);
			Cbuf_AddText("\n");
			allowremotecmd = false;
		}

		break;

	case A2C_PRINT:		// print command from somewhere
		Com_Printf("%s: print\n", NET_AdrToString(&cl_net_from));
		Com_Printf("%s", MSG_ReadString());
		break;

	case svc_disconnect:
		if (cls.demoplayback)
		{
			Com_Printf("\n======== End of demo ========\n\n");
			Host_EndGame();
			Host_Abort();
		}

		break;
	}
}

#ifdef NETQW
void CL_DoNetQWStuff()
{
	unsigned int startframe, numframes;

	NetQW_GenerateFrames(cls.netqw);
	NetQW_CopyFrames(cls.netqw, cl.frames, (unsigned int *)&cls.netchan.outgoing_sequence, &startframe, &numframes);

	while(numframes)
	{
		Cam_FinishMove(&cl.frames[startframe].cmd);

		if (cls.demorecording)
			CL_WriteDemoCmd(&cl.frames[startframe].cmd);

		startframe++;
		startframe &= UPDATE_MASK;
		numframes--;
	}
}
#endif

//Handles playback of demos, on top of NET_ code
qboolean CL_GetMessage(void)
{
#ifdef _WIN32
	CL_CheckQizmoCompletion();
#endif

	if (cls.demoplayback)
		return CL_GetDemoMessage();

#ifndef NETQW
	while(NET_GetPacket(NS_CLIENT, &cl_net_message, &cl_net_from))
	{
		if ((*(int *)cl_net_message.data != -1) && !NET_CompareAdr(&cl_net_from, &cls.netchan.remote_address))
		{
			Com_DPrintf ("%s: sequenced packet without connection\n", NET_AdrToString(&cl_net_from));
		}
		else
			return true;
	}
#endif

#ifdef NETQW
	if (cls.netqw)
	{
		unsigned int size;
		void *p;

		if (!cls.extensionsqueried)
		{
			if (NetQW_GetExtensions(cls.netqw, &cls.ftexsupported))
			{
				cls.extensionsqueried = 1;
			}
		}

		size = NetQW_GetPacketLength(cls.netqw);
		if (size)
		{
			if (size > sizeof(cl_net_message_buffer))
				size = sizeof(cl_net_message_buffer);

			p = NetQW_GetPacketData(cls.netqw);

			memcpy(cl_net_message_buffer, p, size);

			NetQW_FreePacket(cls.netqw);

			SZ_Init(&cl_net_message, cl_net_message_buffer, sizeof(cl_net_message_buffer));

			cl_net_message.cursize = size;

			if (cls.state == ca_disconnected)
				cls.state = ca_connected;

			CL_DoNetQWStuff();

			return true;
		}
	}
#endif

	return false;
}

void CL_ReadPackets(void)
{
#ifdef NETQW
	/* Check connectionless messages (such as rcon) over the non-game socket */

	while(NET_GetPacket(NS_CLIENT, &cl_net_message, &cl_net_from))
	{
		if (*(int *)cl_net_message.data == -1)
		{
			CL_ConnectionlessPacket();
		}
	}
#endif

	while (CL_GetMessage())
	{
		// remote command packet
		if (*(int *)cl_net_message.data == -1)
		{
			CL_ConnectionlessPacket();
			continue;
		}

		if (cl_net_message.cursize < 8 && !cls.mvdplayback)
		{
			Com_DPrintf ("%s: Runt packet\n", NET_AdrToString(&cl_net_from));
			continue;
		}

		if (cls.mvdplayback)
		{
			MSG_BeginReading(&cl_net_message);
		}
		else
		{
#ifdef NETQW
			MSG_BeginReading(&cl_net_message);
			cls.netchan.incoming_sequence = MSG_ReadLong() & 0x7fffffff;
			cls.netchan.incoming_acknowledged = MSG_ReadLong() & 0x7fffffff;
#else
			if (!Netchan_Process(&cls.netchan, &cl_net_message))
				continue;			// wasn't accepted for some reason
#endif
		}

		CL_ParseServerMessage ();
	}

#ifndef NETQW
	// check timeout
	if (!cls.demoplayback && cls.state >= ca_connected)
	{
		if (curtime - cls.netchan.last_received > (cl_timeout.value > 0 ? cl_timeout.value : 60))
		{
			Com_Printf("\nServer connection timed out.\n");
			Host_EndGame();
			return;
		}
	}
#endif
}

#ifndef NETQW
void CL_SendToServer(void)
{
	// when recording demos, request new ping times every cl_demoPingInterval.value seconds
	if (cls.demorecording && !cls.demoplayback && cls.state == ca_active && cl_demoPingInterval.value > 0)
	{
		if (cls.realtime - cl.last_ping_request > cl_demoPingInterval.value)
		{
			cl.last_ping_request = cls.realtime;
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			SZ_Print (&cls.netchan.message, "pings");
		}
	}

	// send intentions now
	// resend a connection request if necessary
	if (cls.state == ca_disconnected)
		CL_CheckForResend ();
	else
		CL_SendCmd ();
}
#endif

#define MAXCMDLINE 256
extern char key_lines[32][MAXCMDLINE];

void ToggleConsole_f(void)
{
	Key_ClearTyping();

	if (key_dest == key_console)
	{
		if (!SCR_NEED_CONSOLE_BACKGROUND)
		{
			key_dest = key_game;
			VID_SetMouseGrab(1);
		}
	}
	else
	{
		key_dest = key_console;
		VID_SetMouseGrab(0);
	}

	CSTC_Console_Close();

	if (con_clearnotify.value)
		Con_ClearNotify();
}

//=============================================================================

void CL_SaveArgv(int argc, char **argv)
{
	char saved_args[512];
	int i, total_length, length;
	qboolean first = true;

	total_length = saved_args[0] = 0;
	for (i = 0; i < argc; i++)
	{
		if (!argv[i][0])
			continue;

		if (!first && total_length + 1 < sizeof(saved_args))
		{
			strcat(saved_args, " ");
			total_length++;
		}
		first = false;
		length = strlen(argv[i]);
		if (total_length + length < sizeof(saved_args))
		{
			strcat(saved_args, argv[i]);
			total_length += length;
		}
	}

	Cvar_ForceSet(&cl_cmdline, saved_args);
}

#if HUFFTEST
void huff_load_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Blah!\n");
		return;
	}

	huff_loadfile(Cmd_Argv(1));
}

void huff_save_f(void)
{
	if (Cmd_Argc() != 2)
	{
		Com_Printf("Blah!\n");
		return;
	}

	huff_savefile(Cmd_Argv(1));
}
#endif

void CL_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register(&cl_parseWhiteText);
	Cvar_Register(&cl_chatsound);

	Cvar_Register(&cl_floodprot);
	Cvar_Register(&cl_fp_messages );
	Cvar_Register(&cl_fp_persecond);


	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&cl_shownet);

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&cl_sbar);
	Cvar_Register(&cl_hudswap);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEWMODEL);
	Cvar_Register(&cl_filterdrawviewmodel);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&cl_model_bobbing);
	Cvar_Register(&cl_nolerp);
	Cvar_Register(&cl_maxfps);
	Cvar_Register(&cl_deadbodyfilter);
	Cvar_Register(&cl_gibfilter);
	Cvar_Register(&cl_muzzleflash);
	Cvar_Register(&cl_rocket2grenade);
	Cvar_Register(&r_explosiontype);
	Cvar_Register(&r_lightflicker);
	Cvar_Register(&r_rockettrail);
	Cvar_Register(&r_grenadetrail);
	Cvar_Register(&r_powerupglow);
	Cvar_Register(&r_rocketlight);
	Cvar_Register(&r_explosionlight);
	Cvar_Register(&r_rocketlightcolor);
	Cvar_Register(&r_explosionlightcolor);
	Cvar_Register(&r_flagcolor);
	Cvar_Register(&cl_trueLightning);

	Cvar_SetCurrentGroup(CVAR_GROUP_DEMO);
	Cvar_Register(&cl_demospeed);
	Cvar_Register(&cl_demoPingInterval);
	Cvar_Register(&qizmo_dir);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register(&cl_staticsounds);

	Cvar_SetCurrentGroup(CVAR_GROUP_USERINFO);
	Cvar_Register(&team);
	Cvar_Register(&spectator);
	Cvar_Register(&skin);
	Cvar_Register(&rate);
	Cvar_Register(&noaim);
	Cvar_Register(&name);
	Cvar_Register(&msg);
	Cvar_Register(&topcolor);
	Cvar_Register(&bottomcolor);
	Cvar_Register(&w_switch);
	Cvar_Register(&b_switch);

	Cvar_SetCurrentGroup(CVAR_GROUP_VIEW);
	Cvar_Register(&default_fov);

	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_predictPlayers);
	Cvar_Register(&cl_solidPlayers);
	Cvar_Register(&cl_oldPL);
	Cvar_Register(&cl_timeout);
	Cvar_Register(&cl_useproxy);

	Cvar_Register(&net_maxfps);
	Cvar_Register(&net_lag);
	Cvar_Register(&net_lag_ezcheat);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&con_clearnotify);

	Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
	Cvar_Register(&password);
	Cvar_Register(&rcon_password);
	Cvar_Register(&rcon_address);
	Cvar_Register(&localid);
	Cvar_Register(&cl_warncmd);
	Cvar_Register(&cl_cmdline);

	Cvar_ResetCurrentGroup();

	Cvar_Register(&cl_confirmquit);

	Cvar_Register(&cl_imitate_client);
	Cvar_Register(&cl_imitate_os);

	Cvar_Register(&r_drawflat_enable);
	Cvar_Register(&r_drawflat_floors_ceilings);
	Cvar_Register(&r_drawflat_slopes);
	Cvar_Register(&r_drawflat_walls);

	Cmd_AddLegacyCommand("demotimescale", "cl_demospeed");

	CL_InitCommands();

#if HUFFTEST
	Cmd_AddCommand("huff_load", huff_load_f);
	Cmd_AddCommand("huff_save", huff_save_f);
#endif

	Cmd_AddCommand("r_drawflat", R_DrawFlat_f);
	Cmd_AddCommand("r_drawflat_shoot", R_DrawFlatShoot_f);
	Cmd_AddCommand("r_drawflat_shoot_unset", R_DrawFlatShootUnset_f);
	Cmd_AddCommand("r_drawflat_set", R_DrawFlat_Set_f);
	Cmd_AddCommand("r_drawflat_unset", R_DrawFlat_Unset_f);
	Cmd_AddCommand("r_drawflat_writeconfig", R_DrawFlat_Write_Config);

	Cmd_AddCommand("disconnect", CL_Disconnect_f);
	Cmd_AddCommand("connect", CL_Connect_f);

	Cmd_AddCommand("join", CL_Join_f);
	Cmd_AddCommand("observe", CL_Observe_f);

	Cmd_AddCommand("qtvplay", CL_QTVPlay_f);

	Cmd_AddCommand("dns", CL_DNS_f);

	Cmd_AddCommand("reconnect", CL_Reconnect_f);

	Cmd_AddMacro("connectiontype", CL_Macro_ConnectionType);
	Cmd_AddMacro("demoplayback", CL_Macro_Demoplayback);
	Cmd_AddMacro("matchstatus", CL_Macro_Serverstatus);

	Cmd_AddCommand("toggleconsole", ToggleConsole_f);
}

static void CL_InitClientVersionInfo()
{
	Info_RemoveKey(cls.userinfo, "*FuhQuake");
	Info_RemoveKey(cls.userinfo, "*client");
	Info_RemoveKey(cls.userinfo, "*Fodquake");

	if (imitatedclientnum == CLIENT_EZQUAKE_1144)
	{
	 	Info_SetValueForStarKey (cls.userinfo, "*client", "ezQuake 1144", MAX_INFO_STRING);
	}
	else if (imitatedclientnum == CLIENT_EZQUAKE_1517)
	{
	 	Info_SetValueForStarKey (cls.userinfo, "*client", "ezQuake 1517", MAX_INFO_STRING);
	}
	else if (imitatedclientnum == CLIENT_EZQUAKE_1_8_2)
	{
	 	Info_SetValueForStarKey (cls.userinfo, "*client", "ezQuake 2029", MAX_INFO_STRING);
	}
	else if (imitatedclientnum == CLIENT_FUHQUAKE_0_31_675)
	{
	 	Info_SetValueForStarKey (cls.userinfo, "*FuhQuake", "0.31", MAX_INFO_STRING);
	}
	else
		Info_SetValueForStarKey (cls.userinfo, "*Fodquake", FODQUAKE_VERSION, MAX_INFO_STRING);
}

void CL_Init (void)
{
	if (dedicated)
		return;

	SZ_Init(&cl_net_message, cl_net_message_buffer, sizeof(cl_net_message_buffer));

	cls.state = ca_disconnected;

	strcpy(cls.gamedirfile, com_gamedirfile);
	strcpy(cls.gamedir, com_gamedir);

	W_LoadWadFile("gfx.wad");

	FChecks_Init();

	host_basepal = (byte *) FS_LoadMallocFile("gfx/palette.lmp");
	if (!host_basepal)
		Sys_Error("Couldn't load gfx/palette.lmp");
	FMod_CheckModel("gfx/palette.lmp", host_basepal, com_filesize);

	host_colormap = (byte *) FS_LoadMallocFile ("gfx/colormap.lmp");
	if (!host_colormap)
		Sys_Error("Couldn't load gfx/colormap.lmp");
	FMod_CheckModel("gfx/colormap.lmp", host_colormap, com_filesize);

	Sys_IO_Create_Directory(va("%s/qw", com_basedir));
	Sys_IO_Create_Directory(va("%s/fodquake", com_basedir));

	Key_Init();
	V_Init();

	VID_Init(host_basepal);
	cl_vidinitialised = 1;

	Image_Init();

	S_Init();

	CDAudio_Init();

	Info_SetValueForStarKey(cls.userinfo, "*z_ext", va("%i", SUPPORTED_EXTENSIONS), MAX_SERVERINFO_STRING);

	CL_InitClientVersionInfo();

	if (!CL_InitEnts())
		Sys_Error("CL_InitEnts() failed\n");

	CL_InitTEnts();

#warning If I want to keep compatibility with Win32 server browsers, I need to open a socket on port 27001 here.

	MT_Init();
	CL_Demo_Init();
	Ignore_Init();
	Stats_Init();
	MP3_Init();

	Sleep_Init();

	M_Init();
}

//============================================================================

void CL_BeginLocalConnection(void)
{
	S_StopAllSounds(true);

	// make sure we're not connected to an external server,
	// and demo playback is stopped
	if (!com_serveractive)
		CL_Disconnect();

	cl.worldmodel = NULL;

	if (cls.state == ca_active)
		cls.state = ca_connected;
}

static double CL_MinFrameTime(void)
{
	double fps;

	if (cls.timedemo || Movie_IsCapturing())
		return 0;

	if (cls.demoplayback)
	{
		if (!cl_maxfps.value)
			return 0;
		fps = max(30.0, cl_maxfps.value);
	}
	else
	{
#ifdef NETQW
#warning This needs updating for in sync net/video.
		if (!cl_maxfps.value)
			return 0;
		fps = max(30.0, cl_maxfps.value);
#else
		double fpscap;

		fpscap = cl.maxfps ? max(30.0, cl.maxfps) : 72;

		fps = cl_maxfps.value ? bound(30.0, cl_maxfps.value, fpscap) : com_serveractive ? fpscap : bound(30.0, rate.value / 80.0, fpscap);
#endif
	}

	return 1 / fps;
}

void CL_Frame (double time)
{
	keynum_t key;
	qboolean down;
	int focuschanged;

	static double extratime = 0.001;
	double minframetime;

	extratime += time;
	minframetime = CL_MinFrameTime();

	if (extratime < minframetime)
	{
		Sleep_Sleep((unsigned int)((minframetime-extratime)*1000000));
		return;
	}

#if 0
	cls.trueframetime = extratime - 0.001;
	cls.trueframetime = max(cls.trueframetime, minframetime);
	extratime -= cls.trueframetime;
#else
	cls.trueframetime = minframetime;
	extratime -= minframetime;

	if (extratime > minframetime)
	{
/*		Com_Printf("Dropping frame: %.4f\n", extratime);*/
		cls.trueframetime+= extratime-minframetime;
		extratime = minframetime;
	}
#endif

	cls.framedev = extratime;

	/* Ugliest hack ever */
	movementkey = rand();

	if (Movie_IsCapturing())
		cls.frametime = Movie_StartFrame();
	else
		cls.frametime = min(0.2, cls.trueframetime);

	if (cls.demoplayback) {
		if (cl.paused & PAUSED_DEMO)
			cls.frametime = 0;
		else if (!cls.timedemo)
			cls.frametime *= bound(0, cl_demospeed.value, 20);

		if (!host_skipframe)
			cls.demotime += cls.frametime;
		host_skipframe = false;
	}

#ifdef NETQW
	cls.realtime = Sys_DoubleTime();
#else
	cls.realtime += cls.frametime;
#endif

	if (!cl.paused) {
		cl.time += cls.frametime;
		cl.servertime += cls.frametime;
		cl.stats[STAT_TIME] = (int) (cl.servertime * 1000);
		cl.gametime += cls.frametime;
	}

	if (cls.demoplayback)
	{
		cl.gametime = cls.realactualdemotime - cls.demotimeoffset;
	}

	focuschanged = VID_FocusChanged();

	M_PerFramePreRender();

	while(VID_GetKeyEvent(&key, &down))
		Key_Event(key, down);

	if (focuschanged)
		Key_ClearStates();

	// process console commands
	Cbuf_Execute();

#ifndef CLIENTONLY
	if (com_serveractive)
		SV_Frame(cls.frametime);
#endif

	// fetch results from server
	CL_ReadPackets();

	if (cls.mvdplayback)
		MVD_Interpolate();

	// process stuffed commands
	Cbuf_ExecuteEx(cbuf_svc);

	if (!cls.demoplayback || cls.mvdplayback)
		Mouse_GetViewAngles(cl.viewangles);

#ifdef NETQW
	if (cls.netqw)
	{
		CL_DoNetQWStuff();

		if (cl.spectator)
		{
			usercmd_t cmd;

#warning Get rid of the cmd arg.
			Cam_Track(&cmd);
		}
	}
	else if (cls.mvdplayback)
	{
		usercmd_t *cmd;

		cmd = &cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].cmd;
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].senttime = cls.realtime;
		cl.frames[cls.netchan.outgoing_sequence & UPDATE_MASK].receivedtime = -1;
		CL_BaseMove(cmd);
		// if we are spectator, try autocam
		if (cl.spectator)
		{
			Cam_Track(cmd);
		}
		CL_FinishMove(cmd);
		Cam_FinishMove(cmd);
		cls.netchan.outgoing_sequence++;
	}

#else
	CL_SendToServer();
#endif

	TP_Frame();

	if (cls.state >= ca_onserver) {	// !!! Tonik
		Cam_SetViewPlayer();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(false);

		// do client side motion prediction
		CL_PredictMove();

		// Set up prediction for other players
		CL_SetUpPlayerPrediction(true);

		// build a refresh entity list
		CL_EmitEntities();
	}

	// update video
	SCR_UpdateScreen();

	CL_DecayLights();

	// update audio
	if (cls.state == ca_active)
		S_Update(r_origin, vpn, vright, vup);
	else
		S_Update(vec3_origin, vec3_origin, vec3_origin, vec3_origin);

	CDAudio_Update();
	MP3_Frame();
	MT_Frame();
	Lua_Frame();

	if (Movie_IsCapturing())
		Movie_FinishFrame();

	cls.framecount++;
	fps_count++;
}

//============================================================================

void CL_Shutdown (void)
{
	M_Shutdown();

	CL_Disconnect();

	CL_WriteConfiguration();

	CL_ShutdownEnts();
	Skin_Shutdown();
	CDAudio_Shutdown();
	S_Shutdown();
	MP3_Shutdown();
	TP_Shutdown();
	Log_Shutdown();
	if (cl_vidinitialised)
		VID_Shutdown();

	free(host_basepal);
	free(host_colormap);

	W_UnloadWadFile();
}

