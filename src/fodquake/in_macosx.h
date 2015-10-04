/*
 Copyright (C) 2011 Florian Zwoch
 Copyright (C) 2011 Mark Olsen
 
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

struct input_data *Sys_Input_Init(void);
void Sys_Input_Shutdown(struct input_data *input);
int Sys_Input_GetKeyEvent(struct input_data *input, keynum_t *keynum, qboolean *down);
void Sys_Input_GetMouseMovement(struct input_data *input, int *mouse_x, int *mouse_y);
void Sys_Input_GrabMouse(struct input_data *input, int dograb);
void Sys_Input_SetFnKeyBehavior(struct input_data *input, int behavior);
