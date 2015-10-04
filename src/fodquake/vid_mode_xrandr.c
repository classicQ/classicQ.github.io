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

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"
#include "sys_lib.h"
#include "vid_mode_xrandr.h"

struct xrandr
{
	struct SysLib *xrandrlib;

	SizeID (*XRRConfigCurrentConfiguration)(XRRScreenConfiguration *config, Rotation *rotation);
	short (*XRRConfigCurrentRate)(XRRScreenConfiguration *config);
	short *(*XRRConfigRates)(XRRScreenConfiguration *config, int size_index, int *nrates);
	XRRScreenSize *(*XRRConfigSizes)(XRRScreenConfiguration *config, int *nsizes);
	Time (*XRRConfigTimes)(XRRScreenConfiguration *config, Time *config_timestamp);
	void (*XRRFreeScreenConfigInfo)(XRRScreenConfiguration *config);
	XRRScreenConfiguration *(*XRRGetScreenInfo)(Display *dpy, Drawable draw);
	Status (*XRRSetScreenConfigAndRate)(Display *dpy, XRRScreenConfiguration *config, Drawable draw, int size_index, Rotation rotation, short rate, Time timestamp);
};

static struct xrandr *xrandr;

int xrandr_init()
{
	xrandr = malloc(sizeof(*xrandr));
	if (xrandr)
	{
		xrandr->xrandrlib = Sys_Lib_Open("Xrandr");
		if (xrandr->xrandrlib)
		{
			xrandr->XRRConfigCurrentConfiguration = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRConfigCurrentConfiguration");
			xrandr->XRRConfigCurrentRate = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRConfigCurrentRate");
			xrandr->XRRConfigRates = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRConfigRates");
			xrandr->XRRConfigSizes = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRConfigSizes");
			xrandr->XRRConfigTimes = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRConfigTimes");
			xrandr->XRRFreeScreenConfigInfo = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRFreeScreenConfigInfo");
			xrandr->XRRGetScreenInfo = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRGetScreenInfo");
			xrandr->XRRSetScreenConfigAndRate = Sys_Lib_GetAddressByName(xrandr->xrandrlib, "XRRSetScreenConfigAndRate");

			if (xrandr->XRRConfigCurrentConfiguration
			 && xrandr->XRRConfigCurrentRate
			 && xrandr->XRRConfigRates
			 && xrandr->XRRConfigSizes
			 && xrandr->XRRConfigTimes
			 && xrandr->XRRFreeScreenConfigInfo
			 && xrandr->XRRGetScreenInfo
			 && xrandr->XRRSetScreenConfigAndRate)
			{
				return 1;
			}

			Sys_Lib_Close(xrandr->xrandrlib);
		}

		free(xrandr);

		xrandr = 0;
	}

	return 0;
}

void xrandr_shutdown()
{
	Sys_Lib_Close(xrandr->xrandrlib);
	free(xrandr);
	xrandr = 0;
}

int modeline_to_xrandrmode(const char *modeline, struct xrandrmode *xrandrmode)
{
	const char *p;
	unsigned int commas;

	if (!xrandr)
		return 0;

	if (strncmp(modeline, "xrandr:", 7) != 0)
		return 0;

	modeline += 7;

	commas = 0;
	p = modeline;
	while((p = strchr(p, ',')))
	{
		commas++;
		p = p + 1;
	}

	if (commas != 2)
		return 0;

	p = modeline;
	xrandrmode->width = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xrandrmode->height = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	xrandrmode->refreshrate = strtoul(p, 0, 0);

	return 1;
}

int xrandr_getcurrentmode(Display *d, struct xrandrmode *oldmode, char *usedmodebuf, unsigned int usedmodelen)
{
	XRRScreenConfiguration *screenconfig;
	Rotation rotation;
	XRRScreenSize *sizes;
	int sizescount;
	unsigned int oldindex;
	unsigned int oldrate;
	int ret;

	if (!xrandr)
		return 0;

	ret = 0;

	screenconfig = xrandr->XRRGetScreenInfo(d, RootWindow(d, 0));
	if (screenconfig)
	{
		oldindex = xrandr->XRRConfigCurrentConfiguration(screenconfig, &rotation);
		oldrate = xrandr->XRRConfigCurrentRate(screenconfig);

		sizes = xrandr->XRRConfigSizes(screenconfig, &sizescount);
		if (sizes)
		{
			if (oldmode)
			{
				oldmode->width = sizes[oldindex].width;
				oldmode->height = sizes[oldindex].height;
				oldmode->refreshrate = oldrate;
			}

			if (usedmodebuf)
				snprintf(usedmodebuf, usedmodelen, "xrandr:%d,%d,%d\n", sizes[oldindex].width, sizes[oldindex].height, oldrate);

			ret = 1;
		}

		xrandr->XRRFreeScreenConfigInfo(screenconfig);
	}

	return ret;
}

