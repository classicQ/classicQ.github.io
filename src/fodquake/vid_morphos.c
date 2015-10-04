/*
Copyright (C) 2006-2011 Mark Olsen

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

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfx.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>

#include <string.h>

#include "quakedef.h"
#include "d_local.h"
#include "input.h"
#include "keys.h"
#include "in_morphos.h"
#include "vid_mode_morphos.h"

struct display
{
	unsigned int width;
	unsigned int height;
	int fullscreen;
	char used_mode[256];

	void *inputdata;

	char *buffer;

	struct Screen *screen;
	struct Window *window;

	struct ScreenBuffer *screenbuffers[3];
	int currentbuffer;

	void *pointermem;

	struct RastPort rastport;

	char pal[256 * 4];
};

void Sys_Video_CvarInit(void)
{
}

int Sys_Video_Init()
{
	return 1;
}

void Sys_Video_Shutdown()
{
}

void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;
	struct modeinfo modeinfo;
	char monitorname[128];
	char *p;
	int i;
	int screenbuffersallocated;

	d = AllocVec(sizeof(*d), MEMF_CLEAR);
	if (d)
	{
		if (fullscreen)
		{
			if (*mode && modeline_to_modeinfo(mode, &modeinfo))
			{
				snprintf(monitorname, sizeof(monitorname), "%s.monitor", modeinfo.monitorname);
				d->screen = OpenScreenTags(0,
					SA_Width, modeinfo.width,
					SA_Height, modeinfo.height,
					SA_Depth, 8,
#ifndef AROS
					SA_MonitorName, monitorname,
#endif
					SA_Quiet, TRUE,
					TAG_DONE);
			}
			else
			{
				d->screen = OpenScreenTags(0,
					SA_Depth, 8,
					SA_Quiet, TRUE,
					TAG_DONE);
			}

			if (d->screen)
			{
				width = d->screen->Width;
				height = d->screen->Height;

				strlcpy(monitorname, "Dunno", sizeof(monitorname));

#ifndef __AROS__
				if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 53))
				{
					GetAttr(SA_MonitorName, d->screen, &p);
					if (p)
					{
						strlcpy(monitorname, p, sizeof(monitorname));
						p = strstr(monitorname, ".monitor");
						if (p)
							*p = 0;
					}
				}
#endif

				snprintf(d->used_mode, sizeof(d->used_mode), "%s,%d,%d,%d", monitorname, width, height, GetBitMapAttr(d->screen->RastPort.BitMap, BMA_DEPTH));
			}
			else
				fullscreen = 0;
		}

		d->window = OpenWindowTags(0,
			WA_InnerWidth, width,
			WA_InnerHeight, height,
			WA_Title, "Fodquake",
			WA_DragBar, d->screen ? FALSE : TRUE,
			WA_DepthGadget, d->screen ? FALSE : TRUE,
			WA_Borderless, d->screen ? TRUE : FALSE,
			WA_RMBTrap, TRUE,
			d->screen ? WA_PubScreen : TAG_IGNORE, (ULONG) d->screen,
			WA_Activate, TRUE,
			WA_ReportMouse, TRUE,
			TAG_DONE);

		if (d->window)
		{
			d->buffer = AllocVec(width * height, MEMF_ANY);
			if (d->buffer)
			{
				d->pointermem = AllocVec(256, MEMF_ANY | MEMF_CLEAR);
				if (d->pointermem)
				{
					SetPointer(d->window, d->pointermem, 16, 16, 0, 0);

					if (d->screen)
					{
						for (screenbuffersallocated = 0; screenbuffersallocated < 3; screenbuffersallocated++)
						{
							d->screenbuffers[screenbuffersallocated] = AllocScreenBuffer(d->screen, 0, screenbuffersallocated ? SB_COPY_BITMAP : SB_SCREEN_BITMAP);
							if (d->screenbuffers[screenbuffersallocated] == 0)
								break;
						}
					}

					if (d->screen == 0 || screenbuffersallocated == 3)
					{
						d->width = width;
						d->height = height;
						d->fullscreen = fullscreen;

						d->currentbuffer = 0;

						InitRastPort(&d->rastport);

						Sys_Video_SetPalette(d, palette);

						d->inputdata = Sys_Input_Init(d->screen, d->window);

						return d;
					}

					if (d->screen)
					{
						screenbuffersallocated--;

						while(screenbuffersallocated >= 0)
						{
							FreeScreenBuffer(d->screen, d->screenbuffers[screenbuffersallocated]);
							screenbuffersallocated--;
						}
					}

					FreeVec(d->pointermem);
				}

				FreeVec(d->buffer);
			}

			CloseWindow(d->window);
		}

		if (d->screen)
		{
			CloseScreen(d->screen);
		}

		FreeVec(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;
	int i;

	Sys_Input_Shutdown(d->inputdata);

	FreeVec(d->buffer);

	CloseWindow(d->window);

	FreeVec(d->pointermem);

	if (d->screen)
	{
		for (i = 0; i < 3; i++)
		{
			FreeScreenBuffer(d->screen, d->screenbuffers[i]);
		}

		CloseScreen(d->screen);
	}

	FreeVec(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	struct display *d;
	
	d = display;

	return d->screen ? 3 : 1;
}

int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down)
{
	struct display *d = display;

	return Sys_Input_GetKeyEvent(d->inputdata, keynum, down);
}
 
void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

	Sys_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

void Sys_Video_Update(void *display, vrect_t * rects)
{
	struct display *d = display;

	while (rects)
	{
		if (d->screen)
		{
			d->rastport.BitMap = d->screenbuffers[d->currentbuffer]->sb_BitMap;
			WritePixelArray(d->buffer, rects->x, rects->y, d->width, &d->rastport, rects->x, rects->y, rects->width, rects->height, RECTFMT_LUT8);
		}
		else
		{
			WriteLUTPixelArray(d->buffer, rects->x, rects->y, d->width, d->window->RPort, d->pal, d->window->BorderLeft + rects->x, d->window->BorderTop + rects->y, rects->width, rects->height, CTABFMT_XRGB8);
		}


		rects = rects->pnext;
	}

	if (d->screen)
	{
		ChangeScreenBuffer(d->screen, d->screenbuffers[d->currentbuffer]);
		d->currentbuffer++;
		d->currentbuffer %= 3;
	}
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	if (!d->screen)
	{
		Sys_Input_GrabMouse(d->inputdata, dograb);

		if (dograb)
		{
			/* Hide pointer */

			SetPointer(d->window, d->pointermem, 16, 16, 0, 0);
		}
		else
		{
			/* Show pointer */

			ClearPointer(d->window);
		}
	}
}

