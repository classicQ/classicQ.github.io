/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2005, 2009-2011 Mark Olsen

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

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

void Con_Init(void);
void Con_Shutdown(void);
void Con_CvarInit(void);
void Con_CheckResize(unsigned int pixelwidth);
void Con_DrawConsole(int lines);
unsigned int Con_DrawNotify(void);
void Con_ClearNotify(void);
void Con_Suppress(void);
void Con_Unsuppress(void);
unsigned int Con_GetColumns(void);

void Con_Print(const char *txt);

void Con_ScrollUp(unsigned int numlines);
void Con_ScrollDown(unsigned int numlines);
void Con_Home(void);
void Con_End(void);

#endif /* _CONSOLE_H_ */

