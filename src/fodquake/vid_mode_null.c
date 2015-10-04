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

typedef struct vrect_s vrect_t;
typedef enum keynum keynum_t;
#include "sys_video.h"

const char * const *Sys_Video_GetModeList(void)
{
	return 0;
}

void Sys_Video_FreeModeList(const char * const *displaymodes)
{
}

const char *Sys_Video_GetModeDescription(const char *mode)
{
	return 0;
}

void Sys_Video_FreeModeDescription(const char *modedescription)
{
}

