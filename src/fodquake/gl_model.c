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
// gl_model.c  -- model loading and caching

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// models are the only shared resource between a client and server running on the same machine.

#include "quakedef.h"
#include "gl_local.h"
#include "crc.h"
#include "wad.h"
#include "skin.h"
#include "teamplay.h"
#include "filesystem.h"
#ifdef NETQW
#include "netqw.h"
#endif

#include "fmod.h"

static void Mod_LoadSpriteModel (model_t *mod, void *buffer);
static void Mod_LoadBrushModel (model_t *mod, void *buffer);
static void Mod_LoadAliasModel (model_t *mod, void *buffer);

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
		d = PlaneDiff(p, plane);
		nodenum = (d > 0) ? node->childrennum[0] : node->childrennum[1];
	}

	return NULL;	// never reached
}

static byte *Mod_DecompressVis(byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS / 8];
	int c, row;
	byte *out;

	row = (model->numleafs + 7) >> 3;
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
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
	aliashdr_t *alias;

	alias = model->extradata;
	if (alias)
	{
		free(alias->collisionmap);
		free(alias->indices);
		free(alias->texcoords);
		free(alias->realposeverts);
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
				spritegroup = (mspritegroup_t *)sprite->frames[i].frameptr;

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
	struct glwarppoly *warppoly, *nextwarppoly;
	unsigned int i;

	free(model->submodels);
	model->submodels = 0;

	free(model->lightdata);
	model->lightdata = 0;

	if (model->surfaces)
	{
		for(i=0;i<model->numsurfaces;i++)
		{
			nextwarppoly = model->surfaces[i].warppolys;

			while((warppoly = nextwarppoly))
			{
				nextwarppoly = warppoly->next;
				free(warppoly);
			}

			free(model->surfaces[i].polys);
		}
	}

	free(model->surfaces);
	model->surfaces = 0;

	free(model->surfvisibleunaligned);
	model->surfvisibleunaligned = 0;
	model->surfvisible = 0;

	free(model->surfflags);
	model->surfflags = 0;

	free(model->visdata);
	model->visdata = 0;

	free(model->entities);
	model->entities = 0;

	free(model->vertexes);
	model->vertexes = 0;

	free(model->edges);
	model->edges = 0;

	free(model->planes);
	model->planes = 0;

	free(model->surfedges);
	model->surfedges = 0;

	free(model->marksurfaces);
	model->marksurfaces = 0;

	free(model->textures);
	model->textures = 0;

	free(model->texinfo);
	model->texinfo = 0;

	free(model->nodes);
	model->nodes = 0;

	free(model->leafsolidunaligned);
	model->leafsolidunaligned = 0;
	model->leafsolid = 0;

	free(model->leafs);
	model->leafs = 0;

	free(model->clipnodes);
	model->clipnodes = 0;

	free(model->hulls[0].clipnodes);
	model->hulls[0].clipnodes = 0;
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
			Sys_Error("Mod_FindName: Requested submodel %d out of range\n", submodel);

		mod = &mod->submodels[submodel-1];
	}
	else if (mod == 0)
	{
		mod = malloc(sizeof(*mod));
		if (mod == 0)
			Sys_Error("Mod_FindName: Out of memory\n");

		memset(mod, 0, sizeof(*mod));

		strcpy(mod->name, name);
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
			Host_Error ("Mod_LoadModel: %s not found", mod->name);

		return NULL;
	}

	// allocate a new model
	FMod_CheckModel(mod->name, buf, com_filesize);

	// call the apropriate loader
	mod->needload = false;

	switch (LittleLong(*((unsigned *) buf)))
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

qboolean Img_HasFullbrights (byte *pixels, int size)
{
    int i;

    for (i = 0; i < size; i++)
        if (pixels[i] >= 224)
            return true;

    return false;
}

/*
===============================================================================
					BRUSHMODEL LOADING
===============================================================================
*/


#define ISTURBTEX(model, name)		(((model)->bspversion == Q1_BSPVERSION && (name)[0] == '*') ||	\
							 ((model)->bspversion == HL_BSPVERSION && (name)[0] == '!'))

#define ISSKYTEX(name)		((name)[0] == 's' && (name)[1] == 'k' && (name)[2] == 'y')

#define ISALPHATEX(model, name)	((model)->bspversion == HL_BSPVERSION && (name)[0] == '{')

byte	*mod_base;


