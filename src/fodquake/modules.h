/*

Copyright (C) 2001-2002       A Nourai
Copyright (C) 2010-2011 Mark Olsen

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

#ifndef _MODULES_H

#define _MODULES_H

#include "sys_lib.h"

#ifdef _WIN32
#define QLIB_LIBRARY_EXTENSION ".dll"
#else
#define QLIB_LIBRARY_EXTENSION ".so"
#endif

#define QLIB_ERROR_MODULE_MISSING_PROC		-2

typedef struct qlib_dllfunction_s {
	char *name;
	void **function;
} qlib_dllfunction_t;

#ifdef SERVERONLY
typedef enum qlib_id_s {qlib_nummodules} qlib_id_t;
#else
typedef enum qlib_id_s {qlib_libpng, qlib_libjpeg, qlib_nummodules} qlib_id_t;
#endif

typedef void (*qlib_shutdown_fn)(void);

void QLib_Init(void);
void QLib_Shutdown(void);
void QLib_RegisterModule(qlib_id_t module, qlib_shutdown_fn shutdown);
qboolean QLib_isModuleLoaded (qlib_id_t module);
qboolean QLib_ProcessProcdef(struct SysLib *lib, qlib_dllfunction_t *procdefs, int size);
void QLib_MissingModuleError(int, char *libname, char *cmdline, char *features);

#endif
