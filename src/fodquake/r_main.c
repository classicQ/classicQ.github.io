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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "r_local.h"
#include "sound.h"

void		*colormap;
float		r_time1;
int			r_numallocatededges;
float		r_aliasuvscale = 1.0;
int			r_outofsurfaces;
int			r_outofedges;

qboolean	r_dowarp, r_dowarpold, r_viewchanged;

mvertex_t	*r_pcurrentvertbase;

int			c_surf;
int			r_maxsurfsseen, r_maxedgesseen, r_cnumsurfs;
int			r_clipflags;

byte		*r_warpbuffer;

entity_t	r_worldentity;

// view origin
vec3_t	vup, base_vup;
vec3_t	vpn, base_vpn;
vec3_t	vright, base_vright;
vec3_t	r_origin;

// screen size info
refdef_t	r_refdef;
float		xcenter, ycenter;
float		xscale, yscale;
float		xscaleinv, yscaleinv;
float		xscaleshrink, yscaleshrink;
float		aliasxscale, aliasyscale, aliasxcenter, aliasycenter;

int		screenwidth;

float	pixelAspect;
float	screenAspect;
float	verticalFieldOfView;
float	xOrigin, yOrigin;

mplane_t	screenedge[4];

// refresh flags
int		r_framecount = 1;	// so frame counts initialized to 0 don't match
int		r_visframecount;
int		d_spanpixcount;
int		r_polycount;
int		r_drawnpolycount;
int		r_wholepolycount;

int			*pfrustum_indexes[4];
int			r_frustum_indexes[4*6];

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

float		r_aliastransition, r_resfudge;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

float	dp_time1, dp_time2, db_time1, db_time2, rw_time1, rw_time2;
float	se_time1, se_time2, de_time1, de_time2, dv_time1, dv_time2;

cvar_t	r_draworder = {"r_draworder", "0"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_timegraph = {"r_timegraph", "0"};
cvar_t	r_netgraph = {"r_netgraph", "0"};
cvar_t	r_zgraph = {"r_zgraph", "0"};
cvar_t	r_graphheight = {"r_graphheight", "15"};
cvar_t	r_clearcolor = {"r_clearcolor", "2"};
cvar_t	r_skycolor = {"r_skycolor", "4"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_waterwarp = {"r_waterwarp", "1"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_lerpframes = {"r_lerpframes", "1"};
cvar_t	r_lerpmuzzlehack = {"r_lerpmuzzlehack", "1"};
cvar_t	r_drawflame = {"r_drawflame", "1"};
cvar_t	r_aliasstats = {"r_polymodelstats", "0"};
cvar_t	r_dspeeds = {"r_dspeeds", "0"};
cvar_t	r_ambient = {"r_ambient", "0"};
cvar_t	r_reportsurfout = {"r_reportsurfout", "0"};
cvar_t	r_maxsurfs = {"r_maxsurfs", "0"};
cvar_t	r_numsurfs = {"r_numsurfs", "0"};
cvar_t	r_reportedgeout = {"r_reportedgeout", "0"};
cvar_t	r_maxedges = {"r_maxedges", "0"};
cvar_t	r_numedges = {"r_numedges", "0"};
cvar_t	r_aliastransbase = {"r_aliastransbase", "200"};
cvar_t	r_aliastransadj = {"r_aliastransadj", "100"};
cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "1"};
cvar_t	r_fastturb = {"r_fastturb", "0"};
cvar_t  r_max_size_1 = {"r_max_size_1", "0"};

extern cvar_t	scr_fov;

void R_NetGraph (void);
void R_ZGraph (void);

static int R_InitTextures(void)
{
	int x,y, m;
	byte *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = malloc(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2);
	if (r_notexture_mip)
	{
		strcpy(r_notexture_mip->name, "notexture");
		r_notexture_mip->width = r_notexture_mip->height = 16;
		r_notexture_mip->offsets[0] = sizeof(texture_t);
		r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
		r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
		r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

		for (m = 0; m < 4; m++)
		{
			dest = (byte *  )r_notexture_mip + r_notexture_mip->offsets[m];
			for (y = 0; y < (16 >> m); y++)
			{
				for (x = 0; x < (16 >> m); x++)
				{
					if ((y < (8 >> m)) ^ (x < (8 >> m)))
						*dest++ = 0;
					else
						*dest++ = 0x0e;
				}
			}
		}

		return 1;
	}

	return 0;
}

static void R_ShutdownTextures()
{
	free(r_notexture_mip);
}

void R_CvarInit(void)
{
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_netgraph);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_fastturb);
	Cvar_Register (&r_waterwarp);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_ambient);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&r_max_size_1);

	Cvar_SetCurrentGroup(CVAR_GROUP_SOFTWARE);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_draworder);
	Cvar_Register (&r_timegraph);
	Cvar_Register (&r_zgraph);
	Cvar_Register (&r_graphheight);
	Cvar_Register (&r_clearcolor);
	Cvar_Register (&r_aliasstats);
	Cvar_Register (&r_dspeeds);
	Cvar_Register (&r_reportsurfout);
	Cvar_Register (&r_maxsurfs);
	Cvar_Register (&r_numsurfs);
	Cvar_Register (&r_reportedgeout);
	Cvar_Register (&r_maxedges);
	Cvar_Register (&r_numedges);
	Cvar_Register (&r_aliastransbase);
	Cvar_Register (&r_aliastransadj);

	Cvar_ResetCurrentGroup();

	Cvar_SetValue (&r_maxedges, (float) NUMSTACKEDGES);
	Cvar_SetValue (&r_maxsurfs, (float) NUMSTACKSURFACES);
}