static int Mod_LoadExternalTexture(model_t *model, texture_t *tx, int mode)
{
	char *name, *mapname, *groupname;

	if (model->bspversion == HL_BSPVERSION)
		return 0;

	if (model->isworldmodel)
	{
		if (!gl_externalTextures_world.value)
			return 0;
	}
	else
	{
		if (!gl_externalTextures_bmodels.value)
			return 0;
	}

	name = tx->name;
	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, NULL);

	if (model->isworldmodel)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s/%s", mapname, name), name, 0, 0, mode)))
		{
			if (!ISTURBTEX(model, name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", mapname, name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		}
		else
		{
			if (groupname)
			{
				if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s/%s", groupname, name), name, 0, 0, mode)))
				{
					if (!ISTURBTEX(model, name))
						tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s/%s_luma", groupname, name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
				}
			}
		}
	}
	else
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s", name), name, 0, 0, mode)))
		{
			if (!ISTURBTEX(model, name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/bmodels/%s_luma", name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		}
	}

	if (!tx->gl_texturenum)
	{
		if ((tx->gl_texturenum = GL_LoadTextureImage (va("textures/%s", name), name, 0, 0, mode)))
		{
			if (!ISTURBTEX(model, name))
				tx->fb_texturenum = GL_LoadTextureImage (va("textures/%s_luma", name), va("@fb_%s", name), 0, 0, mode | TEX_LUMA);
		}
	}

	if (tx->fb_texturenum)
		tx->isLumaTexture = true;

	return tx->gl_texturenum;
}

static void Mod_LoadTextures(model_t *model, lump_t *l)
{
	char *texname;
	int i, j, num, max, altmax, width, height;
	int texmode, alpha_flag, brighten_flag, noscale_flag, mipTexLevel;
	miptex_t *mt;
	texture_t *tx, *tx2, *anims[10], *altanims[10], *txblock;
	dmiptexlump_t *m;
	byte *data;
	int nodata;
	void *freethis;

	if (!l->filelen)
	{
		model->textures = NULL;
		return;
	}

	m = (dmiptexlump_t *) (mod_base + l->fileofs);
	m->nummiptex = LittleLong (m->nummiptex);
	model->numtextures = m->nummiptex;

	if (m->nummiptex)
	{
		model->textures = malloc(m->nummiptex * sizeof(*model->textures) + m->nummiptex * sizeof(**model->textures));
		txblock = (texture_t *)(model->textures + m->nummiptex);

		if (model->textures == 0)
			Sys_Error("Mod_LoadTextures: Out of memory\n");

		memset(model->textures, 0, m->nummiptex * sizeof(*model->textures) + m->nummiptex * sizeof(**model->textures));
	}

	brighten_flag = (lightmode == 2) ? TEX_BRIGHTEN : 0;

	for (i = 0; i < m->nummiptex; i++)
	{
		m->dataofs[i] = LittleLong(m->dataofs[i]);
		if (m->dataofs[i] == -1)
			continue;

		if (m->dataofs[i]&(1<<31))
		{
			nodata = 1;
			m->dataofs[i] &= ~(1<<31);
		}
		else
			nodata = 0;

		mt = (miptex_t *) ((byte *) m + m->dataofs[i]);
		model->textures[i] = tx = txblock + i;

		memcpy (tx->name, mt->name, sizeof(tx->name));
		if (!tx->name[0])
		{
			snprintf(tx->name, sizeof(tx->name), "unnamed%d", i);
			Com_DPrintf("Warning: unnamed texture in %s, renaming to %s\n", model->name, tx->name);
		}

		tx->width = mt->width = LittleLong (mt->width);
		tx->height = mt->height = LittleLong (mt->height);

		if ((mt->width & 15) || (mt->height & 15))
			Host_Error ("Mod_LoadTextures: Texture %s is not 16 aligned", mt->name);

		if (!nodata)
		{
			for (j = 0; j < MIPLEVELS; j++)
				mt->offsets[j] = LittleLong (mt->offsets[j]);
		}

		// HACK HACK HACK
		if (!strcmp(mt->name, "shot1sid") && mt->width == 32 && mt->height == 32
		 && CRC_Block((byte *) (mt + 1), mt->width * mt->height) == 65393)
		{
			// This texture in b_shell1.bsp has some of the first 32 pixels painted white.
			// They are invisible in software, but look really ugly in GL. So we just copy
			// 32 pixels from the bottom to make it look nice.
			memcpy (mt + 1, (byte *) (mt + 1) + 32 * 31, 32);
		}

		if (!nodata && mt->offsets[0])
		{
			data = (byte *) &d_8to24table[*((byte *) mt + mt->offsets[0] + ((mt->height * mt->width) >> 1))];
			tx->colour = (255 << 24) + (data[0] << 0) + (data[1] << 8) + (data[2] << 16);
		}

		if (model->isworldmodel && model->bspversion != HL_BSPVERSION && ISSKYTEX(tx->name))
		{
			if (nodata)
			{
				width = tx->width;
				height = tx->height;
				freethis = malloc(width * height);
				if (!freethis)
					Sys_Error("Out of memory");

				{
					int x, y;
					unsigned char *d;

					d = freethis;

					for(y=0;y<height;y++)
					{
						for(x=0;x<width;x++)
						{
							d[x] = ((!!(x&(16))) ^ (!!(y&(16))))?0x0e:0;
						}

						d += width;
					}
				}
				R_InitSky(freethis);
				free(freethis);
			}
			else
				R_InitSky(((void *)mt) + mt->offsets[0]);

			continue;
		}

		noscale_flag = ((!gl_scaleModelTextures.value && !model->isworldmodel) ||
			(!gl_scaleTurbTextures.value && ISTURBTEX(model, tx->name))) ? TEX_NOSCALE : 0;
		texmode = TEX_MIPMAP | noscale_flag;
		mipTexLevel = noscale_flag ? 0 : gl_miptexLevel.value;

		if (Mod_LoadExternalTexture(model, tx, texmode))
			continue;

		if (model->bspversion == HL_BSPVERSION)
		{
			if ((data = WAD3_LoadTexture(mt)))
			{
				com_netpath[0] = 0;
				alpha_flag = ISALPHATEX(model, tx->name) ? TEX_ALPHA : 0;
				tx->gl_texturenum = GL_LoadTexturePixels (data, tx->name, tx->width, tx->height, texmode | alpha_flag);
				free(data);
				continue;
			}
		}


		if (!nodata && mt->offsets[0])
		{
			texname = tx->name;
			width = tx->width >> mipTexLevel;
			height = tx->height >> mipTexLevel;
			data = (byte *) mt + mt->offsets[mipTexLevel];
			freethis = 0;
		}
		else
		{
			texname = r_notexture_mip->name;
			width = 32 >> mipTexLevel;
			height = 32 >> mipTexLevel;
			data = (byte *) r_notexture_mip + r_notexture_mip->offsets[mipTexLevel];
			freethis = malloc(width * height);
			if (!freethis)
				Sys_Error("Out of memory");

			{
				int x, y;
				unsigned char *d;

				d = freethis;

				for(y=0;y<height;y++)
				{
					for(x=0;x<width;x++)
					{
						d[x] = ((!!(x&(16/(1<<mipTexLevel)))) ^ (!!(y&(16/(1<<mipTexLevel)))))?0x0e:0;
					}

					d += width;
				}
			}
			data = freethis;
		}
		tx->gl_texturenum = GL_LoadTexture (texname, width, height, data, texmode | brighten_flag, 1);
		if (!ISTURBTEX(model, tx->name) && Img_HasFullbrights(data, width * height))
			tx->fb_texturenum = GL_LoadTexture (va("@fb_%s", texname), width, height, data, texmode | TEX_FULLBRIGHT, 1);

		free(freethis);
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
			Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
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
				Host_Error ("Mod_LoadTextures: Bad animating texture %s", tx->name);
			}
		}

#define	ANIM_CYCLE	2
		// link them all together
		for (j = 0; j < max; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j + 1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j = 0; j < altmax; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Host_Error ("Mod_LoadTextures: Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j + 1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j + 1) % altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}


static byte *LoadColoredLighting(char *name, char **litfilename, int *litlength)
{
	char buf[8];
	FILE *f;
	int flen;
	int lit_ver;
	int r;
	qboolean system;
	byte *data;
	char *groupname, *mapname;
	extern cvar_t gl_loadlitfiles;

	if (!gl_loadlitfiles.value)
		return NULL;

	data = 0;

	mapname = TP_MapName();
	groupname = TP_GetMapGroupName(mapname, &system);

	if (strcmp(name, va("maps/%s.bsp", mapname)))
		return NULL;

	*litfilename = va("maps/lits/%s.lit", mapname);
	flen = FS_FOpenFile(*litfilename, &f);

	if (flen == -1)
	{
		*litfilename = va("maps/%s.lit", mapname);
		flen = FS_FOpenFile(*litfilename, &f);
	}

	if (flen == -1)
	{
		*litfilename = va("lits/%s.lit", mapname);
		flen = FS_FOpenFile(*litfilename, &f);
	}

	if (flen == -1 && groupname && !system)
	{
		*litfilename = va("maps/%s.lit", groupname);
		flen = FS_FOpenFile(*litfilename, &f);
	}

	if (flen == -1 && groupname && !system)
	{
		*litfilename = va("lits/%s.lit", groupname);
		flen = FS_FOpenFile(*litfilename, &f);
	}

	if (flen != -1)
	{
		if (flen <= 8 || fread(buf, 8, 1, f) != 1 || strncmp(buf, "QLIT", 4))
		{
			Com_Printf("Corrupt .lit file (%s)...ignoring\n", COM_SkipPath(*litfilename));
		}
		else if ((lit_ver = LittleLong(((int *)buf)[1])) != 1)
		{
			Com_Printf("Unknown .lit file version (v%d)\n", lit_ver);
		}
		else
		{
			*litlength = flen - 8;

			data = malloc(flen - 8);
			if (data == 0)
				Sys_Error("LoadColoredLighting: Out of memory\n");

			r = fread(data, flen - 8, 1, f);
			if (r != 1)
			{
				free(data);
				data = 0;
			}
		}

		fclose(f);
	}

	return data;
}

static void Mod_LoadLighting(model_t *model, lump_t *l)
{
	int i, b;
	byte *in, *out, *data, d;
	char *litfilename;
	int litlength;

	model->lightdata = NULL;
	if (!l->filelen)
		return;

	if (model->bspversion == HL_BSPVERSION)
	{
		model->lightdata = malloc(l->filelen);
		if (model->lightdata == 0)
			Sys_Error("Mod_LoadLighting: Out of memory\n");

		memcpy(model->lightdata, mod_base + l->fileofs, l->filelen);

		return;
	}

	//check for a .lit file
	data = LoadColoredLighting(model->name, &litfilename, &litlength);
	if (data)
	{
		if (l->filelen * 3 != litlength)
		{
			Com_Printf("Warning: .lit file (%s) has incorrect size\n", COM_SkipPath(litfilename));
		}
		else
		{
			if (developer.value || cl_warncmd.value)
				Com_Printf("Static coloured lighting loaded\n");

			model->lightdata = data;

			in = mod_base + l->fileofs;
			out = model->lightdata;
			for (i = 0; i < l->filelen; i++)
			{
				b = max(out[3 * i], max(out[3 * i + 1], out[3 * i + 2]));

				if (!b)
				{
					out[3 * i] = out[3 * i + 1] = out[3 * i + 2] = in[i];
				}
				else
				{
					float r = in[i] / (float) b;
					out[3 *i + 0] = (int) (r * out[3 * i + 0]);
					out[3 *i + 1] = (int) (r * out[3 * i + 1]);
					out[3 *i + 2] = (int) (r * out[3 * i + 2]);
				}
			}

			return;
		}

		free(data);
	}
	//no .lit found, expand the white lighting data to color
	model->lightdata = malloc(l->filelen * 3);
	if (model->lightdata == 0)
		Sys_Error("Mod_LoadLighting: Out of memory\n");

	in = mod_base + l->fileofs;
	out = model->lightdata;
	for (i = 0; i < l->filelen; i++, out += 3)
	{
		d = *in++;
		out[0] = out[1] = out[2] = d;
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


static void Mod_ParseWadsFromEntityLump(char *data)
{
	char *s, key[1024], value[1024];
	int i, j, k;

	if (!data || !(data = COM_Parse(data)))
		return;

	if (com_token[0] != '{')
		return; // error

	while (1)
	{
		if (!(data = COM_Parse(data)))
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		Q_strncpyz(key, (com_token[0] == '_') ? com_token + 1 : com_token, sizeof(key));

		for (s = key + strlen(key) - 1; s >= key && *s == ' '; s--)		// remove trailing spaces
			*s = 0;

		if (!(data = COM_Parse(data)))
			return; // error

		Q_strncpyz(value, com_token, sizeof(value));

		if (!strcmp("sky", key) || !strcmp("skyname", key))
			Cvar_Set(&r_skyname, value);

		if (!strcmp("wad", key))
		{
			j = 0;
			for (i = 0; i < strlen(value); i++)
			{
				if (value[i] != ';' && value[i] != '\\' && value[i] != '/' && value[i] != ':')
					break;
			}
			if (!value[i])
				continue;
			for ( ; i < sizeof(value); i++)
			{
				// ignore path - the \\ check is for HalfLife... stupid windoze 'programmers'...
				if (value[i] == '\\' || value[i] == '/' || value[i] == ':')
				{
					j = i + 1;
				}
				else if (value[i] == ';' || value[i] == 0)
				{
					k = value[i];
					value[i] = 0;
					if (value[j])
						WAD3_LoadTextureWadFile (value + j);
					j = i + 1;
					if (!k)
						break;
				}
			}
		}
	}
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
	if (model->bspversion == HL_BSPVERSION && !dedicated)
		Mod_ParseWadsFromEntityLump(model->entities);
}

static void Mod_LoadVertexes(model_t *model, lump_t *l)
{
	dvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadVertexes: funny lump size in %s", model->name);
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSubmodels: funny lump size in %s", model->name);
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
			{	// spread the mins / maxs by a pixel
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadEdges: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc((count + 1) * sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadEdges: Out of memory\n");

	model->edges = out;
	model->numedges = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

static void Mod_LoadTexinfo(model_t *model, lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int i, count, miptex;

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadTexinfo: funny lump size in %s", model->name);
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
				Host_Error ("Mod_LoadTexinfo: miptex >= model->numtextures");
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

	for (i = 0; i < s->numedges; i++)
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
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Host_Error ("CalcSurfaceExtents: Bad surface extents");
	}
}

static void Mod_LoadFaces(model_t *model, lump_t *l)
{
	dface_t *in;
	msurface_t *out;
	int i, count, surfnum, planenum, side;
	unsigned char flags;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadFaces: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);

	if (count > 65535)
		Sys_Error("Mod_LoadFaces: count > 65535");

	out = malloc(count*sizeof(*out));
	model->surfvisibleunaligned = malloc((((count+31)/32)*sizeof(*model->surfvisibleunaligned)) + 127);
	model->surfflags = malloc(count*sizeof(*model->surfflags));
	if (out == 0 || model->surfvisibleunaligned == 0 || model->surfflags == 0)
		Sys_Error("Mod_LoadBrushModel: Out of memory\n");

	memset(out, 0, count*sizeof(*out));
	memset(model->surfflags, 0, count*sizeof(*model->surfflags));

	model->surfvisible = (void *)((((uintptr_t)model->surfvisibleunaligned)+127)&~127);

	model->surfaces = out;
	model->numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++)
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
		for (i = 0; i < MAXLIGHTMAPS; i++)
			out->styles[i] = in->styles[i];
		i = LittleLong(in->lightofs);
		if (i == -1)
			out->samples = NULL;
		else
			out->samples = model->lightdata + (model->bspversion == HL_BSPVERSION ? i : i * 3);

		// set the drawing flags flag
		// sky, turb and alpha should be mutually exclusive

		if (ISSKYTEX(out->texinfo->texture->name))
		{	// sky
			flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			GL_SubdivideSurface(model, out);	// cut up polygon for warps
			model->surfflags[surfnum] = flags;
			continue;
		}

		if (ISTURBTEX(model, out->texinfo->texture->name))
		{	// turbulent
			flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i = 0; i < 2; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			GL_SubdivideSurface(model, out);	// cut up polygon for warps
			model->surfflags[surfnum] = flags;
			continue;
		}

		if (ISALPHATEX(model, out->texinfo->texture->name))
			flags |= SURF_DRAWALPHA;

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

	in = (void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadNodes: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count * sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadNodes: Out of memory\n");

	memset(out, 0, count * sizeof(*out));

	model->nodes = out;
	model->numnodes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		for (j = 0; j < 3; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadLeafs: funny lump size in %s", model->name);
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
			out->minmaxs[3 + j] = LittleShort (in->maxs[j]);
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

		if (!dedicated && out->contents != CONTENTS_EMPTY)
		{
			for (j = 0; j < out->nummarksurfaces; j++)
				model->surfflags[model->marksurfaces[out->firstmarksurfacenum + j]] |= SURF_UNDERWATER;
		}
	}
}

static void Mod_LoadClipnodes(model_t *model, lump_t *l)
{
	dclipnode_t *in, *out;
	int i, count;
	hull_t *hull;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadClipnodes: funny lump size in %s", model->name);
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

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadMarksurfaces: funny lump size in %s", model->name);
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
			Host_Error ("Mod_LoadMarksurfaces: bad surface number");
		out[i] = j;
	}
}

static void Mod_LoadSurfedges(model_t *model, lump_t *l)
{
	int i, count, *in, *out;

	in = (void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Host_Error ("Mod_LoadSurfedges: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);
	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadSurfEdges: Out of memory\n");

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
		Host_Error ("Mod_LoadPlanes: funny lump size in %s", model->name);
	count = l->filelen / sizeof(*in);

	if (count > 65535)
		Sys_Error("Mod_LoadPlanes: count > 65535");

	out = malloc(count*sizeof(*out));
	if (out == 0)
		Sys_Error("Mod_LoadPlanes: Out of memory\n");

	model->planes = out;
	model->numplanes = count;

	for (i = 0; i < count; i++, in++, out++)
	{
		bits = 0;
		for (j = 0; j < 3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
}

static float RadiusFromBounds(vec3_t mins, vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabs(mins[i]) > fabs(maxs[i]) ? fabs(mins[i]) : fabs(maxs[i]);

	return VectorLength (corner);
}

static void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int i, j;
	dheader_t *header;
	dmodel_t *bm;
	dmodel_t *subdmodels;
	model_t *nextmodel;
	model_t *mainmodel;
	unsigned int checksumvalue;

	mod->type = mod_brush;

	header = (dheader_t *)buffer;

	mod->bspversion = LittleLong (header->version);

	if (mod->bspversion != Q1_BSPVERSION && mod->bspversion != HL_BSPVERSION)
		Host_Error ("Mod_LoadBrushModel: %s has wrong version number (%i should be %i (Quake) or %i (HalfLife))", mod->name, mod->bspversion, Q1_BSPVERSION, HL_BSPVERSION);

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
		((int *) header)[i] = LittleLong (((int *) header)[i]);

	// checksum all of the map, except for entities
	mod->checksum = 0;
	mod->checksum2 = 0;

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

	Mod_LoadEntities(mod, &header->lumps[LUMP_ENTITIES]);
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
	subdmodels = Mod_LoadSubmodels(mod, &header->lumps[LUMP_MODELS]);

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
			bm = &subdmodels[i];

			mod->hulls[0].firstclipnode = bm->headnode[0];
			for (j = 1; j < MAX_MAP_HULLS; j++)
			{
				mod->hulls[j].firstclipnode = bm->headnode[j];
				mod->hulls[j].lastclipnode = mod->numclipnodes - 1;
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

	free(subdmodels);
}

/*
==============================================================================
ALIAS MODELS
==============================================================================
*/

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

static void *Mod_LoadAliasFrame(aliashdr_t *pheader, void * pin, maliasframedesc_t *frame)
{
	trivertx_t *pinframe;
	int i;
	daliasframe_t *pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (frame->name, pdaliasframe->name);
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pdaliasframe->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pdaliasframe->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds (frame->bboxmin, frame->bboxmax);

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

static void *Mod_LoadAliasGroup(aliashdr_t *pheader, void *pin, maliasframedesc_t *frame)
{
	daliasgroup_t *pingroup;
	int i, numframes;
	daliasinterval_t *pin_intervals;
	void *ptemp;

	pingroup = (daliasgroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i = 0; i < 3; i++)
	{
		// these are byte values, so we don't have to worry about endianness
		frame->bboxmin[i] = pingroup->bboxmin.v[i] * pheader->scale[i] + pheader->scale_origin[i];
		frame->bboxmax[i] = pingroup->bboxmax.v[i] * pheader->scale[i] + pheader->scale_origin[i];
	}
	frame->radius = RadiusFromBounds (frame->bboxmin, frame->bboxmax);

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i = 0; i < numframes; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

//=========================================================

typedef struct
{
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}

//Fill background pixels so mipmapping doesn't have haloes - Ed
static void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t fifo[FLOODFILL_FIFO_SIZE];
	int inpt = 0, outpt = 0, filledcolor = -1, i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0))
			{ // alpha 1.0
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int x = fifo[outpt].x, y = fifo[outpt].y, fdc = filledcolor;
		byte *pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}


static int Mod_LoadExternalSkin(model_t *model, char *identifier, int *fb_texnum)
{
	char loadpath[64];
	int texmode, texnum;

	if (model->modhint == MOD_EYES || model->modhint == MOD_BACKPACK)
	{
		*fb_texnum = 0;
		return 0;
	}

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value)
		texmode |= TEX_NOSCALE;

	snprintf(loadpath, sizeof(loadpath), "textures/models/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);

	if (!texnum)
	{
		snprintf(loadpath, sizeof(loadpath), "textures/%s", identifier);
		texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	}

	*fb_texnum = 0;
	return texnum;
}

static void *Mod_LoadAllSkins(model_t *model, aliashdr_t *pheader, int numskins, daliasskintype_t *pskintype)
{
	int i, j, k, s, groupskins, gl_texnum, fb_texnum, texmode;
	char basename[64], identifier[64];
	byte *skin;
	daliasskingroup_t *pinskingroup;
	daliasskininterval_t *pinskinintervals;

	skin = (byte *) (pskintype + 1);

	if (numskins < 1 || numskins > MAX_SKINS)
		Host_Error ("Mod_LoadAllSkins: Invalid # of skins: %d\n", numskins);

	s = pheader->skinwidth * pheader->skinheight;

	COM_CopyAndStripExtension(COM_SkipPath(model->name), basename, sizeof(basename));

	texmode = TEX_MIPMAP;
	if (!gl_scaleModelTextures.value && !model->isworldmodel)
		texmode |= TEX_NOSCALE;

	for (i = 0; i < numskins; i++)
	{
		if (pskintype->type == ALIAS_SKIN_SINGLE)
		{
			Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

			// save 8 bit texels for the player model to remap
			if (model->modhint == MOD_PLAYER)
			{
				Skin_SetDefault(pskintype + 1, pheader->skinwidth, pheader->skinheight);
			}

			snprintf(identifier, sizeof(identifier), "%s_%i", basename, i);

			gl_texnum = fb_texnum = 0;
			if (!(gl_texnum = Mod_LoadExternalSkin(model, identifier, &fb_texnum)))
			{
				gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth, pheader->skinheight,
					(byte *) (pskintype + 1), texmode, 1);

				if (Img_HasFullbrights((byte *)(pskintype + 1),	pheader->skinwidth * pheader->skinheight))
					fb_texnum = GL_LoadTexture (va("@fb_%s", identifier), pheader->skinwidth,
					pheader->skinheight, (byte *) (pskintype + 1), texmode | TEX_FULLBRIGHT, 1);
			}

			pheader->gl_texturenum[i][0] = pheader->gl_texturenum[i][1] =
				pheader->gl_texturenum[i][2] = pheader->gl_texturenum[i][3] = gl_texnum;

			pheader->fb_texturenum[i][0] = pheader->fb_texturenum[i][1] =
				pheader->fb_texturenum[i][2] = pheader->fb_texturenum[i][3] = fb_texnum;

			pskintype = (daliasskintype_t *)((byte *) (pskintype + 1) + s);
		}
		else
		{
			// animating skin group.  yuck.
			pskintype++;
			pinskingroup = (daliasskingroup_t *)pskintype;
			groupskins = LittleLong (pinskingroup->numskins);
			pinskinintervals = (daliasskininterval_t *) (pinskingroup + 1);

			pskintype = (void *) (pinskinintervals + groupskins);

			for (j = 0; j < groupskins; j++)
			{
				Mod_FloodFillSkin (skin, pheader->skinwidth, pheader->skinheight);

				snprintf(identifier, sizeof(identifier), "%s_%i_%i", basename, i, j);

				gl_texnum = fb_texnum = 0;
				if (!(gl_texnum = Mod_LoadExternalSkin(model, identifier, &fb_texnum)))
				{
					gl_texnum = GL_LoadTexture (identifier, pheader->skinwidth,
						pheader->skinheight, (byte *) (pskintype), texmode, 1);

					if (Img_HasFullbrights((byte *) (pskintype), pheader->skinwidth*pheader->skinheight))
						fb_texnum = GL_LoadTexture (va("@fb_%s", identifier),
						pheader->skinwidth,  pheader->skinheight, (byte *) (pskintype), texmode | TEX_FULLBRIGHT, 1);
				}

				pheader->gl_texturenum[i][j & 3] = gl_texnum;
				pheader->fb_texturenum[i][j & 3] = fb_texnum;

				pskintype = (daliasskintype_t *) ((byte *) (pskintype) + s);
			}

			for (k = j; j < 4; j++)
			{
				pheader->gl_texturenum[i][j & 3] = pheader->gl_texturenum[i][j - k];
				pheader->fb_texturenum[i][j & 3] = pheader->fb_texturenum[i][j - k];
			}
		}
	}

	return pskintype;
}

//=========================================================================

static void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
	int i, j, version, numframes, size;
	aliashdr_t *pheader;
	mdl_t *pinmodel;
	stvert_t *pinstverts;
	dtriangle_t *pintriangles;
	daliasframetype_t *pframetype;
	daliasskintype_t *pskintype;
	aliasframetype_t frametype;

	// some models are special
	if(!strcmp(mod->name, "progs/player.mdl"))
		mod->modhint = MOD_PLAYER;
	else if(!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else if (!strcmp(mod->name, "progs/flame.mdl") ||
		!strcmp(mod->name, "progs/flame2.mdl"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/bolt.mdl") || !strcmp(mod->name, "progs/bolt2.mdl") || !strcmp(mod->name, "progs/bolt3.mdl"))
		mod->modhint = MOD_THUNDERBOLT;
	else if (!strcmp(mod->name, "progs/backpack.mdl"))
		mod->modhint = MOD_BACKPACK;
	else
		mod->modhint = MOD_NORMAL;

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
				i = snprintf(st, sizeof(st), "%csetinfo %s %d", clc_stringcmd, mod->modhint == MOD_PLAYER ? pmodel_name : emodel_name, (int) crc);

				if (i < sizeof(st))
					NetQW_AppendReliableBuffer(cls.netqw, st, i + 1);
			}
#else
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			sprintf(st, "setinfo %s %d", mod->modhint == MOD_PLAYER ? pmodel_name : emodel_name, (int) crc);
			SZ_Print (&cls.netchan.message, st);
#endif
		}
	}

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);

	if (version != ALIAS_VERSION)
	{
		Host_Error ("Mod_LoadAliasModel: %s has wrong version number (%i should be %i)\n", mod->name, version, ALIAS_VERSION);
		return;
	}

	numframes = LittleLong(pinmodel->numframes);
	if (numframes < 1 || numframes > 65535)
		Host_Error("Mod_LoadAliasModel: Invalid number of alias frames");

	// allocate space for a working header, plus all the data except the frames, skin and group info
	size = 	sizeof (aliashdr_t) + (numframes - 1) * sizeof (pheader->frames[0]);
	pheader = malloc(size);
	if (pheader == 0)
		Sys_Error("Mod_LoadAliasModel: Out of memory\n");

	mod->flags = LittleLong (pinmodel->flags);

	// endian-adjust and copy the data, starting with the alias model header
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Host_Error ("Mod_LoadAliasModel: model %s has a skin taller than %d", mod->name, MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Host_Error ("Mod_LoadAliasModel: model %s has no vertices", mod->name);

	if (pheader->numverts > MAXALIASVERTS)
		Host_Error ("Mod_LoadAliasModel: model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Host_Error ("Mod_LoadAliasModel: model %s has no triangles", mod->name);

	pheader->numframes = numframes;

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i = 0; i < 3; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	// load the skins
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins(mod, pheader, pheader->numskins, pskintype);

	// load base s and t vertices
	pinstverts = (stvert_t *)pskintype;

	for (i = 0; i < pheader->numverts; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

	// load triangle lists
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i = 0; i < pheader->numtris; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j = 0; j < 3; j++)
			triangles[i].vertindex[j] =	LittleLong (pintriangles[i].vertindex[j]);
	}

	// load the frames
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	mod->mins[0] = mod->mins[1] = mod->mins[2] = 255;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 0;

	for (i = 0; i < numframes; i++)
	{
		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
			pframetype = (daliasframetype_t *) Mod_LoadAliasFrame(pheader, pframetype + 1, &pheader->frames[i]);
		else
			pframetype = (daliasframetype_t *) Mod_LoadAliasGroup(pheader, pframetype + 1, &pheader->frames[i]);

		for (j = 0; j < 3; j++)
		{
			mod->mins[j] = min (mod->mins[j], pheader->frames[i].bboxmin[j]);
			mod->maxs[j] = max (mod->maxs[j], pheader->frames[i].bboxmax[j]);
		}
	}

	mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

	pheader->numposes = posenum;

	mod->type = mod_alias;

	// build the draw lists
	GL_MakeAliasModelDisplayLists (mod, pheader);

	mod->extradata = pheader;
}

//=============================================================================


static int Mod_LoadExternalSpriteSkin(model_t *model, char *identifier, int framenum)
{
	char loadpath[64];
	int texmode, texnum;

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value && !model->isworldmodel)
		texmode |= TEX_NOSCALE;

	snprintf(loadpath, sizeof(loadpath), "textures/sprites/%s", identifier);
	texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);

	if (!texnum)
	{
		snprintf(loadpath, sizeof(loadpath), "textures/%s", identifier);
		texnum = GL_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	}

	return texnum;
}

static void *Mod_LoadSpriteFrame(model_t *model, void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	char basename[64], identifier[64];
	int width, height, size, origin[2], texnum, texmode;

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value && !model->isworldmodel)
		texmode |= TEX_NOSCALE;

	COM_CopyAndStripExtension(COM_SkipPath(model->name), basename, sizeof(basename));

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = malloc(sizeof (mspriteframe_t));
	if (pspriteframe == 0)
		Sys_Error("Mod_LoadSpriteFrame: Out of memory\n");

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf(identifier, sizeof(identifier), "sprites/%s_%i", basename, framenum);
	if (!(texnum = Mod_LoadExternalSpriteSkin(model, identifier, framenum)))
		texnum = GL_LoadTexture (identifier, width, height, (byte *) (pinframe + 1), texmode, 1);

	pspriteframe->gl_texturenum = texnum;

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}

static void *Mod_LoadSpriteGroup(model_t *model, void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	int i, numframes;
	dspriteinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);
	if (numframes < 1 || numframes >= 65536)
		Host_Error("Invalid number of sprite frames");

	pspritegroup = malloc(sizeof (mspritegroup_t) + (numframes - 1) * sizeof (pspritegroup->frames[0]));
	if (pspritegroup == 0)
		Sys_Error("Mod_LoadSpriteGroup: Out of memory\n");

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = malloc(numframes * sizeof (float));
	if (poutintervals == 0)
		Sys_Error("Mod_LoadSpriteGroup: Out of memory\n");

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Host_Error ("Mod_LoadSpriteGroup: interval <= 0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;

	for (i = 0; i < numframes; i++)
		ptemp = Mod_LoadSpriteFrame(model, ptemp, &pspritegroup->frames[i], framenum * 100 + i);

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
	{
		Host_Error ("Mod_LoadSpriteModel: %s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);
		return;
	}

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
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i = 0; i < numframes; i++)
	{
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteFrame(mod, pframetype + 1, &psprite->frames[i].frameptr, i);
		}
		else
		{
			pframetype = (dspriteframetype_t *)Mod_LoadSpriteGroup(mod, pframetype + 1, &psprite->frames[i].frameptr, i);
		}
	}

	mod->type = mod_sprite;
}

void Mod_Init (void)
{
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

void Mod_Shutdown()
{
	Mod_ClearAll();
}

