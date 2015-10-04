#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "vid.h"
#include "common.h"
#include "sound.h"
#include "wad.h"
#include "image.h"
#include "draw.h"

static struct Picture *conchar;

extern cvar_t scr_menualpha;

struct WadHeader
{
	unsigned int width;
	unsigned short height;
};

struct LmpHeader
{
	unsigned int width;
	unsigned int height;
};

struct Picture
{
	unsigned int width;
	unsigned int height;
	int transparentpixels;
};

static struct
{
	struct Picture pic;
	unsigned char data[4];
} dummypicture =
{
	{ 2, 2 },
	{ 1, 0, 0, 1 }
};

static void customCrosshair_Init(void);

void DrawImp_CvarInit()
{
}

static void DrawImp_LoadConChar()
{
	unsigned char *draw_chars;
	unsigned char *dst;
	unsigned int i;

	draw_chars = W_GetLumpName("conchars");

	conchar = malloc(sizeof(*conchar) + 128 * 128);
	if (conchar)
	{
		conchar->width = 128;
		conchar->height = 128;
		conchar->transparentpixels = 1;

		dst = (unsigned char *)(conchar + 1);

		for(i=0;i<128*128;i++)
		{
			if (draw_chars[i] == 0)
				dst[i] = 255;
			else
				dst[i] = draw_chars[i];
		}
	}
}

void DrawImp_Init()
{
	DrawImp_LoadConChar();

	customCrosshair_Init();
}

void DrawImp_Shutdown()
{
	free(conchar);
}

void Draw_SetSize(unsigned int width, unsigned int height)
{
}

static struct Picture *Draw_LoadWadPicture(const char *name)
{
	struct WadHeader *header;
	struct Picture *picture;
	unsigned int width;
	unsigned int height;
	unsigned int size;
	void *data;

	data = W_GetLumpName(name);

	if (data) /* Always true, Quake sucks. */
	{
		header = data;

		width = header->width;
		height = header->height;

		if (width < 32768 && height < 32768)
		{
			size = width * height;

			picture = malloc(sizeof(*picture) + size);
			if (picture)
			{
				memcpy(picture + 1, data + 8, width * height);

				picture->width = width;
				picture->height = height;

				return picture;
			}
		}
	}

	return 0;
}

static struct Picture *Draw_LoadLmpPicture(FILE *fh)
{
	struct LmpHeader header;
	struct Picture *picture;
	int r;
	unsigned int width;
	unsigned int height;
	unsigned int size;

	r = fread(&header, 1, sizeof(header), fh);
	if (r == sizeof(header))
	{
		width = LittleLong(header.width);
		height = LittleLong(header.height);

		if (width < 32768 && height < 32768)
		{
			size = width * height;

			picture = malloc(sizeof(*picture) + size);
			if (picture)
			{
				r = fread(picture + 1, 1, size, fh);
				if (r == size)
				{
					picture->width = width;
					picture->height = height;

					return picture;
				}

				free(picture);
			}
		}
	}

	return 0;
}

struct Picture *Draw_LoadPicture(const char *name, enum Draw_LoadPicture_Fallback fallback)
{
	char *newname;
	char *newnameextension;
	FILE *fh;
	unsigned int namelen;
	struct Picture *picture;
	unsigned int width;
	unsigned int height;
	void *data;

	data = 0;
	picture = 0;

	if (strncmp(name, "wad:", 4) == 0)
	{
		picture = Draw_LoadWadPicture(name + 4);
	}
	else
	{
		namelen = strlen(name);

		newname = malloc(namelen + 4 + 1);
		if (newname)
		{
			COM_CopyAndStripExtension(name, newname, namelen + 1);

			newnameextension = newname + strlen(newname);

			strcpy(newnameextension, ".pcx");
			data = Image_LoadPCX(0, newname, 0, 0, &width, &height);

			free(newname);
		}

		if (data && width <= 32768 && height <= 32728)
		{
			picture = malloc(sizeof(*picture) + width * height);
			if (picture)
			{
				picture->width = width;
				picture->height = height;

				memcpy(picture + 1, data, width * height);
			}
		}

		if (!picture)
		{
			FS_FOpenFile(name, &fh);
			if (fh)
			{
				if (namelen > 4 && strcmp(name + namelen - 4, ".lmp") == 0)
				{
					picture = Draw_LoadLmpPicture(fh);
				}

				fclose(fh);
			}
		}
	}

	if (picture)
	{
		picture->transparentpixels = !!memchr(picture + 1, 255, picture->width * picture->height);

		return picture;
	}

	if (fallback == DRAW_LOADPICTURE_DUMMYFALLBACK)
		return &dummypicture.pic;

	return 0;
}

void Draw_FreePicture(struct Picture *picture)
{
	if (picture == &dummypicture.pic)
		return;

	free(picture);
}

