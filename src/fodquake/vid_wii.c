/*
Copyright (C) 2008 Mark Olsen

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

#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>

#include <stdlib.h>
#include <string.h>

#define false qfalse
#define true qtrue
#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"
#include "in_wii.h"
#undef false
#undef true

struct display
{
	unsigned int width;
	unsigned int height;

	void *inputdata;

	unsigned char *buffer8;
	unsigned short *buffer;

	unsigned char paly[256];
	unsigned char palcb[256];
	unsigned char palcr[256];
};

void Sys_Video_CvarInit(void)
{
}

void *Sys_Video_Open(unsigned int width, unsigned int height, unsigned int depth, int fullscreen, unsigned char *palette)
{
	struct display *d;

	d = malloc(sizeof(*d));
	if (d)
	{
		GXRModeObj *rmode;

		switch(VIDEO_GetCurrentTvMode())
		{
			case VI_NTSC:
				rmode = &TVNtsc480IntDf;
				break;

			case VI_PAL:
				rmode = &TVPal528IntDf;
				break;

			case VI_MPAL:
				rmode = &TVMpal480IntDf;
				break;

			default:
				rmode = &TVNtsc480IntDf;
				break;
		}

		d->width = rmode->fbWidth;
		d->height = rmode->xfbHeight;

		d->buffer8 = malloc(d->width * d->height);
		if (d->buffer8)
		{
			memset(d->buffer8, 0, d->width * d->height);

			d->buffer = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
			if (d->buffer)
			{
				memset(d->buffer, 0, d->width * d->height * 2);
				#if 1
				VIDEO_Configure(rmode);
				VIDEO_SetNextFramebuffer(d->buffer);
				VIDEO_SetBlack(FALSE);
				VIDEO_Flush();
				VIDEO_WaitVSync();
				#endif

				d->inputdata = Sys_Input_Init();
				if (d->inputdata)
				{
					Com_Printf("%s: %dx%d display initialised\n", __func__, d->width, d->height);

					return d;
				}

#warning Free fb
			}

			free(d->buffer8);
		}

		free(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d;

	d = display;

#warning Free fb
	free(d->buffer8);
	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 1;
}

void Sys_Video_GetEvents(void *display)
{
	struct display *d = display;

	Sys_Input_GetEvents(d->inputdata);
}
 
void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

	Sys_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

void Sys_Video_Update(void *display, vrect_t * rects)
{
	struct display *d = display;
	unsigned int *dest;
	unsigned char *source;

	unsigned int startx;
	unsigned int width;
	unsigned int x;
	unsigned int y;
	unsigned int cb;
	unsigned int cr;

#if 1
	while (rects)
	{
		startx = rects->x&~1;
		width = (rects->width + (rects->x - startx) + 1)&~1;
		source = d->buffer8 + rects->y * d->width + startx;
		dest = (unsigned int *)(d->buffer + rects->y * d->width + startx);

		for(y=0;y<rects->height;y++)
		{
			for(x=0;x<width;x+=2)
			{
				cb = (d->palcb[source[x]] + d->palcb[source[x+1]])>>1;
				cr = (d->palcr[source[x]] + d->palcr[source[x+1]])>>1;

				dest[x/2] = (d->paly[source[x]]<<24)|(cb<<16)|(d->paly[source[x+1]]<<8)|cr;
			}

			source+= d->width;
			dest+= d->width/2;
		}

		rects = rects->pnext;
	}
#endif
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
}

void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
	struct display *d = display;
	int i;
	int r, g, b;

	for (i = 0; i < 256; i++)
	{
		r = palette[i * 3 + 0];
		g = palette[i * 3 + 1];
		b = palette[i * 3 + 2];

		d->paly[i] = (299 * r + 587 * g + 114 * b) / 1000;
		d->palcb[i] = (-16874 * r - 33126 * g + 50000 * b + 12800000) / 100000;
		d->palcr[i] = (50000 * r - 41869 * g - 8131 * b + 12800000) / 100000;
	}
}

void VID_LockBuffer()
{
}

void VID_UnlockBuffer()
{
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
}

unsigned int Sys_Video_GetWidth(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	struct display *d;

	d = display;

	return d->height;
}

unsigned int Sys_Video_GetBytesPerRow(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

void *Sys_Video_GetBuffer(void *display)
{
	struct display *d;

	d = display;

	return d->buffer8;
}

