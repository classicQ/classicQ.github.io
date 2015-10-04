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

#include <math.h>

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"

int	r_dlightframecount;


void R_AnimateLight (void)
{
	int i, j, k;

	// light animations : 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int) (cl.time * 10);
	for (j = 0; j < MAX_LIGHTSTYLES; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}

		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k * 22;
		d_lightstylevalue[j] = k;
	}
}


static float bubble_sintable[17], bubble_costable[17];

void R_InitBubble(void)
{
	float a, *bub_sin, *bub_cos;
	int i;

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i = 16; i >= 0; i--)
	{
		a = i/16.0 * M_PI*2;
		*bub_sin++ = sin(a);
		*bub_cos++ = cos(a);
	}
}

static float bubblecolor[NUM_DLIGHTTYPES][4] =
{
	{ 0.2, 0.1, 0.05 },		// dimlight or brightlight (lt_default)
	{ 0.2, 0.1, 0.05 },		// muzzleflash
	{ 0.2, 0.1, 0.05 },		// explosion
	{ 0.2, 0.1, 0.05 },		// rocket
	{ 0.5, 0.05, 0.05 },	// red
	{ 0.05, 0.05, 0.3 },	// blue
	{ 0.5, 0.05, 0.4 },		// red + blue
	{ 0.05, 0.45, 0.05 },	// green
	{ 0.5, 0.5, 0.5},		// white
};

static void R_RenderDlight (dlight_t *light)
{
	int i, j;
	vec3_t v, v_right, v_up;
	float length, rad, *bub_sin, *bub_cos;

	// don't draw our own powerup glow and muzzleflashes
	if (light->key == (cl.viewplayernum + 1) ||
		light->key == -(cl.viewplayernum + 1)) // muzzleflash keys are negative
		return;

	rad = light->radius * 0.35;
	VectorSubtract (light->origin, r_origin, v);
	length = VectorNormalize (v);

	if (length < rad)
	{
		// view is inside the dlight
		V_AddLightBlend (1, 0.5, 0, light->radius * 0.0003);
		return;
	}

	glBegin (GL_TRIANGLE_FAN);
	glColor3fv (bubblecolor[light->type]);

	VectorVectors(v, v_right, v_up);

	if (length - rad > 8)
	{
		VectorScale (v, rad, v);
	}
	else
	{
		// make sure the light bubble will not be clipped by near z clip plane
		VectorScale (v, length - 8, v);
	}

	VectorSubtract (light->origin, v, v);

	glVertex3fv (v);
	glColor3ubv (color_black);

	bub_sin = bubble_sintable;
	bub_cos = bubble_costable;

	for (i = 16; i >= 0; i--)
	{
		for (j = 0; j < 3; j++)
			v[j] = light->origin[j] + (v_right[j]*(*bub_cos) +
				+ v_up[j]*(*bub_sin)) * rad;
		bub_sin++;
		bub_cos++;
		glVertex3fv (v);
	}

	glEnd ();
}

void R_RenderDlights (void)
{
	unsigned int i;
	unsigned int j;
	dlight_t *l;

	if (!gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't advanced yet for this frame
	glDepthMask (GL_FALSE);
	glDisable (GL_TEXTURE_2D);
	glShadeModel (GL_SMOOTH);
	GL_SetAlphaTestBlend(0, 1);
	glBlendFunc (GL_ONE, GL_ONE);

	for(i=0;i<MAX_DLIGHTS/32;i++)
	{
		if (cl_dlight_active[i])
		{
			for(j=0;j<32;j++)
			{
				if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS)
				{
					l = cl_dlights + i*32 + j;

					if (l->bubble && ((int) gl_flashblend.value != 2))
						R_MarkLights(cl.worldmodel, l, 1 << (i*32 + j), 0);
					else
						R_RenderDlight(l);
				}
			}
		}
	}

	glColor3ubv (color_white);
	glEnable (GL_TEXTURE_2D);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);
}


void R_MarkLights(model_t *model, dlight_t *light, int bit, unsigned int nodenum)
{
	mnode_t *node;
	mplane_t *splitplane;
	float dist;
	msurface_t *surf;
	int i;
	unsigned dlightframecount;

	if (nodenum >= model->numnodes)
		return;

	node = model->nodes + nodenum;

	splitplane = model->planes + node->planenum;
	dist = PlaneDiff(light->origin, splitplane);

	if (dist > light->radius)
	{
		R_MarkLights(model, light, bit, node->childrennum[0]);
		return;
	}
	if (dist < -light->radius)
	{
		R_MarkLights(model, light, bit, node->childrennum[1]);
		return;
	}

	dlightframecount = r_dlightframecount;

	// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->dlightframe != dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	R_MarkLights(model, light, bit, node->childrennum[0]);
	R_MarkLights(model, light, bit, node->childrennum[1]);
}

void R_PushDlights (void)
{
	unsigned int i;
	unsigned int j;
	dlight_t *l;

	if (gl_flashblend.value)
		return;

	r_dlightframecount = r_framecount + 1;	// because the count hasn't
											//  advanced yet for this frame

	for(i=0;i<MAX_DLIGHTS/32;i++)
	{
		if (cl_dlight_active[i])
		{
			for(j=0;j<32;j++)
			{
				if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS)
				{
					l = cl_dlights + i*32 + j;

					R_MarkLights(cl.worldmodel, l, 1<<(i*32 + j), 0);
				}
			}
		}
	}
}


