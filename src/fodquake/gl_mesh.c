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
// gl_mesh.c: triangle model functions

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"

#include "gl_local.h"

/*
=================================================================

ALIAS MODEL DISPLAY LIST GENERATION

=================================================================
*/

static int MakeCollisionMap(aliashdr_t *hdr, unsigned short **collisionrevmap_ret, unsigned char **backside_ret)
{
	unsigned short *collisionmap;
	unsigned short *collisionrevmap;
	unsigned int collisions;
	unsigned char *backside;
	unsigned int vert;
	unsigned int tri;
	unsigned int i;
	char new;
	int ret;

	collisionmap = malloc(hdr->numverts*sizeof(*collisionmap));
	collisionrevmap = malloc(hdr->numverts*sizeof(*collisionrevmap));
	backside = malloc(hdr->numverts);

	ret = 0;

	if (collisionmap && collisionrevmap && backside)
	{
		collisions = 0;

		memset(backside, 3, hdr->numverts);

		for(tri=0;tri<hdr->numtris;tri++)
		{
			for(i=0;i<3;i++)
			{
				vert = triangles[tri].vertindex[i];
				if (!triangles[tri].facesfront && stverts[vert].onseam)
					new = 1;
				else
					new = 0;

				if (backside[vert] == 3)
					backside[vert] = new;
				else if (backside[vert] != 2 && backside[vert] != new)
				{
					collisionmap[collisions] = vert;
					collisionrevmap[vert] = hdr->numverts + collisions;
					backside[vert] = 2;
					collisions++;
				}
			}
		}

		hdr->totalverts = hdr->numverts + collisions;
		hdr->collisions = collisions;
		hdr->collisionmap = malloc(collisions * sizeof(*hdr->collisionmap));
		if (hdr->collisionmap)
		{
			for(i=0;i<collisions;i++)
			{
				hdr->collisionmap[i] = collisionmap[i];
			}

			*collisionrevmap_ret = collisionrevmap;
			*backside_ret = backside;

			ret = 1;
		}
	}

	free(collisionmap);
	if (!ret)
	{
		free(collisionrevmap);
		free(backside);
	}

	return ret;
}

static int MakeIndices(aliashdr_t *hdr, unsigned short *collisionrevmap, unsigned char *backside)
{
	unsigned short *indices;
	unsigned short min;
	unsigned short max;
	unsigned int vert;
	unsigned int tri;
	unsigned int i;

	indices = malloc(hdr->numtris*3*sizeof(*indices));
	if (indices)
	{
		min = 65535;
		max = 0;
		for(tri=0;tri<hdr->numtris;tri++)
		{
			for(i=0;i<3;i++)
			{
				vert = triangles[tri].vertindex[i];

				if (!triangles[tri].facesfront && stverts[vert].onseam && backside[vert] != 1)
					vert = collisionrevmap[vert];

				indices[tri*3 + i] = vert;

				if (vert > max)
					max = vert;
				if (vert < min)
					min = vert;
			}
		}

		hdr->indices = indices;
		hdr->indexmin = min;
		hdr->indexmax = max;

		return 1;
	}

	return 0;
}

static int MakeTextureCoordinates(aliashdr_t *hdr, unsigned char *backside)
{
	int vert;
	float s;
	float t;
	float *texcoords;

	texcoords = malloc(hdr->totalverts * 2 * sizeof(texcoords));
	if (texcoords)
	{
		for(vert=0;vert<hdr->numverts;vert++)
		{
			s = stverts[vert].s;
			t = stverts[vert].t;
			if (backside[vert] == 1)
				s += hdr->skinwidth / 2;

			texcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
			texcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
		}

		for(;vert<hdr->totalverts;vert++)
		{
			s = stverts[hdr->collisionmap[vert-hdr->numverts]].s;
			t = stverts[hdr->collisionmap[vert-hdr->numverts]].t;
			s += hdr->skinwidth / 2;

			texcoords[vert*2+0] = (s + 0.5) / hdr->skinwidth;
			texcoords[vert*2+1] = (t + 0.5) / hdr->skinheight;
		}

		hdr->texcoords = texcoords;

		return 1;
	}

	return 0;
}

