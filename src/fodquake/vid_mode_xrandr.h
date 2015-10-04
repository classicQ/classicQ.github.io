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

struct xrandrmode
{
	unsigned int width;
	unsigned int height;
	unsigned int refreshrate;
};

int xrandr_init(void);
void xrandr_shutdown(void);
int modeline_to_xrandrmode(const char *modeline, struct xrandrmode *xrandrmode);
int xrandr_getcurrentmode(Display *d, struct xrandrmode *oldmode, char *usedmodebuf, unsigned int usedmodelen);
int xrandr_switch(Display *d, const struct xrandrmode *newmode, struct xrandrmode *oldmode, char *usedmodebuf, unsigned int usedmodelen);

const char * const *xrandr_GetModeList(void);
const char *xrandr_GetModeDescription(const char *mode);