unsigned int Draw_GetPictureWidth(struct Picture *picture)
{
	return picture->width;
}

unsigned int Draw_GetPictureHeight(struct Picture *picture)
{
	return picture->height;
}

static void Draw_DrawPictureNonScaled(struct Picture *picture, unsigned int srcx, unsigned int srcy, unsigned int srcwidth, unsigned int srcheight, int x, int y)
{
	unsigned int width;
	unsigned int height;
	unsigned char *data;
	unsigned char *dst;

	data = (unsigned char *)(picture + 1);
	data += srcy * picture->width;
	data += srcx;

	width = srcwidth;
	height = srcheight;

	if (x < 0)
	{
		if (-x >= width)
			return;

		data += -x;
		width += x;
		x = 0;
	}

	if (y < 0)
	{
		if (-y >= height)
			return;

		data += (-y) * picture->width;
		height += y;
		y = 0;
	}

	if (x >= vid.displaywidth || y >= vid.displayheight)
		return;

	if (x + width > vid.displaywidth)
		width = vid.displaywidth - x;

	if (y + height > vid.displayheight)
		height = vid.displayheight - y;

	dst = vid.buffer;
	dst += y * vid.rowbytes;
	dst += x;

	for(y=0;y<height;y++)
	{
		if (picture->transparentpixels)
		{
			for(x=0;x<width;x++)
			{
				if (data[x] != 255)
					dst[x] = data[x];
			}
		}
		else
		{
			memcpy(dst, data, width);
		}

		data += picture->width;
		dst += vid.rowbytes;
	}
}

static void Draw_DrawPictureScaled(struct Picture *picture, unsigned int srcx, unsigned int srcy, unsigned int srcwidth, unsigned int srcheight, int x, int y, int width, int height)
{
	unsigned int sx;
	unsigned int sy;

	unsigned int stepx;
	unsigned int stepy;

	unsigned int i;

	unsigned char *data;
	unsigned char *dst;

	unsigned char p;


	stepx = (srcwidth * (1<<16)) / width;
	stepy = (srcheight * (1<<16)) / height;

	if (x < 0)
	{
		if (-x >= width)
			return;

		width += x;
		sx = stepx * (-x);
		x = 0;
	}
	else
		sx = 0;

	if (y < 0)
	{
		if (-y >= height)
			return;

		height += y;
		sy = stepy * (-y);
		y = 0;
	}
	else
		sy = 0;

	if (x >= vid.displaywidth || y >= vid.displayheight)
		return;

	if (x + width > vid.displaywidth)
		width = vid.displaywidth - x;

	if (y + height > vid.displayheight)
		height = vid.displayheight - y;

	dst = vid.buffer;
	dst += y * vid.rowbytes;
	dst += x;

	for(y=0;y<height;y++)
	{
		data = (unsigned char *)(picture + 1);
		data += picture->width * ((sy>>16) + srcy);
		data += srcx;

		if (picture->transparentpixels)
		{
			for(x=0,i=sx;x<width;x++)
			{
				p = data[i>>16];
				if (p != 255)
					dst[x] = p;
				i += stepx;
			}
		}
		else
		{
			for(x=0,i=sx;x<width;x++)
			{
				dst[x] = data[i>>16];
				i += stepx;
			}
		}

		sy += stepy;

		dst += vid.rowbytes;
	}
}

void Draw_DrawPicture(struct Picture *picture, int x, int y, unsigned int width, unsigned int height)
{
	int displayx;
	int displayy;
	int displaywidth;
	int displayheight;

	displayx = (x * (int)vid.displaywidth) / (int)vid.conwidth;
	displayy = (y * (int)vid.displayheight) / (int)vid.conheight;
	displaywidth = (((x + width) * (int)vid.displaywidth) / (int)vid.conwidth) - displayx;
	displayheight = (((y + height) * (int)vid.displayheight) / (int)vid.conheight) - displayy;

	if (displaywidth == picture->width && displayheight == picture->height)
	{
		Draw_DrawPictureNonScaled(picture, 0, 0, picture->width, picture->height, displayx, displayy);
	}
	else
	{
		Draw_DrawPictureScaled(picture, 0, 0, picture->width, picture->height, displayx, displayy, displaywidth, displayheight);
	}
}

void Draw_DrawPictureModulated(struct Picture *picture, int x, int y, unsigned int width, unsigned int height, float r, float g, float b, float alpha)
{
	if (alpha <= 0)
		return;

	Draw_DrawPicture(picture, x, y, width, height);
}

