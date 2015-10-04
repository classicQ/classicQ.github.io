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
// r_surf.c: surface-related refresh code

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

#define MAX_LIGHTMAP_SIZE	(32 * 32)
#define	MAX_LIGHTMAPS		64

extern cvar_t r_drawflat_enable;

static int lightmap_textures;
static unsigned int blocklights[MAX_LIGHTMAP_SIZE * 3];

typedef struct glRect_s
{
	unsigned char l, t, w, h;
} glRect_t;

static glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
static unsigned int lightmap_polys_used[(MAX_LIGHTMAPS+31)/32] __attribute__((aligned(64)));
static qboolean	lightmap_modified[MAX_LIGHTMAPS];
static glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

static int allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte	lightmaps[3 * MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT];

msurface_t	*skychain = NULL;
msurface_t	**skychain_tail = &skychain;

msurface_t	*waterchain = NULL;
msurface_t	**waterchain_tail = &waterchain;

msurface_t	*alphachain = NULL;
msurface_t	**alphachain_tail = &alphachain;

msurface_t	*drawflatchain = NULL;
msurface_t	**drawflatchain_tail = &drawflatchain;

#define CHAIN_SURF_F2B(surf, chain_tail)		\
	{											\
		*(chain_tail) = (surf);					\
		(chain_tail) = &(surf)->texturechain;	\
		(surf)->texturechain = NULL;			\
	}

#define CHAIN_SURF_B2F(surf, chain) 			\
	{											\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	}

static glpoly_t *fullbright_polys[MAX_GLTEXTURES];
static unsigned int fullbright_polys_used[(MAX_GLTEXTURES+31)/32] __attribute__((aligned(64)));

static glpoly_t *luma_polys[MAX_GLTEXTURES];
static unsigned int luma_polys_used[(MAX_GLTEXTURES+31)/32] __attribute__((aligned(64)));

qboolean drawfullbrights = false, drawlumas = false;
glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

