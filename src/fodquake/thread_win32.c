/*
Copyright (C) 2010 Mark Olsen

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

#include "windows.h"

#include "sys_thread.h"

struct SysThread
{
	HANDLE thread;
};

struct SysMutex
{
	CRITICAL_SECTION cs;
};

struct SysThread *Sys_Thread_CreateThread(void (*entrypoint)(void *), void *argument)
{
	struct SysThread *thread;

	thread = malloc(sizeof(*thread));
	if (thread)
	{
		thread->thread = CreateThread(0, 0, entrypoint, argument, 0, 0);
		if (thread->thread)
		{
			return thread;
		}

		free(thread);
	}

	return 0;
}

void Sys_Thread_DeleteThread(struct SysThread *thread)
{
	WaitForSingleObject(thread->thread, INFINITE);
	CloseHandle(thread->thread);
	free(thread);
}

int Sys_Thread_SetThreadPriority(struct SysThread *thread, enum SysThreadPriority priority)
{
	int pri;

	switch(priority)
	{
		case SYSTHREAD_PRIORITY_LOW:
			pri = THREAD_PRIORITY_LOWEST;
			break;

		case SYSTHREAD_PRIORITY_HIGH:
			pri = THREAD_PRIORITY_HIGHEST;
			break;

		default:
			pri = THREAD_PRIORITY_NORMAL;
			break;
	}

	SetThreadPriority(thread->thread, priority);

	return 0;
}

struct SysMutex *Sys_Thread_CreateMutex(void)
{
	struct SysMutex *mutex;

	mutex = malloc(sizeof(*mutex));
	if (mutex)
	{
		InitializeCriticalSection(&mutex->cs);

		return mutex;
	}

	return 0;
}

void Sys_Thread_DeleteMutex(struct SysMutex *mutex)
{
	free(mutex);
}

void Sys_Thread_LockMutex(struct SysMutex *mutex)
{
	EnterCriticalSection(&mutex->cs);
}

void Sys_Thread_UnlockMutex(struct SysMutex *mutex)
{
	LeaveCriticalSection(&mutex->cs);
}

