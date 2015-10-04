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

#ifdef FOD_PPC
#include <altivec.h>
#endif

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"
#include "gl_warp.h"
#include "gl_rsurf.h"
#include "gl_shader.h"
#include "gl_skinimp.h"
#include "skin.h"
#include "sound.h"
#include "utils.h"

static char gl_initialised;

extern int gl_max_size_default;

int vbo_number;

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			playerfbtextures[MAX_CLIENTS];
int			skyboxtextures;
int			underwatertexture, detailtexture;

// view origin
vec3_t		vup, vpn, vright;
vec3_t		r_origin;

// screen size info
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;
mleaf_t		*r_viewleaf2, *r_oldviewleaf2;	// for watervis hack

texture_t	*r_notexture_mip;

int			d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_drawentities = {"r_drawentities", "1"};
cvar_t	r_lerpframes = {"r_lerpframes", "1"};
cvar_t	r_lerpmuzzlehack = {"r_lerpmuzzlehack", "1"};
cvar_t	r_drawflame = {"r_drawflame", "1"};
cvar_t	r_speeds = {"r_speeds", "0"};
cvar_t	r_fullbright = {"r_fullbright", "0"};
cvar_t	r_lightmap = {"r_lightmap", "0"};
cvar_t	gl_shaftlight = {"gl_shaftlight", "1"};
cvar_t	r_wateralpha = {"r_wateralpha", "1"};
cvar_t  r_fastturb = {"r_fastturb", "0"};
cvar_t	r_dynamic = {"r_dynamic", "1"};
cvar_t	r_novis = {"r_novis", "0"};
cvar_t	r_netgraph = {"r_netgraph", "0"};
cvar_t	r_fullbrightSkins = {"r_fullbrightSkins", "0"};
cvar_t	r_fastsky = {"r_fastsky", "0"};
cvar_t	r_skycolor = {"r_skycolor", "4"};
cvar_t	gl_colorlights		= {"gl_colorlights", "1"};

cvar_t	r_farclip			= {"r_farclip", "4096"};
cvar_t	gl_detail			= {"gl_detail","0"};
cvar_t	gl_caustics			= {"gl_caustics", "0"};

