/*
Copyright (C) 2008-2009 Mark Olsen

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

#include <ogcsys.h>
#include <gccore.h>
#include <gctypes.h>

#include <sys/time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#undef TRUE
#undef FALSE
#undef BOOL
#define BOOL FBOOL

#include "fatfs/ff.h"

#undef BOOL

#define true qtrue
#define false qfalse
#include "quakedef.h"
#include "server.h"
#undef false
#undef true

void Sys_Printf (char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vprintf(fmt, va);
	va_end(va);
}

void Sys_Error (char *error, ...)
{
	va_list va;

	printf("Sys_Error: %s\n", error);

	va_start(va, error);
	vprintf(error, va);
	va_end(va);

	while(1);
}

void Sys_Quit()
{
	while(1);
}

void Sys_mkdir(char *path)
{
}

char *Sys_ConsoleInput()
{
	return 0;
}

const char *Sys_Video_GetClipboardText(void *display)
{
	return 0;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
}

#define gettb(u, l) \
do { \
	__asm volatile( \
		"\n1:\n" \
		"mftbu  %0\n" \
		"mftb   %1\n" \
		"mftbu  %%r5\n" \
		"cmpw   %0,%%r5\n" \
		"bne    1b\n" \
		: "=r" (u), "=r" (l) \
		: \
		: "r5"); \
} while(0)

#define TBFREQ (243000000/4)

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	unsigned int tbu;
	unsigned int tbl;

	unsigned long long tb;

	static unsigned long long starttb;

	gettb(tbu, tbl);

	tb = (((unsigned long long)tbu)<<32)|tbl;

	if (tv)
	{
		tv->tv_sec = tb/TBFREQ;
		tv->tv_usec = ((tb%TBFREQ)/6075)*100;
	}

	return 0;
}

double Sys_DoubleTime()
{
	unsigned int tbu;
	unsigned int tbl;

	unsigned long long tb;

	double time;

	static unsigned long long starttb;

	gettb(tbu, tbl);

	tb = (((unsigned long long)tbu)<<32)|tbl;

	if (starttb == 0)
		starttb = tb;

	time = (tb-starttb)/TBFREQ;
	time+= ((double)((tb-starttb)%TBFREQ))/TBFREQ;

	return time;
}

void Sys_CvarInit()
{
}

void Sys_Init()
{
}

static int main_real()
{
	printf("Mounting drive\n");

	{
		FATFS fso;
		FIL myfile;
		int r;

		memset(&fso, 0, sizeof(fso));
		memset(&myfile, 0, sizeof(myfile));

		r = f_mount(0, &fso);

		if (r == 0)
			printf("Succeeded\n");
		else
			printf("Failed\n");
	}

	{
	double mytime, oldtime, newtime;
	char *myargv[] = { "fodquake", 0 };

	printf("Calling Host_Init()\n");

#if 0
	cl.frames = malloc(sizeof(*cl.frames)*UPDATE_BACKUP);
	memset(cl.frames, 0, sizeof(*cl.frames)*UPDATE_BACKUP);
#endif
	Host_Init(1, myargv, 10*1024*1024);

	oldtime = Sys_DoubleTime();
	while(1)
	{
		newtime = Sys_DoubleTime();
		mytime = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(mytime);
	}

	Sys_Error("End of app");

	return 0;
	}
}

int main()
{
	void *xfb;
	GXRModeObj *rmode;
	lwp_t handle;
	int r;
	char *stack;

	IOS_ReloadIOS(30);

	VIDEO_Init();

	switch(VIDEO_GetCurrentTvMode())
	{
		case VI_NTSC:
			rmode = &TVNtsc480IntDf;
			break;

		case VI_PAL:
			rmode = &TVPal528IntDf;
			break;

		case VI_MPAL:
			rmode = &TVMpal480IntDf;
			break;

		default:
			rmode = &TVNtsc480IntDf;
			break;
	}

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb,20,20,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();

#if 1
	printf("Calling main()\n");
	main_real();
	printf("main() returned\n");
#else
	printf("Creating main thread\n");

	stack = malloc(1024*1024);
	if (stack == 0)
	{
		printf("Unable to allocate stack\n");
		while(1);
	}

	handle = 0;
	r = LWP_CreateThread(&handle, main_real, 0, stack, 1024*1024, 50);
	if (r != 0)
	{
		printf("Failed to create thread\n");
		while(1);
	}
	printf("Main thread created\n");
	LWP_SetThreadPriority(0, 0);

	printf("Looping\n");
	while(1);
#endif
}

int getuid()
{
	return 42;
}

