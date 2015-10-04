/*
Copyright (C) 2012 Mark Olsen

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

#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "utils.h"
#include "image.h"
#include "skinimp.h"
#include "skin.h"

static cvar_t baseskin = { "baseskin", "base" };
static cvar_t noskins = { "noskins", "0" };

static void *defaultskin;

enum SkinSourceType
{
	SKINSOURCE_SOLIDCOLOUR,
	SKINSOURCE_TEXTURE_PALETTED,
	SKINSOURCE_TEXTURE_PALETTED_TRANSLATED,
	SKINSOURCE_TEXTURE_TRUECOLOUR,
};

struct SkinSource
{
	struct SkinSource *next;

	enum SkinSourceType type;

	struct SkinTranslation *translations;

	char *skinname;

	union
	{
		struct
		{
			float colours[3];
		} solidcolour;

		struct
		{
			unsigned int width;
			unsigned int height;
			void *data;
		} texture;
	} data;
};

struct SkinTranslation
{
	struct SkinTranslation *next;

	unsigned int topcolour;
	unsigned int bottomcolour;

	struct SkinImp *skinimp;
};

struct SkinSource *skinsources;

static void Skin_SetupTexture(struct SkinSource *source)
{
	unsigned int x;
	unsigned int y;
	unsigned int width;
	unsigned int height;
	const char *p;

	width = source->data.texture.width;
	height = source->data.texture.height;

	if (width > 296)
		width = 296;

	if (height > 194)
		height = 194;

	source->type = SKINSOURCE_TEXTURE_PALETTED;

	p = source->data.texture.data;
	for(y=0;y<height;y++)
	{
		for(x=0;x<width;x++)
		{
			if ((p[x] >= 16 && p[x] < 32) || (p[x] >= 96 && p[x] < 112))
			{
				source->type = SKINSOURCE_TEXTURE_PALETTED_TRANSLATED;
				break;
			}
		}

		if (source->type == SKINSOURCE_TEXTURE_PALETTED_TRANSLATED)
			break;

		p += source->data.texture.width;
	}
}

static struct SkinSource *Skin_GetSource(const char *skinname)
{
	struct SkinSource *source;
	int ok;

	if (skinname[0] == 0 || noskins.value == 1)
		skinname = baseskin.string;

	source = skinsources;
	while(source)
	{
		if (strcmp(source->skinname, skinname) == 0)
			return source;

		source = source->next;
	}

	source = malloc(sizeof(*source));
	if (source)
	{
		memset(source, 0, sizeof(*source));

		source->skinname = strdup(skinname);
		if (source->skinname)
		{
			ok = 1;

			if (ParseColourDescription(skinname, source->data.solidcolour.colours))
			{
				source->type = SKINSOURCE_SOLIDCOLOUR;
			}
			else if (skinname[0] && (source->data.texture.data = Image_LoadPNG(0, va("skins/%s.png", skinname), 0, 0, &source->data.texture.width, &source->data.texture.height)))
			{
				source->type = SKINSOURCE_TEXTURE_TRUECOLOUR;
			}
			else if (skinname[0] && (source->data.texture.data = Image_LoadPCX(0, va("skins/%s.pcx", skinname), 0, 0, &source->data.texture.width, &source->data.texture.height)))
			{
				Skin_SetupTexture(source);
			}
			else if ((source->data.texture.data = Image_LoadPCX(0, "skins/base.pcx", 0, 0, &source->data.texture.width, &source->data.texture.height)))
			{
				Skin_SetupTexture(source);
			}
			else if (strcmp(skinname, "base") == 0)
			{
				if (defaultskin && (source->data.texture.data = malloc(296*194)))
				{
					memcpy(source->data.texture.data, defaultskin, 296*194);
					source->data.texture.width = 296;
					source->data.texture.height = 194;
					Skin_SetupTexture(source);
				}
				else
				{
					source->type = SKINSOURCE_SOLIDCOLOUR;
					source->data.solidcolour.colours[0] = 1;
					source->data.solidcolour.colours[1] = 0;
					source->data.solidcolour.colours[2] = 1;
				}
			}
			else
			{
				ok = 0;
			}

			if (ok)
			{
				source->next = skinsources;
				skinsources = source;

				return source;
			}

			free(source->skinname);
		}

		free(source);
	}

	if (strcmp(skinname, "base") != 0)
		return Skin_GetSource("base");

	return 0;
}

static void Skin_DeleteSource(struct SkinSource *source)
{
	struct SkinSource *s;
	struct SkinTranslation *translation;
	struct SkinTranslation *next;

	next = source->translations;
	while((translation = next))
	{
		next = translation->next;

		SkinImp_Destroy(translation->skinimp);
		free(translation);
	}

	if (source == skinsources)
		skinsources = source->next;
	else
	{
		s = skinsources;
		while(s)
		{
			if (s->next == source)
			{
				s->next = source->next;
				break;
			}

			s = s->next;
		}
	}

	free(source->skinname);
	if (source->type == SKINSOURCE_TEXTURE_PALETTED || source->type == SKINSOURCE_TEXTURE_PALETTED_TRANSLATED || source->type == SKINSOURCE_TEXTURE_TRUECOLOUR)
	{
		if (source->data.texture.data && source->data.texture.data != defaultskin)
		{
			free(source->data.texture.data);
		}
	}
	free(source);
}

struct SkinImp *Skin_GetTranslation(const char *skinname, unsigned int topcolour, unsigned int bottomcolour)
{
	struct SkinSource *source;
	struct SkinTranslation *translation;
	struct SkinImp *skinimp;

	source = Skin_GetSource(skinname);
	if (!source)
		return 0;

	if (topcolour > 13)
		topcolour = 13;

	if (bottomcolour > 13)
		bottomcolour = 13;

	translation = source->translations;
	while(translation)
	{
		if ((translation->topcolour == topcolour && translation->bottomcolour == bottomcolour)
		 || source->type == SKINSOURCE_SOLIDCOLOUR
		 || source->type == SKINSOURCE_TEXTURE_PALETTED)
		{
			return translation->skinimp;
		}

		translation = translation->next;
	}

	translation = malloc(sizeof(*translation));
	if (translation)
	{
		memset(translation, 0, sizeof(*translation));

		skinimp = 0;

		if (source->type == SKINSOURCE_SOLIDCOLOUR)
			skinimp = SkinImp_CreateSolidColour(source->data.solidcolour.colours);
		else if (source->type == SKINSOURCE_TEXTURE_PALETTED)
			skinimp = SkinImp_CreateTexturePaletted(source->data.texture.data, 296, 194, source->data.texture.width);
		else if (source->type == SKINSOURCE_TEXTURE_PALETTED_TRANSLATED)
		{
			unsigned char table[256];
			unsigned char *translated;
			unsigned char *src;
			unsigned char *dst;
			unsigned int top;
			unsigned int bottom;
			unsigned int width;
			unsigned int height;
			unsigned int x;
			unsigned int y;
			unsigned int i;

			translated = malloc(296*194);
			if (translated)
			{
				top = topcolour * 16;
				bottom = bottomcolour * 16;

				for(i=0;i<16;i++)
				{
					table[i] = i;
				}

				if (top < 128)
				{
					for(i=0;i<16;i++)
					{
						table[i + 16] = top + i;
					}
				}
				else
				{
					for(i=0;i<16;i++)
					{
						table[i + 16] = top + 15 - i;
					}
				}

				for(i=32;i<96;i++)
				{
					table[i] = i;
				}

				if (bottom < 128)
				{
					for(i=0;i<16;i++)
					{
						table[i + 96] = bottom + i;
					}
				}
				else
				{
					for(i=0;i<16;i++)
					{
						table[i + 96] = bottom + 15 - i;
					}
				}

				for(i=112;i<256;i++)
				{
					table[i] = i;
				}

				src = source->data.texture.data;
				dst = translated;

				width = source->data.texture.width;
				if (width > 296)
					width = 296;

				height = source->data.texture.height;
				if (height > 194)
					height = 194;

				for(y=0;y<height;y++)
				{
					for(x=0;x<width;x++)
					{
						dst[x] = table[src[x]];
					}

					for(;x<296;x++)
					{
						dst[x] = 0;
					}

					src += source->data.texture.width;
					dst += 296;
				}

				if (y != 194)
					memset(dst, 0, (194-y)*296);

				skinimp = SkinImp_CreateTexturePaletted(translated, 296, 194, 296);

				free(translated);
			}
		}
		else if (source->type == SKINSOURCE_TEXTURE_TRUECOLOUR)
			skinimp = SkinImp_CreateTextureTruecolour(source->data.texture.data, source->data.texture.width, source->data.texture.height);

		if (skinimp)
		{
			translation->topcolour = topcolour;
			translation->bottomcolour = bottomcolour;
			translation->skinimp = skinimp;

			translation->next = source->translations;
			source->translations = translation;

			return skinimp; 
		}

		free(translation);
	}

	return 0;
}

void Skin_SetDefault(void *data, unsigned int width, unsigned int height)
{
	unsigned int x;
	unsigned int y;
	unsigned char *src;
	unsigned char *dst;

	if (width < 296 || height < 194)
		return;

	if (!defaultskin)
		defaultskin = malloc(296*194);

	if (!defaultskin)
		return;

	src = data;
	dst = defaultskin;

	for(y=0;y<194;y++)
	{
		for(x=0;x<296;x++)
		{
			dst[x] = src[x];
		}

		src += width;
		dst += 296;
	}
}

void Skin_FreeAll()
{
	while(skinsources)
		Skin_DeleteSource(skinsources);
}

void Skin_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register(&baseskin);
	Cvar_Register(&noskins);
	Cvar_ResetCurrentGroup();
}

void Skin_Init()
{
}

void Skin_Shutdown()
{
	Skin_FreeAll();

	free(defaultskin);
	defaultskin = 0;
}