cvar_t	gl_subdivide_size = {"gl_subdivide_size", "128", CVAR_ARCHIVE};
cvar_t	gl_clear = {"gl_clear", "0"};
static qboolean OnChange_gl_clearColor(cvar_t *v, char *s);
cvar_t	gl_clearColor = {"gl_clearColor", "0 0 0", 0, OnChange_gl_clearColor};
cvar_t	gl_cull = {"gl_cull", "1"};
cvar_t	gl_ztrick = {"gl_ztrick", "0"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels", "1"};
cvar_t	gl_polyblend = {"gl_polyblend", "1"};
cvar_t	gl_flashblend = {"gl_flashblend", "0"};
cvar_t	gl_playermip = {"gl_playermip", "0"};
cvar_t	gl_finish = {"gl_finish", "0"};
cvar_t	gl_fb_bmodels = {"gl_fb_bmodels", "1"};
cvar_t	gl_fb_models = {"gl_fb_models", "1"};
cvar_t	gl_lightmode = {"gl_lightmode", "2"};
cvar_t	gl_loadlitfiles = {"gl_loadlitfiles", "1"};


cvar_t gl_part_explosions = {"gl_part_explosions", "0"};
cvar_t gl_part_trails = {"gl_part_trails", "0"};
cvar_t gl_part_spikes = {"gl_part_spikes", "0"};
cvar_t gl_part_gunshots = {"gl_part_gunshots", "0"};
cvar_t gl_part_blood = {"gl_part_blood", "0"};
cvar_t gl_part_telesplash = {"gl_part_telesplash", "0"};
cvar_t gl_part_blobs = {"gl_part_blobs", "0"};
cvar_t gl_part_lavasplash = {"gl_part_lavasplash", "0"};
cvar_t gl_part_inferno = {"gl_part_inferno", "0"};


int		lightmode = 2;

void R_MarkLeaves (void);
void R_InitBubble (void);

//Returns true if the box is completely outside the frustom
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qboolean R_CullSphere (vec3_t centre, float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++)
	{
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

static void R_RotateForEntity (entity_t *e)
{
	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (-e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);
}


static mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int i, numframes, frame;
	float *pintervals, fullinterval, targettime, time;

	psprite = currententity->model->extradata;
	frame = currententity->frame;

	if (frame >= psprite->numframes || frame < 0)
	{
		Com_Printf ("R_GetSpriteFrame: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

static void R_DrawSpriteModel (entity_t *e)
{
	vec3_t point, right, up;
	vec3_t pointup;
	vec3_t pointdown;
	float texcoords[2*4];
	float coords[3*4];
	mspriteframe_t *frame;
	msprite_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->extradata;

	if (psprite->type == SPR_ORIENTED)
	{
		// bullet marks on walls
		AngleVectors (currententity->angles, NULL, right, up);
	}
	else if (psprite->type == SPR_FACING_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast (right);
	}
	else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT)
	{
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	}
	else {	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

	GL_Bind(frame->gl_texturenum);
	GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);

	texcoords[0 + 0] = 0;
	texcoords[0 + 1] = 1;

	texcoords[2 + 0] = 0;
	texcoords[2 + 1] = 0;

	texcoords[4 + 0] = 1;
	texcoords[4 + 1] = 0;

	texcoords[6 + 0] = 1;
	texcoords[6 + 1] = 1;

	VectorMA(e->origin, frame->down, up, pointdown);
	VectorMA(e->origin, frame->up, up, pointup);

	VectorMA(pointdown, frame->left, right, point);
	coords[0 + 0] = point[0];
	coords[0 + 1] = point[1];
	coords[0 + 2] = point[2];

	VectorMA(pointup, frame->left, right, point);
	coords[3 + 0] = point[0];
	coords[3 + 1] = point[1];
	coords[3 + 2] = point[2];

	VectorMA(pointup, frame->right, right, point);
	coords[6 + 0] = point[0];
	coords[6 + 1] = point[1];
	coords[6 + 2] = point[2];

	VectorMA(pointdown, frame->right, right, point);
	coords[9 + 0] = point[0];
	coords[9 + 1] = point[1];
	coords[9 + 2] = point[2];

	GL_VertexPointer(3, GL_FLOAT, 0, coords);
	GL_TexCoordPointer(0, 2, GL_FLOAT, 0, texcoords);
	glDrawArrays(GL_QUADS, 0, 4);
}


#define NUMVERTEXNORMALS	162

vec3_t	shadevector;

qboolean	full_light;
float		shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 64
byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;

byte	*shadedots = r_avertexnormal_dots[0];

float	r_framelerp;
float	r_modelalpha;
float	r_lerpdistance;

static void InterpolatePoses_Scalar(float *posedest, unsigned int *lightdest, trivertx_t *src1, trivertx_t *src2, float lerpfrac, unsigned int count, unsigned char modelalpha)
{
	unsigned int i;
	float l;
	unsigned char lc;
	union
	{
		unsigned int ui;
		unsigned char uc[4];
	} lightdestunion;

	for(i=0;i<count;i++)
	{
		l = FloatInterpolate(shadedots[src1[i].lightnormalindex], lerpfrac, shadedots[src2[i].lightnormalindex]) / 127.0;
		l = (l * shadelight + ambientlight);
		l = min(l, 255);
		l = max(l, 0);
		lc = l;

		lightdestunion.uc[0] = lc;
		lightdestunion.uc[1] = lc;
		lightdestunion.uc[2] = lc;
		lightdestunion.uc[3] = modelalpha;
		lightdest[i] = lightdestunion.ui;

		VectorInterpolate(src1[i].v, lerpfrac, src2[i].v, &posedest[i*3]);
	}
}

static void InterpolatePoses_LimitLerp(float *posedest, unsigned int *lightdest, trivertx_t *src1, trivertx_t *src2, float lerpfrac, unsigned int count, unsigned char modelalpha)
{
	unsigned int i;
	float l;
	unsigned char lc;
	union
	{
		unsigned int ui;
		unsigned char uc[4];
	} lightdestunion;

	for(i=0;i<count;i++)
	{
		lerpfrac = VectorL2Compare(src1[i].v, src2[i].v, r_lerpdistance) ? r_framelerp : 1;

		l = FloatInterpolate(shadedots[src1[i].lightnormalindex], lerpfrac, shadedots[src2[i].lightnormalindex]) / 127.0;
		l = (l * shadelight + ambientlight);
		l = min(l, 255);
		l = max(l, 0);
		lc = l;
		lightdestunion.uc[0] = lc;
		lightdestunion.uc[1] = lc;
		lightdestunion.uc[2] = lc;
		lightdestunion.uc[3] = modelalpha;
		lightdest[i] = lightdestunion.ui;

		VectorInterpolate(src1[i].v, lerpfrac, src2[i].v, &posedest[i*3]);
	}
}

static void CopyPoses(float *posedest, unsigned int *lightdest, trivertx_t *src, unsigned int count, unsigned char modelalpha)
{
	unsigned int i;
	float l;
	unsigned char lc;
	union
	{
		unsigned int ui;
		unsigned char uc[4];
	} lightdestunion;

	for(i=0;i<count;i++)
	{
		l = shadedots[src[i].lightnormalindex] / 127.0;
		l = (l * shadelight + ambientlight);
		l = min(l, 255);
		l = max(l, 0);
		lc = l;

		lightdestunion.uc[0] = lc;
		lightdestunion.uc[1] = lc;
		lightdestunion.uc[2] = lc;
		lightdestunion.uc[3] = modelalpha;
		lightdest[i] = lightdestunion.ui;

		VectorCopy(src[i].v, &posedest[i*3]);
	}
}

static void CalcColours(unsigned int *lightdest, trivertx_t *src, unsigned int count, unsigned char modelalpha)
{
	unsigned int i;
	float l;
	unsigned char lc;
	union
	{
		unsigned int ui;
		unsigned char uc[4];
	} lightdestunion;

	for(i=0;i<count;i++)
	{
		l = shadedots[src[i].lightnormalindex] / 127.0;
		l = (l * shadelight + ambientlight);
		l = min(l, 255);
		l = max(l, 0);
		lc = l;

		lightdestunion.uc[0] = lc;
		lightdestunion.uc[1] = lc;
		lightdestunion.uc[2] = lc;
		lightdestunion.uc[3] = modelalpha;
		lightdest[i] = lightdestunion.ui;
	}
}

static void AddCollisions(float *posedest, unsigned int *lightdest, unsigned int numverts, unsigned int collisions, unsigned short *collisionmap)
{
	unsigned int i;

	for(i=0;i<collisions;i++)
	{
		posedest[(numverts+i)*3+0] = posedest[collisionmap[i]*3+0];
		posedest[(numverts+i)*3+1] = posedest[collisionmap[i]*3+1];
		posedest[(numverts+i)*3+2] = posedest[collisionmap[i]*3+2];

		lightdest[numverts+i] = lightdest[collisionmap[i]];
	}
}

static void AddColourCollisions(unsigned int *lightdest, unsigned int numverts, unsigned int collisions, unsigned short *collisionmap)
{
	unsigned int i;

	for(i=0;i<collisions;i++)
	{
		lightdest[numverts+i] = lightdest[collisionmap[i]];
	}
}

#ifdef FOD_PPC
static void __attribute__ ((__noinline__)) InterpolatePoses_Altivec(float *posedest, unsigned int *lightdest, trivertx_t *src1, trivertx_t *src2, float lerpfrac, unsigned int count, unsigned char modelalpha)
{
	vector unsigned char trivert1, trivert2, aux, vindices, vmodelalpha;
	vector float t1, t2, t3, t4, t5, t6, vlerpfrac;
	vector unsigned char *vsrc1 = (vector unsigned char*)src1, *vsrc2 = (vector unsigned char*)src2;
	vector unsigned char p1, p2, p3;
	vector float *vposedest = (vector float*)posedest;
	vector unsigned char *vlightdest = (vector unsigned char*)lightdest;
	unsigned char *indices = (unsigned char*)&vindices;
	vector float vshade1, vshade2, div127, vshadelight, vambientlight;
	vector float v255;
	vector float v0;
	vector unsigned char vperm;
	int i;
	int *shade;

	v255 = (vector float){ 255.0, 255.0, 255.0, 255.0 };
	v0 = (vector float){ 0.0, 0.0, 0.0, 0.0 };
	vperm = (vector unsigned char){3, 3, 3, 19, 7, 7, 7, 23, 11, 11, 11, 27, 15, 15, 15, 31};

	p1 = (vector unsigned char){ 0x10, 0x10, 0x10, 0x00, 0x10, 0x10, 0x10, 0x01, 0x10, 0x10, 0x10, 0x02, 0x10, 0x10, 0x10, 0x04 };
	p2 = (vector unsigned char){ 0x10, 0x10, 0x10, 0x05, 0x10, 0x10, 0x10, 0x06, 0x10, 0x10, 0x10, 0x08, 0x10, 0x10, 0x10, 0x09 };
	p3 = (vector unsigned char){ 0x10, 0x10, 0x10, 0x0A, 0x10, 0x10, 0x10, 0x0C, 0x10, 0x10, 0x10, 0x0D, 0x10, 0x10, 0x10, 0x0E };

	*((float*)&vlerpfrac) = lerpfrac;
	vlerpfrac = vec_splat(vlerpfrac, 0);
	*((float*)&vshadelight) = shadelight;
	vshadelight = vec_splat(vshadelight, 0);
	*((float*)&vambientlight) = ambientlight;
	vambientlight = vec_splat(vambientlight, 0);
	*((unsigned int*)&vmodelalpha) = modelalpha;
	vmodelalpha = (vector unsigned char)vec_splat((vector unsigned int)vmodelalpha, 0);

	div127 = (vector float){ 1.0/127.0, 1.0/127.0, 1.0/127.0, 1.0/127.0 };

	while (count >= 4)
	{
		// Loading data (4 trivertxs from each source).

		trivert1 = *vsrc1++;
		trivert2 = *vsrc2++;

		// Extracting data. 12 'v' coordinates from source 1 are placed in 3 registers t1, t2 and t3.
		// 12 'v' coordinates from source 2 are placed in 3 registers t4, t5 i t6.

		t1 = (vector float)vec_splat_s32(0);     // clear 't' registers to all zeros
		t2 = t1;
		t3 = t1;
		t4 = t1;
		t5 = t1;
		t6 = t1;

		t1 = (vector float)vec_perm(trivert1, (vector unsigned char)t1, p1);
		t2 = (vector float)vec_perm(trivert1, (vector unsigned char)t1, p2);
		t3 = (vector float)vec_perm(trivert1, (vector unsigned char)t1, p3);

		t4 = (vector float)vec_perm(trivert2, (vector unsigned char)t4, p1);
		t5 = (vector float)vec_perm(trivert2, (vector unsigned char)t4, p2);
		t6 = (vector float)vec_perm(trivert2, (vector unsigned char)t4, p3);

		// Now 'v' coordinates are converted to floats.

		t1 = vec_ctf((vector unsigned int)t1, 0);
		t2 = vec_ctf((vector unsigned int)t2, 0);
		t3 = vec_ctf((vector unsigned int)t3, 0);
		t4 = vec_ctf((vector unsigned int)t4, 0);
		t5 = vec_ctf((vector unsigned int)t5, 0);
		t6 = vec_ctf((vector unsigned int)t6, 0);

		// Interpolation step 1. Calculating differencies.

		t4 = vec_sub(t4, t1);
		t5 = vec_sub(t5, t2);
		t6 = vec_sub(t6, t3);

		// Interpolation step 2. Multiplying by fraction and adding source 1.

		t1 = vec_madd(t4, vlerpfrac, t1);
		t2 = vec_madd(t5, vlerpfrac, t2);
		t3 = vec_madd(t6, vlerpfrac, t3);

		// Store results.

		*vposedest++ = t1;
		*vposedest++ = t2;
		*vposedest++ = t3;

		// Extracting lightnormindexes. Source data contains them as xxxAxxxBxxxCxxxD for src 1 and xxxExxxFxxxGxxxH for src 2.
		// Then I pack these two, obtaining xAxBxCxDxExFxGxH and then pack the result with itself, getting ABCDEFGHABCDEFGH.
		// This is stored into ubyte table and used to perform lookup scalarly.

		aux = vec_pack((vector unsigned short)trivert1, (vector unsigned short)trivert2);
		aux = vec_pack((vector unsigned short)aux, (vector unsigned short)aux);
		vindices = aux;
		t1 = (vector float)vec_splat_s32(0);

		// Scalar lookup. Values are expanded to floats on altivec side.

		shade = (int*)&vshade1;
		for (i = 0; i < 4; i++) shade[i] = shadedots[indices[i]];
		shade = (int*)&vshade2;
		for (i = 0; i < 4; i++) shade[i] = shadedots[indices[i + 4]];

		vshade1 = vec_ctf((vector int)vshade1, 0);
		vshade2 = vec_ctf((vector int)vshade2, 0);

		// Interpolation now, then division by 127.0, applying shadeligth and ambientlight.

		vshade2 = vec_sub(vshade2, vshade1);
		vshade1 = vec_madd(vshade2, vlerpfrac, vshade1);
		vshade1 = vec_madd(vshade1, div127, t1);                     // t1 is all zeros
		vshade1 = vec_madd(vshade1, vshadelight, vambientlight);

		// Saturation, conversion to integer, packing, merging with modelalpha.

		vshade1 = vec_min(vshade1, v255);
		vshade1 = vec_max(vshade1, v0);
		trivert1 = (vector unsigned char)vec_ctu(vshade1, 0);
		trivert1 = vec_perm(trivert1, vmodelalpha, vperm);

		// Store.

		*vlightdest++ = trivert1;

		count -= 4;
	}

	if (count > 0)
		InterpolatePoses_Scalar((float*)vposedest, (unsigned int*)vlightdest, (trivertx_t*)vsrc1, (trivertx_t*)vsrc2, lerpfrac, count, modelalpha);
}
#endif

static void InterpolatePoses(float *posedest, unsigned int *lightdest, trivertx_t *src1, trivertx_t *src2, float lerpfrac, unsigned int count, unsigned char modelalpha)
{
#ifdef FOD_PPC
	if (altivec_available)
		InterpolatePoses_Altivec(posedest, lightdest, src1, src2, lerpfrac, count, modelalpha);
	else
#endif
		InterpolatePoses_Scalar(posedest, lightdest, src1, src2, lerpfrac, count, modelalpha);
}

static float *posedest;
static unsigned int *lightdest;
static unsigned int posedestsize;

static void GL_DrawAliasFrame2(aliashdr_t *paliashdr, int pose1, int pose2, qboolean mtex, qboolean dolerp)
{
	float lerpfrac;
	trivertx_t *verts1, *verts2;

	GL_SetAlphaTestBlend(0, r_modelalpha<1);

	if (paliashdr->totalverts > posedestsize)
	{
		float *newposedest;
		unsigned int *newlightdest;

		newposedest = malloc(paliashdr->totalverts * sizeof(*newposedest) * 3);
		newlightdest = malloc(paliashdr->totalverts * sizeof(*newlightdest));
		if (newposedest && newlightdest)
		{
			free(posedest);
			free(lightdest);
			posedest = newposedest;
			lightdest = newlightdest;
			posedestsize = paliashdr->totalverts;
		}
		else
		{
			free(newposedest);
			free(newlightdest);
			return;
		}
	}

	verts2 = verts1 = paliashdr->realposeverts;

	verts1 += pose1 * paliashdr->numverts;
	verts2 += pose2 * paliashdr->numverts;

	if (dolerp)
	{
		lerpfrac = r_framelerp;

		if ((currententity->flags & RF_LIMITLERP))
			InterpolatePoses_LimitLerp(posedest, lightdest, verts1, verts2, lerpfrac, paliashdr->numverts, bound(0, r_modelalpha*255, 255));
		else
			InterpolatePoses(posedest, lightdest, verts1, verts2, lerpfrac, paliashdr->numverts, bound(0, r_modelalpha*255, 255));
	}
	else
	{
		if (gl_vbo)
			CalcColours(lightdest, verts1, paliashdr->numverts, bound(0, r_modelalpha*255, 255));
		else
			CopyPoses(posedest, lightdest, verts1, paliashdr->numverts, bound(0, r_modelalpha*255, 255));
	}

	if (dolerp || !gl_vbo)
		AddCollisions(posedest, lightdest, paliashdr->numverts, paliashdr->collisions, paliashdr->collisionmap);
	else
		AddColourCollisions(lightdest, paliashdr->numverts, paliashdr->collisions, paliashdr->collisionmap);

	if (mtex)
		GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_COLOR_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY_1);
	else
		GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_COLOR_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);

	GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, lightdest);

	if (gl_vbo && !dolerp)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, paliashdr->vert_vbo_number[pose1]);
		GL_VertexPointer(3, GL_FLOAT, 0, 0);
	}
	else
		GL_VertexPointer(3, GL_FLOAT, 0, posedest);

	if (gl_vbo)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, paliashdr->texcoord_vbo_number);

		if (mtex)
		{
			GL_TexCoordPointer(1, 2, GL_FLOAT, 0, 0);
		}

		GL_TexCoordPointer(0, 2, GL_FLOAT, 0, 0);

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
	else
	{
		if (mtex)
		{
			GL_TexCoordPointer(1, 2, GL_FLOAT, 0, paliashdr->texcoords);
		}

		GL_TexCoordPointer(0, 2, GL_FLOAT, 0, paliashdr->texcoords);
	}

	glDrawRangeElements(GL_TRIANGLES, paliashdr->indexmin, paliashdr->indexmax, paliashdr->numtris*3, GL_UNSIGNED_SHORT, paliashdr->indices);
}

