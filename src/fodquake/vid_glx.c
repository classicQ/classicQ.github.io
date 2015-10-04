/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2006-2010 Mark Olsen

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
#include <string.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>

#include "quakedef.h"
#include "keys.h"
#include "input.h"
#include "gl_local.h"

#define GLX_GLXEXT_PROTOTYPES 1
#include <GL/glx.h>

#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include <X11/extensions/xf86vmode.h>

#include "in_x11.h"
#include "vid_mode_x11.h"
#include "vid_mode_xf86vm.h"
#include "vid_mode_xrandr.h"
#include "sys_lib.h"

struct display
{
	void *inputdata;

	int xrandr_active;
	struct xrandrmode oldxrandrmode;

	int xf86vm_active;
	struct xf86vmmode oldxf86vmmode;
	unsigned int width, height;

	struct SysLib *xf86vmlib;
	Bool (*XF86VidModeGetGammaRamp)(Display *display, int screen, int size, unsigned short *red, unsigned short *green, unsigned short *blue);
	Bool (*XF86VidModeGetGammaRampSize)(Display *display, int screen, int *size);
	Bool (*XF86VidModeSetGammaRamp)(Display *display, int screen, int size, unsigned short *red, unsigned short *green, unsigned short *blue);
	qboolean customgamma;

	Display *x_disp;
	Window x_win;
	GLXContext ctx;
	int scrnum;
	int hasfocus;
	int focuschanged;

	int fullscreen;
	char used_mode[256];

	qboolean vid_gammaworks;
	unsigned short systemgammaramp[3][256];
	unsigned short *currentgammaramp;

	int utterly_fucktastically_broken_driver;
};


static void RestoreHWGamma(struct display *d);

int Sys_Video_GetKeyEvent(void *display, keynum_t *key, qboolean *down)
{
	struct display *d = display;

	return X11_Input_GetKeyEvent(d->inputdata, key, down);
}

void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

	X11_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	X11_Input_GrabMouse(d->inputdata, d->fullscreen?1:dograb);
}

void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d;

	d = display;

	XStoreName(d->x_disp, d->x_win, text);
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

unsigned int Sys_Video_GetFullscreen(void *display)
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

void signal_handler(int sig) {
	printf("Received signal %d, exiting...\n", sig);
	VID_Shutdown();
	Sys_Quit();
	exit(0);
}

void InitSig(void) {
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGILL, signal_handler);
	signal(SIGTRAP, signal_handler);
	signal(SIGIOT, signal_handler);
	signal(SIGBUS, signal_handler);
	signal(SIGFPE, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
}

/************************************* HW GAMMA *************************************/

static void InitHWGamma(struct display *d)
{
	int xf86vm_gammaramp_size;

	if (!d->xf86vmlib)
		return;

	if (!d->fullscreen)
		return;

	d->XF86VidModeGetGammaRampSize(d->x_disp, d->scrnum, &xf86vm_gammaramp_size);
	
	d->vid_gammaworks = (xf86vm_gammaramp_size == 256);

	if (d->vid_gammaworks)
	{
		d->XF86VidModeGetGammaRamp(d->x_disp, d->scrnum, xf86vm_gammaramp_size, d->systemgammaramp[0], d->systemgammaramp[1], d->systemgammaramp[2]);
	}
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
	struct display *d = display;

	if (!d->xf86vmlib)
		return;

	if (d->vid_gammaworks && d->hasfocus)
	{
		d->currentgammaramp = ramps;
		d->XF86VidModeSetGammaRamp(d->x_disp, d->scrnum, 256, ramps, ramps + 256, ramps + 512);
		d->customgamma = true;
	}
}

static void RestoreHWGamma(struct display *d)
{
	if (!d->xf86vmlib)
		return;

	if (d->vid_gammaworks && d->customgamma)
	{
		d->customgamma = false;
		d->XF86VidModeSetGammaRamp(d->x_disp, d->scrnum, 256, d->systemgammaramp[0], d->systemgammaramp[1], d->systemgammaramp[2]);
	}
}

/************************************* GL *************************************/

