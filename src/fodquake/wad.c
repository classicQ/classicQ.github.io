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
// wad.c

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "crc.h"
#include "wad.h"

static int			wad_numlumps;
static lumpinfo_t	*wad_lumps;
static byte		*wad_base;

static void W_InsertOcranaLeds (byte *data);

static void SwapPic (qpic_t *pic)
{
	pic->width = LittleLong(pic->width);
	pic->height = LittleShort(pic->height);	
}

/*
Lowercases name and pads with spaces and a terminating 0 to the length of lumpinfo_t->name.
Used so lumpname lookups can proceed rapidly by comparing 4 chars at a time
Space padding is so names can be printed nicely in tables. Can safely be performed in place.
*/
static void W_CleanupName(const char *in, char *out)
{
	int i, c;

	for (i = 0; i < 16; i++)
	{
		c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}

	for ( ; i < 16; i++)
		out[i] = 0;
}

static int lump_filepos_compare(const void *a, const void *b)
{
	const lumpinfo_t *c, *d;

	c = a;
	d = b;

	return c->filepos - d->filepos;
}

void W_LoadWadFile(const char *filename)
{
	lumpinfo_t *lump_p;
	wadinfo_t *header;
	unsigned i;
	unsigned int j;
	int infotableofs;
	unsigned int lump_end;

	wad_base = FS_LoadMallocFile(filename);
	if (!wad_base)
		Sys_Error ("W_LoadWadFile: couldn't load %s", filename);

	if (com_filesize > 512*1024*1024)
		Sys_Error("Wad file %s is too large (%d bytes)\n", filename, com_filesize);

	header = (wadinfo_t *)wad_base;

	if (memcmp(header->identification, "WAD2", 4))
		Sys_Error ("Wad file %s doesn't have WAD2 id\n",filename);

	wad_numlumps = LittleLong(header->numlumps);
	infotableofs = LittleLong(header->infotableofs);
	wad_lumps = (lumpinfo_t *)(wad_base + infotableofs);

	if (wad_numlumps >= 65536)
		Sys_Error("Wad file %s contains too many lumps (%d)\n", filename, wad_numlumps);

	if (infotableofs >= 512*1024*1024)
		Sys_Error("Wad file %s has invalid info table offset (%d)\n", filename, infotableofs);

	if (infotableofs >= com_filesize || infotableofs + wad_numlumps * sizeof(lumpinfo_t) > com_filesize)
		Sys_Error("Was file %s corrupt. Info table out of range.\n", filename);

	for(i=0;i<wad_numlumps;i++)
	{
		wad_lumps[i].filepos = LittleLong(wad_lumps[i].filepos);
		wad_lumps[i].size = LittleLong(wad_lumps[i].size);
		W_CleanupName(wad_lumps[i].name, wad_lumps[i].name);
	}

	qsort(wad_lumps, wad_numlumps, sizeof(*wad_lumps), lump_filepos_compare);

	for (i = 0, lump_p = wad_lumps; i < wad_numlumps; i++,lump_p++)
	{
		for(j=0;j<sizeof(lump_p->name);j++)
		{
			if (lump_p->name[j] == 0)
				break;
		}
		if (j == sizeof(lump_p->name))
			Sys_Error("Was file %s corrupt. Unterminated lump name.\n", filename);

		if (lump_p->size <= 0 || lump_p->size >= 512*1024*1024)
			Sys_Error("Wad file %s contains lump with invalid size (%d)\n", filename, lump_p->size);

		/* Now find the largest possible size of the lump... */
		if (i + 1 != wad_numlumps)
			lump_end = lump_p[1].filepos;
		else
			lump_end = com_filesize;

		/* If the lump info table is somewhere inside the computed max size, crop it to the beginning of the lump info table */
		if (infotableofs >= lump_p->filepos && infotableofs <= lump_end)
			lump_end = infotableofs;

		if (lump_end - lump_p->filepos < lump_p->size)
			lump_p->size = lump_end - lump_p->filepos;

		/* Now verify that the lump is inside the file and not inside the lump info table */
		if (lump_p->filepos < 0 || lump_p->filepos >= com_filesize)
			Sys_Error("Wad file %s contains lump with invalid offset (%d)\n", filename, lump_p->filepos);

		if (lump_p->filepos + lump_p->size >= com_filesize || lump_p->filepos + lump_p->size >= com_filesize)
			Sys_Error("Wad file %s contains out of bounds lump\n", filename);

		if (!((lump_p->filepos <= infotableofs && lump_p->filepos + lump_p->size <= infotableofs)
		  || (lump_p->filepos >= infotableofs + wad_numlumps * sizeof(lumpinfo_t) && lump_p->filepos + lump_p->size >= infotableofs + wad_numlumps * sizeof(lumpinfo_t))))
		{
			Sys_Error("Wad file %s contains lump and info table overlap\n", filename);
		}

		if (lump_p->type == TYP_QPIC)
			SwapPic ( (qpic_t *)(wad_base + lump_p->filepos));
	}
}

