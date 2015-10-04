/*
Copyright (C) 2009 Mark Olsen

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

#include <windows.h>

#include <stdio.h>

#include "vid_mode_win32.h"

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"

int modeline_to_devmode(const char *modeline, DEVMODE *devmode)
{
	const char *p;
	unsigned int commas;

	commas = 0;
	p = modeline;
	while((p = strchr(p, ',')))
	{
		commas++;
		p = p + 1;
	}

	if (commas != 4)
		return 0;

	p = modeline;
	devmode->dmPelsWidth = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	devmode->dmPelsHeight = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	devmode->dmBitsPerPel = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	devmode->dmDisplayFlags = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	devmode->dmDisplayFrequency = strtoul(p, 0, 0);

	return 1;
}

const char * const *Sys_Video_GetModeList(void)
{
	DEVMODE devmode;
	char **ret;
	char **t;
	unsigned int i;
	unsigned int nummodes;
	char buf[256];
	BOOL enumret;

	ret = 0;

	nummodes = 0;

	for(i=0;(enumret = EnumDisplaySettings(0, i, &devmode));i++)
	{
#ifdef GLQUAKE
		if (devmode.dmBitsPerPel <= 8)
			continue;
#else
		if (devmode.dmBitsPerPel != 8)
			continue;
#endif

		t = realloc(ret, (nummodes+2)*sizeof(*ret));
		if (t == 0)
			break;

		ret = t;
		ret[nummodes+1] = 0;

		snprintf(buf, sizeof(buf), "%d,%d,%d,%d,%d", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel, devmode.dmDisplayFlags, devmode.dmDisplayFrequency);

		ret[nummodes] = strdup(buf);
		if (ret[nummodes] == 0)
			break;

		nummodes++;
	}

	if (enumret == 0)
	{
		return ret;
	}

	if (ret)
		Sys_Video_FreeModeList(ret);

	return 0;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;

	for(i=0;displaymodes[i];i++)
		free((void *)displaymodes[i]);

	free((void *)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	DEVMODE devmode;
	char buf[256];
	char *ret;

	if (modeline_to_devmode(mode, &devmode))
	{
		snprintf(buf, sizeof(buf), "%dx%d, %dbpp, %dHz", devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmBitsPerPel, devmode.dmDisplayFrequency);

		ret = strdup(buf);
		if (ret)
			return ret;
	}

	return 0;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	free((void *)modedescription);
}