static void Draw_DrawSubPictureAbsolute(struct Picture *picture, unsigned int sx, unsigned int sy, unsigned int swidth, unsigned int sheight, int x, int y, unsigned int width, unsigned int height)
{
	int displayx;
	int displayy;
	int displaywidth;
	int displayheight;

	displayx = (x * (int)vid.displaywidth) / (int)vid.conwidth;
	displayy = (y * (int)vid.displayheight) / (int)vid.conheight;
	displaywidth = (((x + width) * (int)vid.displaywidth) / (int)vid.conwidth) - displayx;
	displayheight = (((y + height) * (int)vid.displayheight) / (int)vid.conheight) - displayy;

	if (displaywidth == picture->width && displayheight == picture->height)
	{
		Draw_DrawPictureNonScaled(picture, sx, sy, swidth, sheight, displayx, displayy);
	}
	else
	{
		Draw_DrawPictureScaled(picture, sx, sy, swidth, sheight, displayx, displayy, displaywidth, displayheight);
	}
}

void Draw_DrawSubPicture(struct Picture *picture, float sx, float sy, float swidth, float sheight, int x, int y, unsigned int width, unsigned int height)
{
	Draw_DrawSubPictureAbsolute(picture, sx * picture->width, sy * picture->height, swidth * picture->width, sheight * picture->height, x, y, width, height);
}

void Draw_Fill(int x, int y, int width, int height, int c)
{
	unsigned char *p;
	unsigned int i, j;
	int displayx;
	int displayy;
	int displaywidth;
	int displayheight;

	displayx = (x * (int)vid.displaywidth) / (int)vid.conwidth;
	displayy = (y * (int)vid.displayheight) / (int)vid.conheight;
	displaywidth = (((x + width) * (int)vid.displaywidth) / (int)vid.conwidth) - displayx;
	displayheight = (((y + height) * (int)vid.displayheight) / (int)vid.conheight) - displayy;

	if (displayx < 0)
	{
		displaywidth -= displayx;
		displayx = 0;
	}
	else if (displayx >= vid.displaywidth || displaywidth <= 0)
		return;

	if (displayx + displaywidth > vid.displaywidth)
		displaywidth = vid.displaywidth - displayx;

	if (displayy < 0)
	{
		displayheight -= displayy;
		displayy = 0;
	}
	else if (displayy >= vid.displayheight || displayheight <= 0)
		return;

	if (displayy + displayheight > vid.displayheight)
		displayheight = vid.displayheight - displayy;

	p = vid.buffer;
	p += displayy * vid.rowbytes;
	p += displayx;

	for(i=0;i<displayheight;i++)
	{
		for(j=0;j<displaywidth;j++)
		{
			p[j] = c;
		}

		p += vid.rowbytes;
	}
}

void Draw_Line(int x1, int y1, int x2, int y2, float width, float r, float g, float b, float alpha)
{
}

void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha)
{
}

void Draw_AlphaFillRGB(int x, int y, int w, int h, float r, float g, float b, float alpha)
{
}

void Draw_BeginTextRendering()
{
}

void Draw_EndTextRendering()
{
}

void Draw_BeginColoredTextRendering()
{
}

void Draw_EndColoredTextRendering()
{
}

void DrawImp_SetTextColor(int r, int g, int b)
{
}

void DrawImp_Character(int x, int y, unsigned char num)
{
	int displayx;
	int displayy;
	int displaywidth;
	int displayheight;
	unsigned int sx, sy;

	sy = (num/16)*8;
	sx = (num%16)*8;

	displayx = (x * (int)vid.displaywidth) / (int)vid.conwidth;
	displayy = (y * (int)vid.displayheight) / (int)vid.conheight;
	displaywidth = (((x + 8) * (int)vid.displaywidth) / (int)vid.conwidth) - displayx;
	displayheight = (((y + 8) * (int)vid.displayheight) / (int)vid.conheight) - displayy;

	if (displaywidth == 8 && displayheight == 8)
	{
		Draw_DrawPictureNonScaled(conchar, sx, sy, 8, 8, displayx, displayy);
	}
	else
	{
		Draw_DrawPictureScaled(conchar, sx, sy, 8, 8, displayx, displayy, displaywidth, displayheight);
	}
}

/* --- */

static void Draw_Pixel(int x, int y, byte color)
{
	byte *dest;

	dest = vid.buffer + y * vid.rowbytes + x;
	*dest = color;
}


