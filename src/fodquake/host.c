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

// this should be the only file that includes both server.h and client.h

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <locale.h>

#include "quakedef.h"
#include "pmove.h"
#include "version.h"
#include "modules.h"
#include "sound.h"
#ifdef GLQUAKE
#include "gl_local.h"
#else
#include "d_iface.h"
#endif
#include "fchecks.h"
#include "filesystem.h"
#include "fmod.h"
#include "ignore.h"
#include "image.h"
#include "logging.h"
#include "menu.h"
#include "keys.h"
#include "teamplay.h"
#include "mouse.h"
#include "readablechars.h"
#include "input.h"
#include "config_manager.h"
#include "movie.h"
#include "sbar.h"
#include <setjmp.h>
#include "server_browser.h"
#include "ruleset.h"
#include "context_sensitive_tab.h"
#include "lua.h"

#ifndef CLIENTONLY
#include "server.h"
#endif

#if !defined(CLIENTONLY) && !defined(SERVERONLY)
qboolean	dedicated = false;
#endif

double		curtime;

qboolean	host_initialized;	// true if into command execution

static jmp_buf 	host_abort;

void Host_Abort(void)
{
	longjmp(host_abort, 1);
}

void Host_EndGame(void)
{
#ifndef CLIENTONLY
	SV_Shutdown ("Server was killed");
#endif
	CL_Disconnect();
	// clear disconnect messages from loopback
	NET_ClearLoopback();
}