static void R_InitTurb(void)
{
	int i;

	for (i = 0; i < MAXWIDTH + CYCLE; i++)
	{
		sintable[i] = AMP + sin(i * M_PI * 2 / CYCLE) * AMP;
		intsintable[i] = AMP2 + sin(i * M_PI * 2 / CYCLE) * AMP2;	// AMP2, not 20
	}
}

static void *surfacememory;

int R_Init(void)
{
	int dummy;

	R_InitTurb ();

	view_clipplanes[0].leftedge = true;
	view_clipplanes[1].rightedge = true;
	view_clipplanes[1].leftedge = view_clipplanes[2].leftedge = view_clipplanes[3].leftedge = false;
	view_clipplanes[0].rightedge = view_clipplanes[2].rightedge = view_clipplanes[3].rightedge = false;

	r_refdef.xOrigin = XCENTERING;
	r_refdef.yOrigin = YCENTERING;

	r_numallocatededges = r_maxedges.value;

	if (r_numallocatededges < MINEDGES)
		r_numallocatededges = MINEDGES;

	auxedges = malloc(r_numallocatededges * sizeof(*auxedges) + (CACHE_SIZE - 1));
	if (auxedges)
	{
		r_cnumsurfs = r_maxsurfs.value;

		if (r_cnumsurfs <= MINSURFACES)
			r_cnumsurfs = MINSURFACES;

		surfacememory = malloc(r_cnumsurfs * sizeof(struct surf) + (CACHE_SIZE - 1));
		if (surfacememory)
		{
			surfaces = (void *)((((long)surfacememory) + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));
			surf_cur = 1;
			surf_max = r_cnumsurfs;
			// surface 0 doesn't really exist; it's just a dummy because index 0
			// is used to indicate no edge attached to surface
			surfaces--;
#ifdef id386
			R_SurfacePatch ();
#endif
			if (R_InitTextures())
			{
				if (R_InitParticles())
				{
// TODO: collect 386-specific code in one place
#if	id386
					Sys_MakeCodeWriteable ((long) R_EdgeCodeStart, (long) R_EdgeCodeEnd - (long) R_EdgeCodeStart);
#endif	// id386

					D_Init ();

					return 1;
				}

				R_ShutdownTextures();
			}

			free(surfacememory);
		}

		free(auxedges);
	}

	Sys_Error("R_Init() failed.");

	return 0;
}

void R_Shutdown()
{
	R_ShutdownParticles();
	R_ShutdownTextures();
	free(surfacememory);
	free(auxedges);
}

void R_PreMapLoad()
{
}

void R_NewMap(void)
{
	int i;

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	r_maxedgesseen = 0;
	r_maxsurfsseen = 0;

	r_dowarpold = false;
	r_viewchanged = false;
}

