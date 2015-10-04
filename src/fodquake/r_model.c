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
// r_model.c -- model loading and caching

// models are the only shared resource between a client and server running on the same machine.

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "quakedef.h"
#include "crc.h"
#include "r_local.h"
#include "skin.h"
#include "filesystem.h"
#include "image.h"
#ifdef NETQW
#include "netqw.h"
#endif

#include "fmod.h"

static void Mod_LoadSpriteModel(model_t *mod, void *buffer);
static void Mod_LoadBrushModel(model_t *mod, void *buffer);
static void Mod_LoadAliasModel(model_t *mod, void *buffer);

byte	mod_novis[MAX_MAP_LEAFS/8];

static model_t *firstmodel;

//Caches the data if needed
void *Mod_Extradata(model_t *mod)
{
	if (mod->extradata)
		return mod->extradata;

	Mod_LoadModel(mod, true);

	return mod->extradata;
}

mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t *node;
	unsigned int nodenum;
	float d;
	mplane_t *plane;

	if (!model || !model->nodes)
		Sys_Error ("Mod_PointInLeaf: bad model");

	nodenum = 0;
	while (1)
	{
		node = NODENUM_TO_NODE(model, nodenum);
		if (nodenum >= model->numnodes)
			return (mleaf_t *)node;
		plane = model->planes + node->planenum;
		d = PlaneDiff (p,plane);
		nodenum = (d > 0) ? node->childrennum[0] : node->childrennum[1];
	}

	return NULL;	// never reached
}

static byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int c, row;
	byte *out;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in)
	{
		// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);

	return decompressed;
}

byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}

static void Mod_FreeAliasData(model_t *model)
{
	mdl_t *othermodel;
	aliashdr_t *alias;
	maliasgroup_t *aliasgroup;
	maliasskingroup_t *skingroup;
	unsigned int i;
	unsigned int j;

	alias = model->extradata;
	if (alias)
	{
		othermodel = ((void *)alias) + alias->model;

		for(i=0;i<othermodel->numskins;i++)
		{
			if (alias->skindesc[i].type == ALIAS_SKIN_SINGLE)
			{
				free(alias->skindesc[i].skin);
			}
			else
			{
				skingroup = (maliasskingroup_t *)alias->skindesc[i].skin;

				for(j=0;j<skingroup->numskins;j++)
				{
					free(skingroup->skindescs[i].skin);
				}

				free(skingroup);
			}
		}

		for(i=0;i<model->numframes;i++)
		{
			if (alias->frames[i].type == ALIAS_SINGLE)
			{
				free(alias->frames[i].frame);
			}
			else
			{
				aliasgroup = (maliasgroup_t *)alias->frames[i].frame;

				for(j=0;j<aliasgroup->numframes;j++)
					free(aliasgroup->frames[j].frame);

				free(aliasgroup->intervals);
				free(aliasgroup);
			}
		}

		free(alias->skindesc);
		free(alias);
		model->extradata = 0;
	}
}

static void Mod_FreeSpriteData(model_t *model)
{
	unsigned int i;
	unsigned int j;
	msprite_t *sprite;
	mspritegroup_t *spritegroup;

	sprite = model->extradata;
	if (sprite)
	{
		for(i=0;i<sprite->numframes;i++)
		{
			if (sprite->frames[i].type == SPR_SINGLE)
			{
				free(sprite->frames[i].frameptr);
			}
			else
			{
				spritegroup = sprite->frames[i].frameptr;

				for(j=0;j<spritegroup->numframes;j++)
					free(spritegroup->frames[j]);

				free(spritegroup->intervals);

				free(spritegroup);
			}
		}

		free(sprite);
		model->extradata = 0;
	}
}

static void Mod_FreeBrushData(model_t *model)
{
	unsigned int i;

	free(model->submodels);
	model->submodels = 0;

	free(model->lightdata);
	model->lightdata = 0;

	free(model->visdata);
	model->visdata = 0;

	free(model->entities);
	model->entities = 0;

	free(model->vertexes);
	model->vertexes = 0;

	free(model->edges);
	model->edges = 0;

	free(model->texinfo);
	model->texinfo = 0;

	free(model->surfaces);
	model->surfaces = 0;

	free(model->surfvisibleunaligned);
	model->surfvisibleunaligned = 0;
	model->surfvisible = 0;

	free(model->surfflags);
	model->surfflags = 0;

	free(model->nodes);
	model->nodes = 0;

	free(model->leafsolidunaligned);
	model->leafsolidunaligned = 0;
	model->leafsolid = 0;

	free(model->leafs);
	model->leafs = 0;

	free(model->clipnodes);
	model->clipnodes = 0;

	free(model->marksurfaces);
	model->marksurfaces = 0;

	free(model->surfedges);
	model->surfedges = 0;

	free(model->planes);
	model->planes = 0;

	free(model->hulls[0].clipnodes);
	model->hulls[0].clipnodes = 0;

	if (model->textures)
	{
		for(i=0;i<model->numtextures;i++)
			free(model->textures[i]);

		free(model->textures);
		model->textures = 0;
	}
}

void Mod_ClearAll(void)
{
	model_t	*mod;
	model_t *next;

	next = firstmodel;
	while((mod = next))
	{
		next = mod->next;

		if (mod->type == mod_alias)
		{
			Mod_FreeAliasData(mod);
		}
		else if (mod->type == mod_sprite)
		{
			Mod_FreeSpriteData(mod);
		}
		else if (mod->type == mod_brush)
		{
			Mod_FreeBrushData(mod);
		}

		free(mod);
	}

	firstmodel = 0;
}

