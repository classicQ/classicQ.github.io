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
// sys_win.c

#include <windows.h>
#include <shlobj.h>

#include "quakedef.h"
#include "resource.h"
#include "keys.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <io.h>			// _open, etc
#include <direct.h>		// _mkdir
#include <conio.h>		// _putch

#include "sys_win.h"

#define MINIMUM_WIN_MEMORY	0x0c00000
#define MAXIMUM_WIN_MEMORY	0x1000000

#define PAUSE_SLEEP		50				// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP	20				// sleep time when not focus

int do_stdin = 0;
qboolean stdin_ready;


static qboolean OnChange_sys_highpriority(cvar_t *, char *);
static cvar_t	sys_highpriority = {"sys_highpriority", "0", 0, OnChange_sys_highpriority};


static cvar_t	sys_yieldcpu = {"sys_yieldcpu", "0"};

static HANDLE	tevent;
static HANDLE	hinput, houtput;

static HANDLE AdvAPIHandle;
static BOOL (*RtlGenRandom)(PVOID, ULONG);

void MaskExceptions(void);
void Sys_PopFPCW(void);
void Sys_PushFPCW_SetHigh(void);

int Sys_SetPriority(int priority)
{
	DWORD p;

	switch (priority)
	{
		case 0:	p = IDLE_PRIORITY_CLASS; break;
		case 1:	p = NORMAL_PRIORITY_CLASS; break;
		case 2:	p = HIGH_PRIORITY_CLASS; break;
		case 3:	p = REALTIME_PRIORITY_CLASS; break;
		default: return 0;
	}

	return SetPriorityClass(GetCurrentProcess(), p);
}

static qboolean OnChange_sys_highpriority(cvar_t *var, char *s)
{
	int ok, q_priority;
	char *desc;
	float priority;

	priority = Q_atof(s);
	if (priority == 1)
	{
		q_priority = 2;
		desc = "high";
	}
	else if (priority == -1)
	{
		q_priority = 0;
		desc = "low";
	}
	else
	{
		q_priority = 1;
		desc = "normal";
	}

	if (!(ok = Sys_SetPriority(q_priority)))
	{
		Com_Printf("Changing process priority failed\n");
		return true;
	}

	Com_Printf("Process priority set to %s\n", desc);
	return false;
}


/*
===============================================================================
SYSTEM IO
===============================================================================
*/

void Sys_MakeCodeWriteable(unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

	//@@@ copy on write or just read-write?
	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
		Sys_Error("Protection change failed");
}

void Sys_Error(char *error, ...)
{
	va_list argptr;
	char text[1024];

	Host_Shutdown();

	va_start(argptr, error);
	vsnprintf(text, sizeof(text), error, argptr);
	va_end(argptr);

	MessageBox(NULL, text, "Error", 0 /* MB_OK */ );

	exit(1);
}

void Sys_Printf(char *fmt, ...)
{
	va_list argptr;
	char text[1024];
	DWORD dummy;

#if 0
	va_start(argptr,fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
#endif

	if (!dedicated)
		return;

	va_start(argptr,fmt);
	vsnprintf(text, sizeof(text), fmt, argptr);
	va_end(argptr);

	WriteFile(houtput, text, strlen(text), &dummy, NULL);
}

void Sys_Quit(void)
{
	if (AdvAPIHandle)
		FreeLibrary(AdvAPIHandle);

	if (tevent)
		CloseHandle(tevent);

	if (dedicated)
		FreeConsole();

	exit (0);
}

static double pfreq;
static unsigned long long pfreq_int;
static qboolean hwtimer = false;

static __int64 startcount;
static DWORD starttime;

void Sys_InitDoubleTime(void)
{
	__int64 freq;

	timeBeginPeriod(1);

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		pfreq_int = freq;
		pfreq = (double)freq;
		QueryPerformanceCounter((LARGE_INTEGER *)&startcount);
		hwtimer = true;
	}
	else
		starttime = timeGetTime();
}

void Sys_MicroSleep(unsigned int microseconds)
{
	Sleep((microseconds+999)/1000);
}

double Sys_DoubleTime(void)
{
	__int64 pcount;
	DWORD now;

	if (hwtimer)
	{
		QueryPerformanceCounter((LARGE_INTEGER *)&pcount);
		// TODO: check for wrapping
		return (pcount - startcount) / pfreq;
	}

	now = timeGetTime();

	if (now < starttime) // wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	return (now - starttime) / 1000.0;
}