void R_SetVrect(vrect_t *pvrectin, vrect_t *pvrect, int lineadj)
{
	int h;
	float size;
	qboolean full = false;

	if (scr_viewsize.value >= 100.0)
	{
		size = 100.0;
		full = true;
	}
	else
	{
		size = scr_viewsize.value;
	}

	if (cl.intermission)
	{
		full = true;
		size = 100.0;
		lineadj = 0;
	}
	size /= 100.0;

	h = (!cl_sbar.value && full) ? pvrectin->height : pvrectin->height - lineadj;

	pvrect->width = full ? pvrectin->width : pvrectin->width * size;

	if (pvrect->width < 96)
	{
		size = 96.0 / pvrectin->width;
		pvrect->width = 96;	// min for icons
	}
	pvrect->width &= ~7;
	pvrect->height = pvrectin->height * size;
	if (cl_sbar.value || !full)
	{
		if (pvrect->height > pvrectin->height - lineadj)
			pvrect->height = pvrectin->height - lineadj;
	}
	else if (pvrect->height > pvrectin->height)
	{
			pvrect->height = pvrectin->height;
	}

	pvrect->height &= ~1;

	pvrect->x = (pvrectin->width - pvrect->width) / 2;
	if (full)
		pvrect->y = 0;
	else
		pvrect->y = (h - pvrect->height) / 2;
}

//Called every time the vid structure or r_refdef changes.
//Guaranteed to be called before the first refresh
void R_ViewChanged(vrect_t *pvrect, int lineadj, float aspect)
{
	int i;
	float res_scale;

	r_viewchanged = true;

	R_SetVrect (pvrect, &r_refdef.vrect, lineadj);

	r_refdef.horizontalFieldOfView = 2.0 * tan (r_refdef.fov_x / 360 * M_PI);
	r_refdef.fvrectx = (float) r_refdef.vrect.x;
	r_refdef.fvrectx_adj = (float) r_refdef.vrect.x - 0.5;
	r_refdef.vrect_x_adj_shift20 = (r_refdef.vrect.x << 20) + (1 << 19) - 1;
	r_refdef.fvrecty = (float) r_refdef.vrect.y;
	r_refdef.fvrecty_adj = (float) r_refdef.vrect.y - 0.5;
	r_refdef.vrectright = r_refdef.vrect.x + r_refdef.vrect.width;
	r_refdef.vrectright_adj_shift20 = (r_refdef.vrectright << 20) + (1 << 19) - 1;
	r_refdef.fvrectright = (float) r_refdef.vrectright;
	r_refdef.fvrectright_adj = (float) r_refdef.vrectright - 0.5;
	r_refdef.vrectrightedge = (float) r_refdef.vrectright - 0.99;
	r_refdef.vrectbottom = r_refdef.vrect.y + r_refdef.vrect.height;
	r_refdef.fvrectbottom = (float) r_refdef.vrectbottom;
	r_refdef.fvrectbottom_adj = (float) r_refdef.vrectbottom - 0.5;

	r_refdef.aliasvrect.x = (int) (r_refdef.vrect.x * r_aliasuvscale);
	r_refdef.aliasvrect.y = (int) (r_refdef.vrect.y * r_aliasuvscale);
	r_refdef.aliasvrect.width = (int) (r_refdef.vrect.width * r_aliasuvscale);
	r_refdef.aliasvrect.height = (int) (r_refdef.vrect.height * r_aliasuvscale);
	r_refdef.aliasvrectright = r_refdef.aliasvrect.x + r_refdef.aliasvrect.width;
	r_refdef.aliasvrectbottom = r_refdef.aliasvrect.y + r_refdef.aliasvrect.height;

	pixelAspect = aspect;
	xOrigin = r_refdef.xOrigin;
	yOrigin = r_refdef.yOrigin;

	screenAspect = r_refdef.vrect.width*pixelAspect /
			r_refdef.vrect.height;
	// 320 * 200 1.0 pixelAspect = 1.6 screenAspect
	// 320 * 240 1.0 pixelAspect = 1.3333 screenAspect
	// proper 320 * 200 pixelAspect = 0.8333333

	verticalFieldOfView = r_refdef.horizontalFieldOfView / screenAspect;

	// values for perspective projection
	// if math were exact, the values would range from 0.5 to to range+0.5
	// hopefully they wll be in the 0.000001 to range+.999999 and truncate
	// the polygon rasterization will never render in the first row or column
	// but will definately render in the [range] row and column, so adjust the
	// buffer origin to get an exact edge to edge fill
	xcenter = ((float)r_refdef.vrect.width * XCENTERING) + r_refdef.vrect.x - 0.5;
	aliasxcenter = xcenter * r_aliasuvscale;
	ycenter = ((float)r_refdef.vrect.height * YCENTERING) + r_refdef.vrect.y - 0.5;
	aliasycenter = ycenter * r_aliasuvscale;

	xscale = r_refdef.vrect.width / r_refdef.horizontalFieldOfView;
	aliasxscale = xscale * r_aliasuvscale;
	xscaleinv = 1.0 / xscale;
	yscale = xscale * pixelAspect;
	aliasyscale = yscale * r_aliasuvscale;
	yscaleinv = 1.0 / yscale;
	xscaleshrink = (r_refdef.vrect.width - 6) / r_refdef.horizontalFieldOfView;
	yscaleshrink = xscaleshrink*pixelAspect;

	// left side clip
	screenedge[0].normal[0] = -1.0 / (xOrigin * r_refdef.horizontalFieldOfView);
	screenedge[0].normal[1] = 0;
	screenedge[0].normal[2] = 1;
	screenedge[0].type = PLANE_ANYZ;

	// right side clip
	screenedge[1].normal[0] =
			1.0 / ((1.0 - xOrigin) * r_refdef.horizontalFieldOfView);
	screenedge[1].normal[1] = 0;
	screenedge[1].normal[2] = 1;
	screenedge[1].type = PLANE_ANYZ;

	// top side clip
	screenedge[2].normal[0] = 0;
	screenedge[2].normal[1] = -1.0 / (yOrigin * verticalFieldOfView);
	screenedge[2].normal[2] = 1;
	screenedge[2].type = PLANE_ANYZ;

	// bottom side clip
	screenedge[3].normal[0] = 0;
	screenedge[3].normal[1] = 1.0 / ((1.0 - yOrigin) * verticalFieldOfView);
	screenedge[3].normal[2] = 1;
	screenedge[3].type = PLANE_ANYZ;

	for (i = 0; i < 4; i++)
		VectorNormalize (screenedge[i].normal);

	res_scale = sqrt ((double)(r_refdef.vrect.width * r_refdef.vrect.height) /
			          (320.0 * 152.0)) *
			(2.0 / r_refdef.horizontalFieldOfView);
	r_aliastransition = r_aliastransbase.value * res_scale;
	r_resfudge = r_aliastransadj.value * res_scale;

// TODO: collect 386-specific code in one place
#if id386
	Sys_MakeCodeWriteable ((long)R_Surf8Start,
		(long)R_Surf8End - (long)R_Surf8Start);
	colormap = vid.colormap;
	R_Surf8Patch ();
#endif	// id386

	D_ViewChanged ();
}

