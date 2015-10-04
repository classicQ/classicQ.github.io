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
// gl_warp.c -- sky and water polygons

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"
#include "gl_shader.h"

#include "utils.h"

static int waterprogram;

static char sky_initialised;

extern msurface_t *skychain;
extern msurface_t **skychain_tail;

extern cvar_t r_fastturb;

static int solidskytexture, alphaskytexture;
static float speedscale, speedscale2;		// for top sky and bottom sky

qboolean r_skyboxloaded;

static qboolean OnChange_r_skyname(cvar_t *v, char *s);
cvar_t r_skyname = { "r_skyname", "", 0, OnChange_r_skyname };
cvar_t gl_water_program = { "gl_water_program", "1" };

static void BoundPoly(int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int i, j;
	float *v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
	{
		for (j = 0; j < 3; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
	}
}

static void SubdividePolygon(msurface_t *warpface, int numverts, float *verts)
{
	int i, j, k, f, b;
	vec3_t mins, maxs, front[64], back[64];
	float m, *v, dist[64], frac, s, t;
	struct glwarppoly *poly;
	float subdivide_size;	

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = max(1, gl_subdivide_size.value);
	BoundPoly (numverts, verts, mins, maxs);

	for (i = 0; i < 3; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floor (m / subdivide_size + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j = 0; j < numverts; j++, v += 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v -= i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j = 0; j < numverts; j++, v += 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j + 1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j + 1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j + 1]);
				for (k = 0; k < 3; k++)
					front[f][k] = back[b][k] = v[k] + frac * (v[3 + k] - v[k]);
				f++;
				b++;
			}
		}
		SubdividePolygon(warpface, f, front[0]);
		SubdividePolygon(warpface, b, back[0]);
		return;
	}

	poly = malloc(sizeof(struct glwarppoly) + (numverts - 4) * WARPVERTEXSIZE * sizeof(float));
	if (poly == 0)
		Sys_Error("SubdividePolygon: Out of memory\n");
	poly->next = warpface->warppolys;
	warpface->warppolys = poly;
	poly->numverts = numverts;
	for (i = 0; i < numverts; i++, verts += 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

// Breaks a polygon up along axial 64 unit boundaries so that turbulent and sky warps can be done reasonably.
void GL_SubdivideSurface(model_t *model, msurface_t *fa)
{
	vec3_t verts[64];
	int i, lindex;
	float *vec;
	float s;
	float t;

	/* Build simple verts for fastturb/fastsky */
	fa->fastpolys = malloc(fa->numedges * 3 * sizeof(*fa->fastpolys));
	if (fa->fastpolys == 0)
		Sys_Error("GL_SubdivideSurface,: Out of memory\n");

	fa->shadertexcoords = malloc(fa->numedges * 3 * sizeof(*fa->shadertexcoords));
	if (fa->shadertexcoords == 0)
		Sys_Error("GL_SubdivideSurface,: Out of memory\n");

	// convert edges back to a normal polygon
	for (i = 0; i < fa->numedges && i < 64; i++)
	{
		lindex = model->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = model->vertexes[model->edges[lindex].v[0]].position;
		else
			vec = model->vertexes[model->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[i]);

		fa->fastpolys[i*3+0] = vec[0];
		fa->fastpolys[i*3+1] = vec[1];
		fa->fastpolys[i*3+2] = vec[2];

		s = DotProduct(vec, fa->texinfo->vecs[0]);
		t = DotProduct(vec, fa->texinfo->vecs[1]);
		fa->shadertexcoords[i*2+0] = s / 64.0;
		fa->shadertexcoords[i*2+1] = t / 64.0;
	}

	SubdividePolygon(fa, fa->numedges, verts[0]);
}


#define TURBSINSIZE 128
#define TURBSCALE ((float) TURBSINSIZE / (2 * M_PI))

static const byte turbsin[TURBSINSIZE] =
{
	127, 133, 139, 146, 152, 158, 164, 170, 176, 182, 187, 193, 198, 203, 208, 213, 
		217, 221, 226, 229, 233, 236, 239, 242, 245, 247, 249, 251, 252, 253, 254, 254, 
		255, 254, 254, 253, 252, 251, 249, 247, 245, 242, 239, 236, 233, 229, 226, 221, 
		217, 213, 208, 203, 198, 193, 187, 182, 176, 170, 164, 158, 152, 146, 139, 133, 
		127, 121, 115, 108, 102, 96, 90, 84, 78, 72, 67, 61, 56, 51, 46, 41, 
		37, 33, 28, 25, 21, 18, 15, 12, 9, 7, 5, 3, 2, 1, 0, 0, 
		0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 28, 33, 
		37, 41, 46, 51, 56, 61, 67, 72, 78, 84, 90, 96, 102, 108, 115, 121, 
};

__inline static float SINTABLE_APPROX(float time)
{
	float sinlerpf, lerptime, lerp;
	int sinlerp1, sinlerp2;

	sinlerpf = time * TURBSCALE;
	sinlerp1 = floor(sinlerpf);
	sinlerp2 = sinlerp1 + 1;
	lerptime = sinlerpf - sinlerp1;

	lerp =	turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) +
		turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;
	return -8 + 16 * lerp / 255.0;
}


