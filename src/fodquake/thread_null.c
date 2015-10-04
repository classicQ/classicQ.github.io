/*
Copyright (C) 2008-2010 Mark Olsen

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

#include "sys_thread.h"

struct SysThread *Sys_Thread_CreateThread(void (*entrypoint)(void *), void *argument)
{
	return 0;
}

void Sys_Thread_DeleteThread(struct SysThread *thread)
{
}

int Sys_Thread_SetThreadPriority(struct SysThread *thread, enum SysThreadPriority priority)
{
	return 0;
}

struct SysMutex *Sys_Thread_CreateMutex(void)
{
	return (struct SysMutex *)1;
}

void Sys_Thread_DeleteMutex(struct SysMutex *mutex)
{
}

void Sys_Thread_LockMutex(struct SysMutex *mutex)
{
}

void Sys_Thread_UnlockMutex(struct SysMutex *mutex)
{
}