static void R_MarkLeaves(void)
{
	byte *vis;
	mnode_t *node;
	int i;

	if (r_oldviewleaf == r_viewleaf)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *) &cl.worldmodel->leafs[i+1];
			while(1)
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				if (node->parentnum == 0xffff)
					break;
				node = NODENUM_TO_NODE(cl.worldmodel, node->parentnum);
			}
		}
	}
}

static void R_DrawEntitiesOnList(visentlist_t *vislist)
{
	int i;

	if (!r_drawentities.value || !vislist->count)
		return;

	for (i = 0; i < vislist->count; i++)
	{
		currententity = &vislist->list[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSprite ();
			break;
		case mod_alias:
			R_AliasDrawModel (currententity);
			break;
		}
	}
}

static void R_DrawViewModel(void)
{
	centity_t *cent;
	static entity_t gun;

	if (!r_drawentities.value)
		return;

	if (!r_drawentities.value || !cl.viewent.current.modelindex)
		return;

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex]))
		Host_Error ("R_DrawViewModel: bad modelindex");

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.flags = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value)
	{
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
			cent->current.modelindex != cl_modelindices[mi_vbio] &&
			cent->current.modelindex != cl_modelindices[mi_vgrap] &&
			cent->current.modelindex != cl_modelindices[mi_vknife] &&
			cent->current.modelindex != cl_modelindices[mi_vknife2] &&
			cent->current.modelindex != cl_modelindices[mi_vmedi] &&
			cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			extern float r_lerpdistance;
			gun.flags |= RF_LIMITLERP;
			r_lerpdistance =  135;
		}
	}


	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= cl.time)
	{
		gun.oldframe = cent->oldframe;
		gun.framelerp = (cl.time - cent->frametime) * 10;
	}
	else
	{
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}


	R_AliasDrawModel (currententity);
}