static void EmitFlatPoly(msurface_t *fa)
{
	GL_SetArrays(FQ_GL_VERTEX_ARRAY);
	GL_VertexPointer(3, GL_FLOAT, 0, fa->fastpolys);
	glDrawArrays(GL_POLYGON, 0, fa->numedges);
}

static void EmitShaderPoly(msurface_t *fa)
{
	GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);
	GL_VertexPointer(3, GL_FLOAT, 0, fa->fastpolys);
	GL_TexCoordPointer(0, 2, GL_FLOAT, 0, fa->shadertexcoords);
	glDrawArrays(GL_POLYGON, 0, fa->numedges);
}

static void EmitShaderVBOPoly(model_t *model, msurface_t *fa)
{
	GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, model->warp_vbo_number);
	GL_VertexPointer(3, GL_FLOAT, 0, (void *)(intptr_t)(fa->fastpolyfirstindex*3*4));
	GL_TexCoordPointer(0, 2, GL_FLOAT, 0, (void *)(intptr_t)(model->warp_texcoords_vbo_offset + fa->fastpolyfirstindex*2*4));
	glDrawArrays(GL_TRIANGLE_FAN, 0, fa->numedges);
	qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
}

//Does a water warp on the pre-fragmented struct glwarppoly chain
void EmitWaterPolys(model_t *model, msurface_t *fa)
{
	struct glwarppoly *p;
	float *v, s, t, os, ot;
	int cltimeloc;
	int i;

	GL_DisableMultitexture();

	GL_SetAlphaTestBlend(0, 0);

	if (r_fastturb.value)
	{
		glDisable (GL_TEXTURE_2D);

		glColor3ubv ((byte *) &fa->texinfo->texture->colour);

		EmitFlatPoly (fa);

		glColor3ubv (color_white);
		glEnable (GL_TEXTURE_2D);
	}
	else if (waterprogram)
	{
		GL_Bind (fa->texinfo->texture->gl_texturenum);
		qglUseProgram(waterprogram);
		cltimeloc = qglGetUniformLocation(waterprogram, "cltime");
		qglUniform1f(cltimeloc, cl.time * (20.0/64.0));

		if (model->warp_vbo_number)
			EmitShaderVBOPoly(model, fa);
		else
			EmitShaderPoly(fa);

		qglUseProgram(0);
	}
	else
	{
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		GL_Bind (fa->texinfo->texture->gl_texturenum);
		for (p = fa->warppolys; p; p = p->next)
		{
			glBegin(GL_POLYGON);
			for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += WARPVERTEXSIZE)
			{
				os = v[3];
				ot = v[4];

				s = os + SINTABLE_APPROX(ot * 0.125 + cl.time);
				s *= (1.0 / 64);

				t = ot + SINTABLE_APPROX(os * 0.125 + cl.time);
				t *= (1.0 / 64);

				glTexCoord2f (s, t);
				glVertex3fv (v);
			}
			glEnd();
		}
	}
}

