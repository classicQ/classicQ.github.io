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
// d_iface.h: interface header file for rasterization driver modules

#define WARP_WIDTH		320
#define WARP_HEIGHT		200

#define MAX_LBM_HEIGHT	480

typedef struct {
	float	u, v;
	float	s, t;
	float	zi;
} emitpoint_t;

typedef enum {
	pt_static, pt_grav, pt_slowgrav, pt_fire, pt_explode, pt_explode2, pt_blob, pt_blob2
} ptype_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct particle_s
{
// driver-usable fields
	vec3_t		org;
	float		color;
// drivers never touch the following fields
	struct particle_s	*next;
	vec3_t		vel;
	float		ramp;
	float		die;
	ptype_t		type;
} particle_t;

#define PARTICLE_Z_CLIP	8.0

typedef struct polyvert_s {
	float	u, v, zi, s, t;
} polyvert_t;

typedef struct polydesc_s {
	int			numverts;
	float		nearzi;
	msurface_t	*pcurrentface;
	polyvert_t	*pverts;
} polydesc_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct finalvert_s {
	int		v[6];		// u, v, s, t, l, 1/z
	int		flags;
	float	reserved;
} finalvert_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct {
	void				*pskin;
	unsigned int colour;
	maliasskindesc_t	*pskindesc;
	int					skinwidth;
	int					skinheight;
	mtriangle_t			*ptriangles;
	finalvert_t			*pfinalverts;
	int					numtriangles;
	int					drawtype;
	int					seamfixupX16;
} affinetridesc_t;

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
typedef struct {
	float	u, v, zi, color;
} screenpart_t;

typedef struct {
	int			nump;
	emitpoint_t	*pverts;	// there's room for an extra element at [nump], 
							//  if the driver wants to duplicate element [0] at
							//  element [nump] to avoid dealing with wrapping
	mspriteframe_t	*pspriteframe;
	vec3_t			vup, vright, vpn;	// in worldspace
	float			nearzi;
} spritedesc_t;

typedef struct {
	int		u, v;
	float	zi;
	int		color;
} zpointdesc_t;

extern int		d_spanpixcount;
extern int		r_framecount;		// sequence # of current frame since Quake
									//  started
extern float	r_aliasuvscale;		// scale-up factor for screen u and v
									//  on Alias vertices passed to driver
extern qboolean	r_dowarp;

extern affinetridesc_t	r_affinetridesc;
extern spritedesc_t		r_spritedesc;
extern zpointdesc_t		r_zpointdesc;
extern polydesc_t		r_polydesc;

#if 0
extern vec3_t	r_pright, r_pup, r_ppn;
#endif


void D_Aff8Patch (void *pcolormap);
void D_DisableBackBufferAccess (void);
void D_PolysetDraw (void);
void D_PolysetDrawFinalSkinVerts(finalvert_t *fv, int numverts);
void D_PolysetDrawFinalSolidColourVerts(finalvert_t *fv, int numverts, unsigned char colour);
void D_DrawParticleBegin(void);
void D_DrawParticle (particle_t *pparticle);
void D_DrawSprite (void);
void D_DrawSurfaces (void);
void D_DrawZPoint (void);
void D_EnableBackBufferAccess (void);
void D_CvarInit(void);
void D_Init (void);
void D_ViewChanged (void);
void D_SetupFrame (void);
void D_WarpScreen (void);

void D_DrawRect (void);

// currently for internal use only, and should be a do-nothing function in
// hardware drivers
// FIXME: this should go away
void D_PolysetUpdateTables (void);

// these are currently for internal use only, and should not be used by drivers
extern byte				*r_skysource;

// transparency types for D_DrawRect ()
#define DR_SOLID		0
#define DR_TRANSPARENT	1

// !!! must be kept the same as in quakeasm.h !!!
#define TRANSPARENT_COLOR	0xFF

extern void *acolormap;	// FIXME: should go away

//=======================================================================//

// callbacks to Quake

typedef struct {
	pixel_t		*surfdat;	// destination for generated surface
	int			rowbytes;	// destination logical width in bytes
	msurface_t	*surf;		// description for surface to generate
	fixed8_t	lightadj[MAXLIGHTMAPS];
							// adjust for lightmap levels for dynamic lighting
	texture_t	*texture;	// corrected for animating textures
	int			surfmip;	// mipmapped ratio of surface texels / world pixels
	int			surfwidth;	// in mipmapped texels
	int			surfheight;	// in mipmapped texels
	qboolean	dlightonly;	// only have to recalc dynamic lights
} drawsurf_t;

extern drawsurf_t	r_drawsurf;

qboolean R_DrawSurface (void);

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define TURB_TEX_SIZE	64		// base turbulent texture size

// !!! if this is changed, it must be changed in d_ifacea.h too !!!
#define	CYCLE			128		// turbulent cycle size

#define SKYSHIFT		7
#define	SKYSIZE			(1 << SKYSHIFT)
#define SKYMASK			(SKYSIZE - 1)

extern float	skyspeed, skyspeed2;
extern float	skyshift;

extern int		c_surf;
extern vrect_t	scr_vrect;

extern byte		*r_warpbuffer;