void W_UnloadWadFile()
{
	free(wad_base);
}

static lumpinfo_t *W_GetLumpinfo(const char *name)
{
	int i;
	lumpinfo_t	*lump_p;
	char clean[16];

	W_CleanupName (name, clean);
	for (lump_p = wad_lumps, i = 0; i < wad_numlumps; i++,lump_p++)
	{
		if (!strcmp(clean, lump_p->name))
			return lump_p;
	}

	Sys_Error ("W_GetLumpinfo: %s not found", name);
	return NULL;
}

void *W_GetLumpName(const char *name)
{
	lumpinfo_t *lump;

	lump = W_GetLumpinfo (name);

	if (!strcmp(name, "conchars"))
	{
		if (CRC_Block (wad_base + lump->filepos, lump->size) == 798)
			W_InsertOcranaLeds (wad_base + lump->filepos);
	}

	return (void *) (wad_base + lump->filepos);
}

static void *W_GetLumpNum(int num)
{
	lumpinfo_t *lump;

	if (num < 0 || num > wad_numlumps)
		Sys_Error ("W_GetLumpNum: bad number: %i", num);

	lump = wad_lumps + num;

	return (void *) (wad_base + lump->filepos);
}

static const byte ocrana_leds[4][8][8] =
{
	/* green */
	{
		{ 0x00, 0x38, 0x3b, 0x3b, 0x3b, 0x3b, 0x35, 0x00 },
		{ 0x38, 0x3b, 0x3d, 0x3f, 0x3f, 0x3d, 0x38, 0x35 },
		{ 0x3b, 0x3d, 0xfe, 0x3f, 0x3f, 0x3f, 0x3b, 0x35 },
		{ 0x3b, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3b, 0x35 },
		{ 0x3b, 0x3f, 0x3f, 0x3f, 0x3f, 0x3d, 0x3b, 0x35 },
		{ 0x3b, 0x3d, 0x3f, 0x3f, 0x3d, 0x3b, 0x38, 0x35 },
		{ 0x35, 0x38, 0x3b, 0x3b, 0x3b, 0x38, 0x35, 0x35 },
		{ 0x00, 0x35, 0x35, 0x35, 0x35, 0x35, 0x35, 0x00 }
	},
	/* red */
	{
		{ 0x00, 0xf8, 0xf9, 0xf9, 0xf9, 0xf9, 0x4c, 0x00 },
		{ 0xf8, 0xf9, 0xfa, 0xfb, 0xfb, 0xfa, 0xf8, 0x4c },
		{ 0xf9, 0xfa, 0xfe, 0xfb, 0xfb, 0xfb, 0xf9, 0x4c },
		{ 0xf9, 0xfb, 0xfb, 0xfb, 0xfb, 0xfb, 0xf9, 0x4c },
		{ 0xf9, 0xfb, 0xfb, 0xfb, 0xfb, 0xfa, 0xf9, 0x4c },
		{ 0xf9, 0xfa, 0xfb, 0xfb, 0xfa, 0xf9, 0xf8, 0x4c },
		{ 0x4c, 0xf8, 0xf9, 0xf9, 0xf9, 0xf8, 0x4c, 0x4c },
		{ 0x00, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x4c, 0x00 }
	},
	/* yellow */
	{
		{ 0x00, 0xc8, 0xc5, 0xc5, 0xc5, 0xc5, 0xcb, 0x00 },
		{ 0xc8, 0xc5, 0xc2, 0x6f, 0x6f, 0xc2, 0xc8, 0xcb },
		{ 0xc5, 0xc2, 0xfe, 0x6f, 0x6f, 0x6f, 0xc5, 0xcb },
		{ 0xc5, 0x6f, 0x6f, 0x6f, 0x6f, 0x6f, 0xc5, 0xcb },
		{ 0xc5, 0x6f, 0x6f, 0x6f, 0x6f, 0xc2, 0xc5, 0xcb },
		{ 0xc5, 0xc2, 0x6f, 0x6f, 0xc2, 0xc5, 0xc8, 0xcb },
		{ 0xcb, 0xc8, 0xc5, 0xc5, 0xc5, 0xc8, 0xcb, 0xcb },
		{ 0x00, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0xcb, 0x00 }
	},
	/* blue */
	{
		{ 0x00, 0xd8, 0xd5, 0xd5, 0xd5, 0xd5, 0xdc, 0x00 },
		{ 0xd8, 0xd5, 0xd2, 0xd0, 0xd0, 0xd2, 0xd8, 0xdc },
		{ 0xd5, 0xd2, 0xfe, 0xd0, 0xd0, 0xd0, 0xd5, 0xdc },
		{ 0xd5, 0xd0, 0xd0, 0xd0, 0xd0, 0xd0, 0xd5, 0xdc },
		{ 0xd5, 0xd0, 0xd0, 0xd0, 0xd0, 0xd2, 0xd5, 0xdc },
		{ 0xd5, 0xd2, 0xd0, 0xd0, 0xd2, 0xd5, 0xd8, 0xdc },
		{ 0xdc, 0xd8, 0xd5, 0xd5, 0xd5, 0xd8, 0xdc, 0xdc },
		{ 0x00, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0xdc, 0x00 }
	}
};