void Sys_Video_BeginFrame(void *display)
{
	struct display *d;
	XEvent event;

	d = display;

	while(XPending(d->x_disp))
	{
		XNextEvent(d->x_disp, &event);
		switch (event.type)
		{
			case FocusIn:
				d->hasfocus = 1;
				d->focuschanged = 1;
				V_UpdatePalette(true);
				break;
			case FocusOut:
				d->hasfocus = 0;
				d->focuschanged = 1;
				RestoreHWGamma(display);
				break;
		}
	}
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;

	glFlush();
	glXSwapBuffers(d->x_disp, d->x_win);

	if (!d->utterly_fucktastically_broken_driver)
		glXWaitGL();
}

/************************************* VID SHUTDOWN *************************************/

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	X11_Input_Shutdown(d->inputdata);

	RestoreHWGamma(d);

	glXMakeCurrent(d->x_disp, None, 0);
	glXDestroyContext(d->x_disp, d->ctx);
	XDestroyWindow(d->x_disp, d->x_win);

	if (d->xrandr_active)
		xrandr_switch(d->x_disp, &d->oldxrandrmode, 0, 0, 0);
	else if (d->xf86vm_active)
		xf86vm_switch(d->x_disp, &d->oldxf86vmmode, 0, 0, 0);

	if (d->xf86vmlib)
		Sys_Lib_Close(d->xf86vmlib);

	XCloseDisplay(d->x_disp);

	free(d);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 2;
}

/************************************* VID INIT *************************************/

static Cursor CreateNullCursor(Display *display, Window root)
{
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;
	Cursor cursor;

	cursormask = XCreatePixmap(display, root, 1, 1, 1);
	xgc.function = GXclear;
	gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
	XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	cursor = XCreatePixmapCursor(display, cursormask, cursormask,
		&dummycolour,&dummycolour, 0,0);
	XFreePixmap(display,cursormask);
	XFreeGC(display,gc);
	return cursor;
}

void Sys_Video_CvarInit()
{
	X11_Input_CvarInit();
}

int Sys_Video_Init()
{
	vidmode_init();

	return 1;
}

void Sys_Video_Shutdown()
{
	vidmode_shutdown();
}