static void DrawGLPoly (glpoly_t *p)
{
	int i;
	float *v;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i = 0; i < p->numverts; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

static void R_RenderFullbrights (void)
{
	unsigned int i;
	unsigned int j;
	glpoly_t *p;

	if (!drawfullbrights)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	GL_SetAlphaTestBlend(1, 0);


	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for(j=0;j<(MAX_GLTEXTURES+31)/32;j++)
	{
		if (!fullbright_polys_used[j])
			continue;

		for(i=0;i<32;i++)
		{
			if (!(fullbright_polys_used[j]&(1<<i)))
				continue;
			GL_Bind(j*32+i);
			for (p = fullbright_polys[j*32+i]; p; p = p->fb_chain)
				DrawGLPoly (p);
		}

		fullbright_polys_used[j] = 0;
	}

	glDepthMask (GL_TRUE);

	drawfullbrights = false;
}

static void R_RenderLumas (void)
{
	unsigned int i;
	unsigned int j;
	glpoly_t *p;

	if (!drawlumas)
		return;

	glDepthMask (GL_FALSE);	// don't bother writing Z
	GL_SetAlphaTestBlend(0, 1);
	glBlendFunc(GL_ONE, GL_ONE);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	for(j=0;j<(MAX_GLTEXTURES+31)/32;j++)
	{
		if (!luma_polys_used[j])
			continue;

		for(i=0;i<32;i++)
		{
			if (!(luma_polys_used[j]&(1<<i)))
				continue;
			GL_Bind(j*32+i);
			for (p = luma_polys[j*32+i]; p; p = p->luma_chain)
				DrawGLPoly (p);
		}

		luma_polys_used[j] = 0;
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);

	drawlumas = false;
}


static void EmitDetailPolys (void)
{
	glpoly_t *p;
	int i;
	float *v;

	if (!detail_polys)
		return;

	GL_Bind(detailtexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_SetAlphaTestBlend(0, 1);

	for (p = detail_polys; p; p = p->detail_chain)
	{
		glBegin(GL_POLYGON);
		v = p->verts[0];
		for (i = 0; i < p->numverts; i++, v += VERTEXSIZE)
		{
			glTexCoord2f (v[7] * 18, v[8] * 18);
			glVertex3fv (v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	detail_polys = NULL;
}

typedef struct dlightinfo_s
{
	int local[2];
	int rad;
	int minlight;	// rad - minlight
	int type;
} dlightinfo_t;

static dlightinfo_t dlightlist[MAX_DLIGHTS];

static int R_BuildDlightList (msurface_t *surf)
{
	float dist;
	vec3_t impact;
	mtexinfo_t *tex;
	int lnum, i, smax, tmax, irad, iminlight, local[2], tdmin, sdmin, distmin;
	dlightinfo_t *light;
	unsigned int dlightbits;
	int numdlights;

	numdlights = 0;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	dlightbits = surf->dlightbits;

	for (lnum = 0; lnum < MAX_DLIGHTS && dlightbits; lnum++)
	{
		if ( !(dlightbits & (1 << lnum) ) )
			continue;		// not lit by this light

		dlightbits &= ~(1<<lnum);

		dist = PlaneDiff(cl_dlights[lnum].origin, surf->plane);
		irad = (cl_dlights[lnum].radius - fabs(dist)) * 256;
		iminlight = cl_dlights[lnum].minlight * 256;
		if (irad < iminlight)
			continue;

		iminlight = irad - iminlight;

		for (i = 0; i < 3; i++)
			impact[i] = cl_dlights[lnum].origin[i] - surf->plane->normal[i] * dist;

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->texturemins[0];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->texturemins[1];

		// check if this dlight will touch the surface
		if (local[1] > 0)
		{
			tdmin = local[1] - (tmax << 4);
			if (tdmin < 0)
				tdmin = 0;
		}
		else
		{
			tdmin = -local[1];
		}

		if (local[0] > 0)
		{
			sdmin = local[0] - (smax << 4);
			if (sdmin < 0)
				sdmin = 0;
		}
		else
		{
			sdmin = -local[0];
		}

		if (sdmin > tdmin)
			distmin = (sdmin << 8) + (tdmin << 7);
		else
			distmin = (tdmin << 8) + (sdmin << 7);

		if (distmin < iminlight)
		{
			// save dlight info
			light = &dlightlist[numdlights];
			light->minlight = iminlight;
			light->rad = irad;
			light->local[0] = local[0];
			light->local[1] = local[1];
			light->type = cl_dlights[lnum].type;
			numdlights++;
		}
	}

	return numdlights;
}

static const int dlightcolor[NUM_DLIGHTTYPES][3] =
{
	{ 100, 90, 80 },	// dimlight or brightlight
	{ 100, 50, 10 },	// muzzleflash
	{ 100, 50, 10 },	// explosion
	{ 90, 60, 7 },		// rocket
	{ 128, 0, 0 },		// red
	{ 0, 0, 128 },		// blue
	{ 128, 0, 128 },	// red + blue
	{ 0, 128, 0 },		// green
	{ 128, 128, 128},	// white
};


//R_BuildDlightList must be called first!
static void R_AddDynamicLights(msurface_t *surf, int numdlights)
{
	int i, smax, tmax, s, t, sd, td, _sd, _td, irad, idist, iminlight, color[3], tmp;
	dlightinfo_t *light;
	unsigned int *dest;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	for (i = 0, light = dlightlist; i < numdlights; i++, light++)
	{
		extern cvar_t gl_colorlights;
		if (gl_colorlights.value)
		{
			VectorCopy(dlightcolor[light->type], color);
		}
		else
		{
			VectorSet(color, 128, 128, 128);
		}

		irad = light->rad;
		iminlight = light->minlight;

		_td = light->local[1];
		dest = blocklights;
		for (t = 0; t < tmax; t++)
		{
			td = _td;
			if (td < 0)	td = -td;
			_td -= 16;
			_sd = light->local[0];

			for (s = 0; s < smax; s++)
			{
				sd = _sd < 0 ? -_sd : _sd;
				_sd -= 16;
				if (sd > td)
					idist = (sd << 8) + (td << 7);
				else
					idist = (td << 8) + (sd << 7);

				if (idist < iminlight)
				{
					tmp = (irad - idist) >> 7;
					dest[0] += tmp * color[0];
					dest[1] += tmp * color[1];
					dest[2] += tmp * color[2];
				}
				dest += 3;
			}
		}
	}
}

static void AddAllLightMaps(byte *lightmap, msurface_t *surf, int blocksize)
{
	int maps;
	int i;
	unsigned scale;
	unsigned int *bl;

	// add all the lightmaps
	if (lightmap)
	{
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			bl = blocklights;
			for (i = 0; i < blocksize; i++)
				*bl++ += lightmap[i] * scale;
			lightmap += blocksize;		// skip to next lightmap
		}
	}
}

static void lightmapstore_mode2(int stride, int smax, int tmax, byte *dest)
{
	unsigned int *bl;
	int i;
	int j;
	int t;

	bl = blocklights;

	for (i = 0; i < tmax; i++, dest += stride)
	{
		for (j = smax; j; j--)
		{
			t = bl[0]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
			dest[0] = t;
			t = bl[1]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
			dest[1] = t;
			t = bl[2]; t = (t >> 8) + (t >> 9); if (t > 255) t = 255;
			dest[2] = t;
			bl += 3;
			dest += 3;
		}
	}
}

static void lightmapstore_mode0(int stride, int smax, int tmax, byte *dest)
{
	unsigned int *bl;
	int i;
	int j;
	int t;

	bl = blocklights;

	for (i = 0; i < tmax; i++, dest += stride)
	{
		for (j = smax; j; j--)
		{
			t = bl[0]; t = t >> 7; if (t > 255) t = 255;
			dest[0] = t;
			t = bl[1]; t = t >> 7; if (t > 255) t = 255;
			dest[1] = t;
			t = bl[2]; t = t >> 7; if (t > 255) t = 255;
			dest[2] = t;
			bl += 3;
			dest += 3;
		}
	}
}

static void StoreLightMap(int stride, int smax, int tmax, byte *dest)
{
	if (lightmode == 2)
	{
		lightmapstore_mode2(stride, smax, tmax, dest);
	}
	else
	{
		lightmapstore_mode0(stride, smax, tmax, dest);
	}
}

//Combine and scale multiple lightmaps into the 8.8 format in blocklights
static void R_BuildLightMap(msurface_t *surf, byte *dest, int stride, int numdlights)
{
	int smax, tmax, size, i, blocksize;
	byte *lightmap;

	surf->cached_dlight = !!numdlights;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	stride -= smax * 3;
	blocksize = size * 3;
	lightmap = surf->samples;

	// set to full bright if no light data
	if (/* r_fullbright.value || */ !cl.worldmodel->lightdata) 
	{
		for (i = 0; i < blocksize; i++)
			blocklights[i] = 255 << 8;
		goto store;
	}

	// clear to no light
	memset (blocklights, 0, blocksize * sizeof(*blocklights));

	AddAllLightMaps(lightmap, surf, blocksize);

	// add all the dynamic lights
	if (numdlights)
		R_AddDynamicLights(surf, numdlights);

	// bound, invert, and shift
store:
	StoreLightMap(stride, smax, tmax, dest);

}

static void R_UploadLightMap (int lightmapnum)
{
	glRect_t	*theRect;

	lightmap_modified[lightmapnum] = false;
	theRect = &lightmap_rectchange[lightmapnum];
	glPixelStorei(GL_UNPACK_ROW_LENGTH, BLOCK_WIDTH);
	glTexSubImage2D (GL_TEXTURE_2D, 0, theRect->l, theRect->t, theRect->w, theRect->h, GL_RGB, GL_UNSIGNED_BYTE,
		lightmaps + (((lightmapnum * BLOCK_HEIGHT + theRect->t) * BLOCK_WIDTH) + theRect->l) * 3);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	theRect->l = BLOCK_WIDTH;
	theRect->t = BLOCK_HEIGHT;
	theRect->h = 0;
	theRect->w = 0;
}

//Returns the proper texture for a given time and base texture
static texture_t *R_TextureAnimation (texture_t *base)
{
	int relative, count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (cl.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative)
	{
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


static void R_BlendLightmaps (void)
{
	unsigned int i;
	unsigned int j;
	unsigned int k;
	glpoly_t *p;
	float *v;

	glDepthMask (GL_FALSE);		// don't bother writing Z
	glBlendFunc (GL_ZERO, GL_SRC_COLOR);

	GL_SetAlphaTestBlend(0, !r_lightmap.value);

	for(k=0;k<(MAX_GLTEXTURES+31)/32;k++)
	{
		if (!lightmap_polys_used[k])
			continue;

		for (i=0;i<32;i++)
		{
			if (!(lightmap_polys_used[k]&(1<<i)))
				continue;

			p = lightmap_polys[k*32+i];
			GL_Bind(lightmap_textures + k*32 + i);
			if (lightmap_modified[k*32+i])
				R_UploadLightMap(k*32+i);
			for ( ; p; p = p->chain)
			{
				glBegin (GL_POLYGON);
				v = p->verts[0];
				for (j = 0; j < p->numverts; j++, v+= VERTEXSIZE)
				{
					glTexCoord2f (v[5], v[6]);
				glVertex3fv (v);
				}
				glEnd ();
			}
		}

		lightmap_polys_used[k] = 0;
	}

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);		// back to normal Z buffering
}

static void R_RenderDynamicLightmaps (msurface_t *fa)
{
	byte *base;
	int maps, smax, tmax;
	glRect_t *theRect;
	qboolean lightstyle_modified = false;
	int numdlights;

	c_brush_polys++;

	if (!r_dynamic.value && !fa->cached_dlight)
		return;

	// check for lightmap modification
	for (maps = 0; maps < MAXLIGHTMAPS && fa->styles[maps] != 255; maps++)
	{
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
		{
			lightstyle_modified = true;
			break;
		}
	}

	if (r_dynamic.value)
	{
		if (fa->dlightframe == r_framecount)
			numdlights = R_BuildDlightList (fa);
		else
			numdlights = 0;

		if (numdlights == 0 && !fa->cached_dlight && !lightstyle_modified)
			return;
	}
	else
		numdlights = 0;

	lightmap_modified[fa->lightmaptexturenum] = true;
	theRect = &lightmap_rectchange[fa->lightmaptexturenum];
	if (fa->light_t < theRect->t)
	{
		if (theRect->h)
			theRect->h += theRect->t - fa->light_t;
		theRect->t = fa->light_t;
	}
	if (fa->light_s < theRect->l)
	{
		if (theRect->w)
			theRect->w += theRect->l - fa->light_s;
		theRect->l = fa->light_s;
	}
	smax = (fa->extents[0] >> 4) + 1;
	tmax = (fa->extents[1] >> 4) + 1;
	if (theRect->w + theRect->l < fa->light_s + smax)
		theRect->w = fa->light_s - theRect->l + smax;
	if (theRect->h + theRect->t < fa->light_t + tmax)
		theRect->h = fa->light_t - theRect->t + tmax;
	base = lightmaps + fa->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 3;
	base += (fa->light_t * BLOCK_WIDTH + fa->light_s) * 3;
	R_BuildLightMap(fa, base, BLOCK_WIDTH * 3, numdlights);
}

static void R_RenderAllDynamicLightmaps(model_t *model)
{
	msurface_t *s;
	unsigned int waterline;
	unsigned int i;
	unsigned int k;

	for (i = 0; i < model->numtextures; i++)
	{
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		for (waterline = 0; waterline < 2; waterline++)
		{
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for ( ; s; s = s->texturechain)
			{
				GL_Bind(lightmap_textures + s->lightmaptexturenum);
				R_RenderDynamicLightmaps(s);
				k = s->lightmaptexturenum;
				if (lightmap_modified[k])
					R_UploadLightMap(k);
			}
		}
	}

	for (s = drawflatchain; s; s = s->texturechain)
	{
		GL_Bind(lightmap_textures + s->lightmaptexturenum);
		R_RenderDynamicLightmaps(s);
		k = s->lightmaptexturenum;
		if (lightmap_modified[k])
			R_UploadLightMap(k);
	}
}

void R_DrawWaterSurfaces (void)
{
	msurface_t *s;
	float wateralpha;

	if (!waterchain)
		return;

	wateralpha = cl.watervis ? bound(0, r_wateralpha.value, 1) : 1;

	GL_SetAlphaTestBlend(0, wateralpha < 1);

	if (wateralpha < 1.0)
	{
		glColor4f (1, 1, 1, wateralpha);
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		if (wateralpha < 0.9)
			glDepthMask (GL_FALSE);
	}

	GL_DisableMultitexture();
	for (s = waterchain; s; s = s->texturechain)
	{
		GL_Bind (s->texinfo->texture->gl_texturenum);
		EmitWaterPolys(cl.worldmodel, s);
	}
	waterchain = NULL;
	waterchain_tail = &waterchain;

	if (wateralpha < 1.0)
	{
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		glColor3ubv (color_white);
		if (wateralpha < 0.9)
			glDepthMask (GL_TRUE);
	}
}

//draws transparent textures for HL world and nonworld models
static void R_DrawAlphaChain (void)
{
	int k;
	msurface_t *s;
	texture_t *t;
	float *v;

	if (!alphachain)
		return;

	GL_SetAlphaTestBlend(1, 0);

	for (s = alphachain; s; s = s->texturechain)
	{


		t = s->texinfo->texture;
		R_RenderDynamicLightmaps (s);

		//bind the world texture
		GL_DisableMultitexture();
		GL_Bind (t->gl_texturenum);

		if (gl_mtexable)
		{
			//bind the lightmap texture
			GL_EnableMultitexture();
			GL_Bind (lightmap_textures + s->lightmaptexturenum);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			//update lightmap if its modified by dynamic lights
			k = s->lightmaptexturenum;
			if (lightmap_modified[k])
				R_UploadLightMap(k);
		}

		glBegin(GL_POLYGON);
		v = s->polys->verts[0];
		for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE)
		{
			if (gl_mtexable)
			{
				glMultiTexCoord2f (GL_TEXTURE0, v[3], v[4]);
				glMultiTexCoord2f (GL_TEXTURE1, v[5], v[6]);
			}
			else
			{
				glTexCoord2f (v[3], v[4]);
			}
			glVertex3fv (v);
		}
		glEnd ();
	}

	alphachain = NULL;

	GL_DisableMultitexture();
}

#define CHAIN_RESET(chain)			\
{								\
	chain = NULL;				\
	chain##_tail = &chain;		\
}

static void R_ClearTextureChains(model_t *clmodel)
{
	int i;
	texture_t *texture;

	memset (lightmap_polys_used, 0, sizeof(lightmap_polys_used));
	memset (fullbright_polys_used, 0, sizeof(fullbright_polys_used));
	memset (luma_polys_used, 0, sizeof(luma_polys_used));

	for (i = 0; i < clmodel->numtextures; i++)
	{
		if ((texture = clmodel->textures[i]))
		{
			texture->texturechain[0] = NULL;
			texture->texturechain[1] = NULL;
			texture->texturechain_tail[0] = &texture->texturechain[0];
			texture->texturechain_tail[1] = &texture->texturechain[1];
		}
	}

	r_notexture_mip->texturechain[0] = NULL;
	r_notexture_mip->texturechain_tail[0] = &r_notexture_mip->texturechain[0];
	r_notexture_mip->texturechain[1] = NULL;
	r_notexture_mip->texturechain_tail[1] = &r_notexture_mip->texturechain[1];


	CHAIN_RESET(skychain);
	if (clmodel == cl.worldmodel)
		CHAIN_RESET(waterchain);
	CHAIN_RESET(alphachain);
	CHAIN_RESET(drawflatchain);
}

static void FinishDraw(GLvoid *glelements, unsigned int *numglelements)
{
	if (*numglelements == 0)
		return;

	glDrawElements(GL_TRIANGLES, *numglelements, GL_UNSIGNED_INT, glelements);

	*numglelements = 0;
}

static void DrawTextureChains (model_t *model)
{
	int waterline, i, k, GL_LIGHTMAP_TEXTURE, GL_FB_TEXTURE;
	msurface_t *s;
	msurface_t *sprev;
	texture_t *t;
	float *v;
	unsigned int glindices[300];
	unsigned int numglelements;
	unsigned int basearrays;
	unsigned int extraarrays;

	qboolean render_lightmaps = false;
	qboolean drawLumasGlowing, doMtex1, doMtex2;

	qboolean draw_fbs, draw_caustics, draw_details;

	qboolean can_mtex_lightmaps, can_mtex_fbs;

	qboolean draw_mtex_fbs;

	qboolean mtex_lightmaps, mtex_fbs;

	drawLumasGlowing = (com_serveractive || cl.allow_lumas) && gl_fb_bmodels.value;

	draw_caustics = underwatertexture && gl_caustics.value;
	draw_details = detailtexture && gl_detail.value;

	GL_LIGHTMAP_TEXTURE = 0;
	GL_FB_TEXTURE = 0;

	if (drawLumasGlowing)
	{
		can_mtex_lightmaps = gl_mtexable;
		can_mtex_fbs = gl_textureunits >= 3;
	}
	else
	{
		can_mtex_lightmaps = gl_textureunits >= 3;
		can_mtex_fbs = gl_textureunits >= 3 && gl_add_ext;
	}

	GL_DisableMultitexture();
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	if (model->vertcoords)
	{
		if (gl_vbo)
		{
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->vbo_number);
			GL_VertexPointer(3, GL_FLOAT, 0, 0);
#if 0
			GL_ColorPointer(4, GL_FLOAT, 0, model->colouroffset);
#endif

			GL_TexCoordPointer(0, 2, GL_FLOAT, 0, (void *)(intptr_t)model->texcoords_vbo_offset[0]);

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
		else
		{
			glVertexPointer(3, GL_FLOAT, 0, model->vertcoords);

			glClientActiveTexture(GL_TEXTURE0);
			glTexCoordPointer(2, GL_FLOAT, 0, model->verttexcoords[0]);
		}

		basearrays = FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY;
	}

	for (i = 0; i < model->numtextures; i++)
	{
		extraarrays = 0;

		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1]))
			continue;

		t = R_TextureAnimation (model->textures[i]);
		//bind the world texture
		GL_SelectTexture(GL_TEXTURE0);
		GL_Bind (t->gl_texturenum);

		draw_fbs = t->isLumaTexture || gl_fb_bmodels.value;
		draw_mtex_fbs = draw_fbs && can_mtex_fbs;

		if (gl_mtexable)
		{
			if (t->isLumaTexture && !drawLumasGlowing)
			{
				if (gl_add_ext)
				{
					doMtex1 = true;
					GL_EnableTMU(GL_TEXTURE1);
					GL_FB_TEXTURE = GL_TEXTURE1;
					extraarrays |= FQ_GL_TEXTURE_COORD_ARRAY_1;
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);
					GL_Bind (t->fb_texturenum);

					mtex_lightmaps = can_mtex_lightmaps;
					mtex_fbs = true;

					if (mtex_lightmaps)
					{
						doMtex2 = true;
						GL_LIGHTMAP_TEXTURE = GL_TEXTURE2;
						extraarrays |= FQ_GL_TEXTURE_COORD_ARRAY_2;
						GL_EnableTMU(GL_LIGHTMAP_TEXTURE);
						glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
					}
					else
					{
						doMtex2 = false;
						render_lightmaps = true;
					}
				}
				else
				{
					GL_DisableTMU(GL_TEXTURE1);
					render_lightmaps = true;
					doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
				}
			}
			else
			{
				doMtex1 = true;
				GL_EnableTMU(GL_TEXTURE1);
				GL_LIGHTMAP_TEXTURE = GL_TEXTURE1;
				extraarrays |= FQ_GL_TEXTURE_COORD_ARRAY_1;
				glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

				mtex_lightmaps = true;
				mtex_fbs = t->fb_texturenum && draw_mtex_fbs;

				if (mtex_fbs)
				{
					doMtex2 = true;
					GL_FB_TEXTURE = GL_TEXTURE2;
					extraarrays |= FQ_GL_TEXTURE_COORD_ARRAY_2;
					GL_EnableTMU(GL_FB_TEXTURE);
					GL_Bind (t->fb_texturenum);
					glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, t->isLumaTexture ? GL_ADD : GL_DECAL);
				}
				else
				{
					doMtex2 = false;
				}
			}
		}
		else
		{
			render_lightmaps = true;
			doMtex1 = doMtex2 = mtex_lightmaps = mtex_fbs = false;
		}

		if (model->vertcoords)
			GL_SetArrays(basearrays | extraarrays);

		if (model->vertcoords && (mtex_fbs || mtex_lightmaps))
		{
			if (gl_vbo)
			{
				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->vbo_number);

				if (mtex_fbs)
				{
					GL_TexCoordPointer(GL_FB_TEXTURE - GL_TEXTURE0, 2, GL_FLOAT, 0, (void *)(intptr_t)model->texcoords_vbo_offset[0]);
				}

				if (mtex_lightmaps)
				{
					GL_TexCoordPointer(GL_LIGHTMAP_TEXTURE - GL_TEXTURE0, 2, GL_FLOAT, 0, (void *)(intptr_t)model->texcoords_vbo_offset[1]);
				}

				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			}
			else
			{
				if (mtex_fbs)
				{
					GL_TexCoordPointer(GL_FB_TEXTURE - GL_TEXTURE0, 2, GL_FLOAT, 0, model->verttexcoords[0]);
				}

				if (mtex_lightmaps)
				{
					GL_TexCoordPointer(GL_LIGHTMAP_TEXTURE - GL_TEXTURE0, 2, GL_FLOAT, 0, model->verttexcoords[1]);
				}
			}
		}

		numglelements = 0;
		sprev = 0;

		for (waterline = 0; waterline < 2; waterline++)
		{
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for ( ; s; s = s->texturechain)
			{
				if ((sprev && s->lightmaptexturenum != sprev->lightmaptexturenum) || numglelements + (s->polys->numverts-2)*3 > sizeof(glindices)/sizeof(*glindices))
					FinishDraw(glindices, &numglelements);

				if (mtex_lightmaps)
				{
					//bind the lightmap texture
					GL_SelectTexture(GL_LIGHTMAP_TEXTURE);
					GL_Bind (lightmap_textures + s->lightmaptexturenum);
				}
				else
				{
					if ((lightmap_polys_used[s->lightmaptexturenum/32]&(1<<(s->lightmaptexturenum%32))))
					{
						s->polys->chain = lightmap_polys[s->lightmaptexturenum];
					}
					else
					{
						s->polys->chain = 0;
					}

					lightmap_polys[s->lightmaptexturenum] = s->polys;

					if (!(lightmap_polys_used[s->lightmaptexturenum/32]&(1<<(s->lightmaptexturenum%32))))
						lightmap_polys_used[s->lightmaptexturenum/32] |= (1<<(s->lightmaptexturenum%32));
				}

				if (model->vertcoords)
				{
					for(k=0;k<s->polys->numverts-2;k++)
					{
						glindices[numglelements++] = s->polys->firstindex;
						glindices[numglelements++] = s->polys->firstindex + k + 1;
						glindices[numglelements++] = s->polys->firstindex + k + 2;
					}
					sprev = s;
				}
				else
				{
					glBegin(GL_POLYGON);
					v = s->polys->verts[0];
					for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE)
					{
						if (doMtex1)
						{
							glMultiTexCoord2f(GL_TEXTURE0, v[3], v[4]);

							if (mtex_lightmaps)
								glMultiTexCoord2f(GL_LIGHTMAP_TEXTURE, v[5], v[6]);

							if (mtex_fbs)
								glMultiTexCoord2f(GL_FB_TEXTURE, v[3], v[4]);
						}
						else
						{
							glTexCoord2f(v[3], v[4]);
						}
						glVertex3fv(v);
					}
					glEnd();
				}

				if (waterline && draw_caustics)
				{
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}
				if (!waterline && draw_details)
				{
					s->polys->detail_chain = detail_polys;
					detail_polys = s->polys;
				}

				if (t->fb_texturenum && draw_fbs && !mtex_fbs)
				{
					if (t->isLumaTexture)
					{
						if ((luma_polys_used[t->fb_texturenum/32]&((1<<(t->fb_texturenum%32)))))
						{
							s->polys->luma_chain = luma_polys[t->fb_texturenum];
						}
						else
						{
							s->polys->luma_chain = 0;
						}

						luma_polys[t->fb_texturenum] = s->polys;

						if (!(luma_polys_used[t->fb_texturenum/32]&((1<<(t->fb_texturenum%32)))))
						{
							luma_polys_used[t->fb_texturenum/32] |= (1<<(t->fb_texturenum%32));
							drawlumas = true;
						}
					}
					else
					{
						if ((fullbright_polys_used[t->fb_texturenum/32])&(1<<(t->fb_texturenum%32)))
						{
							s->polys->fb_chain = fullbright_polys[t->fb_texturenum];
						}
						else
						{
							s->polys->fb_chain = 0;
						}

						fullbright_polys[t->fb_texturenum] = s->polys;

						if (!((fullbright_polys_used[t->fb_texturenum/32])&(1<<(t->fb_texturenum%32))))
						{
							fullbright_polys_used[t->fb_texturenum/32] |= (1<<(t->fb_texturenum%32));
							drawfullbrights = true;
						}
					}
				}
			}
		}

		FinishDraw(glindices, &numglelements);

		if (doMtex1)
			GL_DisableTMU(GL_TEXTURE1);
		if (doMtex2)
			GL_DisableTMU(GL_TEXTURE2);
	}

	if (gl_mtexable)
		GL_SelectTexture(GL_TEXTURE0);

	if (drawLumasGlowing)
	{
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
		if (drawlumas)
			R_RenderLumas();
	}
	else
	{
		if (drawlumas)
			R_RenderLumas();
		if (render_lightmaps)
			R_BlendLightmaps();
		if (drawfullbrights)
			R_RenderFullbrights();
	}

	EmitCausticsPolys();
	EmitDetailPolys();
}

