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
// r_light.c

#include "quakedef.h"
#include "r_local.h"

int	r_dlightframecount;


/*
==================
R_AnimateLight
==================
*/
void R_AnimateLight (void)
{
	int			i,j,k;
	
//
// light animations
// 'm' is normal light, 'a' is no light, 'z' is double bright
	i = (int)(cl.time*10);
	for (j=0 ; j<MAX_LIGHTSTYLES ; j++)
	{
		if (!cl_lightstyle[j].length)
		{
			d_lightstylevalue[j] = 256;
			continue;
		}
		k = i % cl_lightstyle[j].length;
		k = cl_lightstyle[j].map[k] - 'a';
		k = k*22;
		d_lightstylevalue[j] = k;
	}	
}


/*
=============================================================================

DYNAMIC LIGHTS

=============================================================================
*/

/*
=============
R_MarkLights
=============
*/
void R_MarkLights(model_t *model, dlight_t *light, int bit, unsigned int nodenum)
{
	mnode_t *node;
	mplane_t	*splitplane;
	float		dist;
	msurface_t	*surf;
	int			i;
loc0:

	if (nodenum >= model->numnodes)
		return;

	node = model->nodes + nodenum;

	splitplane = model->planes + node->planenum;

	dist = PlaneDiff(light->origin, splitplane);
	
	if (dist > light->radius)
	{
		nodenum = node->childrennum[0];
		goto loc0;
	}
	if (dist < -light->radius)
	{
		nodenum = node->childrennum[1];
		goto loc0;
	}
		
// mark the polygons
	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i = 0; i < node->numsurfaces; i++, surf++)
	{
		if (surf->dlightframe != r_dlightframecount)
		{
			surf->dlightbits = 0;
			surf->dlightframe = r_dlightframecount;
		}
		surf->dlightbits |= bit;
	}

	if (node->childrennum[0] < model->numnodes)
	{
		if (node->childrennum[1] < model->numnodes)
		{
			R_MarkLights(model, light, bit, node->childrennum[0]);
			nodenum = node->childrennum[1];
			goto loc0;
		}
		else
		{
			nodenum = node->childrennum[0];
			goto loc0;
		}
	}
	else if (node->childrennum[1] < model->numnodes)
	{
		nodenum = node->childrennum[1];
		goto loc0;
	}
}


/*
=============
R_PushDlights
=============
*/
void R_PushDlights (void)
{
	unsigned int i;
	unsigned int j;
	dlight_t	*l;

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


/*
=============================================================================

LIGHT SAMPLING

=============================================================================
*/

static int RecursiveLightPoint(model_t *model, unsigned int nodenum, vec3_t start, vec3_t end)
{
	mnode_t *node;
	int			r;
	float		front, back, frac;
	int			side;
	mplane_t	*plane;
	vec3_t		mid;
	msurface_t	*surf;
	int			s, t, ds, dt;
	int			i;
	mtexinfo_t	*tex;
	byte		*lightmap;
	unsigned	scale;
	int			maps;

	if (nodenum >= model->numnodes)
		return -1;		// didn't hit anything
	
	node = model->nodes + nodenum;

// calculate mid point

// FIXME: optimize for axial
	plane = model->planes + node->planenum;
	if (plane->type < 3) {
		front = start[plane->type] - plane->dist;
		back = end[plane->type] - plane->dist;
	} else {
		front = DotProduct (start, plane->normal) - plane->dist;
		back = DotProduct (end, plane->normal) - plane->dist;
	}
	side = front < 0;
	
	if ((back < 0) == side)
		return RecursiveLightPoint(model, node->childrennum[side], start, end);
	
	frac = front / (front-back);
	mid[0] = start[0] + (end[0] - start[0])*frac;
	mid[1] = start[1] + (end[1] - start[1])*frac;
	mid[2] = start[2] + (end[2] - start[2])*frac;
	
// go down front side	
	r = RecursiveLightPoint(model, node->childrennum[side], start, mid);
	if (r >= 0)
		return r;		// hit something
		
	if ( (back < 0) == side )
		return -1;		// didn't hit anuthing
		
// check for impact on this node

	surf = cl.worldmodel->surfaces + node->firstsurface;
	for (i=0 ; i<node->numsurfaces ; i++, surf++)
	{
		if (cl.worldmodel->surfflags[node->firstsurface + i] & SURF_DRAWTILED)
			continue;	// no lightmaps

		tex = surf->texinfo;
		
		s = DotProduct (mid, tex->vecs[0]) + tex->vecs[0][3];
		t = DotProduct (mid, tex->vecs[1]) + tex->vecs[1][3];;

		if (s < surf->texturemins[0] ||
		t < surf->texturemins[1])
			continue;
		
		ds = s - surf->texturemins[0];
		dt = t - surf->texturemins[1];
		
		if ( ds > surf->extents[0] || dt > surf->extents[1] )
			continue;

		if (!surf->samples)
			return 0;

		ds >>= 4;
		dt >>= 4;

		lightmap = surf->samples;
		r = 0;
		if (lightmap)
		{

			lightmap += dt * ((surf->extents[0]>>4)+1) + ds;

			for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
					maps++)
			{
				scale = d_lightstylevalue[surf->styles[maps]];
				r += *lightmap * scale;
				lightmap += ((surf->extents[0]>>4)+1) *
						((surf->extents[1]>>4)+1);
			}
			
			r >>= 8;
		}
		
		return r;
	}

// go down back side
	return RecursiveLightPoint(model, node->childrennum[!side], mid, end);
}

int R_LightPoint (vec3_t p)
{
	vec3_t		end;
	int			r;
	
	if (!cl.worldmodel->lightdata)
		return 255;
	
	end[0] = p[0];
	end[1] = p[1];
	end[2] = p[2] - 2048;
	
	r = RecursiveLightPoint(cl.worldmodel, 0, p, end);
	
	if (r == -1)
		r = 0;

	if (r < r_refdef.ambientlight)
		r = r_refdef.ambientlight;

	return r;
}