static int R_BmodelCheckBBox(model_t *clmodel, float *minmaxs)
{
	int i, *pindex, clipflags;
	vec3_t acceptpt, rejectpt;
	double d;

	clipflags = 0;

	if (currententity->angles[0] || currententity->angles[1] || currententity->angles[2])
	{
		for (i = 0; i < 4; i++)
		{
			d = DotProduct (currententity->origin, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= -clmodel->radius)
				return BMODEL_FULLY_CLIPPED;

			if (d <= clmodel->radius)
				clipflags |= (1 << i);
		}
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			// generate accept and reject points
			// FIXME: do with fast look-ups or integer tests based on the sign bit
			// of the floating point values

			pindex = pfrustum_indexes[i];

			rejectpt[0] = minmaxs[pindex[0]];
			rejectpt[1] = minmaxs[pindex[1]];
			rejectpt[2] = minmaxs[pindex[2]];

			d = DotProduct (rejectpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				return BMODEL_FULLY_CLIPPED;

			acceptpt[0] = minmaxs[pindex[3 + 0]];
			acceptpt[1] = minmaxs[pindex[3 + 1]];
			acceptpt[2] = minmaxs[pindex[3 + 2]];

			d = DotProduct (acceptpt, view_clipplanes[i].normal);
			d -= view_clipplanes[i].dist;

			if (d <= 0)
				clipflags |= (1 << i);
		}
	}

	return clipflags;
}

static unsigned int R_FindTopNode(vec3_t mins, vec3_t maxs)
{
	mplane_t *splitplane;
	int sides;
	unsigned int nodenum;
	mnode_t *node;
	model_t *model;
	unsigned int leafnum;

	model = cl.worldmodel;
	nodenum = 0;

	while (1)
	{
		node = NODENUM_TO_NODE(model, nodenum);

		if (node->visframe != r_visframecount)
			return 0xffff;		// not visible at all

		if (nodenum >= model->numnodes)
		{
			leafnum = nodenum - model->numnodes;
			if (!(model->leafsolid[leafnum/32] & (1<<(leafnum%32))))
				return nodenum; // we've reached a non-solid leaf, so it's
							//  visible and not BSP clipped
			return 0xffff;	// in solid, so not visible
		}

		splitplane = model->planes + node->planenum;
		sides = BOX_ON_PLANE_SIDE (mins, maxs, splitplane);

		if (sides == 3)
			return nodenum;	// this is the splitter

		// not split yet; recurse down the contacted side
		nodenum = (sides & 1) ? node->childrennum[0] : node->childrennum[1];
	}
}

static void R_DrawBEntitiesOnList(visentlist_t *vislist)
{
	int i, k, clipflags;
	unsigned int li;
	unsigned int lj;
	vec3_t oldorigin;
	model_t *clmodel;
	float minmaxs[6];
	unsigned int topnodenum;
	mnode_t *topnode;

	if (!r_drawentities.value || !vislist->count)
		return;

	VectorCopy (modelorg, oldorigin);
	insubmodel = true;
	r_dlightframecount = r_framecount;

	for (i = 0; i < vislist->count; i++)
	{
		currententity = &vislist->list[i];
		clmodel = currententity->model;

		// see if the bounding box lets us trivially reject, also sets trivial accept status
		VectorAdd (clmodel->mins, currententity->origin, minmaxs);
		VectorAdd (clmodel->maxs, currententity->origin, minmaxs + 3);

		clipflags = R_BmodelCheckBBox (clmodel, minmaxs);

		if (clipflags == BMODEL_FULLY_CLIPPED)
			continue;		// off the edge of the screen

		if ((topnodenum = R_FindTopNode(minmaxs, minmaxs + 3)) == 0xffff)
			continue;	// no part in a visible leaf

		VectorCopy (currententity->origin, r_entorigin);
		VectorSubtract (r_origin, r_entorigin, modelorg);

		r_pcurrentvertbase = clmodel->vertexes;

		// FIXME: stop transforming twice
		R_RotateBmodel ();

		// calculate dynamic lighting for bmodel if it's not an instanced model
		if (clmodel->firstmodelsurface != 0)
		{
			for(li=0;li<MAX_DLIGHTS/32;li++)
			{
				if (cl_dlight_active[li])
				{
					for(lj=0;lj<32;lj++)
					{
						if ((cl_dlight_active[li]&(1<<lj)) && li*32+lj < MAX_DLIGHTS)
						{
							k = li*32 + lj;

							/* This will fail for k >= 32 */
							R_MarkLights(clmodel, &cl_dlights[k], 1<<k, clmodel->hulls[0].firstclipnode);
						}
					}
				}
			}
		}

		topnode = NODENUM_TO_NODE(cl.worldmodel, topnodenum);
		currententity->topnode = topnode;

		if (topnodenum < cl.worldmodel->numnodes)
		{
			// not a leaf; has to be clipped to the world BSP
			r_clipflags = clipflags;
			R_DrawSolidClippedSubmodelPolygons (clmodel);
		}
		else
		{
			// falls entirely in one leaf, so we just put all the
			// edges in the edge list and let 1/z sorting handle drawing order
			R_DrawSubmodelPolygons (clmodel, clipflags);
		}

		currententity->topnode = NULL;

		// put back world rotation and frustum clipping
		// FIXME: R_RotateBmodel should just work off base_vxx
		VectorCopy (base_vpn, vpn);
		VectorCopy (base_vup, vup);
		VectorCopy (base_vright, vright);
		VectorCopy (base_modelorg, modelorg);
		VectorCopy (oldorigin, modelorg);
		R_TransformFrustum ();
	}

	insubmodel = false;
}