static void R_UpdateFlatColours(model_t *model)
{
	if (model->surface_colours_dirty && model->vertcolours && gl_vbo)
	{
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->vbo_number);
		qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, model->colours_vbo_offset, sizeof(*model->vertcolours)*3*model->num_vertices, model->vertcolours);
		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		model->surface_colours_dirty = 0;
	}
}

static void R_DrawFlat (model_t *model)
{
	msurface_t *s;
	msurface_t *sprev;
	int k;
	float *v;
	unsigned int glindices[300];
	unsigned int numglelements;

	if (r_drawflat_enable.value == 0)
		return;

	GL_DisableMultitexture();

	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_SelectTexture(GL_TEXTURE0);

	if (model->vertcoords)
	{
		glClientActiveTexture(GL_TEXTURE0);

		if (gl_vbo)
		{
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->vbo_number);
			GL_VertexPointer(3, GL_FLOAT, 0, (void *)(intptr_t)model->coords_vbo_offset);
			GL_ColorPointer(3, GL_FLOAT, 0, (void *)(intptr_t)model->colours_vbo_offset);
			GL_TexCoordPointer(0, 2, GL_FLOAT, 0, (void *)(intptr_t)model->texcoords_vbo_offset[1]);
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		}
		else
		{
			GL_VertexPointer(3, GL_FLOAT, 0, model->vertcoords);
			GL_ColorPointer(3, GL_FLOAT, 0, model->vertcolours);
			GL_TexCoordPointer(0, 2, GL_FLOAT, 0, model->verttexcoords[1]);
		}

		GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_COLOR_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);
	}

	numglelements = 0;
	sprev = 0;

	for (s = drawflatchain; s; s = s->texturechain)
	{
		if ((sprev && s->lightmaptexturenum != sprev->lightmaptexturenum) || numglelements + (s->polys->numverts-2)*3 > sizeof(glindices)/sizeof(*glindices))
			FinishDraw(glindices, &numglelements);

		GL_Bind (lightmap_textures + s->lightmaptexturenum);

		if (model->vertcoords)
		{
			for(k=0;k<s->polys->numverts-2;k++)
			{
				glindices[numglelements++] = s->polys->firstindex;
				glindices[numglelements++] = s->polys->firstindex + k + 1;
				glindices[numglelements++] = s->polys->firstindex + k + 2;
			}
			sprev = s;
		}
		else
		{
			v = s->polys->verts[0];
			glColor3f(s->color[0], s->color[1], s->color[2]);

			glBegin(GL_POLYGON);
			for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE)
			{
				glTexCoord2f(v[5], v[6]);
				glVertex3fv (v);
			}
			glEnd ();
		}
	}

	FinishDraw(glindices, &numglelements);

	glColor3f(1.0f, 1.0f, 1.0f);
}



