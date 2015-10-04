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

#include <exec/exec.h>
#ifndef AROS
#include <exec/system.h>
#endif
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#ifndef AROS
#include <proto/random.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <signal.h>

#include "quakedef.h"

int __stack = 1024*1024*2;

struct Library *SocketBase;

int do_stdin = 0;
qboolean stdin_ready;

cvar_t sys_dynamicpriority = { "sys_dynamicpriority", "0" };

void Sys_Shutdown()
{
	if (SocketBase)
	{
		CloseLibrary(SocketBase);
		SocketBase = 0;
	}
}

void Sys_CvarInit()
{
	Cvar_Register(&sys_dynamicpriority);
}

void Sys_Init()
{
}

void Sys_Quit()
{
	Sys_Shutdown();

	exit(0);
}

void Sys_Error(char *error, ...)
{
	va_list va;

	char string[1024];

	va_start(va, error);
	vsnprintf(string, sizeof(string), error, va);
	va_end(va);

	fprintf(stderr, "Error: %s\n", string);

	Host_Shutdown();

	Sys_Shutdown();

	exit(1);
}

void Sys_Printf(char *fmt, ...)
{
	va_list va;
	char text[2048];
	char *p;

	if (!dedicated)
		return;

	va_start(va, fmt);
	vsnprintf(text, sizeof(text), fmt, va);
	va_end(va);

	p = text;

	while(*p != 0)
	{
		if ((*p > 128 || *p < 32) && *p != 10 && *p != 13 && *p != 9)
			printf("[%02x]", *p);
		else
			putc(*p, stdout);

		p++;
	}
}

char *Sys_ConsoleInput()
{
	return 0;
}

void Sys_MicroSleep(unsigned int microseconds)
{
	usleep(microseconds);
}

static int secbase;

double Sys_DoubleTime()
{
	struct timeval tp;

	gettimeofday(&tp, 0);

	if (secbase == 0)
	{
		secbase = tp.tv_sec;
		return tp.tv_usec/1000000.0;
	}

	return (tp.tv_sec - secbase) + tp.tv_usec / 1000000.0;
}

unsigned long long Sys_IntTime()
{
	struct timeval tp;
	struct timezone tzp;
	unsigned long long ret;

	gettimeofday(&tp, &tzp);

	if (!secbase)
		secbase = tp.tv_sec;

	ret = tp.tv_sec - secbase;
	ret *= 1000000;
	ret += tp.tv_usec;

	return ret;
}

#include <devices/clipboard.h>
const char *Sys_Video_GetClipboardText(void *display)
{
	struct IOClipReq *clipreq;
	struct MsgPort *clipport;
	unsigned int iffstuff[5];
	char *clipdata;

	clipdata = 0;

	clipport = CreateMsgPort();
	if (clipport)
	{
		clipreq = CreateIORequest(clipport, sizeof(*clipreq));
		if (clipreq)
		{
			if (OpenDevice("clipboard.device", 0, (struct IORequest *)clipreq, 0) == 0)
			{
				clipreq->io_Command = CMD_READ;
				clipreq->io_Length = sizeof(iffstuff);
				clipreq->io_Data = (void *)iffstuff;
				clipreq->io_Offset = 0;
				clipreq->io_ClipID = 0;

				DoIO((struct IORequest *)clipreq);

				if (clipreq->io_Error == 0
				 && clipreq->io_Actual == sizeof(iffstuff)
				 && iffstuff[0] == 0x464F524D
				 && iffstuff[2] == 0x46545854
				 && iffstuff[3] == 0x43485253)
				{
					clipdata = AllocVec(iffstuff[4]+1, MEMF_ANY);
					if (clipdata)
					{
						clipreq->io_Command = CMD_READ;
						clipreq->io_Length = iffstuff[4]<sizeof(clipdata)?iffstuff[4]:sizeof(clipdata)-1;
						clipreq->io_Data = clipdata;

						DoIO((struct IORequest *)clipreq);

						if (!(clipreq->io_Error != 0 || clipreq->io_Actual != iffstuff[4]))
						{
							clipdata[iffstuff[4]] = 0;
						}
						else
						{
							FreeVec(clipdata);
							clipdata = 0;
						}
					}
				}

				do
				{
					clipreq->io_Command = CMD_READ;
					clipreq->io_Length = 0x100000;
					clipreq->io_Data = 0;

					DoIO((struct IORequest *)clipreq);
				} while(clipreq->io_Error == 0 && clipreq->io_Actual == 0x100000);

				CloseDevice((struct IORequest *)clipreq);
			}

			DeleteIORequest((struct IORequest *)clipreq);
		}

		DeleteMsgPort(clipport);
	}

	return clipdata;
}

