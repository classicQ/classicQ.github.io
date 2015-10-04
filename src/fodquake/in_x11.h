/*
Copyright (C) 2006-2007 Mark Olsen

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

void X11_Input_CvarInit(void);
void *X11_Input_Init(Window x_win, unsigned int windowwidth, unsigned int windowheight, int fullscreen);
void X11_Input_Shutdown(void *inputdata);
int X11_Input_GetKeyEvent(void *inputdata, keynum_t *key, qboolean *down);
void X11_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey);
void X11_Input_GetConfigNotify(void *inputdata, int *config_notify, int *config_notify_width, int *config_notify_height);
void X11_Input_GrabMouse(void *inputdata, int dograb);