//This shuts down both the client and server
void Host_Error(char *error, ...)
{
	va_list argptr;
	char string[1024];
	static qboolean inerror = false;

	if (inerror)
		Sys_Error("Host_Error: recursively entered");

	inerror = true;

	va_start(argptr,error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	Com_Printf("\n===========================\n");
	Com_Printf("Host_Error: %s\n",string);
	Com_Printf("===========================\n\n");

#ifndef CLIENTONLY
	SV_Shutdown(va("server crashed: %s\n", string));
#endif
	CL_Disconnect();

	if (dedicated)
	{
		NET_Shutdown();
		COM_Shutdown();
		Sys_Error("%s", string);
	}

	if (!host_initialized)
		Sys_Error("Host_Error: %s", string);

	inerror = false;

	Host_Abort();
}

//memsize is the recommended amount of memory to use for hunk
static void Host_InitMemory()
{
	setlocale(LC_NUMERIC, "C");

	Memory_Init();
}

static void Host_ShutdownMemory()
{
	Memory_Shutdown();
}

void Host_Frame(double time)
{
	if (setjmp(host_abort))
		return;			// something bad happened, or the server disconnected

	rand();

	curtime += time;

#ifndef CLIENTONLY
	if (dedicated)
		SV_Frame(time);
	else
#endif
		CL_Frame(time);	// will also call SV_Frame
}

static char *Host_PrintBars(char *s, int len)
{
	static char temp[512];
	int i, count;

	temp[0] = 0;

	count = (len - 2 - 2 - strlen(s)) / 2;
	if (count < 0 || count > sizeof(temp) / 2 - 8)
		return temp;

	strcat(temp, "\x1d");
	for (i = 0; i < count; i++)
		strcat(temp, "\x1e");
	strcat(temp, " ");
	strcat(temp, s);
	strcat(temp, " ");
	for (i = 0; i < count; i++)
		strcat(temp, "\x1e");
	strcat(temp, "\x1f");
	strcat(temp, "\n\n");

	return temp;
}

void CL_SaveArgv(int, char **);

int cvarsregged;

void Host_Init(int argc, char **argv)
{
	srand(time(0));

	COM_InitArgv(argc, argv);

	Host_InitMemory();

	ReadableChars_Init();
	Cbuf_Init();
	Cmd_Init();
	Cvar_Init();
	COM_Init();
	FS_Init();

	VID_CvarInit();
	S_CvarInit();
#ifdef GLQUAKE
	GL_CvarInit();
#else
	D_CvarInit();
#endif
	Image_CvarInit();
	R_CvarInit();
	Con_CvarInit();
	SCR_CvarInit();
	if (!dedicated)
	{
		CL_CvarInit();
		CL_CvarInitInput();
		CL_CvarInitPrediction();
		CL_CvarInitCam();
		CL_CvarDemoInit();
	}
	Mouse_CvarInit();
	ConfigManager_CvarInit();
	Movie_CvarInit();
	MT_CvarInit();
#ifndef CLIENTONLY
	SV_CvarInit();
#endif
	Sbar_CvarInit();
	TP_CvarInit();
	Ignore_CvarInit();
	V_CvarInit();
	Log_CvarInit();
	Netchan_CvarInit();
	Stats_CvarInit();
	Draw_CvarInit();
	Key_CvarInit();
	M_CvarInit();
#ifndef CLIENTONLY
	PR_CvarInit();
#endif
	FMod_CvarInit();
	FChecks_CvarInit();
	Sys_CvarInit();
	SB_CvarInit();
	Lua_CvarInit();
	Ruleset_CvarInit();
	Context_Sensitive_Tab_Completion_CvarInit();

	cvarsregged = 1;

	Con_Init ();

	FS_InitFilesystem();
	COM_CheckRegistered();

	Con_Suppress();

	if (dedicated)
	{
		Cbuf_AddText("exec server.cfg\n");
		Cmd_StuffCmds_f();		// process command line arguments
		Cbuf_Execute();
	}
	else
	{
		Cbuf_AddText("exec default.cfg\n");
		if (FS_FileExists("config.cfg"))
		{
			Cbuf_AddText("exec config.cfg\n");
		}
		if (FS_FileExists("autoexec.cfg"))
		{
			Cbuf_AddText("exec autoexec.cfg\n");
		}
		Cbuf_AddText("cfg_load default.cfg\n");
		Cbuf_Execute();
		Cmd_StuffCmds_f();		// process command line arguments
		Cbuf_AddText("cl_warncmd 1\n");
	}

	Cmd_ParseLegacyCmdLineCmds();
	Cbuf_Execute();

	Con_Unsuppress();

	if (!Mouse_Init())
	{
		Host_Error("Unable to initialise mouse input\n");
	}

	NET_Init();
	QLib_Init();
	Sys_Init();
	PM_Init();
	Mod_Init();

	Lua_Init();
	SB_Init();
#ifndef CLIENTONLY
	SV_Init();
#endif
	CL_Init();

	Context_Weighting_Init();

#ifndef SERVERONLY
	if (!dedicated)
		CL_SaveArgv(argc, argv);
#endif

	host_initialized = true;

	Cmd_EnableFunctionExecution();

	Com_Printf("Exe: "__TIME__" "__DATE__"\n");

	Com_Printf("\nFodquake version %s\n\n", VersionString());

	if (dedicated)
	{
		// if a map wasn't specified on the command line, spawn start map
		if (!com_serveractive)
			Cmd_ExecuteString("map start");
		if (!com_serveractive)
			Host_Error("Couldn't spawn a server");
	}
	else
		VID_Open();
}

//FIXME: this is a callback from Sys_Quit and Sys_Error.  It would be better
//to run quit through here before the final handoff to the sys code.
void Host_Shutdown(void)
{
	static qboolean isdown = false;

	if (isdown)
	{
		printf("recursive shutdown\n");
		return;
	}
	isdown = true;

	Context_Weighting_Shutdown();

	SB_Quit();
	Lua_Shutdown();
#ifndef CLIENTONLY
	SV_Shutdown("Server quit\n");
#endif
	QLib_Shutdown();
	CL_Shutdown();
	Mod_Shutdown();
	FS_ShutdownFilesystem();
	NET_Shutdown();
	Mouse_Shutdown();
#ifndef SERVERONLY
	Con_Shutdown();
#endif
	COM_Shutdown();

	Cvar_Shutdown();
	Cmd_Shutdown();
	Cbuf_Shutdown();
	CSTC_Shutdown();

	Host_ShutdownMemory();
}

void Host_Quit(void)
{
	Host_Shutdown ();
	Sys_Quit();
}