static int MakeVBO(aliashdr_t *hdr)
{
	int *vertposes;
	float *vboverts;
	int pose;
	int vert;
	int i;
	unsigned int totalverts;

	if (!gl_vbo)
		return 1;

	totalverts = hdr->totalverts;

	vboverts = malloc(hdr->numposes * totalverts * 3 * sizeof(*vboverts));
	vertposes = malloc(hdr->numposes*sizeof(*vertposes));

	if (vboverts && vertposes)
	{
		for(pose=0;pose<hdr->numposes;pose++)
		{
			for(vert=0;vert<hdr->numverts;vert++)
			{
				vboverts[pose*totalverts*3 + vert*3 + 0] = poseverts[pose][vert].v[0];
				vboverts[pose*totalverts*3 + vert*3 + 1] = poseverts[pose][vert].v[1];
				vboverts[pose*totalverts*3 + vert*3 + 2] = poseverts[pose][vert].v[2];
			}

			for(;vert<totalverts;vert++)
			{
				vboverts[pose*totalverts*3 + vert*3 + 0] = poseverts[pose][hdr->collisionmap[vert-hdr->numverts]].v[0];
				vboverts[pose*totalverts*3 + vert*3 + 1] = poseverts[pose][hdr->collisionmap[vert-hdr->numverts]].v[1];
				vboverts[pose*totalverts*3 + vert*3 + 2] = poseverts[pose][hdr->collisionmap[vert-hdr->numverts]].v[2];
			}
		}

		hdr->vert_vbo_number = vertposes;
		vertposes = 0;

		for(i=0;i<hdr->numposes;i++)
		{
			hdr->vert_vbo_number[i] = vbo_number++;

			qglBindBufferARB(GL_ARRAY_BUFFER_ARB, hdr->vert_vbo_number[i]);
			qglBufferDataARB(GL_ARRAY_BUFFER_ARB, totalverts*3*sizeof(*vboverts), vboverts + totalverts*3*i, GL_STATIC_DRAW_ARB);
		}

		hdr->texcoord_vbo_number = vbo_number++;

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, hdr->texcoord_vbo_number);
		qglBufferDataARB(GL_ARRAY_BUFFER_ARB, totalverts*2*sizeof(*hdr->texcoords), hdr->texcoords, GL_STATIC_DRAW_ARB);

		qglBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);

		free(vboverts);

		return 1;
	}

	free(vertposes);
	free(vboverts);

	return 0;
}

/*
================
GL_MakeAliasModelDisplayLists
================
*/
void GL_MakeAliasModelDisplayLists (model_t *m, aliashdr_t *hdr)
{
	aliashdr_t *paliashdr;
	unsigned short *collisionrevmap;
	unsigned char *backside;
	int i;

	paliashdr = hdr;	// (aliashdr_t *)Mod_Extradata (m);

#if 0
	// Tonik: don't cache anything, because it seems just as fast
	// (if not faster) to rebuild the tris instead of loading them from disk
	BuildTris(paliashdr);		// trifans or lists
#endif

	if (!MakeCollisionMap(hdr, &collisionrevmap, &backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeCollisionMap() failed.");

	if (!MakeIndices(hdr, collisionrevmap, backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeIndices() failed.");

	if (!MakeTextureCoordinates(hdr, backside))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeTextureCoordinates() failed.");

	if (!MakeVBO(hdr))
		Sys_Error("GL_MakeAliasModelDisplayLists: MakeVBO() failed.");

	free(collisionrevmap);
	free(backside);

	paliashdr->realposeverts = malloc(paliashdr->numverts * paliashdr->numposes * sizeof(*paliashdr->realposeverts));
	for(i=0;i<paliashdr->numposes;i++)
	{
		memcpy(paliashdr->realposeverts + i * paliashdr->numverts, poseverts[i], paliashdr->numverts * sizeof(*paliashdr->realposeverts));
	}
}

