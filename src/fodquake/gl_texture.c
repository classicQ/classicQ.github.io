/*

Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "crc.h"
#include "image.h"
#include "filesystem.h"
#include "gl_local.h"

#include "config.h"

static qboolean OnChange_gl_max_size(cvar_t *var, char *string);
static qboolean OnChange_gl_texturemode(cvar_t *var, char *string);
static qboolean OnChange_gl_miptexLevel(cvar_t *var, char *string);


static qboolean no24bit, forceTextureReload;


extern unsigned d_8to24table2[256];
extern byte vid_gamma_table[256];
extern float vid_gamma;


int texture_extension_number = 1;
int gl_max_size_default;
int gl_lightmap_format = 3, gl_solid_format = 3, gl_alpha_format = 4;


cvar_t	gl_max_size			= {"gl_max_size", "1024", 0, OnChange_gl_max_size};
static cvar_t	gl_picmip			= {"gl_picmip", "0"};
cvar_t	gl_miptexLevel		= {"gl_miptexLevel", "0", 0, OnChange_gl_miptexLevel};
static cvar_t	gl_lerpimages		= {"gl_lerpimages", "1"};
static cvar_t	gl_texturemode		= {"gl_texturemode", "GL_LINEAR_MIPMAP_NEAREST", 0, OnChange_gl_texturemode};

cvar_t	gl_scaleModelTextures		= {"gl_scaleModelTextures", "0"};
cvar_t	gl_scaleTurbTextures		= {"gl_scaleTurbTextures", "1"};
cvar_t	gl_externalTextures_world	= {"gl_externalTextures_world", "1"};
cvar_t	gl_externalTextures_bmodels	= {"gl_externalTextures_bmodels", "1"};

struct gltexture
{
	int			texnum;
	char		identifier[MAX_QPATH];
	char		*pathname;
	int			width, height;
	int			scaled_width, scaled_height;
	int			texmode;
	unsigned	crc;
	int			bpp;
};

static struct gltexture	gltextures[MAX_GLTEXTURES];
static int	numgltextures;

#define Q_ROUND_POWER2(in, out) {						\
	int _mathlib_temp_int1 = in;							\
	for (out = 1; out < _mathlib_temp_int1; out <<= 1)	\
	;												\
}

static qboolean OnChange_gl_max_size(cvar_t *var, char *string)
{
	int i;
	float newvalue = Q_atof(string);

	if (gl_max_size_default && newvalue > gl_max_size_default)
	{
		Com_Printf("Your hardware doesn't support texture sizes bigger than %dx%d\n", gl_max_size_default, gl_max_size_default);
		return true;
	}

	Q_ROUND_POWER2(newvalue, i);

	if (i != newvalue)
	{
		Com_Printf("Valid values for %s are powers of 2 only\n", var->name);
		return true;
	}

	return false;
}

struct glmode
{
	char *name;
	int	minimize, maximize;
};

static struct glmode modes[] =
{
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

#define GLMODE_NUMODES	(sizeof(modes) / sizeof(*modes))

static int gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
static int gl_filter_max = GL_LINEAR;

static qboolean OnChange_gl_texturemode(cvar_t *var, char *string)
{
	int i;
	struct gltexture *glt;

	for (i = 0; i < GLMODE_NUMODES; i++)
	{
		if (!Q_strcasecmp(modes[i].name, string ) )
			break;
	}
	if (i == GLMODE_NUMODES)
	{
		Com_Printf("bad filter name: %s\n", string);
		return true;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;


	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (glt->texmode & TEX_MIPMAP)
		{
			GL_Bind(glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}

	return false;
}

static qboolean OnChange_gl_miptexLevel(cvar_t *var, char *string)
{
	float newval = Q_atof(string);

	if (newval != 0 && newval != 1 && newval != 2 && newval != 3)
	{
		Com_Printf("Valid values for %s are 0,1,2,3 only\n", var->name);
		return true;
	}
	return false;
}

int currenttexture = -1;			

void GL_Bind(int texnum)
{
	if (currenttexture == texnum)
		return;

	currenttexture = texnum;
	glBindTexture(GL_TEXTURE_2D, texnum);
}

static GLenum oldtarget = GL_TEXTURE0;
static int cnttextures[4] = {-1, -1, -1, -1};   
static qboolean mtexenabled = false;

void GL_SelectTexture(GLenum target)
{
	if (target == oldtarget) 
		return;

	glActiveTexture(target);

	cnttextures[oldtarget - GL_TEXTURE0] = currenttexture;
	currenttexture = cnttextures[target - GL_TEXTURE0];
	oldtarget = target;
}

void GL_DisableMultitexture(void)
{
	if (mtexenabled)
	{
		glDisable(GL_TEXTURE_2D);
		GL_SelectTexture(GL_TEXTURE0);
		mtexenabled = false;
	}
}

void GL_EnableMultitexture(void)
{
	if (gl_mtexable)
	{
		GL_SelectTexture(GL_TEXTURE1);
		glEnable(GL_TEXTURE_2D);
		mtexenabled = true;
	}
}

void GL_EnableTMU(GLenum target)
{
	GL_SelectTexture(target);
	glEnable(GL_TEXTURE_2D);
}

void GL_DisableTMU(GLenum target)
{
	GL_SelectTexture(target);
	glDisable(GL_TEXTURE_2D);
}

static void ScaleDimensions(int width, int height, int *scaled_width, int *scaled_height, int mode)
{
	int maxsize, picmip;
	qboolean scale;

	scale = (mode & TEX_MIPMAP) && !(mode & TEX_NOSCALE);

	Q_ROUND_POWER2(width, *scaled_width);
	Q_ROUND_POWER2(height, *scaled_height);

	if (scale)
	{
		picmip = (int) bound(0, gl_picmip.value, 16);
		*scaled_width >>= picmip;
		*scaled_height >>= picmip;		
	}

	maxsize = scale ? gl_max_size.value : gl_max_size_default;

	*scaled_width = bound(1, *scaled_width, maxsize);
	*scaled_height = bound(1, *scaled_height, maxsize);
}

void GL_Upload32(unsigned int *data, int width, int height, int mode)
{
	int internal_format, tempwidth, tempheight, miplevel;
	unsigned int *newdata;

	Q_ROUND_POWER2(width, tempwidth);
	Q_ROUND_POWER2(height, tempheight);

	newdata = Q_Malloc(tempwidth * tempheight * 4);
	if (width < tempwidth || height < tempheight)
	{
		Image_Resample(data, width, height, newdata, tempwidth, tempheight, 4, !!gl_lerpimages.value);
		width = tempwidth;
		height = tempheight;
	}
	else
	{
		memcpy(newdata, data, width * height * 4);
	}

	ScaleDimensions(width, height, &tempwidth, &tempheight, mode);

	while (width > tempwidth || height > tempheight)
		Image_MipReduce((byte *) newdata, (byte *) newdata, &width, &height, 4);

	if (mode & TEX_NOCOMPRESS)
		internal_format = (mode & TEX_ALPHA) ? 4 : 3;
	else
		internal_format = (mode & TEX_ALPHA) ? gl_alpha_format : gl_solid_format;

	miplevel = 0;
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
	if (mode & TEX_MIPMAP)
	{
		while (width > 1 || height > 1)
		{
			Image_MipReduce((byte *) newdata, (byte *) newdata, &width, &height, 4);
			miplevel++;
			glTexImage2D(GL_TEXTURE_2D, miplevel, internal_format, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, newdata);
		}
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

	free(newdata);
}

void GL_Upload8(byte *data, int width, int height, int mode)
{
	static unsigned int trans[640 * 480];
	int i, image_size, p;
	unsigned int *table;

	table = (mode & TEX_BRIGHTEN) ? d_8to24table2 : d_8to24table;
	image_size = width * height;

	if (image_size * 4 > sizeof(trans))
		Sys_Error("GL_Upload8: image too big");

	if (mode & TEX_FULLBRIGHT)
	{
		mode |= TEX_ALPHA;
		for (i = 0; i < image_size; i++)
		{
			p = data[i];
			if (p < 224)
				trans[i] = table[p] & COLOURMASK_RGBA;
			else
				trans[i] = table[p];			
		}
	}
	else if (mode & TEX_ALPHA)
	{
		mode &= ~TEX_ALPHA;
		for (i = 0; i < image_size; i++)
		{
			if ((p = data[i]) == 255)
				mode |= TEX_ALPHA;
			trans[i] = table[p];
		}
	}
	else
	{
		if (image_size & 3)
			Sys_Error("GL_Upload8: image_size & 3");
		for (i = 0; i < image_size; i += 4)
		{
			trans[i] = table[data[i]];
			trans[i + 1] = table[data[i + 1]];
			trans[i + 2] = table[data[i + 2]];
			trans[i + 3] = table[data[i + 3]];
		}
	}
	GL_Upload32(trans, width, height, mode);
}

int GL_LoadTexture(char *identifier, int width, int height, byte *data, int mode, int bpp)
{
	int i, scaled_width, scaled_height, crc = 0;
	struct gltexture *glt;

	ScaleDimensions(width, height, &scaled_width, &scaled_height, mode);

	if (identifier[0])
	{
		crc = CRC_Block(data, width * height * bpp);
		for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
		{
			if (!strncmp(identifier, glt->identifier, sizeof(glt->identifier) - 1))
			{
				if (width == glt->width && height == glt->height
				 && scaled_width == glt->scaled_width && scaled_height == glt->scaled_height
				 && crc == glt->crc
				 && glt->bpp == bpp
				 && (mode & ~(TEX_COMPLAIN|TEX_NOSCALE)) == (glt->texmode & ~(TEX_COMPLAIN|TEX_NOSCALE)))
				{
					GL_Bind(gltextures[i].texnum);
					return gltextures[i].texnum;	
				}
				else
				{
					goto setup_gltexture;	
				}
			}
		}
	}

	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error("GL_LoadTexture: numgltextures == MAX_GLTEXTURES");

	glt = &gltextures[numgltextures];
	numgltextures++;

	Q_strncpyz(glt->identifier, identifier, sizeof(glt->identifier));
	glt->texnum = texture_extension_number;
	texture_extension_number++;

setup_gltexture:
	glt->width = width;
	glt->height = height;
	glt->scaled_width = scaled_width;
	glt->scaled_height = scaled_height;
	glt->texmode = mode;
	glt->crc = crc;
	glt->bpp = bpp;
	if (glt->pathname)
	{
		Z_Free(glt->pathname);
		glt->pathname = NULL;
	}
	if (bpp == 4 && com_netpath[0])	
		glt->pathname = CopyString(com_netpath);

	GL_Bind(glt->texnum);

	switch (bpp)
	{
		case 1:
			GL_Upload8(data, width, height, mode);
			break;
		case 4:
			GL_Upload32((void *) data, width, height, mode);
			break;
		default:
			Sys_Error("GL_LoadTexture: unknown bpp\n");
			break;
	}

	return glt->texnum;
}

static struct gltexture *GL_FindTexture(const char *identifier)
{
	int i;

	for (i = 0; i < numgltextures; i++)
	{
		if (!strcmp(identifier, gltextures[i].identifier))
			return &gltextures[i];
	}

	return NULL;
}

static void GL_FlushTextures()
{
	struct gltexture *glt;
	unsigned int textures[MAX_GLTEXTURES];
	int i;

	for(glt=gltextures,i=0;i<numgltextures;glt++,i++)
	{
		textures[i] = glt->texnum;
		if (glt->pathname)
			Z_Free(glt->pathname);
	}

	glDeleteTextures(numgltextures, textures);

	memset(gltextures, 0, sizeof(gltextures));
	numgltextures = 0;
}

static struct gltexture *current_texture = NULL;

#define CHECK_TEXTURE_ALREADY_LOADED	\
	if (CheckTextureLoaded(mode)) {		\
		current_texture = NULL;			\
		fclose(f);						\
		return NULL;					\
	}

static qboolean CheckTextureLoaded(int mode)
{
	int scaled_width, scaled_height;

	if (!forceTextureReload)
	{
		if (current_texture && current_texture->pathname && !strcmp(com_netpath, current_texture->pathname))
		{
			ScaleDimensions(current_texture->width, current_texture->height, &scaled_width, &scaled_height, mode);
			if (current_texture->scaled_width == scaled_width && current_texture->scaled_height == scaled_height)
				return true;
		}
	}

	return false;
}

byte *GL_LoadImagePixels(char *filename, int matchwidth, int matchheight, unsigned int *imagewidth, unsigned int *imageheight, int mode)
{
	char basename[MAX_QPATH], name[MAX_QPATH];
	char *c;
	byte *data;
	FILE *f;

	COM_CopyAndStripExtension(filename, basename, sizeof(basename));
	for (c = basename; *c; c++)
		if (*c == '*')
			*c = '#';

	snprintf(name, sizeof(name), "%s.tga", basename);
	if (FS_FOpenFile(name, &f) != -1)
	{
		CHECK_TEXTURE_ALREADY_LOADED;
		if ((data = Image_LoadTGA(f, name, matchwidth, matchheight, imagewidth, imageheight)))
			return data;
	}

#if USE_PNG
	snprintf(name, sizeof(name), "%s.png", basename);
	if (FS_FOpenFile(name, &f) != -1)
	{
		CHECK_TEXTURE_ALREADY_LOADED;
		if ((data = Image_LoadPNG(f, name, matchwidth, matchheight, imagewidth, imageheight)))
			return data;
	}
#endif

	if (mode & TEX_COMPLAIN)
	{
		if (!no24bit)
			Com_Printf("Couldn't load %s image\n", COM_SkipPath(filename));
	}

	return NULL;
}

int GL_LoadTexturePixels(byte *data, char *identifier, int width, int height, int mode)
{
	int i, j, image_size;
	qboolean gamma;

	image_size = width * height;
	gamma = (vid_gamma != 1);

	if (mode & TEX_LUMA)
	{
		gamma = false;
	}
	else if (mode & TEX_ALPHA)
	{
		mode &= ~TEX_ALPHA;
		for (j = 0; j < image_size; j++)
		{
			if ( ( (((unsigned int *) data)[j] >> 24 ) & 0xFF ) < 255 )
			{
				mode |= TEX_ALPHA;
				break;
			}
		}
	}

	if (gamma)
	{
		for (i = 0; i < image_size; i++)
		{
			data[4 * i] = vid_gamma_table[data[4 * i]];
			data[4 * i + 1] = vid_gamma_table[data[4 * i + 1]];
			data[4 * i + 2] = vid_gamma_table[data[4 * i + 2]];
		}
	}

	return GL_LoadTexture(identifier, width, height, data, mode, 4);
}

int GL_LoadTextureImage(char *filename, char *identifier, int matchwidth, int matchheight, int mode)
{
	int texnum;
	byte *data;
	struct gltexture *gltexture;
	unsigned int imagewidth, imageheight;

	if (no24bit)
		return 0;

	if (!identifier)
		identifier = filename;

	gltexture = current_texture = GL_FindTexture(identifier);

	if (!(data = GL_LoadImagePixels(filename, matchwidth, matchheight, &imagewidth, &imageheight, mode)))
	{
		texnum = (gltexture && !current_texture) ? gltexture->texnum : 0;
	}
	else
	{
		texnum = GL_LoadTexturePixels(data, identifier, imagewidth, imageheight, mode);
		free(data);
	}

	current_texture = NULL;
	return texnum;
}

int GL_LoadCharsetImage(char *filename, char *identifier)
{
	int i, texnum, image_size;
	byte *data, *buf, *dest, *src;
	unsigned int imagewidth, imageheight;

	if (no24bit)
		return 0;

	if (!(data = GL_LoadImagePixels(filename, 0, 0, &imagewidth, &imageheight, 0)))
		return 0;

	if (!identifier)
		identifier = filename;

	if (imagewidth >= 32768 || imageheight >= 32768)
		return 0;

	image_size = imagewidth * imageheight;

	if (image_size >= 256*1024*1024)
		return 0;

	buf = dest = Q_Calloc(image_size * 2, 4); 
	src = data;
	for (i = 0 ; i < 16 ; i++)
	{
		memcpy(dest, src, image_size >> 2);
		src += image_size >> 2;
		dest += image_size >> 1;
	}

	texnum = GL_LoadTexture(identifier, imagewidth, imageheight * 2, buf, TEX_ALPHA | TEX_NOCOMPRESS, 4);

	free(buf);
	free(data);
	return texnum;
}

void GL_Texture_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register(&gl_max_size);
	Cvar_Register(&gl_picmip);
	Cvar_Register(&gl_lerpimages);
	Cvar_Register(&gl_texturemode);
	Cvar_Register(&gl_scaleModelTextures);
	Cvar_Register(&gl_scaleTurbTextures);
	Cvar_Register(&gl_miptexLevel);
	Cvar_Register(&gl_externalTextures_world);
	Cvar_Register(&gl_externalTextures_bmodels);
	Cvar_ResetCurrentGroup();
}


void GL_Texture_Init(void)
{
	unsigned int i;
	int oldflags;

	/* Prevent setting the modified flag */
	oldflags = Cvar_GetFlags(&gl_max_size);
	if (gl_max_size.value > gl_max_size_default)
		Cvar_SetValue(&gl_max_size, gl_max_size_default);
	Cvar_SetFlags(&gl_max_size, oldflags);

	no24bit = COM_CheckParm("-no24bit") ? true : false;
	forceTextureReload = COM_CheckParm("-forceTextureReload") ? true : false;

	oldtarget = GL_TEXTURE0;
	currenttexture = -1;
	for(i=0;i<(sizeof(cnttextures)/sizeof(*cnttextures));i++)
		cnttextures[i] = -1;
}

void GL_Texture_Shutdown()
{
	GL_FlushTextures();
}