static void W_InsertOcranaLeds (byte *data)
{
	byte *leddata;
	int i, x, y;

	for (i = 0; i < 4; i++)
	{
		leddata = data + (0x80 >> 4 << 10) + (0x06 << 3) + (i << 3);

		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
				*leddata++ = ocrana_leds[i][y][x];
			leddata += 128 - 8;
		}
	}
}

/*
=============================================================================
WAD3 Texture Loading for BSP 3.0 Support
=============================================================================
*/

#ifdef GLQUAKE

#define TEXWAD_MAXIMAGES 16384

typedef struct
{
	char name[MAX_QPATH];
	FILE *file;
	int position;
	int size;
} texwadlump_t;

static texwadlump_t *texwadlump;
static unsigned int texwadlumpcount;

void WAD3_LoadTextureWadFile(const char *filename)
{
	texwadlump_t *newtexwadlump;
	lumpinfo_t *lumps, *lump_p;
	wadinfo_t header;
	int i, j, infotableofs, numlumps;
	FILE *file;

	if (FS_FOpenFile (va("textures/wad3/%s", filename), &file) == -1
	 && FS_FOpenFile (va("textures/halflife/%s", filename), &file) == -1
	 && FS_FOpenFile (va("textures/%s", filename), &file) == -1
	 && FS_FOpenFile (filename, &file) == -1)
		Host_Error ("Couldn't load halflife wad \"%s\"\n", filename);

	if (fread(&header, 1, sizeof(wadinfo_t), file) != sizeof(wadinfo_t))
	{
		Com_Printf ("WAD3_LoadTextureWadFile: unable to read wad header");
		return;
	}

	if (memcmp(header.identification, "WAD3", 4))
	{
		Com_Printf ("WAD3_LoadTextureWadFile: Wad file %s doesn't have WAD3 id\n",filename);
		return;
	}

	numlumps = LittleLong(header.numlumps);
	if (numlumps < 1 || numlumps > TEXWAD_MAXIMAGES)
	{
		Com_Printf ("WAD3_LoadTextureWadFile: invalid number of lumps (%i)\n", numlumps);
		return;
	}

	if (texwadlumpcount + numlumps > TEXWAD_MAXIMAGES)
	{
		Com_Printf("WAD3_LoadTextureWadFile: Too many lumps loaded\n");
		return;
	}

	infotableofs = LittleLong(header.infotableofs);
	if (fseek(file, infotableofs, SEEK_SET))
	{
		Com_Printf ("WAD3_LoadTextureWadFile: unable to seek to lump table");
		return;
	}

	if (!(lumps = malloc(sizeof(lumpinfo_t) * numlumps)))
	{
		Com_Printf ("WAD3_LoadTextureWadFile: unable to allocate temporary memory for lump table");
	}
	else
	{
		newtexwadlump = realloc(texwadlump, (texwadlumpcount + numlumps) * sizeof(*texwadlump));
		if (newtexwadlump == 0)
			Sys_Error("WAD3_LoadTextureWadFile: Out of memory\n");

		texwadlump = newtexwadlump;

		if (fread(lumps, 1, sizeof(lumpinfo_t) * numlumps, file) != sizeof(lumpinfo_t) * numlumps)
		{
			Com_Printf ("WAD3_LoadTextureWadFile: unable to read lump table");
		}
		else
		{
			for (i = 0, lump_p = lumps; i < numlumps; i++,lump_p++)
			{
				W_CleanupName(lump_p->name, lump_p->name);
				for (j = 0; j < texwadlumpcount; j++)
				{
					if (strcmp(lump_p->name, texwadlump[j].name) == 0)
						break;
				}

				if (j != texwadlumpcount)
					Q_strncpyz(texwadlump[j].name, lump_p->name, sizeof(texwadlump[j].name));

				texwadlump[j].file = file;
				texwadlump[j].position = LittleLong(lump_p->filepos);
				texwadlump[j].size = LittleLong(lump_p->disksize);

				if (j == texwadlumpcount)
					texwadlumpcount++;
			}
		}

		free(lumps);
	}
}