static void GL_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qboolean mtex)
{
	if (pose1 == pose2 || r_framelerp == 0)
		GL_DrawAliasFrame2(paliashdr, pose1, pose1, mtex, false);
	else if (r_framelerp == 1)
		GL_DrawAliasFrame2(paliashdr, pose2, pose2, mtex, false);
	else
		GL_DrawAliasFrame2(paliashdr, pose1, pose2, mtex, true);
}

static void R_SetupAliasFrame (maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qboolean mtex)
{
	int oldpose, pose, numposes;
	float interval;

	oldpose = oldframe->firstpose;
	numposes = oldframe->numposes;
	if (numposes > 1)
	{
		interval = oldframe->interval;
		oldpose += (int) (cl.time / interval) % numposes;
	}

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1)
	{
		interval = frame->interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, oldpose, pose, mtex);
}

static void R_AliasSetupFullLight(model_t *model)
{
	if (model->modhint == MOD_THUNDERBOLT
	 || model->modhint == MOD_FLAME
	 || (model->modhint == MOD_PLAYER && bound(0, r_fullbrightSkins.value, cl.fbskins)))
	{
		full_light = true;
	}
	else
	{
		full_light = false;
	}
}

static void R_AliasSetupLighting(entity_t *ent)
{
	int minlight, lnum;
	float add, fbskins;
	unsigned int i;
	unsigned int j;
	vec3_t dist;
	model_t *clmodel;

	clmodel = ent->model;

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT)
	{
		ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		shadelight = 0;
		return;
	}
	else if (clmodel->modhint == MOD_FLAME)
	{
		ambientlight = 255;
		shadelight = 0;
		return;
	}

	//normal lighting
	ambientlight = shadelight = R_LightPoint (ent->origin);

	for(i=0;i<MAX_DLIGHTS/32;i++)
	{
		if (cl_dlight_active[i])
		{
			for(j=0;j<32;j++)
			{
				if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS)
				{
					lnum = i*32 + j;

					VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
					add = cl_dlights[lnum].radius - VectorLength(dist);

					if (add > 0)
						ambientlight += add;
				}
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// always give the gun some light
	if ((ent->flags & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER)
	{
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (clmodel->modhint == MOD_PLAYER)
	{
		fbskins = bound(0, r_fullbrightSkins.value, cl.fbskins);
		if (fbskins)
		{
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
		}
	}

	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;
}

static void R_DrawAliasModelList2TMU(entity_t *ent, unsigned int entcount)
{
	int i, anim, skinnum, texture, fb_texture;
	int oldtexture;
	int oldfb_texture;
	int oldmtex;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	extern	cvar_t r_viewmodelsize, cl_drawgun;

	oldmtex = 0;
	oldtexture = -1;
	oldfb_texture = -1;

	VectorSubtract (r_origin, ent->origin, modelorg);

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	R_AliasSetupFullLight(clmodel);

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	c_alias_polys += paliashdr->numtris * entcount;
	anim = (int) (cl.time * 10) & 3;

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	GL_SelectTexture(GL_TEXTURE1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	GL_SelectTexture(GL_TEXTURE0);

	/* Stuff that depends on the entity itself. */

	ent--;

	while(ent++, entcount--)
	{
		currententity = ent;

		if (ent->frame >= paliashdr->numframes || ent->frame < 0)
		{
			Com_DPrintf ("R_DrawAliasModel: no such frame %d\n", ent->frame);
			ent->frame = 0;
		}
		if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0)
		{
			Com_DPrintf ("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
			ent->oldframe = 0;
		}

		frame = &paliashdr->frames[ent->frame];
		oldframe = &paliashdr->frames[ent->oldframe];

		if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
			r_framelerp = 1.0;
		else
			r_framelerp = min (ent->framelerp, 1);

		//culling
		if (!(ent->flags & RF_WEAPONMODEL))
		{
			if (ent->angles[0] || ent->angles[1] || ent->angles[2])
			{
				if (R_CullSphere (ent->origin, max(oldframe->radius, frame->radius)))
					continue;
			}
			else
			{
				if (r_framelerp == 1)
				{
					VectorAdd(ent->origin, frame->bboxmin, mins);
					VectorAdd(ent->origin, frame->bboxmax, maxs);
				}
				else
				{
					for (i = 0; i < 3; i++)
					{
						mins[i] = ent->origin[i] + min (oldframe->bboxmin[i], frame->bboxmin[i]);
						maxs[i] = ent->origin[i] + max (oldframe->bboxmax[i], frame->bboxmax[i]);
					}
				}
				if (R_CullBox (mins, maxs))
					continue;
			}
		}

		//get lighting information
		R_AliasSetupLighting(ent);

		shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

		//draw all the triangles
		glPushMatrix ();
		R_RotateForEntity (ent);

		if (clmodel->modhint == MOD_EYES)
		{
			glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
			// double size of eyes, since they are really hard to see in gl
			glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
		}
		else if (ent->flags & RF_WEAPONMODEL)
		{
			scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
			glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
			glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
		}
		else
		{
			glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
			glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
		}

		skinnum = ent->skinnum;
		if (skinnum >= paliashdr->numskins || skinnum < 0)
		{
			Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
			skinnum = 0;
		}

		texture = paliashdr->gl_texturenum[skinnum][anim];
		fb_texture = paliashdr->fb_texturenum[skinnum][anim];

		r_modelalpha = ((ent->flags & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;

		if (ent->scoreboard)
		{
			i = ent->scoreboard - cl.players;
			if (i >= 0 && i < MAX_CLIENTS)
			{
				struct SkinImp *skinimp;

				skinimp = Skin_GetTranslation(cl.players[i].skin, cl.players[i].topcolor, cl.players[i].bottomcolor);
				if (skinimp)
				{
					texture = skinimp->texid;
					fb_texture = skinimp->fbtexid;
				}
			}
		}

		if (full_light || !gl_fb_models.value)
			fb_texture = 0;

		if (fb_texture)
		{
			if (oldmtex != 1)
			{
				GL_EnableMultitexture();
				oldmtex = 1;
			}

			if (texture != oldtexture)
			{
				GL_SelectTexture(GL_TEXTURE0);
				GL_Bind(texture);
				oldtexture = texture;
			}

			if (fb_texture != oldfb_texture)
			{
				GL_SelectTexture(GL_TEXTURE1);
				GL_Bind(fb_texture);
				oldfb_texture = fb_texture;
			}

			R_SetupAliasFrame(oldframe, frame, paliashdr, true);
		}
		else
		{
			if (oldmtex != 0)
			{
				GL_SelectTexture(GL_TEXTURE1);
				GL_DisableMultitexture();
				oldmtex = 0;
			}

			if (texture != oldtexture)
			{
				GL_Bind(texture);
				oldtexture = texture;
			}

			R_SetupAliasFrame(oldframe, frame, paliashdr, false);
		}

		glPopMatrix ();
	}

	/* End of per-entity stuff */

	if (oldmtex == 1)
	{
		GL_SelectTexture(GL_TEXTURE1);
		GL_DisableMultitexture();
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_FLAT);

	glColor3ubv (color_white);
}

static void R_DrawAliasModel(entity_t *ent)
{
	int i, anim, skinnum, texture, fb_texture;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	extern	cvar_t r_viewmodelsize, cl_drawgun;

	VectorSubtract (r_origin, ent->origin, modelorg);

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0)
	{
		Com_DPrintf ("R_DrawAliasModel: no such frame %d\n", ent->frame);
		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0)
	{
		Com_DPrintf ("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];


	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame)
		r_framelerp = 1.0;
	else
		r_framelerp = min (ent->framelerp, 1);

	//culling
	if (!(ent->flags & RF_WEAPONMODEL))
	{
		if (ent->angles[0] || ent->angles[1] || ent->angles[2])
		{
			if (R_CullSphere (ent->origin, max(oldframe->radius, frame->radius)))
				return;
		}
		else
		{
			if (r_framelerp == 1)
			{
				VectorAdd(ent->origin, frame->bboxmin, mins);
				VectorAdd(ent->origin, frame->bboxmax, maxs);
			}
			else
			{
				for (i = 0; i < 3; i++)
				{
					mins[i] = ent->origin[i] + min (oldframe->bboxmin[i], frame->bboxmin[i]);
					maxs[i] = ent->origin[i] + max (oldframe->bboxmax[i], frame->bboxmax[i]);
				}
			}
			if (R_CullBox (mins, maxs))
				return;
		}
	}

	//get lighting information
	R_AliasSetupFullLight(clmodel);
	R_AliasSetupLighting(ent);

	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//draw all the triangles
	c_alias_polys += paliashdr->numtris;
	glPushMatrix ();
	R_RotateForEntity (ent);

	if (clmodel->modhint == MOD_EYES)
	{
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->flags & RF_WEAPONMODEL)
	{
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else
	{
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int) (cl.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0)
	{
		Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	r_modelalpha = ((ent->flags & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;

	if (ent->scoreboard)
	{
		i = ent->scoreboard - cl.players;
		if (i >= 0 && i < MAX_CLIENTS)
		{
			struct SkinImp *skinimp;

			skinimp = Skin_GetTranslation(cl.players[i].skin, cl.players[i].topcolor, cl.players[i].bottomcolor);
			if (skinimp)
			{
				texture = skinimp->texid;
				fb_texture = skinimp->fbtexid;
			}
		}
	}

	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (fb_texture && gl_mtexable)
	{
		GL_Bind (texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		GL_EnableMultitexture ();
		GL_Bind (fb_texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true);

		GL_DisableMultitexture ();
	}
	else
	{
		GL_Bind (texture);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		R_SetupAliasFrame (oldframe, frame, paliashdr, false);

		if (fb_texture)
		{
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			GL_SetAlphaTestBlend(1, 0);
			GL_Bind (fb_texture);

			R_SetupAliasFrame (oldframe, frame, paliashdr, false);
		}
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_FLAT);

	glPopMatrix ();

	glColor3ubv (color_white);
}


void R_DrawEntitiesOnList(visentlist_t *vislist)
{
	int i;
	int j;

	if (!r_drawentities.value || !vislist->count)
		return;

	GL_SetAlphaTestBlend(vislist->alpha, 0);

	// draw sprites separately, because of alpha_test
	for (i = 0; i < vislist->count; i++)
	{
		currententity = &vislist->list[i];
		switch (currententity->model->type)
		{
			case mod_alias:
				if (gl_mtexable)
				{
					for(j=1;i+j < vislist->count && vislist->list[i+j].model == currententity->model;j++);
					if (j > 1)
					{
						R_DrawAliasModelList2TMU(currententity, j);
						i += j-1;
						break;
					}
				}

				R_DrawAliasModel (currententity);
				break;
			case mod_brush:
				R_DrawBrushModel (currententity);
				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
		}
	}
}

static void R_DrawViewModel(void)
{
	centity_t *cent;
	static entity_t gun;

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
		if (cent->current.modelindex != cl_modelindices[mi_vaxe]
		 && cent->current.modelindex != cl_modelindices[mi_vbio]
		 && cent->current.modelindex != cl_modelindices[mi_vgrap]
		 && cent->current.modelindex != cl_modelindices[mi_vknife]
		 && cent->current.modelindex != cl_modelindices[mi_vknife2]
		 && cent->current.modelindex != cl_modelindices[mi_vmedi]
		 && cent->current.modelindex != cl_modelindices[mi_vspan])
		{
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

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}


void R_PolyBlend (void)
{
	extern cvar_t gl_hwblend;
	float coords[4*2];

	if (VID_HWGammaSupported() && gl_hwblend.value && !cl.teamfortress)
		return;
	if (!v_blend[3])
		return;

	GL_SetAlphaTestBlend(0, 1);
	glDisable (GL_TEXTURE_2D);

	glColor4fv (v_blend);

	coords[0*2 + 0] = r_refdef.vrect.x;
	coords[0*2 + 1] = r_refdef.vrect.y;
	coords[1*2 + 0] = r_refdef.vrect.x + r_refdef.vrect.width;
	coords[1*2 + 1] = r_refdef.vrect.y;
	coords[2*2 + 0] = r_refdef.vrect.x + r_refdef.vrect.width;
	coords[2*2 + 1] = r_refdef.vrect.y + r_refdef.vrect.height;
	coords[3*2 + 0] = r_refdef.vrect.x;
	coords[3*2 + 1] = r_refdef.vrect.y + r_refdef.vrect.height;

	GL_SetArrays(FQ_GL_VERTEX_ARRAY);

	GL_VertexPointer(2, GL_FLOAT, 0, coords);

	glDrawArrays(GL_QUADS, 0, 4);

	glEnable (GL_TEXTURE_2D);

	glColor3ubv (color_white);
}

void R_BrightenScreen (void)
{
	extern float vid_gamma;
	float f;
	float coords[4*2];
	unsigned int colours[4];
	union
	{
		unsigned char uc[4];
		unsigned int ui;
	} col;

	if (VID_HWGammaSupported())
		return;
	if (v_contrast.value <= 1.0)
		return;

	f = min (v_contrast.value, 3);
	f = pow (f, vid_gamma);

	GL_SetAlphaTestBlend(0, 1);
	glDisable (GL_TEXTURE_2D);
	glBlendFunc (GL_DST_COLOR, GL_ONE);

	coords[0*2 + 0] = 0;
	coords[0*2 + 1] = 0;
	coords[1*2 + 0] = vid.conwidth;
	coords[1*2 + 1] = 0;
	coords[2*2 + 0] = vid.conwidth;
	coords[2*2 + 1] = vid.conheight;
	coords[3*2 + 0] = 0;
	coords[3*2 + 1] = vid.conheight;

	GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);
	GL_VertexPointer(2, GL_FLOAT, 0, coords);
	GL_ColorPointer(4, GL_UNSIGNED_BYTE, 0, colours);

	if (f > 2)
	{
		colours[0] = 0xffffffff;
		colours[1] = 0xffffffff;
		colours[2] = 0xffffffff;
		colours[3] = 0xffffffff;
	}

	while (f > 1)
	{
		if (f < 2)
		{
			col.uc[0] = (f - 1) * 255;
			col.uc[1] = (f - 1) * 255;
			col.uc[2] = (f - 1) * 255;
			col.uc[3] = 255;

			colours[0] = col.ui;
			colours[1] = col.ui;
			colours[2] = col.ui;
			colours[3] = col.ui;
		}

		glDrawArrays(GL_QUADS, 0, 4);

		f *= 0.5;
	}
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	glColor3ubv (color_white);
}

static int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


static void R_SetFrustum (void)
{
	int i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for (i = 0; i < 4; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

static void R_SetupFrame (void)
{
	vec3_t testorigin;
	mleaf_t	*leaf;

	// don't allow cheats in multiplayer
	r_fullbright.value = 0;
	r_lightmap.value = 0;

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA)
	{
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	}
	else if (r_viewleaf->contents == CONTENTS_EMPTY)
	{
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

__inline void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum(xmin, xmax, ymin, ymax, zNear, zFar);
}

static void R_SetupGL (void)
{
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth / vid.conwidth;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.conwidth;
	y = (vid.conheight-r_refdef.vrect.y) * glheight / vid.conheight;
	y2 = (vid.conheight - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.conheight;

	w = x2 - x;
	h = y - y2;

	glViewport(x, y2, w, h);
	screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	farclip = max((int) r_farclip.value, 4096);
	MYgluPerspective (r_refdef.fov_y, screenaspect, 4, farclip);

	glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90, 1, 0, 0);	    // put Z going up
	glRotatef (90,  0, 0, 1);	    // put Z going up
	glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

	// set drawing parms
	if (gl_cull.value)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	GL_SetAlphaTestBlend(0, 0);
	glEnable(GL_DEPTH_TEST);
}

void R_CvarInit(void)
{
	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_inferno);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&r_fastturb);
	Cvar_Register (&r_novis);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&gl_detail);
	Cvar_Register (&gl_caustics);

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register (&gl_polyblend);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&gl_shaftlight);
	Cvar_Register (&gl_loadlitfiles);
	Cvar_Register (&gl_colorlights);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_subdivide_size);

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&r_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_clearColor);
	Cvar_Register (&gl_cull);
	Cvar_Register (&gl_ztrick);
	Cvar_Register (&gl_finish);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);

	Cvar_ResetCurrentGroup();

	GL_Particles_CvarInit();
	GL_Warp_CvarInit();
}

int R_Init(void)
{
	texture_extension_number = 1;
	vbo_number = 1;

	GL_Init();

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gl_max_size_default);
	Cvar_SetDefault(&gl_max_size, gl_max_size_default);

	GL_Texture_Init();

	GL_Shader_Init();

	GL_RSurf_Init();

	GL_Warp_Init();

	if (R_InitTextures())
	{
		R_InitBubble();

		if (R_InitParticles())
		{
			netgraphtexture = texture_extension_number;
			texture_extension_number++;

			playertextures = texture_extension_number;
			texture_extension_number += MAX_CLIENTS;

			// fullbright skins
			texture_extension_number += MAX_CLIENTS;
			skyboxtextures = texture_extension_number;
			texture_extension_number += 6;

			return 1;
		}

		R_ShutdownTextures();
	}

	Sys_Error("R_Init() failed.");

	return 0;
}

void R_Shutdown()
{
	R_ShutdownParticles();
	R_ShutdownTextures();
	GL_RSurf_Shutdown();
	GL_Warp_Shutdown();
	GL_Texture_Shutdown();
	GL_Shader_Shutdown();
}

void R_InitGL(void)
{
	byte *clearColor;

	Classic_LoadParticleTextures();

	R_InitOtherTextures ();

	clearColor = StringToRGB(gl_clearColor.string);
	glClearColor(clearColor[0] / 255.0, clearColor[1] / 255.0, clearColor[2] / 255.0, 1.0);

	gl_initialised = 1;
}

extern msurface_t *alphachain;

static void R_RenderScene(void)
{
	R_SetupFrame();

	R_SetFrustum();

	R_SetupGL();

	R_MarkLeaves();	// done here so we know if we're in water

	R_DrawWorld();		// adds static entities to the list

	S_ExtraUpdate();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList(&cl_visents);
	R_DrawEntitiesOnList(&cl_alphaents);

	R_DrawWaterSurfaces();

	GL_DisableMultitexture();
}

int gl_ztrickframe = 0;

static qboolean OnChange_gl_clearColor(cvar_t *v, char *s)
{
	byte *clearColor;

	if (gl_initialised)
	{
		clearColor = StringToRGB(s);
		glClearColor(clearColor[0] / 255.0, clearColor[1] / 255.0, clearColor[2] / 255.0, 1.0);
	}

	return false;
}

static void R_Clear(void)
{
	int clearbits = 0;

	if (gl_clear.value || (!VID_HWGammaSupported() && v_contrast.value > 1))
		clearbits |= GL_COLOR_BUFFER_BIT;

	if (gl_ztrick.value)
	{
		if (clearbits)
			glClear(clearbits);

		gl_ztrickframe = !gl_ztrickframe;
		if (gl_ztrickframe)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc(GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc(GL_GEQUAL);
		}
	}
	else
	{
		clearbits |= GL_DEPTH_BUFFER_BIT;
		glClear(clearbits);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc(GL_LEQUAL);
	}

	glDepthRange(gldepthmin, gldepthmax);
}


void R_RenderView(void)
{
	double time1 = 0, time2;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish();
		time1 = Sys_DoubleTime();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish();

	R_Clear();

	// render normal view
	R_RenderScene();
	R_RenderDlights();
	R_DrawParticles();
	R_DrawViewModel();

	if (r_speeds.value)
	{
		time2 = Sys_DoubleTime();
		Com_Printf("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys);
	}
}