void R_DrawBrushModel (entity_t *e)
{
	int i, k, underwater;
	unsigned int li;
	unsigned int lj;
	vec3_t mins, maxs;
	msurface_t *psurf;
	float dot;
	mplane_t *pplane;
	model_t *clmodel;
	qboolean rotated;
	unsigned char flags;

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		if (R_CullSphere (e->origin, clmodel->radius))
			return;
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);

		if (R_CullBox (mins, maxs))
			return;
	}

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface)
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
						if (!gl_flashblend.value || (cl_dlights[k].bubble && gl_flashblend.value != 2))
							R_MarkLights(clmodel, &cl_dlights[k], 1 << k, clmodel->hulls[0].firstclipnode);
					}
				}
			}
		}
	}

	glPushMatrix ();

	glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);
	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (e->angles[0], 0, 1, 0);
	glRotatef (e->angles[2], 1, 0, 0);

	R_ClearTextureChains(clmodel);

	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = PlaneDiff(modelorg, pplane);

		flags = clmodel->surfflags[clmodel->firstmodelsurface + i];

		//draw the water surfaces now, and setup sky/normal chains
		if (((flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON))
		 || (!(flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (flags & SURF_DRAWSKY)
			{
				CHAIN_SURF_B2F(psurf, skychain);
			}
			else if (flags & SURF_DRAWTURB)
			{
				EmitWaterPolys(clmodel, psurf);
			}
			else if (flags & SURF_DRAWALPHA)
			{
				CHAIN_SURF_B2F(psurf, alphachain);
			}
			else if (r_drawflat_enable.value == 1 && psurf->is_drawflat)
			{
				CHAIN_SURF_B2F(psurf, drawflatchain);
			}
			else
			{
				underwater = (flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain[underwater]);
			}
		}
	}

	//draw the textures chains for the model
	R_RenderAllDynamicLightmaps(clmodel);
	R_UpdateFlatColours(clmodel);
	DrawTextureChains(clmodel);
	R_DrawFlat(clmodel);
	R_DrawSkyChain();
	R_DrawAlphaChain ();

	glPopMatrix ();
}

