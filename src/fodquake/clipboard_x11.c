/*
Copyright (C) 2007-2009 Mark Olsen

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

#include <string.h>

const char *Sys_Video_GetClipboardText(void *display)
{
	struct display *d;
	char *xbuf;
	char *buf;
	int len;

	d = display;
	buf = 0;

	xbuf = XFetchBytes(d->x_disp, &len);
	if (xbuf)
	{
		buf = malloc(len+1);
		if (buf)
		{
			memcpy(buf, xbuf, len);
			buf[len] = 0;
		}

		XFree(xbuf);
	}

	return buf;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	free((char *)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
	struct display *d;

	d = display;

	XStoreBytes(d->x_disp, text, strlen(text));
}

