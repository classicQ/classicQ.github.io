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
// sys.h -- non-portable functions

#include "compiler.h"

void Sys_MicroSleep(unsigned int microseconds);

void Sys_RandomBytes(void *target, unsigned int numbytes);

// memory protection
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length);

// an error will cause the entire program to exit
void Sys_Error (char *error, ...) PRINTFWARNING(1, 2) __attribute__((noreturn));

// send text to the console
void Sys_Printf (char *fmt, ...) PRINTFWARNING(1, 2);

void Sys_Quit (void);

double Sys_DoubleTime (void);
unsigned long long Sys_IntTime(void);

void Sys_SleepTime(unsigned int usec);

char *Sys_ConsoleInput (void);

void Sys_LowFPPrecision (void);
void Sys_HighFPPrecision (void);
void Sys_SetFPCW (void);

void Sys_CvarInit(void);
void Sys_Init (void);

const char *Sys_GetRODataPath(void);
const char *Sys_GetUserDataPath(void);
const char *Sys_GetLegacyDataPath(void);

void Sys_FreePathString(const char *);

#include "sys_video.h"