void Sys_Video_FreeClipboardText(void *display, const char *text)
{
	FreeVec((char *)text);
}

void Sys_Video_SetClipboardText(void *display, const char *text)
{
}

void Sys_SleepTime(unsigned int usec)
{
	static int curstate;
	static int oldpri;

	if (sys_dynamicpriority.value)
	{
		if (usec >= 1000)
		{
			if (curstate == 0)
			{
				oldpri = SetTaskPri(FindTask(0), sys_dynamicpriority.value);
				curstate = 1;
			}
		}
		else
		{
			if (curstate == 1)
			{
				SetTaskPri(FindTask(0), oldpri);
				curstate = 0;
			}
		}
	}
	else if (curstate == 1)
	{
		SetTaskPri(FindTask(0), oldpri);
		curstate = 0;
	}
}

#ifdef AROS
#warning Fix this.
void Sys_RandomBytes(void *target, unsigned int numbytes)
{
	Sys_Error("Sys_RandomBytes() is not implemented on AROS");
}
#else
void Sys_RandomBytes(void *target, unsigned int numbytes)
{
	unsigned char *t;

	t = target;

	while(numbytes--)
		*t++ = RandomByte();
}
#endif

int main(int argc, char **argv)
{
	double time, oldtime, newtime;
#ifndef AROS
	struct Resident *morphos;
	ULONG r1;
	ULONG r2;

	morphos = FindResident("MorphOS");
	if (morphos)
	{
		r1 = NewGetSystemAttrs(&r2, sizeof(r2), SYSTEMINFOTYPE_PPC_ALTIVEC, TAG_DONE);
		if (r1 == sizeof(r2) && r2 != 0 && morphos && (morphos->rt_Flags&RTF_EXTENDED) && (morphos->rt_Version > 1 || (morphos->rt_Version == 1 && morphos->rt_Revision >= 5)))
			altivec_available = 1;
	}
#endif

	signal(SIGINT, SIG_IGN);

	SocketBase = OpenLibrary("bsdsocket.library", 0);

	COM_InitArgv(argc, argv);

	Host_Init(argc, argv);

	oldtime = Sys_DoubleTime();
	while((SetSignal(0, 0)&SIGBREAKF_CTRL_C) == 0)
	{
		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		oldtime = newtime;

		Host_Frame(time);
	}

	Sys_Error("End of app");

	return 0;
}

static const char *Sys_ResolvePath(const char *path)
{
	BPTR lock;
	char buf[1024];
	char *ret;

	ret = 0;

	lock = Lock(path, ACCESS_READ);
	if (lock)
	{
		if (NameFromLock(lock, buf, sizeof(buf)))
		{
			ret = AllocVec(strlen(buf) + 1, MEMF_ANY);
			if (ret)
			{
				strcpy(ret, buf);
			}
		}

		UnLock(lock);
	}

	return ret;
}

const char *Sys_GetRODataPath(void)
{
	return Sys_ResolvePath("PROGDIR:");
}

const char *Sys_GetUserDataPath(void)
{
	return 0;
}

const char *Sys_GetLegacyDataPath(void)
{
	char buf[1024];

	if (GetCurrentDirName(buf, sizeof(buf)))
	{
		return Sys_ResolvePath(buf);
	}

	return 0;
}

void Sys_FreePathString(const char *p)
{
	FreeVec((void *)p);
}