unsigned long long Sys_IntTime()
{
	unsigned long long ret;

	if (hwtimer)
	{
		QueryPerformanceCounter(&ret);
		ret -= startcount;
		ret *= 1000000;
		ret /= pfreq_int;
		return ret;
	}

	return Sys_DoubleTime()*1000000;
}

char *Sys_ConsoleInput(void)
{
	static char text[256];
	static int len;
	INPUT_RECORD rec;
	int i, dummy, ch, numread, numevents;
	char *textCopied;

	while (1)
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
			Sys_Error("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, &rec, 1, &numread))
			Sys_Error("Error reading console input");

		if (numread != 1)
			Sys_Error("Couldn't read console input");

		if (rec.EventType == KEY_EVENT)
		{
			if (rec.Event.KeyEvent.bKeyDown)
			{
				ch = rec.Event.KeyEvent.uChar.AsciiChar;
				switch (ch)
				{
					case '\r':
						WriteFile(houtput, "\r\n", 2, &dummy, NULL);
						if (len)
						{
							text[len] = 0;
							len = 0;
							return text;
						}
						break;

					case '\b':
						WriteFile(houtput, "\b \b", 3, &dummy, NULL);
						if (len)
							len--;
						break;

					default:
						if ((ch == ('V' & 31)) /* ctrl-v */ ||
							((rec.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED) && (rec.Event.KeyEvent.wVirtualKeyCode == VK_INSERT)))
							{

#if 0
								if ((textCopied = Sys_GetClipboardData()))
								{
									i = strlen(textCopied);
									if (i + len >= sizeof(text))
										i = sizeof(text) - len - 1;
									if (i > 0)
									{
										textCopied[i] = 0;
										text[len] = 0;
										strcat(text, textCopied);
										WriteFile(houtput, textCopied, i, &dummy, NULL);
										len += dummy;
									}
								}
#endif
							}
							else if (ch >= ' ')
							{
								WriteFile(houtput, &ch, 1, &dummy, NULL);
								text[len] = ch;
								len = (len + 1) & 0xff;
							}
							break;
				}
			}
		}
	}

	return NULL;
}

BOOL WINAPI HandlerRoutine(DWORD dwCtrlType)
{
	switch (dwCtrlType)
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			Cbuf_AddText("quit\n");
			return true;
	}
	return false;
}

//Quake calls this so the system can register variables before host_hunklevel is marked
void Sys_CvarInit(void)
{

	Cvar_SetCurrentGroup(CVAR_GROUP_SYSTEM_SETTINGS);
	Cvar_Register(&sys_highpriority);
	Cvar_Register(&sys_yieldcpu);
	Cvar_ResetCurrentGroup();
}

void Sys_Init(void)
{
}

void Sys_Init_(void)
{
	OSVERSIONINFO vinfo;

#if 0
	MaskExceptions();
	Sys_SetFPCW();
#endif

	Sys_InitDoubleTime();

	vinfo.dwOSVersionInfoSize = sizeof(vinfo);

	if (!GetVersionEx(&vinfo))
		Sys_Error("Couldn't get OS info");

	if ((vinfo.dwMajorVersion < 5) || (vinfo.dwPlatformId == VER_PLATFORM_WIN32s))
		Sys_Error("Fodquake requires at least Windows 2000");
}

/********************************* CLIPBOARD *********************************/

#if 0
/* Deprecated interface */
#define SYS_CLIPBOARD_SIZE		256

char *Sys_GetClipboardData(void)
{
	HANDLE th;
	char *clipText, *s, *t;
	static char clipboard[SYS_CLIPBOARD_SIZE];

	if (!OpenClipboard(NULL))
		return NULL;

	if (!(th = GetClipboardData(CF_TEXT)))
	{
		CloseClipboard();
		return NULL;
	}

	if (!(clipText = GlobalLock(th)))
	{
		CloseClipboard();
		return NULL;
	}

	s = clipText;
	t = clipboard;
	while (*s && t - clipboard < SYS_CLIPBOARD_SIZE - 1 && *s != '\n' && *s != '\r' && *s != '\b')
		*t++ = *s++;
	*t = 0;

	GlobalUnlock(th);
	CloseClipboard();

	return clipboard;
}
#endif