static void EmitSkyPolys(msurface_t *fa, qboolean mtex)
{
	struct glwarppoly *p;
	float *v, s, t, ss, tt, length;
	int i;
	vec3_t dir;

	for (p = fa->warppolys; p; p = p->next)
	{
		glBegin (GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += WARPVERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = VectorLength (dir);
			length = 6 * 63 / length;

			dir[0] *= length;
			dir[1] *= length;

			if (mtex)
			{
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);

				ss = (speedscale2 + dir[0]) * (1.0 / 128);
				tt = (speedscale2 + dir[1]) * (1.0 / 128);
			}
			else
			{
				s = (speedscale + dir[0]) * (1.0 / 128);
				t = (speedscale + dir[1]) * (1.0 / 128);
			}

			if (mtex)
			{
				glMultiTexCoord2f (GL_TEXTURE0, s, t);
				glMultiTexCoord2f (GL_TEXTURE1, ss, tt);
			}
			else
			{
				glTexCoord2f (s, t);
			}

			glVertex3fv (v);
		}
		glEnd ();
	}
}


void R_DrawSkyChain(void)
{
	msurface_t *fa;
	byte *col;

	if (!skychain)
		return;

	GL_DisableMultitexture();

	if (r_fastsky.value || cl.worldmodel->bspversion == HL_BSPVERSION)
	{
		glDisable (GL_TEXTURE_2D);

		col = StringToRGB(r_skycolor.string);
		glColor3ubv (col);

		for (fa = skychain; fa; fa = fa->texturechain)
			EmitFlatPoly (fa);

		glEnable (GL_TEXTURE_2D);
		glColor3ubv (color_white);
	}
	else
	{
		if (gl_mtexable)
		{
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			GL_Bind (solidskytexture);

			GL_EnableMultitexture();
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
			GL_Bind (alphaskytexture);

			speedscale = cl.time * 8;
			speedscale -= (int) speedscale & ~127;
			speedscale2 = cl.time * 16;
			speedscale2 -= (int) speedscale2 & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, true);

			GL_DisableMultitexture();
		}
		else
		{
			GL_Bind(solidskytexture);
			speedscale = cl.time * 8;
			speedscale -= (int) speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, false);

			GL_SetAlphaTestBlend(0, 1);
			GL_Bind (alphaskytexture);

			speedscale = cl.time * 16;
			speedscale -= (int) speedscale & ~127;

			for (fa = skychain; fa; fa = fa->texturechain)
				EmitSkyPolys (fa, false);
		}
	}
	skychain = NULL;
	skychain_tail = &skychain;
}

//A sky texture is 256 * 128, with the right side being a masked overlay
void R_InitSky(void *texturedata)
{
	int i, j, p, r, g, b;
	byte *src;
	unsigned trans[128 * 128], transpix, *rgba;

	src = texturedata;

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i = 0; i < 128; i++)
	{
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}
	}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	GL_Bind (solidskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	for (i = 0; i < 128; i++)
	{
		for (j = 0; j < 128; j++)
		{
			p = src[i * 256 + j];
			trans[(i * 128) + j] = p ? d_8to24table[p] : transpix;
		}
	}

	GL_Bind(alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	sky_initialised = 1;

	OnChange_r_skyname(&r_skyname, r_skyname.string);
}


static char *skybox_ext[6] = {"rt", "bk", "lf", "ft", "up", "dn"};


int R_SetSky(char *skyname)
{
	int i, error = 0;
	byte *data[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
	unsigned int imagewidth, imageheight;

	
	for (i = 0; i < 6; i++)
	{
		if (
		    !(data[i] = GL_LoadImagePixels (va("env/%s%s", skyname, skybox_ext[i]), 0, 0, &imagewidth, &imageheight, 0)) &&
		    !(data[i] = GL_LoadImagePixels (va("gfx/env/%s%s", skyname, skybox_ext[i]), 0, 0, &imagewidth, &imageheight, 0)) &&
		    !(data[i] = GL_LoadImagePixels (va("env/%s_%s", skyname, skybox_ext[i]), 0, 0, &imagewidth, &imageheight, 0)) &&
		    !(data[i] = GL_LoadImagePixels (va("gfx/env/%s_%s", skyname, skybox_ext[i]), 0, 0, &imagewidth, &imageheight, 0))
		   )
		{
			Com_Printf ("Couldn't load skybox \"%s\"\n", skyname);
			error = 1;
			
			goto cleanup;
		}
	}

	for (i = 0; i < 6; i++)
	{
		GL_Bind (skyboxtextures + i);
		GL_Upload32 ((unsigned int *) data[i], imagewidth, imageheight, TEX_NOCOMPRESS);
	}

	r_skyboxloaded = true;

cleanup:
	for (i = 0; i < 6; i++)
	{
		if (data[i])
			free(data[i]);
		else
			break;
	}

	return error;
}

static qboolean OnChange_r_skyname(cvar_t *v, char *skyname)
{
	if (!sky_initialised)
		return false;

	if (!skyname[0])
	{
		r_skyboxloaded = false;
		return false;
	}

	return R_SetSky(skyname);
}

void R_LoadSky_f(void)
{
	switch (Cmd_Argc())
	{
		case 1:
			if (r_skyboxloaded)
				Com_Printf("Current skybox is \"%s\"\n", r_skyname.string);
			else
				Com_Printf("No skybox has been set\n");
			break;
		case 2:
			if (!Q_strcasecmp(Cmd_Argv(1), "none"))
				Cvar_Set(&r_skyname, "");
			else
				Cvar_Set(&r_skyname, Cmd_Argv(1));
			break;
		default:
			Com_Printf("Usage: %s <skybox>\n", Cmd_Argv(0));
	}
}

static vec3_t skyclip[6] =
{
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1} 
};

// 1 = s, 2 = t, 3 = 2048
static int st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down
};

