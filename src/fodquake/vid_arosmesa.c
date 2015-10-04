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

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/extensions.h>
#include <cybergraphx/cybergraphics.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <GL/arosmesa.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "gl_local.h"
#include "in_morphos.h"
#include "vid_mode_morphos.h"

struct Library *MesaBase = 0;

struct display
{
	void *inputdata;

	unsigned int width, height;
	int fullscreen;
	char used_mode[256];

	struct Screen *screen;
	struct Window *window;

	void *pointermem;

	char pal[256*4];

	AROSMesaContext mesacontext;
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
	int r;
	int i;

	d = AllocVec(sizeof(*d), MEMF_CLEAR);
	if (d)
	{
		MesaBase = OpenLibrary("mesa.library", 0);
		if (MesaBase)
		{
			if (fullscreen)
			{
				if (*mode && modeline_to_modeinfo(mode, &modeinfo))
				{
					snprintf(monitorname, sizeof(monitorname), "%s.monitor", modeinfo.monitorname);
					d->screen = OpenScreenTags(0,
						SA_Width, modeinfo.width,
						SA_Height, modeinfo.height,
						SA_Depth, modeinfo.depth,
#if 0
						SA_MonitorName, monitorname,
#endif
						SA_Quiet, TRUE,
						TAG_DONE);
				}
				else
				{
					d->screen = OpenScreenTags(0,
						SA_Quiet, TRUE,
						TAG_DONE);
				}

				if (d->screen)
				{
					width = d->screen->Width;
					height = d->screen->Height;

					snprintf(d->used_mode, sizeof(d->used_mode), "Dunno,%d,%d,42", width, height);
				}
				else
					fullscreen = 0;
			}

			if (d->screen || !fullscreen)
			{
				d->window = OpenWindowTags(0,
					d->screen?WA_Width:WA_InnerWidth, width,
					d->screen?WA_Height:WA_InnerHeight, height,
					WA_Title, "Fodquake",
					WA_DragBar, d->screen?FALSE:TRUE,
					WA_DepthGadget, d->screen?FALSE:TRUE,
					WA_Borderless, d->screen?TRUE:FALSE,
					WA_RMBTrap, TRUE,
					d->screen?WA_CustomScreen:TAG_IGNORE, (IPTR)d->screen,
					WA_Activate, TRUE,
					TAG_DONE);

				if (d->window)
				{
					d->mesacontext = AROSMesaCreateContextTags(
						AMA_Window, d->window,
						AMA_Left, d->screen?0:d->window->BorderLeft,
						AMA_Top, d->screen?0:d->window->BorderTop,
						AMA_Width, width,
						AMA_Height, height,
						AMA_NoStencil, TRUE,
						AMA_NoAccum, TRUE,
						TAG_DONE);

					if (d->mesacontext)
					{
						AROSMesaMakeCurrent(d->mesacontext);
						
						d->pointermem = AllocVec(256, MEMF_ANY|MEMF_CLEAR);
						if (d->pointermem)
						{
							SetPointer(d->window, d->pointermem, 16, 16, 0, 0);

							d->width = width;
							d->height = height;
							d->fullscreen = fullscreen;

							d->inputdata = Sys_Input_Init(d->screen, d->window);
							if (d->inputdata)
							{
								return d;
							}

							FreeVec(d->pointermem);
						}

						AROSMesaMakeCurrent(0);
						AROSMesaDestroyContext(d->mesacontext);
					}

					CloseWindow(d->window);
				}

				if (d->screen)
					CloseScreen(d->screen);
			}

			CloseLibrary(MesaBase);
		}

		FreeVec(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	Sys_Input_Shutdown(d->inputdata);

	AROSMesaMakeCurrent(0);
	AROSMesaDestroyContext(d->mesacontext);

	CloseWindow(d->window);

	FreeVec(d->pointermem);

	if (d->screen)
		CloseScreen(d->screen);

	CloseLibrary(MesaBase);

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

void Sys_Video_BeginFrame(void *display)
{
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;

	AROSMesaSwapBuffers(d->mesacontext);
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

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	return 0; 
}

void *Sys_Video_GetProcAddress(void *display, const char *name)
{
	return AROSMesaGetProcAddress(name);
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

