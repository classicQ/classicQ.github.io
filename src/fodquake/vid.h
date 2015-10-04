/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// vid.h -- video driver defs

#ifndef VID_H
#define VID_H

#include "keys.h"

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

// a pixel can be one, two, or four bytes
typedef byte pixel_t;

typedef struct vrect_s
{
	int				x,y,width,height;
	struct vrect_s	*pnext;
} vrect_t;

typedef struct
{
	pixel_t			*buffer;		// invisible buffer
	pixel_t			*colormap;		// 256 * VID_GRADES size
	unsigned		displaywidth;		
	unsigned		displayheight;
	float			aspect;		// width / height -- < 0 is taller than wide
	int				numpages;
	int				recalc_refdef;	// if true, recalc vid-based stuff
	unsigned		conwidth;
	unsigned		conheight;
#ifndef GLQUAKE
	unsigned		rowbytes;	// may be > width if displayed in a window
	int				maxwarpwidth;
	int				maxwarpheight;
#endif
} viddef_t;

extern	viddef_t	vid;				// global video state
extern	unsigned short	d_8to16table[256];
extern	unsigned	d_8to24table[256];

void	VID_SetPalette (unsigned char *palette);
// called at startup and after any gamma correction

void	VID_Init(unsigned char *palette);
// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again

void VID_CvarInit(void);
void VID_Open(void);
void VID_Close(void);

void VID_Shutdown (void);
// Called at shutdown

void VID_Restart(void);

void VID_Update (vrect_t *rects);
// flushes the given rectangles from the view buffer to the screen

int VID_GetKeyEvent(keynum_t *key, qboolean *down);

void VID_LockBuffer(void);
void VID_UnlockBuffer(void);

void VID_SetCaption(const char *text);

void VID_GetMouseMovement(int *mousex, int *mousey);

void VID_SetMouseGrab(int on);

const char *VID_GetClipboardText(void);
void VID_FreeClipboardText(const char *text);
void VID_SetClipboardText(const char *text);

#ifdef GLQUAKE
void VID_BeginFrame(void);
void VID_SetDeviceGammaRamp (unsigned short *ramps);
qboolean VID_HWGammaSupported(void);
void *VID_GetProcAddress(const char *name);
#endif

unsigned int VID_GetWidth(void);
unsigned int VID_GetHeight(void);
qboolean VID_GetFullscreen(void);
const char *VID_GetMode(void);

int VID_FocusChanged(void);

#endif