// copies given text to clipboard
void Sys_CopyToClipboard(char *text)
{
	char *clipText;
	HGLOBAL hglbCopy;

	if (!OpenClipboard(NULL))
		return;

	if (!EmptyClipboard())
	{
		CloseClipboard();
		return;
	}

	if (!(hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1)))
	{
		CloseClipboard();
		return;
	}

	if (!(clipText = GlobalLock(hglbCopy)))
	{
		CloseClipboard();
		return;
	}

	strcpy((char *) clipText, text);
	GlobalUnlock(hglbCopy);
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

/*
==============================================================================
 WINDOWS CRAP
==============================================================================
*/

#define MAX_NUM_ARGVS	50

int		argc;
char	*argv[MAX_NUM_ARGVS];
static char	*empty_string = "";

static void ParseCommandLine(char *lpCmdLine)
{
	argc = 1;
	argv[0] = empty_string;

	while (*lpCmdLine && (argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[argc] = lpCmdLine;
			argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}
}

static void SleepUntilInput(int time)
{
	MsgWaitForMultipleObjects(1, &tevent, FALSE, time, QS_ALLINPUT);
}

HINSTANCE global_hInstance;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	double time, oldtime, newtime;

	global_hInstance = hInstance;

	ParseCommandLine(lpCmdLine);

	// we need to check some parms before Host_Init is called
	COM_InitArgv(argc, argv);

#if !defined(CLIENTONLY)
	dedicated = COM_CheckParm("-dedicated");
#endif

	if (dedicated)
	{
		if (!AllocConsole())
			Sys_Error("Couldn't allocate dedicated server console");
		SetConsoleCtrlHandler(HandlerRoutine, TRUE);
		SetConsoleTitle("fqds");
		hinput = GetStdHandle(STD_INPUT_HANDLE);
		houtput = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	AdvAPIHandle = LoadLibrary("Advapi32.dll");
	if (AdvAPIHandle == 0)
	{
		Sys_Error("Couldn't open Advapi32.dll");
	}

	RtlGenRandom = (void *)GetProcAddress(AdvAPIHandle, "SystemFunction036");
	if (RtlGenRandom == 0)
	{
		Sys_Error("Unable to locate the symbol SystemFunction036 in Advapi32.dll");
	}

	tevent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!tevent)
		Sys_Error("Couldn't create event");

	Sys_Init_();

	Sys_Printf("Host_Init\n");
	Host_Init(argc, argv);

	oldtime = Sys_DoubleTime();

	/* main window message loop */
	while (1)
	{
		if (dedicated)
			NET_Sleep(1);

		newtime = Sys_DoubleTime();
		time = newtime - oldtime;
		Host_Frame(time);
		oldtime = newtime;

	}

	return TRUE;	/* return success of application */
}

void Sys_RandomBytes(void *target, unsigned int numbytes)
{
	RtlGenRandom(target, numbytes);
}

const char *Sys_GetRODataPath(void)
{
	char path[MAX_PATH];
	DWORD r;
	char *ret;
	char *p;

	ret = 0;

	r = GetModuleFileName(0, path, sizeof(path));
	if (r < sizeof(path))
	{
		p = strrchr(path, '\\');
		if (p)
		{
			ret = malloc(p - path + 1);
			if (ret)
			{
				memcpy(ret, path, p - path);
				ret[p - path] = 0;
			}
		}
	}

	return ret;
}

const char *Sys_GetUserDataPath(void)
{
	char path[MAX_PATH];
	HRESULT res;
	char *ret;

	ret = 0;

	res = SHGetFolderPathA(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, NULL, 0, path);
	if (res == S_OK)
	{
		ret = malloc(strlen(path) + strlen("\\Fodquake") + 1);
		if (ret)
		{
			sprintf(ret, "%s\\Fodquake", path);
		}
	}

	return ret;
}

const char *Sys_GetLegacyDataPath(void)
{
	char path[MAX_PATH];
	DWORD r;
	char *ret;

	ret = 0;

	r = GetCurrentDirectoryA(sizeof(path), path);
	if (r < sizeof(path))
	{
		ret = malloc(strlen(path) + 1);
		if (ret)
		{
			strcpy(ret, path);
		}
	}

	return ret;
}

void Sys_FreePathString(const char *p)
{
	free((void *)p);
}