// s = [0]/[2], t = [1]/[2]
static int vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}
};

static float skymins[2][6], skymaxs[2][6];

static void DrawSkyPolygon(int nump, vec3_t vecs)
{
	int i,j, axis;
	vec3_t v, av;
	float s, t, dv, *vp;

	// decide which face it maps to
	VectorClear (v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd (vp, v, v);

	av[0] = fabs(v[0]);
	av[1] = fabs(v[1]);
	av[2] = fabs(v[2]);
	if (av[0] > av[1] && av[0] > av[2])
		axis = (v[0] < 0) ? 1 : 0;
	else if (av[1] > av[2] && av[1] > av[0])
		axis = (v[1] < 0) ? 3 : 2;
	else
		axis = (v[2] < 0) ? 5 : 4;

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];
		dv = (j > 0) ? vecs[j - 1] : -vecs[-j - 1];

		j = vec_to_st[axis][0];
		s = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		j = vec_to_st[axis][1];
		t = (j < 0) ? -vecs[-j -1] / dv : vecs[j-1] / dv;

		if (s < skymins[0][axis])
			skymins[0][axis] = s;
		if (t < skymins[1][axis])
			skymins[1][axis] = t;
		if (s > skymaxs[0][axis])
			skymaxs[0][axis] = s;
		if (t > skymaxs[1][axis])
			skymaxs[1][axis] = t;
	}
}

