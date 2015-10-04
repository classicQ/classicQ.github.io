/*
Copyright (C) 2009-2010 Mark Olsen

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

#include <graphics/displayinfo.h>

#include <proto/exec.h>
#include <proto/graphics.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "vid_mode_morphos.h"

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"

int modeline_to_modeinfo(const char *modeline, struct modeinfo *modeinfo)
{
	const char *p;
	const char *p2;
	unsigned int commas;

	commas = 0;
	p = modeline;
	while((p = strchr(p, ',')))
	{
		commas++;
		p = p + 1;
	}

	if (commas != 3)
		return 0;

	p = modeline;
	p2 = strchr(p, ',');
	strlcpy(modeinfo->monitorname, p, p2-p<sizeof(modeinfo->monitorname)?p2-p+1:sizeof(modeinfo->monitorname));
	p = p2 + 1;
	modeinfo->width = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	modeinfo->height = strtoul(p, 0, 0);
	p = strchr(p, ',') + 1;
	modeinfo->depth = strtoul(p, 0, 0);

	return 1;
}

const char * const *Sys_Video_GetModeList(void)
{
	const char **ret;
	char buf[256];
	ULONG id;
	unsigned int nummodes;
	unsigned int i;
	APTR handle;
	struct DisplayInfo dispinfo;
	struct DimensionInfo diminfo;
	struct MonitorInfo moninfo;
	
	id = INVALID_ID;
	nummodes = 0;

	while((id = NextDisplayInfo(id)) != INVALID_ID)
	{
		nummodes++;
	}

	ret = AllocVec((nummodes + 1) * sizeof(*ret), MEMF_ANY|MEMF_CLEAR);
	if (ret == 0)
		return 0;

	id = INVALID_ID;
	i = 0;

	while((id = NextDisplayInfo(id)) != INVALID_ID)
	{
		if (i == nummodes)
			break;

		handle = FindDisplayInfo(id);
		if (handle)
		{
			if (!GetDisplayInfoData(handle, &dispinfo, sizeof(dispinfo), DTAG_DISP, 0)
			 || !GetDisplayInfoData(handle, &diminfo, sizeof(diminfo), DTAG_DIMS, 0)
			 || !GetDisplayInfoData(handle, &moninfo, sizeof(moninfo), DTAG_MNTR, 0))
				continue;

			if (dispinfo.NotAvailable || !(dispinfo.PropertyFlags&DIPF_IS_WB))
				continue;

#ifdef GLQUAKE
			if (diminfo.MaxDepth <= 8)
				continue;
#else
			if (diminfo.MaxDepth != 8)
				continue;
#endif

			snprintf(buf, sizeof(buf), "%s,%d,%d,%d", moninfo.Mspc->ms_Node.xln_Name, diminfo.StdOScan.MaxX + 1, diminfo.StdOScan.MaxY + 1, diminfo.MaxDepth);

			ret[i] = AllocVec(strlen(buf) + 1, MEMF_ANY);
			if (ret[i] == 0)
				break;

			strcpy((char *)ret[i], buf);

			i++;
		}
	}

	if (id == INVALID_ID)
	{
		ret[i] = 0;
		return ret;
	}

	for(i=0;ret[i];i++)
		FreeVec((void *)ret[i]);

	FreeVec(ret);

	return 0;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
	unsigned int i;

	for(i=0;displaymodes[i];i++)
		FreeVec((void *)displaymodes[i]);

	FreeVec((void *)displaymodes);
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	struct modeinfo mi;
	char monitorname[64]; /* Mneh, this is longer than it can be anyway :P */
	char buf[256];
	char *p;

	if (modeline_to_modeinfo(mode, &mi))
	{
		strlcpy(monitorname, mi.monitorname, sizeof(monitorname));
		p = strchr(monitorname, '.');
		if (p)
			*p = 0;

		snprintf(buf, sizeof(buf), "%s: %dx%d %dbpp", monitorname, mi.width, mi.height, mi.depth);
		p = AllocVec(strlen(buf) + 1, MEMF_ANY);
		if (p)
		{
			strcpy(p, buf);
			return p;
		}
	}

	return 0;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
	FreeVec((void *)modedescription);
}
