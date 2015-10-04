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

#include <windows.h>

#include <stdlib.h>
#include <string.h>

const char *Sys_Video_GetClipboardText(void *display)
{
	HANDLE handle;
	char *ret;
	unsigned int len;
	const char *src;

	ret = 0;

	if (OpenClipboard(0))
	{
		if ((handle = GetClipboardData(CF_TEXT)))
		{
			if ((src = GlobalLock(handle)))
			{
				len = 0;
				while(src[len] && src[len] != '\n' && src[len] != '\r' && src[len] != '\b')
					len++;

				ret = malloc(len + 1);
				if (ret)
				{
					memcpy(ret, src, len);
					ret[len] = 0;
				}

				GlobalUnlock(handle);
			}
		}

		CloseClipboard();
	}

	return ret;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	free((void *)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
}

