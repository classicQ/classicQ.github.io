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

#include <stdlib.h>

#include "gl_local.h"
#include "gl_texture.h"
#include "gl_skinimp.h"
#include "skinimp.h"

struct SkinImp *SkinImp_CreateSolidColour(float *colours)
{
	struct SkinImp *skinimp;
	unsigned char tex[3];

	skinimp = malloc(sizeof(*skinimp));
	if (skinimp)
	{
		skinimp->texid = texture_extension_number++;

		tex[0] = colours[0]*255;
		tex[1] = colours[1]*255;
		tex[2] = colours[2]*255;

		GL_Bind(skinimp->texid);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, tex);

		return skinimp;
	}

	return 0;
}

#define ISPOT(x) (((x) & -(x)) == (x))

struct SkinImp *SkinImp_CreateTexturePaletted(void *data, unsigned int width, unsigned int height, unsigned int modulo)
{
	struct SkinImp *skinimp;
	unsigned int *skinmem;
	unsigned char *src;
	unsigned int *dst;
	unsigned int x;
	unsigned int y;
	unsigned int dofullbright;

	dofullbright = 0;
	src = data;
	for(y=0;y<height;y++)
	{
		for(x=0;x<width;x++)
		{
			if (src[x] >= 224)
			{
				dofullbright = 1;
				break;
			}
		}

		if (dofullbright)
			break;

		src += modulo;
	}

	skinimp = malloc(sizeof(*skinimp));
	if (skinimp)
	{
		if (gl_npot || (ISPOT(width) && ISPOT(height)))
		{
			skinmem = malloc(width*height*4);
			if (skinmem)
			{
				src = data;
				dst = skinmem;

				for(y=0;y<height;y++)
				{
					for(x=0;x<width;x++)
					{
						dst[x] = d_8to24table[src[x]];
					}

					src += modulo;
					dst += width;
				}

				skinimp->texid = texture_extension_number++;

				GL_Bind(skinimp->texid);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, skinmem);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

				if (dofullbright)
				{
					src = data;
					dst = skinmem;

					for(y=0;y<height;y++)
					{
						for(x=0;x<width;x++)
						{
							if (src[x] < 224)
								dst[x] = 0;
						}

						src += modulo;
						dst += width;
					}

					skinimp->fbtexid = texture_extension_number++;

					GL_Bind(skinimp->fbtexid);
					glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, skinmem);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				}
				else
					skinimp->fbtexid = 0;

				free(skinmem);

				return skinimp;
			}
		}

		free(skinimp);
	}

	return 0;
}

struct SkinImp *SkinImp_CreateTextureTruecolour(void *data, unsigned int width, unsigned int height)
{
	struct SkinImp *skinimp;

	skinimp = malloc(sizeof(*skinimp));
	if (skinimp)
	{
		if (gl_npot || (ISPOT(width) && ISPOT(height)))
		{
			skinimp->texid = texture_extension_number++;
			skinimp->fbtexid = 0;

			GL_Bind(skinimp->texid);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			return skinimp;
		}

		free(skinimp);
	}

	return 0;
}

void SkinImp_Destroy(struct SkinImp *skinimp)
{
	glDeleteTextures(1, &skinimp->texid);
	free(skinimp);
}