static model_t *Mod_FindName(const char *name)
{
	model_t	*mod;
	char namebuf[MAX_QPATH];
	const char *p;
	const char *searchname;
	int submodel;

	if ((p = strchr(name, '*')))
	{
		memcpy(namebuf, name, p-name);
		namebuf[p-name] = 0;
		searchname = namebuf;
	}
	else
		searchname = name;

	if (!searchname[0])
		Sys_Error ("Mod_ForName: NULL name");

	// search the currently loaded models
	mod = firstmodel;
	while(mod)
	{
		if (strcmp(mod->name, searchname) == 0)
			break;

		mod = mod->next;
	}

	if (p)
	{
		if (!mod)
			Sys_Error("Mod_FindName: Submodel for non-existant model %s requested\n", searchname);

		submodel = atoi(p + 1);
		if (!(submodel > 0 && submodel < mod->numsubmodels))
			Sys_Error("Mod_FindName: Requested submodel out of range\n", submodel);

		mod = &mod->submodels[submodel-1];
	}
	else if (mod == 0)
	{
		mod = malloc(sizeof(*mod));
		if (mod == 0)
			Sys_Error("Mod_FindName: Out of memory\n");

		memset(mod, 0, sizeof(*mod));

		strcpy (mod->name, name);
		mod->needload = true;

		mod->next = firstmodel;
		firstmodel = mod;
	}

	return mod;
}

//Loads a model into the cache
model_t *Mod_LoadModel (model_t *mod, qboolean crash)
{
	unsigned *buf;

	if (!mod->needload)
	{
		if (mod->type == mod_alias)
		{
			if (mod->extradata)
				return mod;
		}
		else
		{
			return mod;		// not cached at all
		}
	}

	// because the world is so huge, load it one piece at a time

	// load the file
	buf = (unsigned *)FS_LoadMallocFile(mod->name);
	if (!buf)
	{
		if (crash)
			Host_Error("Mod_LoadModel: %s not found", mod->name);
		return NULL;
	}

	// allocate a new model
	FMod_CheckModel(mod->name, buf, com_filesize);

	// fill it in

	// call the apropriate loader
	mod->needload = false;
	mod->modhint = 0;

	switch (LittleLong(*(unsigned *)buf))
	{
	case IDPOLYHEADER:
		Mod_LoadAliasModel (mod, buf);
		break;
	case IDSPRITEHEADER:
		Mod_LoadSpriteModel (mod, buf);
		break;
	default:
		Mod_LoadBrushModel (mod, buf);
		break;
	}

	free(buf);

	return mod;
}

//Loads in a model for the given name
model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;

	mod = Mod_FindName (name);

	return Mod_LoadModel (mod, crash);
}

/*
===============================================================================
					BRUSHMODEL LOADING
===============================================================================
*/

#define ISTURBTEX(model, name)		(((model)->bspversion == Q1_BSPVERSION && (name)[0] == '*') ||	\
							 ((model)->bspversion == HL_BSPVERSION && (name)[0] == '!'))

#define ISSKYTEX(name)		((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

byte	*mod_base;

