/*
Copyright (C) 2010 Mark Olsen

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

#define true qtrue
#define false qfalse
#include "qtypes.h"
#undef true
#undef false

#include <ApplicationServices/ApplicationServices.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "gl_local.h"
#include "in_macosx.h"

struct display
{
	CGLContextObj context;

	unsigned int width;
	unsigned int height;
	int fullscreen;
#warning is that one necessary?
	int gammaenabled;

	void *inputdata;
};

void Sys_Video_CvarInit(void)
{
}

void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;
	CGLError cglerror;
	CGLPixelFormatObj pixelformatobjs;
	GLint pixelformatobjscount;
	CGDisplayErr cgerror;
	CGSize cgsize;

	CGLPixelFormatAttribute attribs[] =
	{
/*		kCGLPFAColorSize, 32, */
		kCGLPFADepthSize, 16,
		kCGLPFADisplayMask, CGDisplayIDToOpenGLDisplayMask(kCGDirectMainDisplay),
		kCGLPFAFullScreen,
		kCGLPFADoubleBuffer,
		kCGLPFAAccelerated,
		kCGLPFANoRecovery,
		0
	};

	d = malloc(sizeof(*d));
	if (d)
	{
		cglerror = CGLChoosePixelFormat(attribs, &pixelformatobjs, &pixelformatobjscount);
		if (cglerror == kCGLNoError)
		{
			cglerror = CGLCreateContext(pixelformatobjs, 0, &d->context);
			if (cglerror == kCGLNoError)
			{
				cgerror = CGCaptureAllDisplays();
				if (cgerror == kCGErrorSuccess)
				{
					cglerror = CGLSetCurrentContext(d->context);
					if (cglerror == kCGLNoError)
					{
						cglerror = CGLSetFullScreen(d->context);
						if (cglerror == kCGLNoError)
						{
							printf("Clearing\n");
							glClearColor(1,1,1,1);
							glClear(GL_COLOR_BUFFER_BIT);

							CGLFlushDrawable(d->context);

							CGAssociateMouseAndMouseCursorPosition(0);

							d->width = CGDisplayPixelsWide(kCGDirectMainDisplay);
							d->height = CGDisplayPixelsHigh(kCGDirectMainDisplay);

							d->inputdata = Sys_Input_Init();
							if (d->inputdata)
								return d;
						}
					}

					CGReleaseAllDisplays();
				}

				CGLDestroyContext(d->context);
			}
		}

		free(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	Sys_Input_Shutdown(d->inputdata);

	CGReleaseAllDisplays();

	CGLDestroyContext(d->context);

	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	struct display *d;
	
	d = display;

	return d->fullscreen ? 3 : 1;
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
	return 0;
}

void Sys_Video_BeginFrame(void *display)
{
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;

	CGLFlushDrawable(d->context);

}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	Sys_Input_GrabMouse(d->inputdata, dograb);
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
	struct display *d = display;
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	struct display *d = display;

	return d->gammaenabled;
}

int Sys_Video_FocusChanged(void *display)
{
	return 0;
}