static mplane_t	*lightplane;
static vec3_t		lightspot;
static vec3_t		lightcolor;

static int RecursiveLightPoint(model_t *model, vec3_t color, unsigned int nodenum, vec3_t start, vec3_t end)
{
	mnode_t *node;
	mplane_t *plane;
	float front, back, frac;
	vec3_t mid;

loc0:
	if (nodenum >= model->numnodes)
		return false;		// didn't hit anything

	node = model->nodes + nodenum;

	plane = model->planes + node->planenum;

	// calculate mid point
	if (plane->type < 3)
	{
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	}
	else
	{
		front = DotProduct(start, plane->normal) - plane->dist;
		back = DotProduct(end, plane->normal) - plane->dist;
	}
	// optimized recursion
	if ((back < 0) == (front < 0))
	{
		nodenum = node->childrennum[front < 0];
		goto loc0;
	}

	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0]) * frac;
	mid[1] = start[1] + (end[1] - start[1]) * frac;
	mid[2] = start[2] + (end[2] - start[2]) * frac;

	// go down front side
	if (RecursiveLightPoint(model, color, node->childrennum[front < 0], start, mid))
	{
		return true;	// hit something
	}
	else
	{
		int i, ds, dt;
		msurface_t *surf;
		// check for impact on this node
		VectorCopy (mid, lightspot);
		lightplane = plane;
		surf = cl.worldmodel->surfaces + node->firstsurface;
		for (i = 0; i < node->numsurfaces; i++, surf++)
		{
			if (cl.worldmodel->surfflags[node->firstsurface + i] & SURF_DRAWTILED)
				continue;	// no lightmaps
			ds = (int) ((float) DotProduct (mid, surf->texinfo->vecs[0]) + surf->texinfo->vecs[0][3]);
			dt = (int) ((float) DotProduct (mid, surf->texinfo->vecs[1]) + surf->texinfo->vecs[1][3]);
			if (ds < surf->texturemins[0] || dt < surf->texturemins[1])
				continue;

			ds -= surf->texturemins[0];
			dt -= surf->texturemins[1];

			if (ds > surf->extents[0] || dt > surf->extents[1])
				continue;

			if (surf->samples)
			{
				//enhanced to interpolate lighting
				byte *lightmap;
				int maps, line3, dsfrac = ds & 15, dtfrac = dt & 15, r00 = 0, g00 = 0, b00 = 0, r01 = 0, g01 = 0, b01 = 0, r10 = 0, g10 = 0, b10 = 0, r11 = 0, g11 = 0, b11 = 0;
				float scale;
				line3 = ((surf->extents[0] >> 4) + 1) * 3;
				lightmap = surf->samples + ((dt >> 4) * ((surf->extents[0] >> 4) + 1) + (ds >> 4)) * 3; // LordHavoc: *3 for color

				for (maps = 0;maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
				{
					scale = (float) d_lightstylevalue[surf->styles[maps]] * 1.0 / 256.0;
					r00 += (float) lightmap[0] * scale;
					g00 += (float) lightmap[1] * scale;
					b00 += (float) lightmap[2] * scale;

					r01 += (float) lightmap[3] * scale;
					g01 += (float) lightmap[4] * scale;
					b01 += (float) lightmap[5] * scale;

					r10 += (float) lightmap[line3 + 0] * scale;
					g10 += (float) lightmap[line3 + 1] * scale;
					b10 += (float) lightmap[line3 + 2] * scale;

					r11 += (float) lightmap[line3 + 3] * scale;
					g11 += (float) lightmap[line3 + 4] * scale;
					b11 += (float) lightmap[line3 + 5] * scale;

					lightmap += ((surf->extents[0] >> 4) + 1) * ((surf->extents[1] >> 4) + 1) * 3; // LordHavoc: *3 for colored lighting
				}
				color[0] += (float) ((int) ((((((((r11 - r10) * dsfrac) >> 4) + r10)
					- ((((r01 - r00) * dsfrac) >> 4) + r00)) * dtfrac) >> 4)
					+ ((((r01 - r00) * dsfrac) >> 4) + r00)));
				color[1] += (float) ((int) ((((((((g11 - g10) * dsfrac) >> 4) + g10)
					- ((((g01 - g00) * dsfrac) >> 4) + g00)) * dtfrac) >> 4)
					+ ((((g01 - g00) * dsfrac) >> 4) + g00)));
				color[2] += (float) ((int) ((((((((b11 - b10) * dsfrac) >> 4) + b10)
					- ((((b01 - b00) * dsfrac) >> 4) + b00)) * dtfrac) >> 4)
					+ ((((b01 - b00) * dsfrac) >> 4) + b00)));
			}
			return true; // success
		}
		// go down back side
		return RecursiveLightPoint(model, color, node->childrennum[front >= 0], mid, end);
	}
}

int R_LightPoint (vec3_t p)
{
	vec3_t end;

	if (!cl.worldmodel->lightdata)
		return 255;

	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;

	lightcolor[0] = lightcolor[1] = lightcolor[2] = 0;
	RecursiveLightPoint(cl.worldmodel, lightcolor, 0, p, end);
	return (lightcolor[0] + lightcolor[1] + lightcolor[2]) / 3.0;
}
