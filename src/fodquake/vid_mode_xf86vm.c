/*
Copyright (C) 2009-2012 Mark Olsen

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

#include <X11/Xlib.h>
#include <X11/extensions/xf86vmode.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"
#include "sys_lib.h"
#include "vid_mode_xf86vm.h"

struct xf86vm
{
	struct SysLib *xf86vmlib;

	Bool (*XF86VidModeGetAllModeLines)(Display *display, int screen, int *modecount_return, XF86VidModeModeInfo ***modesinfo);
	Bool (*XF86VidModeGetModeLine)(Display *display, int screen, int *dotclock_return, XF86VidModeModeLine *modeline);
	Bool (*XF86VidModeSetViewPort)(Display *display, int screen, int x, int y);
	Bool (*XF86VidModeSwitchToMode)(Display *display, int screen, XF86VidModeModeInfo *modeline);
};

static struct xf86vm *xf86vm;

int xf86vm_init()
{
	xf86vm = malloc(sizeof(*xf86vm));
	if (xf86vm)
	{
		xf86vm->xf86vmlib = Sys_Lib_Open("Xxf86vm");

		if (xf86vm->xf86vmlib)
		{
			xf86vm->XF86VidModeGetAllModeLines = Sys_Lib_GetAddressByName(xf86vm->xf86vmlib, "XF86VidModeGetAllModeLines");
			xf86vm->XF86VidModeGetModeLine = Sys_Lib_GetAddressByName(xf86vm->xf86vmlib, "XF86VidModeGetModeLine");
			xf86vm->XF86VidModeSetViewPort = Sys_Lib_GetAddressByName(xf86vm->xf86vmlib, "XF86VidModeSetViewPort");
			xf86vm->XF86VidModeSwitchToMode = Sys_Lib_GetAddressByName(xf86vm->xf86vmlib, "XF86VidModeSwitchToMode");

			if (xf86vm->XF86VidModeGetAllModeLines
			 && xf86vm->XF86VidModeGetModeLine
			 && xf86vm->XF86VidModeSetViewPort
			 && xf86vm->XF86VidModeSwitchToMode)
			{
				return 1;
			}

			Sys_Lib_Close(xf86vm->xf86vmlib);
		}

		free(xf86vm);

		xf86vm = 0;
	}

	return 0;
}

void xf86vm_shutdown()
{
	Sys_Lib_Close(xf86vm->xf86vmlib);
	free(xf86vm);
	xf86vm = 0;
}

int modeline_to_xf86vmmode(const char *modeline, struct xf86vmmode *xf86vmmode)
{
	const char *p;
	unsigned int commas;

	if (strncmp(modeline, "xf86vm:", 7) == 0)
		modeline += 7;

	commas = 0;
	p = modeline;
	while((p = strchr(p, ',')))
	{
		commas++;
		p = p + 1;
	}

	if (commas != 10)
		return 0;

	p = modeline;
	xf86vmmode->dotclock = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->hdisplay = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->hsyncstart = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->hsyncend = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->htotal = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->hskew = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->vdisplay = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->vsyncstart = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->vsyncend = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->vtotal = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xf86vmmode->flags = strtoul(p, 0, 0);

	return 1;
}

int xf86vm_getcurrentmode(Display *d, struct xf86vmmode *oldmode, char *usedmodebuf, unsigned int usedmodelen)
{
	XF86VidModeModeInfo origvidmode;
	int scrnum;

	if (!xf86vm)
		return 0;

	scrnum = DefaultScreen(d);

	if (!xf86vm->XF86VidModeGetModeLine(d, scrnum, (int *)&origvidmode.dotclock, (XF86VidModeModeLine *)&origvidmode.hdisplay))
		return 0;

	if (usedmodebuf)
		snprintf(usedmodebuf, usedmodelen, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", origvidmode.dotclock, origvidmode.hdisplay, origvidmode.hsyncstart, origvidmode.hsyncend, origvidmode.htotal, origvidmode.hskew, origvidmode.vdisplay, origvidmode.vsyncstart, origvidmode.vsyncend, origvidmode.vtotal, origvidmode.flags);

	if (oldmode)
	{
		oldmode->dotclock = origvidmode.dotclock;
		oldmode->hdisplay = origvidmode.hdisplay;
		oldmode->hsyncstart = origvidmode.hsyncstart;
		oldmode->hsyncend = origvidmode.hsyncend;
		oldmode->htotal = origvidmode.htotal;
		oldmode->hskew = origvidmode.hskew;
		oldmode->vdisplay = origvidmode.vdisplay;
		oldmode->vsyncstart = origvidmode.vsyncstart;
		oldmode->vsyncend = origvidmode.vsyncend;
		oldmode->vtotal = origvidmode.vtotal;
		oldmode->flags = origvidmode.flags;
	}

	return 1;
}

int xf86vm_switch(Display *d, const struct xf86vmmode *newmode, struct xf86vmmode *oldmode, char *usedmodebuf, unsigned int usedmodelen)
{
	XF86VidModeModeInfo **vidmodes;
	XF86VidModeModeInfo origvidmode;
	int num_vidmodes;
	unsigned int i;
	int scrnum;
	int ret;

	if (!xf86vm)
		return 0;

	scrnum = DefaultScreen(d);

	if (!xf86vm->XF86VidModeGetModeLine(d, scrnum, (int *)&origvidmode.dotclock, (XF86VidModeModeLine *)&origvidmode.hdisplay))
		return 0;

	if (!xf86vm->XF86VidModeGetAllModeLines(d, scrnum, &num_vidmodes, &vidmodes))
		return 0;

	ret = 0;

	for (i=0;i<num_vidmodes;i++)
	{
		if (vidmodes[i]->dotclock == newmode->dotclock
		 && vidmodes[i]->hdisplay == newmode->hdisplay
		 && vidmodes[i]->hsyncstart == newmode->hsyncstart
		 && vidmodes[i]->hsyncend == newmode->hsyncend
		 && vidmodes[i]->htotal == newmode->htotal
		 && vidmodes[i]->hskew == newmode->hskew
		 && vidmodes[i]->vdisplay == newmode->vdisplay
		 && vidmodes[i]->vsyncstart == newmode->vsyncstart
		 && vidmodes[i]->vsyncend == newmode->vsyncend
		 && vidmodes[i]->vtotal == newmode->vtotal
		 && vidmodes[i]->flags == newmode->flags)
		{
			break;
		}
	}

	if (i != num_vidmodes)
	{
		xf86vm->XF86VidModeSwitchToMode(d, scrnum, vidmodes[i]);
		xf86vm->XF86VidModeSetViewPort(d, scrnum, 0, 0);

		if (usedmodebuf)
			snprintf(usedmodebuf, usedmodelen, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", vidmodes[i]->dotclock, vidmodes[i]->hdisplay, vidmodes[i]->hsyncstart, vidmodes[i]->hsyncend, vidmodes[i]->htotal, vidmodes[i]->hskew, vidmodes[i]->vdisplay, vidmodes[i]->vsyncstart, vidmodes[i]->vsyncend, vidmodes[i]->vtotal, vidmodes[i]->flags);

		if (oldmode)
		{
			oldmode->dotclock = origvidmode.dotclock;
			oldmode->hdisplay = origvidmode.hdisplay;
			oldmode->hsyncstart = origvidmode.hsyncstart;
			oldmode->hsyncend = origvidmode.hsyncend;
			oldmode->htotal = origvidmode.htotal;
			oldmode->hskew = origvidmode.hskew;
			oldmode->vdisplay = origvidmode.vdisplay;
			oldmode->vsyncstart = origvidmode.vsyncstart;
			oldmode->vsyncend = origvidmode.vsyncend;
			oldmode->vtotal = origvidmode.vtotal;
			oldmode->flags = origvidmode.flags;
		}

		ret = 1;
	}

	XFree(vidmodes);

	return ret;
}

const char * const *xf86vm_GetModeList(void)
{
	Display *disp;
	int scrnum;
	int num_vidmodes;
	XF86VidModeModeInfo **vidmodes;
	const char **ret;
	char buf[256];
	unsigned int i;

	ret = 0;

	disp = XOpenDisplay(NULL);
	if (disp)
	{
		scrnum = DefaultScreen(disp);

		{
			if (xf86vm->XF86VidModeGetAllModeLines(disp, scrnum, &num_vidmodes, &vidmodes))
			{
				if (num_vidmodes < 65536)
				{
					ret = malloc(sizeof(*ret) * (num_vidmodes + 1));
					if (ret)
					{
						for(i=0;i<num_vidmodes;i++)
						{
							snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", vidmodes[i]->dotclock, vidmodes[i]->hdisplay, vidmodes[i]->hsyncstart, vidmodes[i]->hsyncend, vidmodes[i]->htotal, vidmodes[i]->hskew, vidmodes[i]->vdisplay, vidmodes[i]->vsyncstart, vidmodes[i]->vsyncend, vidmodes[i]->vtotal, vidmodes[i]->flags);

							ret[i] = malloc(strlen(buf)+1);
							if (ret[i] == 0)
								break;

							strcpy((void *)ret[i], buf);
						}

						ret[i] = 0;

						if (i != num_vidmodes)
						{
							Sys_Video_FreeModeList(ret);
							ret = 0;
						}

					}
				}

				XFree(vidmodes);
			}
		}

		XCloseDisplay(disp);
	}

	return ret;
}

const char *xf86vm_GetModeDescription(const char *mode)
{
	char buf[256];
	char *ret;
	struct xf86vmmode xf86vmmode;
	unsigned long long dotclock;

	if (modeline_to_xf86vmmode(mode, &xf86vmmode))
	{
		dotclock = xf86vmmode.dotclock;

		snprintf(buf, sizeof(buf), "%dx%d, %dHz", xf86vmmode.hdisplay, xf86vmmode.vdisplay, (int)((((dotclock*1000000)/(xf86vmmode.htotal*xf86vmmode.vtotal))+500)/1000));

		ret = malloc(strlen(buf) + 1);
		if (ret)
		{
			strcpy(ret, buf);

			return ret;
		}
	}

	return 0;
}