void Sys_Video_SetPalette(void *display, unsigned char *palette)
{
	struct display *d = display;
	int i;

	ULONG spal[1 + (256 * 3) + 1];

	if (d->screen)
	{
		spal[0] = 256 << 16;

		for (i = 0; i < 256; i++)
		{
			spal[1 + (i * 3)] = ((unsigned int)palette[i * 3]) << 24;
			spal[2 + (i * 3)] = ((unsigned int)palette[i * 3 + 1]) << 24;
			spal[3 + (i * 3)] = ((unsigned int)palette[i * 3 + 2]) << 24;
		}

		spal[1 + (3 * 256)] = 0;

		LoadRGB32(&d->screen->ViewPort, spal);
	}

	for (i = 0; i < 256; i++)
	{
#ifdef FOD_BIGENDIAN
		d->pal[i * 4] = 0;
		d->pal[i * 4 + 1] = palette[i * 3 + 0];
		d->pal[i * 4 + 2] = palette[i * 3 + 1];
		d->pal[i * 4 + 3] = palette[i * 3 + 2];
#else
		d->pal[i * 4] = palette[i * 3 + 2];
		d->pal[i * 4 + 1] = palette[i * 3 + 1];
		d->pal[i * 4 + 2] = palette[i * 3 + 0];
		d->pal[i * 4 + 3] = 0;
#endif
	}
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d;

	d = display;

	SetWindowTitles(d->window, text, (void *)-1);
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

qboolean Sys_Video_GetFullscreen(void *display)
{
	struct display *d;

	d = display;

	return d->fullscreen;
}

const char *Sys_Video_GetMode(void *display)
{
	struct display *d;

	d = display;

	return d->used_mode;
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

	return d->buffer;
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