#define		NUMCROSSHAIRS 6
static const qboolean crosshairdata[NUMCROSSHAIRS][64] =
{
	{
		false, false, false, true,  false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, true,  false, false, false, false,
		true,  false, true,  false, true,  false, true,  false,
		false, false, false, true,  false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, true,  false, false, false, false,
		false, false, false, false, false, false, false, false,
	},
	{
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, true,  false, false, false, false,
		false, false, true,  true,  true,  false, false, false,
		false, false, false, true,  false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
	},
	{
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, true,  false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
	},
	{
		true,  false, false, false, false, false, false, true,
		false, true,  false, false, false, false, true, false,
		false, false, true,  false, false, true,  false, false,
		false, false, false, false, false, false, false, false,
		false, false, false, false, false, false, false, false,
		false, false, true,  false, false, true,  false, false,
		false, true,  false, false, false, false, true,  false,
		true,  false, false, false, false, false, false, true,
	},
	{
		false, false, true,  true,  true,  false, false, false, 
		false, true,  false, true,  false, true,  false, false, 
		true,  true,  false, true,  false, true,  true,  false, 
		true,  true,  true,  true,  true,  true,  true,  false, 
		true,  false, true,  true,  true,  false, true,  false, 
		false, true,  false, false, false, true,  false, false, 
		false, false, true,  true,  true,  false, false, false, 
		false, false, false, false, false, false, false, false,
	},
	{
		false, false, true,  true,  true,  false, false, false, 
		false, false, false, false, false, false, false, false, 
		true,  false, false, false, false, false, true,  false, 
		true,  false, false, true,  false, false, true,  false, 
		true,  false, false, false, false, false, true,  false, 
		false, false, false, false, false, false, false, false, 
		false, false, true,  true,  true,  false, false, false, 
		false, false, false, false, false, false, false, false
	}
};

static qboolean customcrosshairdata[64];
static qboolean customcrosshair_loaded;
static void customCrosshair_Init(void)
{
	FILE *f;
	int i = 0, c;

	customcrosshair_loaded = false;
	if (FS_FOpenFile("crosshairs/crosshair.txt", &f) == -1)
		return;

	while (i < 64)
	{
		c = fgetc(f);
		if (c == EOF)
		{
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");
			fclose(f);
			return;
		}

		if (c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v' || c == ' ')
			continue;

		if (c  != 'x' && c  != 'X' && c  != 'O' && c  != 'o')
		{
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			fclose(f);
			return;
		}
		else if (c == 'x' || c  == 'X' )
			customcrosshairdata[i++] = true;
		else
			customcrosshairdata[i++] = false;		
	}

	fclose(f);
	customcrosshair_loaded = true;
}

void Draw_RecalcCrosshair()
{
}

void Draw_Crosshair(void)
{
	int x, y, crosshair_val, row, col;
	extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
	extern vrect_t scr_vrect;
	byte c = (byte) crosshaircolor.value;
	const qboolean *data;

	if (!crosshair.value)
		return;

	x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value; 
	y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;

	crosshair_val = (int) crosshair.value;
	if ((crosshair_val >= 2 && crosshair_val < 2 + NUMCROSSHAIRS) || (crosshair_val == 1 && customcrosshair_loaded))
	{
		data = (crosshair_val == 1) ? customcrosshairdata : crosshairdata[crosshair_val - 2];
		for (row = 0; row < 8; row++)
		{
			for (col = 0; col < 8; col++)
			{
				if (data[row * 8 + col])
				{
					if (crosshairsize.value >= 3)
					{
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) - 1,	y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3),		y + 3 * (row - 3) + 1,	c);

						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) - 1,	c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3),		c);
						Draw_Pixel(x + 3 * (col - 3) + 1,	y + 3 * (row - 3) + 1,	c);
					}
					else if (crosshairsize.value >= 2)
					{
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3),		y + 2 * (row - 3) - 1,	c);

						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3),		c);
						Draw_Pixel(x + 2 * (col - 3) - 1,	y + 2 * (row - 3) - 1,	c);
					}
					else
					{
						Draw_Pixel(x + col - 3, y + row - 3, c);
					}
				}
			}
		}
	}
	else
	{
		Draw_Character(x - 4, y - 4, '+');
	}
}

void Draw_FadeScreen(void)
{
	int x,y;
	byte *pbuf;
	float alpha;

	alpha = bound(0, scr_menualpha.value, 1);

	if (!alpha)
		return;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

	for (y = 0; y < vid.displayheight; y++)
	{
		int t;

		pbuf = (byte *) (vid.buffer + vid.rowbytes * y);

		if (alpha < 1 / 3.0)
		{
			t = (y & 1) << 1;

			for (x = 0; x < vid.displaywidth; x++)
			{
				if ((x & 3) == t)
					pbuf[x] = 0;
			}
		}
		else if (alpha < 2 / 3.0)
		{
			t = (y & 1) << 1;

			for (x = 0; x < vid.displaywidth; x++)
			{
				if ((x & 1) == t)
					pbuf[x] = 0;
			}
		}
		else if (alpha < 1)
		{
			t = (y & 1) << 1;

			for (x = 0; x < vid.displaywidth; x++)
			{
				if ((x & 3) != t)
					pbuf[x] = 0;
			}
		}
		else
		{
			for (x = 0; x < vid.displaywidth; x++)
				pbuf[x] = 0;
		}
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