//converts paletted to rgba
static byte *ConvertWad3ToRGBA(miptex_t *tex)
{
	byte *in, *data, *pal;
	int i, p, image_size;

	if (!tex->offsets[0])
		Sys_Error("ConvertWad3ToRGBA: tex->offsets[0] == 0");

	image_size = tex->width * tex->height;
	in = (byte *) ((byte *) tex + tex->offsets[0]);
	data = Q_Malloc(image_size * 4);

	pal = in + ((image_size * 85) >> 6) + 2;
	for (i = 0; i < image_size; i++)
	{
		p = *in++;
		if (tex->name[0] == '{' && p == 255)
		{
			((int *) data)[i] = 0;
		}
		else
		{
			p *= 3;
			data[i * 4 + 0] = pal[p];
			data[i * 4 + 1] = pal[p + 1];
			data[i * 4 + 2] = pal[p + 2];
			data[i * 4 + 3] = 255;
		}
	}
	return data;
}

byte *WAD3_LoadTexture(miptex_t *mt)
{
	char texname[MAX_QPATH];
	int i, j;
	FILE *file;
	miptex_t *tex;
	byte *data;

	if (mt->offsets[0])
		return ConvertWad3ToRGBA(mt);

	texname[sizeof(texname) - 1] = 0;
	W_CleanupName (mt->name, texname);
	for (i = 0; i < texwadlumpcount; i++)
	{
		if (strcmp(texname, texwadlump[i].name))
			continue;

		file = texwadlump[i].file;
		if (fseek(file, texwadlump[i].position, SEEK_SET))
		{
			Com_Printf("WAD3_LoadTexture: corrupt WAD3 file");
			return NULL;
		}

		data = 0;

		tex = malloc(texwadlump[i].size);
		if (tex == 0)
		{
			Com_Printf("WAD3_LoadTexture: Out of memory\n");
		}
		else
		{
			if (fread(tex, 1, texwadlump[i].size, file) < texwadlump[i].size)
			{
				Com_Printf("WAD3_LoadTexture: corrupt WAD3 file");
			}
			else
			{
				tex->width = LittleLong(tex->width);
				tex->height = LittleLong(tex->height);

				if (tex->width == mt->width && tex->height == mt->height)
				{
					for (j = 0;j < MIPLEVELS;j++)
						tex->offsets[j] = LittleLong(tex->offsets[j]);

					data = ConvertWad3ToRGBA(tex);
				}
			}

			free(tex);
		}

		return data;
	}
	return NULL;
}

#endif