static void R_RecursiveWorldNode(model_t *model, unsigned int nodenum, int clipflags)
{
	mnode_t *node;
	int c, side, clipped, underwater;
	mplane_t *plane, *clipplane;
	msurface_t *surf;
	unsigned int surfnum;
	unsigned short *mark;
	unsigned char flags;
	mleaf_t *pleaf;
	float dot;
	unsigned int leafnum;
	int isleaf;

	if (nodenum >= model->numnodes)
	{
		isleaf = 1;
		leafnum = nodenum - model->numnodes;

		if ((model->leafsolid[leafnum/32] & (1<<(leafnum%32))))
			return; // solid

		node = (mnode_t *)(model->leafs + leafnum);
	}
	else
	{
		isleaf = 0;
		node = model->nodes + nodenum;
	}

	if (node->visframe != r_visframecount)
		return;

	if (clipflags)
	{
		for (c = 0, clipplane = frustum; c < 4; c++, clipplane++)
		{
			if (!(clipflags & (1 << c)))
				continue;	// don't need to clip against it

			clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
			if (clipped == 2)
				return;
			else if (clipped == 1)
				clipflags &= ~(1<<c);	// node is entirely on screen
		}
	}

	// if a leaf node, draw stuff
	if (isleaf)
	{
		pleaf = (mleaf_t *)node;

		mark = model->marksurfaces + pleaf->firstmarksurfacenum;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				surfnum = *mark;
				cl.worldmodel->surfvisible[surfnum/32] |= (1<<(surfnum%32));
				mark++;
			} while(--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags(&pleaf->efrags);
	}
	else
	{
		// node is just a decision point, so go down the apropriate sides

		// find which side of the node we are on
		plane = model->planes + node->planenum;

		dot = PlaneDiff(modelorg, plane);
		side = (dot >= 0) ? 0 : 1;

		// recurse down the children, front side first
		R_RecursiveWorldNode(model, node->childrennum[side], clipflags);

		// draw stuff
		c = node->numsurfaces;

		if (c)
		{
			surf = cl.worldmodel->surfaces + node->firstsurface;
			surfnum = node->firstsurface;

			if (dot < -BACKFACE_EPSILON)
				side = SURF_PLANEBACK;
			else if (dot > BACKFACE_EPSILON)
				side = 0;

			for ( ; c; c--, surf++, surfnum++)
			{
				if (!(cl.worldmodel->surfvisible[surfnum/32]&(1<<(surfnum%32))))
					continue;

				flags = cl.worldmodel->surfflags[surfnum];

				if ((dot < 0) ^ !!(flags & SURF_PLANEBACK))
					continue;		// wrong side

				// add surf to the right chain
				if (flags & SURF_DRAWSKY)
				{
					CHAIN_SURF_F2B(surf, skychain_tail);
				}
				else if (flags & SURF_DRAWTURB)
				{
					CHAIN_SURF_F2B(surf, waterchain_tail);
				}
				else if (flags & SURF_DRAWALPHA)
				{
					CHAIN_SURF_B2F(surf, alphachain);
				}
				else if (r_drawflat_enable.value == 1 && surf->is_drawflat)
				{
					CHAIN_SURF_F2B(surf, drawflatchain_tail);
				}
				else
				{
					underwater = (flags & SURF_UNDERWATER) ? 1 : 0;
					CHAIN_SURF_F2B(surf, surf->texinfo->texture->texturechain_tail[underwater]);
				}
			}
		}

		// recurse down the back side
		R_RecursiveWorldNode(model, node->childrennum[!side], clipflags);
	}
}