void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	struct display *d;

	int attrib[] =
	{
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};
	XSetWindowAttributes attr;
	unsigned long mask;
	Window root;
	XVisualInfo *visinfo;
	struct xrandrmode xrandrmode;
	struct xf86vmmode xf86vmmode;

	d = malloc(sizeof(*d));
	if (d)
	{
		memset(d, 0, sizeof(*d));

		d->hasfocus = 1;
		d->x_disp = XOpenDisplay(NULL);
		if (d->x_disp)
		{
			d->scrnum = DefaultScreen(d->x_disp);
			visinfo = glXChooseVisual(d->x_disp, d->scrnum, attrib);
			if (visinfo)
			{
				root = RootWindow(d->x_disp, d->scrnum);

				if (fullscreen)
				{
					if (*mode == 0)
					{
						Window rootwindow;
						XWindowAttributes attributes;

						rootwindow = RootWindow(d->x_disp, d->scrnum);

						if (XGetWindowAttributes(d->x_disp, rootwindow, &attributes))
						{
							width = attributes.width;
							height = attributes.height;

							d->used_mode[0] = 0;

							if (!xrandr_getcurrentmode(d->x_disp, 0, d->used_mode, sizeof(d->used_mode)))
							{
								xf86vm_getcurrentmode(d->x_disp, 0, d->used_mode, sizeof(d->used_mode));
							}
						}
						else
						{
							fullscreen = 0;
						}
					}
					else if (modeline_to_xrandrmode(mode, &xrandrmode))
					{
						if (xrandr_switch(d->x_disp, &xrandrmode, &d->oldxrandrmode, d->used_mode, sizeof(d->used_mode)))
						{
							d->xrandr_active = 1;
							width = xrandrmode.width;
							height = xrandrmode.height;
						}
						else
							fullscreen = 0;
					}
					else if (modeline_to_xf86vmmode(mode, &xf86vmmode))
					{
						if (xf86vm_switch(d->x_disp, &xf86vmmode, &d->oldxf86vmmode, d->used_mode, sizeof(d->used_mode)))
						{
							d->xf86vm_active = 1;
							width = xf86vmmode.hdisplay;
							height = xf86vmmode.vdisplay;
						}
						else
							fullscreen = 0;
					}
				}

				if (fullscreen)
				{
					d->xf86vmlib = Sys_Lib_Open("Xxf86vm");
					if (d->xf86vmlib)
					{
						d->XF86VidModeSetGammaRamp = Sys_Lib_GetAddressByName(d->xf86vmlib, "XF86VidModeSetGammaRamp");
						d->XF86VidModeGetGammaRamp = Sys_Lib_GetAddressByName(d->xf86vmlib, "XF86VidModeGetGammaRamp");
						d->XF86VidModeGetGammaRampSize = Sys_Lib_GetAddressByName(d->xf86vmlib, "XF86VidModeGetGammaRampSize");

						if (!d->XF86VidModeSetGammaRamp
						 || !d->XF86VidModeGetGammaRamp
						 || !d->XF86VidModeGetGammaRampSize)
						{
							Sys_Lib_Close(d->xf86vmlib);
							d->xf86vmlib = 0;
						}
					}
				}

				d->width = width;
				d->height = height;
				d->fullscreen = fullscreen;

				// window attributes
				attr.background_pixel = 0;
				attr.border_pixel = 0;
				attr.colormap = XCreateColormap(d->x_disp, root, visinfo->visual, AllocNone);
				attr.event_mask = VisibilityChangeMask | FocusChangeMask;
				mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
	
				if (fullscreen)
				{
					mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWSaveUnder | CWBackingStore | CWOverrideRedirect;
					attr.override_redirect = True;
					attr.backing_store = NotUseful;
					attr.save_under = False;
				}

				d->x_win = XCreateWindow(d->x_disp, root, 0, 0, width, height,0, visinfo->depth, InputOutput, visinfo->visual, mask, &attr);

				XStoreName(d->x_disp, d->x_win, "Fodquake");

				XDefineCursor(d->x_disp, d->x_win, CreateNullCursor(d->x_disp, d->x_win));

				XMapWindow(d->x_disp, d->x_win);

				if (fullscreen)
				{
					XSync(d->x_disp, 0);
					XRaiseWindow(d->x_disp, d->x_win);
					XWarpPointer(d->x_disp, None, d->x_win, 0, 0, 0, 0, 0, 0);
					XSync(d->x_disp, 0);
				}

				XFlush(d->x_disp);

				d->ctx = glXCreateContext(d->x_disp, visinfo, NULL, True);

				glXMakeCurrent(d->x_disp, d->x_win, d->ctx);

				if (strcmp((const char *)glGetString(GL_VENDOR), "ATI Technologies Inc.") == 0)
				{
					d->utterly_fucktastically_broken_driver = 1;
				}

				if (strstr((const char *)glGetString(GL_VERSION), "Mesa") && strstr(glXGetClientString(d->x_disp, GLX_EXTENSIONS), "GLX_SGI_swap_control"))
				{
					int (*SwapIntervalSGI)(int);
					SwapIntervalSGI = (void *)glXGetProcAddressARB((const GLubyte *)"glXSwapIntervalSGI");
					if (SwapIntervalSGI)
						SwapIntervalSGI(0);
				}

				InitSig(); // trap evil signals

				InitHWGamma(d);

				Com_Printf ("Video mode %dx%d initialized.\n", width, height);

				XFree(visinfo);

				d->inputdata = X11_Input_Init(d->x_win, width, height, fullscreen);
				if (d->inputdata)
					return d;
			}

			XCloseDisplay(d->x_disp);
		}

		free(d);
	}

	return 0;
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	struct display *d;

	d = display;

	return d->vid_gammaworks;
}

void *Sys_Video_GetProcAddress(void *display, const char *name)
{
	return glXGetProcAddressARB((GLubyte *)name);
}

int Sys_Video_FocusChanged(void *display)
{
	struct display *d;

	d = display;

	if (d->focuschanged)
	{
		d->focuschanged = 0;

		return 1;
	}

	return 0;
}

#include "clipboard_x11.c"

