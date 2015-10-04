/*

Copyright (C) 2001-2002       A Nourai
Copyright (C) 2007, 2010-2011 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "modules.h"
#include "version.h"

#include "sys_lib.h"

typedef struct registeredModule_s
{
	qlib_id_t id;
	qboolean loaded;
	qlib_shutdown_fn shutdown;
} registeredModule_t;

static registeredModule_t registeredModules[qlib_nummodules];

void QLib_Init(void)
{
	int i;

	for (i = 0; i < qlib_nummodules; i++)
	{
		registeredModules[i].id = i;
		registeredModules[i].loaded = false;
		registeredModules[i].shutdown = NULL;
	}
}

void QLib_Shutdown(void)
{
	int i;

	for (i = 0; i < qlib_nummodules; i++)
	{
		if (registeredModules[i].loaded)
		{
			registeredModules[i].shutdown();
			registeredModules[i].loaded = false;
		}
	}
}

void QLib_RegisterModule(qlib_id_t module, qlib_shutdown_fn shutdown)
{
	if (module < 0 || module >= qlib_nummodules)
		Sys_Error("QLib_isModuleLoaded: bad module %d", module);

	registeredModules[module].loaded = true;
	registeredModules[module].shutdown = shutdown;
}

qboolean QLib_isModuleLoaded(qlib_id_t module)
{
	if (module < 0 || module >= qlib_nummodules)
		Sys_Error("QLib_isModuleLoaded: bad module %d", module);

	return registeredModules[module].loaded;
}

#ifndef __MORPHOS__
qboolean QLib_ProcessProcdef(struct SysLib *lib, qlib_dllfunction_t *procdefs, int size)
{
	int i;

	for (i = 0; i < size; i++)
	{
		if (!(*procdefs[i].function = Sys_Lib_GetAddressByName(lib, procdefs[i].name)))
		{
			Com_ErrorPrintf("Unable to find function \"%s\"\n", procdefs[i].name);

			for (i = 0; i < size; i++)
				procdefs[i].function = NULL;

			return false;
		}
	}

	return true;
}
#endif

void QLib_MissingModuleError(int errortype, char *libname, char *cmdline, char *features)
{
	switch (errortype)
	{
		case QLIB_ERROR_MODULE_MISSING_PROC:
			Sys_Error("Broken \"%s" QLIB_LIBRARY_EXTENSION "\" library - required function missing.", libname);

			break;
		default:
			Sys_Error("QLib_MissingModuleError: unknown error type (%d)", errortype);
	}
}