int xrandr_switch(Display *d, const struct xrandrmode *newmode, struct xrandrmode *oldmode, char *usedmodebuf, unsigned int usedmodelen)
{
	XRRScreenConfiguration *screenconfig;
	Time randrconfigtime;
	Time randrtime;
	Rotation rotation;
	XRRScreenSize *sizes;
	int sizescount;
	unsigned int i;
	unsigned int oldindex;
	unsigned int oldrate;
	int ret;

	if (!xrandr)
		return 0;

	ret = 0;

	screenconfig = xrandr->XRRGetScreenInfo(d, RootWindow(d, 0));
	if (screenconfig)
	{
		oldindex = xrandr->XRRConfigCurrentConfiguration(screenconfig, &rotation);
		oldrate = xrandr->XRRConfigCurrentRate(screenconfig);

		sizes = xrandr->XRRConfigSizes(screenconfig, &sizescount);
		if (sizes)
		{
			for(i=0;i<sizescount;i++)
			{
				if (sizes[i].width == newmode->width && sizes[i].height == newmode->height)
				{
					randrtime = xrandr->XRRConfigTimes(screenconfig, &randrconfigtime);

					if (xrandr->XRRSetScreenConfigAndRate(d, screenconfig, RootWindow(d, 0), i, rotation, newmode->refreshrate, randrtime) == 0)
					{
						if (usedmodebuf)
							snprintf(usedmodebuf, usedmodelen, "xrandr:%d,%d,%d\n", newmode->width, newmode->height, newmode->refreshrate);

						if (oldmode)
						{
							oldmode->width = sizes[oldindex].width;
							oldmode->height = sizes[oldindex].height;
							oldmode->refreshrate = oldrate;
						}

						ret = 1;
					}
				}
			}
		}

		xrandr->XRRFreeScreenConfigInfo(screenconfig);
	}

	return ret;
}

const char * const *xrandr_GetModeList(void)
{
	Display *disp;
	int scrnum;
	const char **ret;
	char buf[256];
	int sizescount;
	XRRScreenSize *sizes;
	XRRScreenConfiguration *screenconfig;
	short *rates;
	int ratescount;
	unsigned int i;
	unsigned int j;
	unsigned int modecount;
	int fail;

	fail = 0;
	ret = 0;

	disp = XOpenDisplay(NULL);
	if (disp)
	{
		scrnum = DefaultScreen(disp);

		screenconfig = xrandr->XRRGetScreenInfo(disp, RootWindow(disp, scrnum));
		if (screenconfig)
		{
			sizes = xrandr->XRRConfigSizes(screenconfig, &sizescount);
			if (sizes)
			{
				modecount = 0;

				for(i=0;i<sizescount;i++)
				{
					rates = xrandr->XRRConfigRates(screenconfig, i, &ratescount);
					if (!rates)
					{
						fail = 1;
						break;
					}

					modecount += ratescount;
				}
			}
			else
			{
				fail = 1;
			}

			if (!fail && modecount < 65536)
			{
				ret = malloc(sizeof(*ret) * (modecount + 1));
				if (ret)
				{
					memset(ret, 0, sizeof(*ret) * (modecount + 1));

					modecount = 0;

					for(i=0;i<sizescount&&!fail;i++)
					{
						rates = xrandr->XRRConfigRates(screenconfig, i, &ratescount);
						if (!rates)
						{
							fail = 1;
							break;
						}

						for(j=0;j<ratescount;j++)
						{
							snprintf(buf, sizeof(buf), "xrandr:%d,%d,%d", sizes[i].width, sizes[i].height, rates[j]);

							ret[modecount] = malloc(strlen(buf)+1);
							if (ret[modecount] == 0)
							{
								fail = 1;
								break;
							}

							strcpy((void *)ret[modecount], buf);

							modecount++;
						}
					}
				}
			}

			xrandr->XRRFreeScreenConfigInfo(screenconfig);
		}

		if (fail && ret)
		{
			Sys_Video_FreeModeList(ret);
			ret = 0;
		}

		XCloseDisplay(disp);
	}

	return ret;
}

const char *xrandr_GetModeDescription(const char *mode)
{
	char buf[256];
	char *ret;
	struct xrandrmode xrandrmode;

	if (modeline_to_xrandrmode(mode, &xrandrmode))
	{
		snprintf(buf, sizeof(buf), "%dx%d, %dHz", xrandrmode.width, xrandrmode.height, xrandrmode.refreshrate);

		ret = malloc(strlen(buf) + 1);
		if (ret)
		{
			strcpy(ret, buf);

			return ret;
		}
	}

	return 0;
}

