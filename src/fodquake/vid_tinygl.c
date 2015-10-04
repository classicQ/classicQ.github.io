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
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>

#include <string.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "gl_local.h"
#include "in_morphos.h"
#include "vid_mode_morphos.h"

#ifndef SA_GammaControl
#define SA_GammaControl (SA_Dummy + 123)
#endif

#ifndef SA_3DSupport
#define SA_3DSupport (SA_Dummy + 127)
#endif

#ifndef SA_GammaRed
#define SA_GammaRed (SA_Dummy + 124)
#endif

#ifndef SA_GammaBlue
#define SA_GammaBlue (SA_Dummy + 125)
#endif

#ifndef SA_GammaGreen
#define SA_GammaGreen (SA_Dummy + 126)
#endif

struct Library *TinyGLBase = 0;
GLContext *__tglContext;

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

	int gammaenabled;
	unsigned char *gammatable;

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
	int r;
	int i;

	d = AllocVec(sizeof(*d), MEMF_CLEAR);
	if (d)
	{
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 74))
		{
			d->gammaenabled = 1;
		}

		TinyGLBase = OpenLibrary("tinygl.library", 0);
		if (TinyGLBase)
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
						SA_MonitorName, monitorname,
						SA_Quiet, TRUE,
						SA_GammaControl, TRUE,
						SA_3DSupport, TRUE,
						TAG_DONE);
				}
				else
				{
					d->screen = OpenScreenTags(0,
						SA_Quiet, TRUE,
						SA_GammaControl, TRUE,
						SA_3DSupport, TRUE,
						TAG_DONE);
				}

				if (d->screen)
				{
					width = d->screen->Width;
					height = d->screen->Height;

					strlcpy(monitorname, "Dunno", sizeof(monitorname));

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

					snprintf(d->used_mode, sizeof(d->used_mode), "%s,%d,%d,%d", monitorname, width, height, GetBitMapAttr(d->screen->RastPort.BitMap, BMA_DEPTH));

					if (d->gammaenabled)
					{
						d->gammatable = AllocVec(256*3, MEMF_ANY);
						if (d->gammatable == 0)
						{
							CloseScreen(d->screen);
							d->screen = 0;
						}
					}
				}
				else
					fullscreen = 0;
			}

			if (d->screen || !fullscreen)
			{
				d->window = OpenWindowTags(0,
					WA_InnerWidth, width,
					WA_InnerHeight, height,
					WA_Title, "Fodquake",
					WA_DragBar, d->screen?FALSE:TRUE,
					WA_DepthGadget, d->screen?FALSE:TRUE,
					WA_Borderless, d->screen?TRUE:FALSE,
					WA_RMBTrap, TRUE,
					d->screen?WA_PubScreen:TAG_IGNORE, (ULONG)d->screen,
					WA_Activate, TRUE,
					TAG_DONE);

				if (d->window)
				{
					__tglContext = GLInit();
					if (__tglContext)
					{
						if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
						{
							r = glAInitializeContextScreen(d->screen);
						}
						else
						{
							r = glAInitializeContextWindowed(d->window);
						}

						if (r)
						{
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

							if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
								glADestroyContextScreen();
							else
								glADestroyContextWindowed();
						}

						GLClose(__tglContext);
					}

					CloseWindow(d->window);
				}

				if (d->screen)
					CloseScreen(d->screen);
			}

			CloseLibrary(TinyGLBase);
		}

		FreeVec(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	Sys_Input_Shutdown(d->inputdata);

	if (d->screen && !(TinyGLBase->lib_Version == 0 && TinyGLBase->lib_Revision < 4))
		glADestroyContextScreen();
	else
		glADestroyContextWindowed();

	GLClose(__tglContext);

	CloseWindow(d->window);

	FreeVec(d->pointermem);

	if (d->screen)
		CloseScreen(d->screen);

	CloseLibrary(TinyGLBase);

	if (d->gammatable)
		FreeVec(d->gammatable);
	
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

	glASwapBuffers();
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
	struct display *d = display;
	int i;

	if (d->screen && d->gammaenabled)
	{
		for(i=0;i<768;i++)
		{
			d->gammatable[i] = ramps[i]>>8;
		}

		SetAttrs(d->screen,
			SA_GammaRed, d->gammatable,
			SA_GammaGreen, d->gammatable+256,
			SA_GammaBlue, d->gammatable+512,
			TAG_DONE);
	}
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	struct display *d = display;

	return d->gammaenabled;
}

static void stub_glBindBuffer(GLenum target, GLuint buffer)
{
	glBindBuffer(target, buffer);
}

static void stub_glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
	glBufferData(target, size, data, usage);
}

static void stub_glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
{
	glBufferSubData(target, offset, size, data);
}

void *Sys_Video_GetProcAddress(void *display, const char *name)
{
	if (TinyGLBase->lib_Version > 52 || (TinyGLBase->lib_Version == 52 && TinyGLBase->lib_Revision >= 5))
	{
		if (strcmp(name, "glBindBuffer") == 0 || strcmp(name, "glBindBufferARB") == 0)
			return stub_glBindBuffer;
		else if (strcmp(name, "glBufferData") == 0 || strcmp(name, "glBufferDataARB") == 0)
			return stub_glBufferData;
		else if (strcmp(name, "glBufferSubData") == 0 || strcmp(name, "glBufferSubDataARB") == 0)
			return stub_glBufferSubData;
	}

	return 0;
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

