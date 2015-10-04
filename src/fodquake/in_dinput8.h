/*
Copyright (C) 2010 Mark Olsen

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

struct InputData *Sys_Input_Init(HWND window);
void Sys_Input_Shutdown(struct InputData *inputdata);
void Sys_Input_MainThreadFrameStart(struct InputData *inputdata);
int Sys_Input_GetKeyEvent(struct InputData *inputdata, keynum_t *keynum, qboolean *down);
void Sys_Input_GetMouseMovement(struct InputData *inputdata, int *mousex, int *mousey);
void Sys_Input_GrabMouse(struct InputData *inputdata, int dograb);
void Sys_Input_ClearRepeat(struct InputData *inputdata);