#define	MAX_CLIP_VERTS	64
static void ClipSkyPolygon(int nump, vec3_t vecs, int stage)
{
	float *norm, *v, d, e, dists[MAX_CLIP_VERTS];
	qboolean front, back;
	int sides[MAX_CLIP_VERTS], newc[2], i, j;
	vec3_t newv[2][MAX_CLIP_VERTS];

	if (nump > MAX_CLIP_VERTS - 2)
		Sys_Error ("ClipSkyPolygon: nump > MAX_CLIP_VERTS - 2");
	if (stage == 6)
	{
		// fully clipped, so draw it
		DrawSkyPolygon (nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct (v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
		{
			sides[i] = SIDE_ON;
		}
		dists[i] = d;
	}

	if (!front || !back)
	{
		// not clipped
		ClipSkyPolygon (nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy (vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy (v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy (v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i+1]);
		for (j = 0; j < 3; j++)
		{
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon (newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon (newc[1], newv[1][0], stage + 1);
}

static void R_AddSkyBoxSurface(msurface_t *fa)
{
	int i;
	vec3_t verts[MAX_CLIP_VERTS];
	struct glwarppoly *p;

	// calculate vertex values for sky box
	for (p = fa->warppolys; p; p = p->next)
	{
		for (i = 0; i < p->numverts; i++)
			VectorSubtract (p->verts[i], r_origin, verts[i]);
		ClipSkyPolygon (p->numverts, verts[0], 0);
	}
}

static void R_ClearSkyBox(void) 
{
	int i;

	for (i = 0; i < 6; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}

static void MakeSkyVec(float s, float t, int axis)
{
	vec3_t v, b;
	int j, k, farclip;

	farclip = max((int) r_farclip.value, 4096);
	b[0] = s * (farclip >> 1);
	b[1] = t * (farclip >> 1);
	b[2] = (farclip >> 1);

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		v[j] = (k < 0) ? -b[-k - 1] : b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	s = bound(1.0 / 512, s, 511.0 / 512);
	t = bound(1.0 / 512, t, 511.0 / 512);

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

static int skytexorder[6] = {0, 2, 1, 3, 4, 5};
void R_DrawSkyBox(void)
{
	int i;
	msurface_t *fa;

	if (!skychain)
		return;

	R_ClearSkyBox();
	for (fa = skychain; fa; fa = fa->texturechain)
		R_AddSkyBoxSurface (fa);

	GL_DisableMultitexture();

	for (i = 0; i < 6; i++)
	{
		if (skymins[0][i] >= skymaxs[0][i] || skymins[1][i] >= skymaxs[1][i])
			continue;

		GL_Bind (skyboxtextures + skytexorder[i]);

		glBegin (GL_QUADS);
		MakeSkyVec (skymins[0][i], skymins[1][i], i);
		MakeSkyVec (skymins[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymaxs[1][i], i);
		MakeSkyVec (skymaxs[0][i], skymins[1][i], i);
		glEnd ();
	}

	glDisable(GL_TEXTURE_2D);
	glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	GL_SetAlphaTestBlend(0, 1);
	glBlendFunc(GL_ZERO, GL_ONE);

	for (fa = skychain; fa; fa = fa->texturechain)
		EmitFlatPoly (fa);

	glEnable (GL_TEXTURE_2D);
	glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	skychain = NULL;
	skychain_tail = &skychain;
}

static void CalcCausticTexCoords(float *v, float *s, float *t)
{
	float os, ot;

	os = v[3];
	ot = v[4];

	*s = os + SINTABLE_APPROX(0.465 * (cl.time + ot));
	*s *= -3 * (0.5 / 64);

	*t = ot + SINTABLE_APPROX(0.465 * (cl.time + os));
	*t *= -3 * (0.5 / 64);
}

void EmitCausticsPolys(void)
{
	glpoly_t *p;
	int i;
	float s, t, *v;
	extern glpoly_t *caustics_polys;

	if (!underwatertexture || !caustics_polys)
		return;

	GL_Bind (underwatertexture);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	GL_SetAlphaTestBlend(0, 1);

	for (p = caustics_polys; p; p = p->caustics_chain)
	{
		glBegin(GL_POLYGON);
		for (i = 0, v = p->verts[0]; i < p->numverts; i++, v += VERTEXSIZE)
		{
			CalcCausticTexCoords(v, &s, &t);

			glTexCoord2f(s, t);
			glVertex3fv(v);
		}
		glEnd();
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	caustics_polys = NULL;
}

void GL_Warp_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register(&r_skyname);
	Cvar_Register(&gl_water_program);
	Cvar_ResetCurrentGroup();
}

void GL_Warp_Init()
{
	solidskytexture = texture_extension_number++;
	alphaskytexture = texture_extension_number++;

	if (gl_fs && gl_water_program.value)
	{
		const char *prog = "#version 120\n"
		"uniform sampler2D mytex;\n"
		"uniform float cltime;\n"
		"const float mypi = 3.14159265358979323846;\n"
		"void main(void)\n"
		"{\n"
		"vec2 mycoords;\n"
		"float s;\n"
		"float t;\n"
		"float os;\n"
		"float ot;\n"
		"mycoords = vec2(gl_TexCoord[0]);\n"
		"os = mycoords[0];\n"
		"ot = mycoords[1];\n"
		"s = os + ((8.0/64.0) + sin((ot + cltime) * mypi) * (8.0/64.0));\n"
		"t = ot + ((8.0/64.0) + sin((os + cltime) * mypi) * (8.0/64.0));\n"
		"gl_FragColor = texture2D(mytex, vec2(s, t));\n"
		"}\n";

		waterprogram = GL_SetupShaderProgram(0, 0, 0, prog);
	}
}

void GL_Warp_Shutdown()
{
	if (waterprogram)
	{
		qglDeleteProgram(waterprogram);
		waterprogram = 0;
	}
}