static void Mod_LoadTextures(model_t *model, lump_t *l)
{
	int i, j, pixels, num, max, width, altmax, palette[224];
	char *s;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10];
	dmiptexlump_t *m;
	extern cvar_t r_max_size_1;
	int nodata;
	void *skydata;

	if (!l->filelen)
	{
		model->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *)(mod_base + l->fileofs);

	m->nummiptex = LittleLong (m->nummiptex);
	model->numtextures = m->nummiptex;

	if (m->nummiptex < 0 || m->nummiptex > 65535)
		Sys_Error("Mod_LoadTextures: Invalid number of textures\n");

	if (m->nummiptex)
	{
		model->textures = malloc(m->nummiptex * sizeof(*model->textures));
		if (model->textures == 0)
			Sys_Error("Mod_LoadTextures: Out of memory\n");
	}

	for (i = 0; i < m->nummiptex; i++)
	{
		model->textures[i] = 0;
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		if ((m->dataofs[i] & (1<<31)))
		{
			m->dataofs[i] &= ~(1<<31);
			nodata = 1;
		}
		else
			nodata = 0;

		mt = (miptex_t *)((byte *) m + m->dataofs[i]);
		mt->width = LittleLong (mt->width);
		mt->height = LittleLong (mt->height);

		if (!nodata)
		{
			for (j = 0; j < MIPLEVELS; j++)
				mt->offsets[j] = LittleLong (mt->offsets[j]);

			skydata = ((void *)mt) + mt->offsets[0];
		}
		else
			skydata = 0;

		if ((mt->width & 15) || (mt->height & 15))
			Host_Error("Mod_LoadTextures: Texture %s is not 16 aligned", mt->name);

		if (model->bspversion == HL_BSPVERSION)
		{
			width = ISTURBTEX(model, mt->name) ? 64 : 16;
			pixels = width * width;

			tx = model->textures[i] = malloc(sizeof(texture_t) + pixels);
			if (tx == 0)
				Sys_Error("Mod_LoadTextures: Out of memory\n");

			memset(tx, 0, sizeof(texture_t));

			memcpy (tx->name, mt->name, sizeof(tx->name));
			tx->height = tx->width = width;

			for (num = (mt->width << 4 | mt->height), s = mt->name; *s; s++)
				num += *s ^ i;

			memset (tx + 1, 16 + (num % 52) * 4, pixels);

			for (j = 0; j < MIPLEVELS; j++)
				tx->offsets[j] = sizeof(texture_t);
		}
		else if (model->isworldmodel && r_max_size_1.value && !ISTURBTEX(model, mt->name))
		{
			model->textures[i] = tx = malloc(sizeof(texture_t) + 16 * 16);
			if (tx == 0)
				Sys_Error("Mod_LoadTextures: Out of memory\n");

			memset(tx, 0, sizeof(texture_t));

			memcpy (tx->name, mt->name, sizeof(tx->name));
			tx->width = tx->height = 16;

			if (nodata)
			{
				num = 0;
			}
			else
			{
				num = *((byte *) mt + mt->offsets[0] + 2);
				if (r_max_size_1.value != 2 || num >= 224)
				{
					//find the most popular non-fullbright colour
					memset(palette, 0, sizeof(palette));
					for (j = 0; j < mt->width * mt->height; j++)
					{
						num = ((byte *) mt + mt->offsets[0])[j];
						if (num < 224)
							palette[num]++;
					}
					for (num = max = 0, j = 1; j < 224; j++)
					{
						if (palette[j] > max)
						{
							num = j;
							max = palette[j];
						}
					}

					if (!max)
						num = *((byte *) mt + mt->offsets[0] + ((mt->width * mt->height) >> 1));
				}
			}

			memset (tx + 1, num, 16 * 16);

			for (j = 0; j < MIPLEVELS; j++)
				tx->offsets[j] = sizeof(texture_t);
		}
		else
		{
			pixels = mt->width * mt->height / 64 * 85;

			model->textures[i] = tx = malloc(sizeof(texture_t) + pixels);
			if (tx == 0)
				Sys_Error("Mod_LoadTextures: Out of memory\n");

			memset(tx, 0, sizeof(texture_t));

			memcpy (tx->name, mt->name, sizeof(tx->name));

			tx->width = mt->width;
			tx->height = mt->height;

			if (nodata)
			{
				tx->offsets[0] = sizeof(*tx);
				tx->offsets[1] = tx->offsets[0] + tx->width * tx->height;
				tx->offsets[2] = tx->offsets[1] + (tx->width * tx->height) / 4;
				tx->offsets[3] = tx->offsets[2] + (tx->width * tx->height) / 16;

				{
					int x, y;
					unsigned char *d;

					for(j=0;j<4;j++)
					{
						d = ((void *)tx) + tx->offsets[j];
						for(y=0;y<tx->height/(1<<j);y++)
						{
							for(x=0;x<tx->width/(1<<j);x++)
							{
								d[x] = ((!!(x&(16/(1<<j)))) ^ (!!(y&(16/(1<<j)))))?0x0e:0;
							}

							d += tx->width/(1<<j);
						}
					}
				}
			}
			else
			{
				memcpy (tx + 1, mt + 1, pixels);

				for (j = 0; j < MIPLEVELS; j++)
					tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);
			}
		}

		if (model->isworldmodel && model->bspversion != HL_BSPVERSION && ISSKYTEX(mt->name))
		{
			if (skydata)
				R_InitSky(skydata);
			else
				R_InitSky(((void *)tx) + tx->offsets[0]);
		}
	}

	// sequence the animations
	for (i = 0; i < m->nummiptex; i++)
	{
		tx = model->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// already sequenced

		// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
		{
			Host_Error("Mod_LoadTextures: Bad animating texture %s", tx->name);
		}

		for (j = i + 1; j < m->nummiptex; j++)
		{
			tx2 = model->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
			{
				Host_Error("Mod_LoadTextures: Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_Error("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_Error("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

static void Mod_LoadLighting(model_t *model, lump_t *l)
{
	int i, j;
	byte *in;

	if (!l->filelen)
	{
		model->lightdata = NULL;
		return;
	}

	if (model->bspversion == HL_BSPVERSION)
	{
		if (l->filelen % 3)
			Host_Error("Mod_LoadLighting: l->filelen % 3");

		model->lightdata = malloc(l->filelen / 3);
		if (model->lightdata == 0)
			Sys_Error("Mod_LoadLighting: Out of memory\n");

		in = mod_base + l->fileofs;
		for (i = j = 0; i < l->filelen; i += 3, j++)
			model->lightdata[j] = (in[i + 0] + in[i + 1] + in[i + 2]) / 3.0;
	}
	else
	{
		model->lightdata = malloc(l->filelen);
		if (model->lightdata == 0)
			Sys_Error("Mod_LoadLighting: Out of memory\n");

		memcpy(model->lightdata, mod_base + l->fileofs, l->filelen);
	}
}

static void Mod_LoadVisibility(model_t *model, lump_t *l)
{
	if (!l->filelen)
	{
		model->visdata = NULL;
		return;
	}
	model->visdata = malloc(l->filelen);
	if (model->visdata == 0)
		Sys_Error("Mod_LoadVisibility: Out of memory\n");

	memcpy(model->visdata, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadEntities(model_t *model, lump_t *l)
{
	if (!l->filelen)
	{
		model->entities = NULL;
		return;
	}
	model->entities = malloc(l->filelen);
	if (model->entities == 0)
		Sys_Error("Mod_LoadEntities: Out of memory\n");

	memcpy(model->entities, mod_base + l->fileofs, l->filelen);
}

static void Mod_LoadVertexes(model_t *model, lump_t *l)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadVertexes: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadVertexes: Out of memory\n");

	model->vertexes = out;
	model->numvertexes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

static dmodel_t *Mod_LoadSubmodels(model_t *model, lump_t *l)
{
	dmodel_t *in, *out;
	dmodel_t *ret;
	int i, j, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadSubmodels: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);
	if (count > MAX_MODELS)
		Host_Error("Mod_LoadSubmodels : count > MAX_MODELS");

	model->numsubmodels = count;

	if (count)
	{
		out = malloc(count*sizeof(*out));
		if (out == 0)
			Sys_Error("Mod_LoadSubmodels: Out of memory\n");

		ret = out;

		for (i = 0; i < count; i++, in++, out++)
		{
			for (j = 0; j < 3; j++)
			{
				// spread the mins / maxs by a pixel
				out->mins[j] = LittleFloat (in->mins[j]) - 1;
				out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
				out->origin[j] = LittleFloat (in->origin[j]);
			}

			for (j = 0; j < MAX_MAP_HULLS; j++)
				out->headnode[j] = LittleLong (in->headnode[j]);

			out->visleafs = LittleLong (in->visleafs);
			out->firstface = LittleLong (in->firstface);
			out->numfaces = LittleLong (in->numfaces);
		}
	}
	else
		ret = 0;

	return ret;
}

static void Mod_LoadEdges(model_t *model, lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int i, count;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadEdges: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);
	out = malloc((count + 1) * sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadEdges: Out of memory\n");

	model->edges = out;
	model->numedges = count;

	for (i = 0; i < count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
		out->cachededgeoffset = 0;
	}
}

static void Mod_LoadTexinfo(model_t *model, lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, count, miptex;
	float len1, len2;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadTexinfo: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadTexinfo: Out of memory\n");

	model->texinfo = out;
	model->numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->vecs[0][0] = LittleFloat(in->vecs[0][0]);
		out->vecs[0][1] = LittleFloat(in->vecs[0][1]);
		out->vecs[0][2] = LittleFloat(in->vecs[0][2]);
		out->vecs[0][3] = LittleFloat(in->vecs[0][3]);
		out->vecs[1][0] = LittleFloat(in->vecs[1][0]);
		out->vecs[1][1] = LittleFloat(in->vecs[1][1]);
		out->vecs[1][2] = LittleFloat(in->vecs[1][2]);
		out->vecs[1][3] = LittleFloat(in->vecs[1][3]);

		len1 = VectorLength (out->vecs[0]);
		len2 = VectorLength (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32)
			out->mipadjust = 4;
		else if (len1 < 0.49)
			out->mipadjust = 3;
		else if (len1 < 0.99)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);

		if (!model->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= model->numtextures)
				Host_Error("Mod_LoadTexinfo: miptex >= model->numtextures");
			out->texture = model->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

//Fills in s->texturemins[] and s->extents[]
static void CalcSurfaceExtents(model_t *model, msurface_t *s)
{
	float mins[2], maxs[2], val;
	int i,j, e, bmins[2], bmaxs[2];
	mvertex_t *v;
	mtexinfo_t *tex;

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;

	for (i = 0;  i< s->numedges; i++)
	{
		e = model->surfedges[s->firstedge+i];
		if (abs(e) >= model->numedges)
			Host_Error("CalcSurfaceExtents: Bad surface edge");

		if (e >= 0)
			v = &model->vertexes[model->edges[e].v[0]];
		else
			v = &model->vertexes[model->edges[-e].v[1]];

		for (j = 0; j < 2; j++)
		{
			val = v->position[0] * tex->vecs[j][0] +
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i = 0; i < 2; i++)
	{
		bmins[i] = floor(mins[i]/16);
		bmaxs[i] = ceil(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if (!(tex->flags & TEX_SPECIAL) && s->extents[i] > 256)
			Host_Error("CalcSurfaceExtents: Bad surface extents");
	}
}

static void Mod_LoadFaces(model_t *model, lump_t *l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum, planenum, side;
	unsigned char flags;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadFaces: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);

	if (count > 65535)
		Sys_Error("Mod_LoadFaces: count > 65535");

	out = malloc(count*sizeof(*out));
	model->surfvisibleunaligned = malloc((((count+31)/32)*sizeof(*model->surfvisibleunaligned)) + 127);
	model->surfflags = malloc(count*sizeof(*model->surfflags));

	if (out == 0 || model->surfvisibleunaligned == 0 || model->surfflags == 0)
		Sys_Error("Mod_LoadFaces: Out of memory\n");

	memset(out, 0, count*sizeof(*out));

	model->surfvisible = (void *)((((uintptr_t)model->surfvisibleunaligned)+127)&~127);

	model->surfaces = out;
	model->numsurfaces = count;

	for (surfnum = 0; surfnum<count; surfnum++, in++, out++)
	{
		flags = 0;

		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			flags |= SURF_PLANEBACK;

		out->plane = model->planes + planenum;

		out->texinfo = model->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents(model, out);

		// lighting info
		for (i = 0 ; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];

		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = model->lightdata + (model->bspversion == HL_BSPVERSION ? i / 3: i);

		// set the drawing flags flag
		if (ISSKYTEX(out->texinfo->texture->name))
		{	// sky
			flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			model->surfflags[surfnum] = flags;
			continue;
		}

		if (ISTURBTEX(model, out->texinfo->texture->name))
		{		// turbulent
			flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			model->surfflags[surfnum] = flags;
			continue;
		}

		model->surfflags[surfnum] = flags;
	}
}

static void Mod_SetParent(model_t *model, unsigned int nodenum, unsigned int parentnum)
{
	mnode_t *node;

	node = NODENUM_TO_NODE(model, nodenum);

	node->parentnum = parentnum;
	if (nodenum >= model->numnodes)
		return;

	Mod_SetParent(model, node->childrennum[0], nodenum);
	Mod_SetParent(model, node->childrennum[1], nodenum);
}

static void Mod_LoadNodes(model_t *model, lump_t *l)
{
	int i, j, count, p;
	dnode_t *in;
	mnode_t *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadNodes: funny lump size in %s", model->name);

	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadNodes: Out of memory\n");

	memset(out, 0, count*sizeof(*out));

	model->nodes = out;
	model->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->planenum = p;

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);

		for (j = 0; j < 2; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->childrennum[j] = p;
			else
				out->childrennum[j] = model->numnodes + (-1 - p);
		}
	}

	Mod_SetParent(model, 0, 0xffff);	// sets nodes and leafs
}

static void Mod_LoadLeafs(model_t *model, lump_t *l)
{
	dleaf_t *in;
	mleaf_t *out;
	unsigned int i;
	int j, count, p;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadLeafs: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	model->leafsolidunaligned = malloc((((count+31)/32)*sizeof(*model->leafsolidunaligned)) + 127);
	if (out == 0 || model->leafsolidunaligned == 0)
		Sys_Error("Mod_LoadLeafs: Out of memory\n");

	memset(out, 0, count*sizeof(*out));

	model->leafsolid = (void *)((((uintptr_t)model->leafsolidunaligned)+127)&~127);
	memset(model->leafsolid, 0, ((count+31)/32)*sizeof(*model->leafsolid));

	model->leafs = out;
	model->numleafs = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		if (p == CONTENTS_SOLID)
			model->leafsolid[i/32] |= (1<<(i%32));

		out->firstmarksurfacenum = LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);

		p = LittleLong(in->visofs);
		out->compressed_vis = (p == -1) ? NULL : model->visdata + p;
		out->efrags = NULL;

		for (j = 0; j < 4; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];
	}
}

static void Mod_LoadClipnodes(model_t *model, lump_t *l)
{
	dclipnode_t *in, *out;
	int i, count;
	hull_t *hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadClipnodes: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadClipnodes: Out of memory\n");

	model->clipnodes = out;
	model->numclipnodes = count;

	if (model->bspversion == HL_BSPVERSION)
	{
		hull = &model->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = model->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;

		hull = &model->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = model->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;

		hull = &model->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = model->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -18;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 18;
	}
	else
	{
		hull = &model->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = model->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;

		hull = &model->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = model->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 64;
	}

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

//Deplicate the drawing hull structure as a clipping hull
static void Mod_MakeHull0(model_t *model)
{
	mnode_t *in, *child;
	dclipnode_t *out;
	int i, j, count;
	hull_t *hull;

	hull = &model->hulls[0];

	in = model->nodes;
	count = model->numnodes;
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_MakeHull0: Out of memory\n");

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = model->planes;

	for (i = 0; i < count; i++, out++, in++)
	{
		out->planenum = in->planenum;
		for (j = 0; j < 2; j++)
		{
			child = NODENUM_TO_NODE(model, in->childrennum[j]);
			if (in->childrennum[j] >= model->numnodes)
				out->children[j] = ((mleaf_t *)child)->contents;
			else
				out->children[j] = in->childrennum[j];
		}
	}
}

static void Mod_LoadMarksurfaces(model_t *model, lump_t *l)
{
	int i, j, count;
	short *in;
	unsigned short *out;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadMarksurfaces: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadMarksurfaces: Out of memory\n");

	model->marksurfaces = out;
	model->nummarksurfaces = count;

	for (i = 0; i < count; i++)
	{
		j = LittleShort(in[i]);
		if (j >= model->numsurfaces)
			Host_Error("Mod_LoadMarksurfaces: bad surface number");
		out[i] = j;
	}
}

static void Mod_LoadSurfedges(model_t *model, lump_t *l)
{
	int i, count, *in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadSurfedges: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadSurfedges: Out of memory\n");

	model->surfedges = out;
	model->numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong (in[i]);
}

static void Mod_LoadPlanes(model_t *model, lump_t *l)
{
	int i, j, count, bits;
	mplane_t *out;
	dplane_t *in;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error("Mod_LoadPlanes: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);

	if (count > 65535)
		Sys_Error("Mod_LoadPlanes: count > 65535");

	out = malloc(count*2*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadPlanes: Out of memory\n");

	model->planes = out;
	model->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;
		for (j = 0; j < 3; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}
		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

static float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength (corner);
}

static void Mod_LoadBrushModel(model_t *mod, void *buffer)
{
	int i, j;
	dheader_t *header;
	dmodel_t *bm;
	dmodel_t *submodels;
	model_t *nextmodel;
	model_t *mainmodel;
	unsigned int checksumvalue;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong (header->version);

	if (mod->bspversion != Q1_BSPVERSION && mod->bspversion != HL_BSPVERSION)
		Host_Error("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake) or %i (HalfLife))", mod->name, mod->bspversion, Q1_BSPVERSION, HL_BSPVERSION);

	mod->isworldmodel = !strcmp(mod->name, va("maps/%s.bsp", mapname.string));

#ifndef CLIENTONLY
	if (mod->isworldmodel)
	{
		extern cvar_t sv_halflifebsp;
		Cvar_ForceSet(&sv_halflifebsp, mod->bspversion == HL_BSPVERSION ? "1" : "0");
	}
#endif

	// swap all the lumps
	mod_base = (byte *)header;

	for (i = 0; i < sizeof(dheader_t) / 4; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

	mod->checksum = 0;
	mod->checksum2 = 0;

	// checksum all of the map, except for entities
	for (i = 0; i < HEADER_LUMPS; i++)
	{
		if (i == LUMP_ENTITIES)
			continue;

		checksumvalue = Com_BlockChecksum(mod_base + header->lumps[i].fileofs, header->lumps[i].filelen);

		mod->checksum ^= checksumvalue;

		if (i == LUMP_VISIBILITY || i == LUMP_LEAFS || i == LUMP_NODES)
			continue;

		mod->checksum2 ^= checksumvalue;
	}

	// load into heap
	if (!dedicated)
	{
		Mod_LoadVertexes(mod, &header->lumps[LUMP_VERTEXES]);
		Mod_LoadEdges(mod, &header->lumps[LUMP_EDGES]);
		Mod_LoadSurfedges(mod, &header->lumps[LUMP_SURFEDGES]);
		Mod_LoadTextures(mod, &header->lumps[LUMP_TEXTURES]);
		Mod_LoadLighting(mod, &header->lumps[LUMP_LIGHTING]);
	}
	Mod_LoadPlanes(mod, &header->lumps[LUMP_PLANES]);
	if (!dedicated)
	{
		Mod_LoadTexinfo(mod, &header->lumps[LUMP_TEXINFO]);
		Mod_LoadFaces(mod, &header->lumps[LUMP_FACES]);
		Mod_LoadMarksurfaces(mod, &header->lumps[LUMP_MARKSURFACES]);
	}
	Mod_LoadVisibility(mod, &header->lumps[LUMP_VISIBILITY]);
	Mod_LoadLeafs(mod, &header->lumps[LUMP_LEAFS]);
	Mod_LoadNodes(mod, &header->lumps[LUMP_NODES]);
	Mod_LoadClipnodes(mod, &header->lumps[LUMP_CLIPNODES]);
	Mod_LoadEntities(mod, &header->lumps[LUMP_ENTITIES]);
	submodels = Mod_LoadSubmodels(mod, &header->lumps[LUMP_MODELS]);

	Mod_MakeHull0(mod);

	mod->numframes = 2;		// regular and alternate animation

	if (mod->numsubmodels)
	{
		if (mod->numsubmodels > 1)
		{
			mod->submodels = malloc(sizeof(*mod->submodels)*(mod->numsubmodels-1));
			if (mod->submodels == 0)
				Sys_Error("Mod_LoadBrushModel: Out of memory\n");
		}
		else
			mod->submodels = 0;

		mainmodel = mod;

		// set up the submodels (FIXME: this is confusing)
		for (i = 0; i < mod->numsubmodels; i++)
		{
			bm = &submodels[i];

			mod->hulls[0].firstclipnode = bm->headnode[0];
			for (j = 1; j < MAX_MAP_HULLS; j++)
			{
				mod->hulls[j].firstclipnode = bm->headnode[j];
				mod->hulls[j].lastclipnode = mod->numclipnodes-1;
			}

			mod->firstmodelsurface = bm->firstface;
			mod->nummodelsurfaces = bm->numfaces;

			VectorCopy (bm->maxs, mod->maxs);
			VectorCopy (bm->mins, mod->mins);

			mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

			mod->numleafs = bm->visleafs;

			if (i < mod->numsubmodels - 1)
			{
				// duplicate the basic information

				nextmodel = &mod->submodels[i];
				*nextmodel = *mod;
				sprintf(nextmodel->name, "%s*%i", mainmodel->name, i+1);
				mod = nextmodel;
			}
		}
	}

	free(submodels);
}

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

static void *Mod_LoadAliasFrame(void * pin, trivertx_t **pframeindex, int numv, trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	trivertx_t *pframe, *pinframe;
	int	 i, j;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (name, pdaliasframe->name);

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);
	pframe = malloc(numv * sizeof(*pframe));
	if (pframe == 0)
		Sys_Error("Mod_LoadAliasFrame: Out of memory\n");

	*pframeindex = pframe;

	for (j = 0; j < numv; j++)
	{
		int		k;

		// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k = 0; k < 3; k++)
			pframe[j].v[k] = pinframe[j].v[k];
	}

	pinframe += numv;
	return pinframe;
}

static void *Mod_LoadAliasGroup(void * pin, maliasgroup_t **pframeindex, int numv, trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	daliasgroup_t *pingroup;
	maliasgroup_t *paliasgroup;
	int i, numframes;
	daliasinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	paliasgroup = malloc(sizeof (maliasgroup_t) + (numframes - 1) * sizeof (paliasgroup->frames[0]));
	if (paliasgroup == 0)
		Sys_Error("Mod_LoadAliasGroup: Out of memory\n");

	paliasgroup->numframes = numframes;

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	*pframeindex = paliasgroup;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	poutintervals = malloc(numframes * sizeof (float));
	if (poutintervals == 0)
		Sys_Error("Mod_LoadAliasGroup: Out of memory\n");

	paliasgroup->intervals = poutintervals;

	for (i = 0 ; i < numframes; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error("Mod_LoadAliasGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		ptemp = Mod_LoadAliasFrame(ptemp, &paliasgroup->frames[i].frame, numv, &paliasgroup->frames[i].bboxmin, &paliasgroup->frames[i].bboxmax, pheader, name);
	}

	return ptemp;
}

static void *Mod_LoadAliasSkin(model_t *model, mdl_t *mdl, unsigned int skinnum, void *pin, byte **pskinindex, aliashdr_t *pheader)
{
	char basename[64];
	char identifier[256];
	unsigned int skinwidth;
	unsigned int skinheight;
	unsigned int skinsize;
	byte *pskin;
	unsigned int w, h;

	skinwidth = mdl->skinwidth;
	skinheight = mdl->skinheight;
	skinsize = skinwidth * skinheight;

	if (model->modhint == MOD_PLAYER)
		Skin_SetDefault(pin, skinwidth, skinheight);

	COM_CopyAndStripExtension(COM_SkipPath(model->name), basename, sizeof(basename));

	snprintf(identifier, sizeof(identifier), "textures/models/%s_%i.pcx", basename, skinnum);
	pskin = Image_LoadPCX(0, identifier, skinwidth, skinheight, &w, &h);
	if (pskin == 0)
	{
		snprintf(identifier, sizeof(identifier), "textures/%s_%i.pcx", basename, skinnum);
		pskin = Image_LoadPCX(0, identifier, skinwidth, skinheight, &w, &h);
	}

	if (pskin == 0)
	{
		pskin = malloc(skinsize);
		if (pskin == 0)
			Sys_Error("Mod_LoadAliasSkin: Out of memory\n");

		memcpy(pskin, pin, skinsize);
	}

	*pskinindex = pskin;

	return pin + skinsize;
}

static void *Mod_LoadAliasSkinGroup(model_t *model, mdl_t *mdl, unsigned int skinnum, void *pin, byte **pskinindex, aliashdr_t *pheader)
{
	daliasskingroup_t *pinskingroup;
	maliasskingroup_t *paliasskingroup;
	int i, numskins;
	daliasskininterval_t *pinskinintervals;
	float *poutskinintervals;
	void *ptemp;

	pinskingroup = (daliasskingroup_t *)pin;

	numskins = LittleLong (pinskingroup->numskins);

	paliasskingroup = malloc(sizeof(maliasskingroup_t) + (numskins - 1) * sizeof(paliasskingroup->skindescs[0]));
	if (paliasskingroup == 0)
		Sys_Error("Mod_LoadAliasSkinGroup: Out of memory\n");

	paliasskingroup->numskins = numskins;

	*pskinindex = (byte *)paliasskingroup;

	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

	poutskinintervals = malloc(numskins * sizeof(float));
	if (poutskinintervals == 0)
		Sys_Error("Mod_LoadAliasSkinGroup: Out of memory\n");

	paliasskingroup->intervals = poutskinintervals;

	for (i = 0; i < numskins; i++)
	{
		*poutskinintervals = LittleFloat (pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Host_Error("Mod_LoadAliasSkinGroup: interval<=0");

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void *) pinskinintervals;

	for (i = 0; i < numskins; i++)
		ptemp = Mod_LoadAliasSkin(model, mdl, skinnum, ptemp, &paliasskingroup->skindescs[i].skin, pheader);

	return ptemp;
}

static void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
	int i, j, version, numframes, numskins, size;
	mdl_t *pmodel, *pinmodel;
	stvert_t *pstverts, *pinstverts;
	aliashdr_t *pheader;
	mtriangle_t *ptri;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	maliasskindesc_t *pskindesc;

	// some models are special
	if(!strcmp(mod->name, "progs/player.mdl"))
		mod->modhint = MOD_PLAYER;
	else if(!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else if (!strcmp(mod->name, "progs/flame.mdl") ||
		!strcmp(mod->name, "progs/flame2.mdl"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/backpack.mdl"))
		mod->modhint = MOD_BACKPACK;

	if (mod->modhint == MOD_PLAYER || mod->modhint == MOD_EYES)
	{
		unsigned short crc;
		char st[128];

		crc = CRC_Block (buffer, com_filesize);

		sprintf(st, "%d", (int) crc);
		Info_SetValueForKey (cls.userinfo, mod->modhint == MOD_PLAYER ? pmodel_name : emodel_name, st, MAX_INFO_STRING);

		if (cls.state >= ca_connected)
		{
#ifdef NETQW
			if (cls.netqw)
			{
				i = snprintf(st, sizeof(st), "%csetinfo %s %d",
					clc_stringcmd,
					mod->modhint == MOD_PLAYER ? pmodel_name : emodel_name,
					(int)crc);

				if (i < sizeof(st))
					NetQW_AppendReliableBuffer(cls.netqw, st, i + 1);
			}
#else
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			sprintf(st, "setinfo %s %d",
				mod->modhint == MOD_PLAYER ? pmodel_name : emodel_name,
				(int)crc);
			SZ_Print (&cls.netchan.message, st);
#endif
		}
	}

	pinmodel = (mdl_t *) buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Host_Error("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)", mod->name, version, ALIAS_VERSION);

	numframes = LittleLong(pinmodel->numframes);
	if (numframes < 1 || numframes > 65535)
		Host_Error("Mod_LoadAliasModel: Invalid number of alias frames");

	// allocate space for a working header, plus all the data except the frames, skin and group info
	size = sizeof (aliashdr_t) + (numframes - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mdl_t) +
			LittleLong (pinmodel->numverts) * sizeof (stvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = malloc(size);
	if (pheader == 0)
		Sys_Error("Mod_LoadAliasModel: Out of memory\n");

	pmodel = (mdl_t *) ((byte *)&pheader[1] +
			(numframes - 1) *
			 sizeof (pheader->frames[0]));

	//	mod->cache.data = pheader;
	mod->flags = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Host_Error("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	if (pmodel->skinwidth > 32768 || pmodel->skinwidth <= 0 || pmodel->skinheight > 32768 || pmodel->skinheight <= 0)
		Host_Error("Mod_LoadAliasModel: Invalid skin dimensions in %s", mod->name);

	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Host_Error("Mod_LoadAliasModel: model %s has no vertices", mod->name);

	if (pmodel->numverts > MAXALIASVERTS)
		Host_Error("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Host_Error("Mod_LoadAliasModel: model %s has no triangles", mod->name);

	pmodel->numframes = numframes;
	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i = 0; i < 3; i++)
	{
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
		Host_Error("Mod_LoadAliasModel: skinwidth not multiple of 4");

	pheader->model = (byte *) pmodel - (byte *)pheader;

	if (numskins < 1)
		Host_Error("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	pskintype = (daliasskintype_t *)&pinmodel[1];

	pskindesc = malloc(numskins * sizeof (maliasskindesc_t));
	if (pskindesc == 0)
		Sys_Error("Mod_LoadAliasModel: Out of memory\n");

	pheader->skindesc = pskindesc;

	for (i = 0; i < numskins; i++)
	{
		aliasskintype_t	skintype;

		skintype = LittleLong (pskintype->type);
		pskindesc[i].type = skintype;

		if (skintype == ALIAS_SKIN_SINGLE)
			pskintype = (daliasskintype_t *) Mod_LoadAliasSkin(mod, pmodel, i, pskintype + 1, &pskindesc[i].skin, pheader);
		else
			pskintype = (daliasskintype_t *) Mod_LoadAliasSkinGroup(mod, pmodel, i, pskintype + 1, &pskindesc[i].skin, pheader);
	}

	// set base s and t vertices
	pstverts = (stvert_t *)&pmodel[1];
	pinstverts = (stvert_t *)pskintype;

	pheader->stverts = (byte *)pstverts - (byte *)pheader;

	for (i = 0 ; i < pmodel->numverts; i++)
	{
		pstverts[i].onseam = LittleLong (pinstverts[i].onseam);
		// put s and t in 16.16 format
		pstverts[i].s = LittleLong (pinstverts[i].s) << 16;
		pstverts[i].t = LittleLong (pinstverts[i].t) << 16;
	}

	// set up the triangles
	ptri = (mtriangle_t *)&pstverts[pmodel->numverts];
	pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

	pheader->triangles = (byte *)ptri - (byte *)pheader;

	for (i = 0; i < pmodel->numtris; i++)
	{
		ptri[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++)
			ptri[i].vertindex[j] = LittleLong (pintriangles[i].vertindex[j]);
	}

	// load the frames
	pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

	for (i = 0; i < numframes; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;


		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1,
										(maliasgroup_t **)&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
	}

	mod->type = mod_alias;

	// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;

	mod->extradata = pheader;
}

//=============================================================================

static void *Mod_LoadSpriteFrame(void * pin, mspriteframe_t **ppframe)
{
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	int width, height, size, origin[2];

	pinframe = (dspriteframe_t *)pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = malloc(sizeof (mspriteframe_t) + size);
	if (pspriteframe == 0)
		Sys_Error("Mod_LoadSpriteFrame: Out of memory\n");

	memset (pspriteframe, 0, sizeof (mspriteframe_t) + size);
	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	memcpy (&pspriteframe->pixels[0], (byte *) (pinframe + 1), size);

	return (void *)((byte *)pinframe + sizeof (dspriteframe_t) + size);
}

static void *Mod_LoadSpriteGroup(void * pin, mspriteframe_t **ppframe)
{
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	int	 i, numframes;
	dspriteinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (dspritegroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = malloc(sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));
	if (pspritegroup == 0)
		Sys_Error("Mod_LoadSpriteGroup: Out of memory\n");

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)(pingroup + 1);

	poutintervals = malloc(numframes * sizeof (float));
	if (poutintervals == 0)
		Sys_Error("Mod_LoadSpriteGroup: Out of memory\n");

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error("Mod_LoadSpriteGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++)
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i]);

	return ptemp;
}

static void Mod_LoadSpriteModel(model_t *mod, void *buffer)
{
	int i, version, numframes, size;
	dsprite_t *pin;
	msprite_t *psprite;
	dspriteframetype_t *pframetype;

	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION)
		Host_Error("Mod_LoadSpriteModel: %s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = malloc(size);
	if (psprite == 0)
		Host_Error("Mod_LoadSpriteModel: Out of memory\n");

	mod->extradata = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

	// load the frames
	if (numframes < 1)
		Host_Error("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i = 0; i < numframes; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr);
		else
			pframetype = (dspriteframetype_t *) Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr);
	}

	mod->type = mod_sprite;
}

void Mod_Init(void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

void Mod_Shutdown(void)
{
	Mod_ClearAll();
}

