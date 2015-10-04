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

#include "qtypes.h"

void Sys_Video_CvarInit(void);
int Sys_Video_Init(void);
void Sys_Video_Shutdown(void);
void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette);
void Sys_Video_Close(void *display);
unsigned int Sys_Video_GetNumBuffers(void *display);
void Sys_Video_Update(void *display, vrect_t *rects);
int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down);
void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey);
void Sys_Video_GrabMouse(void *display, int dograb);
void Sys_Video_SetWindowTitle(void *display, const char *text);
unsigned int Sys_Video_GetWidth(void *display);
unsigned int Sys_Video_GetHeight(void *display);
qboolean Sys_Video_GetFullscreen(void *display);
const char *Sys_Video_GetMode(void *display);
int Sys_Video_FocusChanged(void *display);

#ifdef GLQUAKE
void Sys_Video_BeginFrame(void *display);
void Sys_Video_SetGamma(void *display, unsigned short *ramps);
qboolean Sys_Video_HWGammaSupported(void *display);
void *Sys_Video_GetProcAddress(void *display, const char *name);
#else
void Sys_Video_SetPalette(void *display, unsigned char *palette);
unsigned int Sys_Video_GetBytesPerRow(void *display);
void *Sys_Video_GetBuffer(void *display);
#endif

/* Video mode functions */

const char * const *Sys_Video_GetModeList(void);
void Sys_Video_FreeModeList(const char * const *displaymodes);
const char *Sys_Video_GetModeDescription(const char *mode);
void Sys_Video_FreeModeDescription(const char *modedescription);

/* Clipboard functions */

const char *Sys_Video_GetClipboardText(void *display);
void Sys_Video_FreeClipboardText(void *display, const char *text);
void Sys_Video_SetClipboardText(void *display, const char *text);

