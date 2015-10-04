/*
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

#include <dlfcn.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "sys_lib.h"

struct
{
	const char *inname;
	const char *libname;
} libname_translation_table[] =
{
	{ "png12", "libpng12.so.0" },
	{ "png14", "libpng14.so" },
	{ "png15", "libpng15.so" },
	{ "z", "libz.so.1" },
	{ "jpeg", "libjpeg.so.62" },
	{ "Xxf86vm", "libXxf86vm.so.1" },
	{ "Xrandr", "libXrandr.so.2" },
};

#define LIBNAME_TRANSLATION_TABLE_SIZE (sizeof(libname_translation_table)/sizeof(*libname_translation_table))

struct SysLib
{
	void *elf_handle;
};

struct SysLib *Sys_Lib_Open(const char *libname)
{
	struct SysLib *lib;
	unsigned int i;

	lib = malloc(sizeof(*lib));
	if (lib)
	{
		for(i=0;i<LIBNAME_TRANSLATION_TABLE_SIZE;i++)
		{
			if (strcmp(libname_translation_table[i].inname, libname) == 0)
			{
				lib->elf_handle = dlopen(libname_translation_table[i].libname, RTLD_NOW);
				if (lib->elf_handle)
				{
					return lib;
				}
			}
		}

		free(lib);
	}

	return 0;
}

void Sys_Lib_Close(struct SysLib *lib)
{
	dlclose(lib->elf_handle);
	free(lib);
}

void *Sys_Lib_GetAddressByName(struct SysLib *lib, const char *symbolname)
{
	return dlsym(lib->elf_handle, symbolname);
}