static void R_EdgeDrawing(void)
{
	r_edges = (void *)((((long)auxedges) + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));

	R_BeginEdgeFrame ();

	if (r_dspeeds.value)
		rw_time1 = Sys_DoubleTime ();

	R_RenderWorld ();

	if (r_dspeeds.value)
	{
		rw_time2 = Sys_DoubleTime ();
		db_time1 = rw_time2;
	}

	R_DrawBEntitiesOnList (&cl_visbents);

	if (r_dspeeds.value)
	{
		db_time2 = Sys_DoubleTime ();
		se_time1 = db_time2;
	}

	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	R_ScanEdges ();
}

//r_refdef must be set before the first call
void R_RenderView(void)
{
	byte warpbuffer[WARP_WIDTH * WARP_HEIGHT];

	r_warpbuffer = warpbuffer;

	if (r_timegraph.value || r_speeds.value || r_dspeeds.value)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();

	R_MarkLeaves ();	// done here so we know if we're in water

	// make FDIV fast. This reduces timing precision after we've been running for a
	// while, so we don't do it globally.  This also sets chop mode, and we do it
	// here so that setup stuff like the refresh area calculations match what's done in screen.c
#ifdef id386
	Sys_LowFPPrecision ();
#endif

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	R_EdgeDrawing ();

	if (!r_dspeeds.value)
	{
		VID_UnlockBuffer ();
		S_ExtraUpdate ();	// don't let sound get messed up if going slow
		VID_LockBuffer ();
	}

	if (r_dspeeds.value)
	{
		se_time2 = Sys_DoubleTime ();
		de_time1 = se_time2;
	}

	R_DrawEntitiesOnList (&cl_visents);

	if (r_dspeeds.value)
	{
		de_time2 = Sys_DoubleTime ();
		dv_time1 = de_time2;
	}

	R_DrawViewModel ();

	if (r_dspeeds.value)
	{
		dv_time2 = Sys_DoubleTime ();
		dp_time1 = Sys_DoubleTime ();
	}

	R_DrawParticles ();

	if (r_dspeeds.value)
		dp_time2 = Sys_DoubleTime ();

	if (r_dowarp)
		D_WarpScreen ();

	V_SetContentsColor (r_viewleaf->contents);

	if (r_timegraph.value)
		R_TimeGraph ();

	if (r_netgraph.value)
		R_NetGraph ();

	if (r_zgraph.value)
		R_ZGraph ();

	if (r_aliasstats.value)
		R_PrintAliasStats ();

	if (r_speeds.value)
		R_PrintTimes ();

	if (r_dspeeds.value)
		R_PrintDSpeeds ();

	if (r_reportsurfout.value && r_outofsurfaces)
		Com_Printf ("Short %d surfaces\n", r_outofsurfaces);

	if (r_reportedgeout.value && r_outofedges)
		Com_Printf ("Short roughly %d edges\n", r_outofedges * 2 / 3);

	// back to high floating-point precision
#ifdef id386
	Sys_HighFPPrecision ();
#endif
}