void R_DrawWorld (void)
{
	entity_t ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	R_ClearTextureChains(cl.worldmodel);

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	//set up texture chains for the world
	memset(cl.worldmodel->surfvisible, 0, ((cl.worldmodel->numsurfaces+31)/32)*sizeof(*cl.worldmodel->surfvisible));
	R_RecursiveWorldNode(cl.worldmodel, 0, 15);

	//draw the world sky
	if (r_skyboxloaded)
		R_DrawSkyBox ();
	else
		R_DrawSkyChain ();

	R_DrawEntitiesOnList (&cl_firstpassents);

	//draw the world
	R_RenderAllDynamicLightmaps(cl.worldmodel);
	R_UpdateFlatColours(cl.worldmodel);
	DrawTextureChains(cl.worldmodel);
	R_DrawFlat(cl.worldmodel);

	//draw the world alpha textures
	R_DrawAlphaChain ();
}

void R_MarkLeaves (void)
{
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[MAX_MAP_LEAFS/8];

	if (!r_novis.value && r_oldviewleaf == r_viewleaf
		&& r_oldviewleaf2 == r_viewleaf2)	// watervis hack
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	}
	else
	{
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		if (r_viewleaf2)
		{
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (cl.worldmodel->numleafs + 7) >> 3;
			memcpy (solid, vis, count);
			src = (unsigned *) Mod_LeafPVS (r_viewleaf2, cl.worldmodel);
			dest = (unsigned *) solid;
			count = (count + 3) >> 2;
			for (i = 0; i < count; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}

	for (i = 0; i < cl.worldmodel->numleafs; i++)
	{
		if (vis[i >> 3] & (1 << (i & 7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
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


// returns a texture number and the position inside it
static int AllocBlock (int w, int h, int *x, int *y)
{
	int i, j, best, best2, texnum;

	if (w < 1 || w > BLOCK_WIDTH || h < 1 || h > BLOCK_HEIGHT)
		Sys_Error ("AllocBlock: Bad dimensions");

	for (texnum = 0; texnum < MAX_LIGHTMAPS; texnum++)
	{
		best = BLOCK_HEIGHT + 1;

		for (i = 0; i < /*BLOCK_WIDTH*/128 - w; i++)
		{
			best2 = 0;

			for (j = i; j < i + w; j++)
			{
				if (allocated[texnum][j] >= best)
				{
					i = j;
					break;
				}
				if (allocated[texnum][j] > best2)
					best2 = allocated[texnum][j];
			}
			if (j == i + w)
			{
				// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i = 0; i < w; i++)
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("AllocBlock: full");
	return 0;
}


static void BuildSurfaceDisplayList(model_t *model, msurface_t *fa)
{
	int i, lindex, lnumverts;
	medge_t *pedges, *r_pedge;
	float *vec, s, t;
	glpoly_t *poly;
	mvertex_t *vertbase;

	vertbase = model->vertexes;

	// reconstruct the polygon
	pedges = model->edges;
	lnumverts = fa->numedges;

	// draw texture
	poly = malloc(sizeof(glpoly_t) + (lnumverts - 4) * VERTEXSIZE*sizeof(float));
	if (poly == 0)
		Sys_Error("BuildSurfaceDisplayList: Out of memory\n");
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i = 0; i < lnumverts; i++)
	{
		lindex = model->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = vertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = vertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		// lightmap texture coordinates
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s * 16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t * 16;
		t += 8;
		t /= BLOCK_HEIGHT * 16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;

		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= 128;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= 128;

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][7] = s;
		poly->verts[i][8] = t;
	}

	poly->numverts = lnumverts;
}

static void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int smax, tmax;
	byte *base;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;

	if (smax > BLOCK_WIDTH)
		Host_Error("GL_CreateSurfaceLightmap: smax = %d > BLOCK_WIDTH", smax);
	if (tmax > BLOCK_HEIGHT)
		Host_Error("GL_CreateSurfaceLightmap: tmax = %d > BLOCK_HEIGHT", tmax);
	if (smax * tmax > MAX_LIGHTMAP_SIZE)
		Host_Error("GL_CreateSurfaceLightmap: smax * tmax = %d > MAX_LIGHTMAP_SIZE", smax * tmax);

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	base = lightmaps + surf->lightmaptexturenum * BLOCK_WIDTH * BLOCK_HEIGHT * 3;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * 3;
	R_BuildLightMap(surf, base, BLOCK_WIDTH * 3, 0);
}

static void BuildGLArrays(model_t *model)
{
	glpoly_t *polys;
	unsigned int i;
	unsigned int j;
	unsigned int vert;
	unsigned int totalverts;

#if VERTEXSIZE != 9
#error Fix this up.
#endif

	/* Performance-wise this doesn't make sense unless VBO support is available, so... */
	if (!gl_vbo && 0)
		return;

	totalverts = 0;
	for(i=0;i<model->numsurfaces;i++)
	{
		if (model->surfaces[i].polys)
			totalverts += model->surfaces[i].polys->numverts;
	}

	model->num_vertices = totalverts;

	if (totalverts > 200)
		printf("Model \"%s\" has %d verts\n", model->name, totalverts);

	if (totalverts && totalverts <= 131072)
	{
		model->vertcoords = malloc(sizeof(*model->vertcoords)*3*totalverts);
		model->vertcolours = malloc(sizeof(*model->vertcolours)*3*totalverts);
		model->verttexcoords[0] = malloc(sizeof(**model->verttexcoords)*2*totalverts);
		model->verttexcoords[1] = malloc(sizeof(**model->verttexcoords)*2*totalverts);
		model->verttexcoords[2] = malloc(sizeof(**model->verttexcoords)*2*totalverts);

		if (model->vertcoords && model->vertcolours && model->verttexcoords[0] && model->verttexcoords[1] && model->verttexcoords[2])
		{
			vert = 0;
			for(i=0;i<model->numsurfaces;i++)
			{
				polys = model->surfaces[i].polys;

				if (!polys)
					continue;

				polys->firstindex = vert;
				for(j=0;j<polys->numverts;j++)
				{
					model->vertcoords[3*vert+0] = polys->verts[j][0];
					model->vertcoords[3*vert+1] = polys->verts[j][1];
					model->vertcoords[3*vert+2] = polys->verts[j][2];
					model->vertcolours[3*vert+0] = 1;
					model->vertcolours[3*vert+1] = 1;
					model->vertcolours[3*vert+2] = 1;
					model->verttexcoords[0][2*vert+0] = polys->verts[j][3];
					model->verttexcoords[0][2*vert+1] = polys->verts[j][4];
					model->verttexcoords[1][2*vert+0] = polys->verts[j][5];
					model->verttexcoords[1][2*vert+1] = polys->verts[j][6];
					model->verttexcoords[2][2*vert+0] = polys->verts[j][7];
					model->verttexcoords[2][2*vert+1] = polys->verts[j][8];
					vert++;
				}
			}

			if (gl_vbo)
			{
				model->vbo_number = vbo_number++;
				model->coords_vbo_offset = 0;
				model->colours_vbo_offset = model->coords_vbo_offset + sizeof(*model->vertcoords)*3*totalverts;
				model->texcoords_vbo_offset[0] = model->colours_vbo_offset + sizeof(*model->vertcolours)*3*totalverts;
				model->texcoords_vbo_offset[1] = model->texcoords_vbo_offset[0] + sizeof(**model->verttexcoords)*2*totalverts;
				model->texcoords_vbo_offset[2] = model->texcoords_vbo_offset[1] + sizeof(**model->verttexcoords)*2*totalverts;

				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->vbo_number);

				qglBufferDataARB(GL_ARRAY_BUFFER_ARB, model->texcoords_vbo_offset[2] + sizeof(**model->verttexcoords)*2*totalverts, 0, GL_STATIC_DRAW_ARB);

				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, model->coords_vbo_offset, sizeof(*model->vertcoords)*3*totalverts, model->vertcoords);
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, model->texcoords_vbo_offset[0], sizeof(**model->verttexcoords)*2*totalverts, model->verttexcoords[0]);
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, model->texcoords_vbo_offset[1], sizeof(**model->verttexcoords)*2*totalverts, model->verttexcoords[1]);
				qglBufferSubDataARB(GL_ARRAY_BUFFER_ARB, model->texcoords_vbo_offset[2], sizeof(**model->verttexcoords)*2*totalverts, model->verttexcoords[2]);

				qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
			}

			return;
		}

		free(model->vertcoords);
		free(model->vertcolours);
		free(model->verttexcoords[0]);
		free(model->verttexcoords[1]);
		free(model->verttexcoords[2]);

		model->vertcoords = 0;
		model->vertcolours = 0;
		model->verttexcoords[0] = 0;
		model->verttexcoords[1] = 0;
		model->verttexcoords[2] = 0;
	}
}

static void BuildWarpVBOArrays(model_t *model)
{
	unsigned int totalverts;
	unsigned int vert;
	unsigned int i;
	unsigned int j;
	float *fastpolys;
	float *shadertexcoords;
	float *vertexdata;
	unsigned int texcoordoffset;

	if (!gl_vbo)
		return;

	totalverts = 0;
	for(i=0;i<model->numsurfaces;i++)
	{
		if (model->surfaces[i].fastpolys)
			totalverts += model->surfaces[i].numedges;
	}

	if (totalverts && totalverts <= 131072)
	{
		vertexdata = malloc(sizeof(*vertexdata)*5*totalverts);

		if (vertexdata)
		{
			vert = 0;
			texcoordoffset = totalverts*3;

			for(i=0;i<model->numsurfaces;i++)
			{
				fastpolys = model->surfaces[i].fastpolys;

				if (!fastpolys)
					continue;

				shadertexcoords = model->surfaces[i].shadertexcoords;

				model->surfaces[i].fastpolyfirstindex = vert;
				for(j=0;j<model->surfaces[i].numedges;j++)
				{
					vertexdata[vert*3 + 0] = *fastpolys++;
					vertexdata[vert*3 + 1] = *fastpolys++;
					vertexdata[vert*3 + 2] = *fastpolys++;

					vertexdata[texcoordoffset + vert*2 + 0] = *shadertexcoords++;
					vertexdata[texcoordoffset + vert*2 + 1] = *shadertexcoords++;

					vert++;
				}
			}

			model->warp_vbo_number = vbo_number++;
			model->warp_texcoords_vbo_offset = texcoordoffset * 4;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->warp_vbo_number);
			qglBufferDataARB(GL_ARRAY_BUFFER_ARB, totalverts * 5 * 4, vertexdata, GL_STATIC_DRAW_ARB);
			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

			free(vertexdata);
		}
	}
}

//Builds the lightmap texture with all the surfaces from all brush models
void GL_BuildLightmaps (void)
{
	int i, j;
	model_t	*m;

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;
		if (strchr(m->name, '*'))
			continue;
		if (m->type != mod_brush)
			continue;

		for (i = 0; i < m->numsurfaces; i++)
		{
			if (m->surfflags[i] & (SURF_DRAWTURB | SURF_DRAWSKY))
				continue;
			if (m->surfaces[i].texinfo->flags & TEX_SPECIAL)
				continue;
			GL_CreateSurfaceLightmap(m->surfaces + i);
			BuildSurfaceDisplayList(m, m->surfaces + i);
		}

		BuildGLArrays(m);
		BuildWarpVBOArrays(m);
	}

 	if (gl_mtexable)
 		GL_EnableMultitexture();

	// upload all lightmaps that were filled
	for (i = 0; i < MAX_LIGHTMAPS; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;
		GL_Bind(lightmap_textures + i);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D (GL_TEXTURE_2D, 0, gl_lightmap_format, BLOCK_WIDTH, BLOCK_HEIGHT, 0,
			GL_RGB, GL_UNSIGNED_BYTE, lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * 3);
	}

	if (gl_mtexable)
 		GL_DisableMultitexture();
}

void GL_RSurf_Init()
{
	lightmap_textures = texture_extension_number;
	texture_extension_number += MAX_LIGHTMAPS;
}

void GL_RSurf_Shutdown()
{
}

