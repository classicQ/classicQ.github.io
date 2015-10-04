/*
Copyright (C) 2009 Mark Olsen

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

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "mathlib.h"
#include "cvar.h"
#include "sys_thread.h"
#include "mouse.h"

static qboolean mouse_cvar_callback(cvar_t *var, char *string);

cvar_t sensitivity = { "sensitivity", "3", CVAR_ARCHIVE, mouse_cvar_callback };
cvar_t m_pitch = { "m_pitch", "0.022", CVAR_ARCHIVE, mouse_cvar_callback };
cvar_t m_yaw = { "m_yaw", "0.022", 0, mouse_cvar_callback };
cvar_t m_accel = { "m_accel", "0", 0, mouse_cvar_callback };
cvar_t m_filter = { "m_filter", "0", 0, mouse_cvar_callback };

struct Mouse
{
	struct SysMutex *mutex;
	vec3_t viewangles;
	int oldmousex, oldmousey;
	short forwardmove;
	short sidemove;

	/* cvar shadowing */
	float sensitivity;
	float m_pitch;
	float m_yaw;
	float m_accel;
	float m_filter;
};

static struct Mouse *mouse_global;

static qboolean mouse_cvar_callback(cvar_t *var, char *string)
{
	Cvar_Set(var, string);

	if (mouse_global)
	{
		Sys_Thread_LockMutex(mouse_global->mutex);

		if (var == &sensitivity)
			mouse_global->sensitivity = sensitivity.value;
		else if (var == &m_pitch)
			mouse_global->m_pitch = m_pitch.value;
		else if (var == &m_yaw)
			mouse_global->m_yaw = m_yaw.value;
		else if (var == &m_accel)
			mouse_global->m_accel = m_accel.value;
		else if (var == &m_filter)
			mouse_global->m_filter = m_filter.value;

		Sys_Thread_UnlockMutex(mouse_global->mutex);
	}

	return 1;
}

static void Mouse_UpdateValues(struct Mouse *mouse)
{
	double mx, my;
	int mousex, mousey;
	float filterfrac;
	float mousespeed;

	VID_GetMouseMovement(&mousex, &mousey);

	if (mouse->m_filter)
	{
		filterfrac = bound(0, mouse->m_filter, 1) / 2.0;
		mx = (mousex * (1 - filterfrac) + mouse->oldmousex * filterfrac);
		my = (mousey * (1 - filterfrac) + mouse->oldmousey * filterfrac);
	}
	else
	{
		mx = mousex;
		my = mousey;
	}
	
	mouse->oldmousex = mousex;
	mouse->oldmousey = mousey;

	if (mouse->m_accel)
	{
		mousespeed = sqrt(mousex * mousex + mousey * mousey);
		mx *= (mousespeed * mouse->m_accel + mouse->sensitivity);
		my *= (mousespeed * mouse->m_accel + mouse->sensitivity);
	}   
	else
	{
		mx *= mouse->sensitivity;
		my *= mouse->sensitivity;
	}

	mouse->viewangles[YAW] -= mouse->m_yaw * mx;

	if (mouse->viewangles[YAW] < 0)
		mouse->viewangles[YAW] = 360-fmod(-mouse->viewangles[YAW], 360);
	else if (mouse->viewangles[YAW] >= 360)
		mouse->viewangles[YAW] = fmod(mouse->viewangles[YAW], 360);

	mouse->viewangles[PITCH] += mouse->m_pitch * my;
	mouse->viewangles[PITCH] = bound(-70, mouse->viewangles[PITCH], 80);
}

void Mouse_GetViewAngles(vec3_t viewangles)
{
	Sys_Thread_LockMutex(mouse_global->mutex);

	Mouse_UpdateValues(mouse_global);

	VectorCopy(mouse_global->viewangles, viewangles);

	Sys_Thread_UnlockMutex(mouse_global->mutex);
}

void Mouse_SetViewAngles(vec3_t viewangles)
{
	Sys_Thread_LockMutex(mouse_global->mutex);

	VectorCopy(viewangles, mouse_global->viewangles);

	Sys_Thread_UnlockMutex(mouse_global->mutex);
}

void Mouse_AddViewAngles(vec3_t viewangles)
{
	Sys_Thread_LockMutex(mouse_global->mutex);

	VectorAdd(viewangles, mouse_global->viewangles, mouse_global->viewangles);

	Sys_Thread_UnlockMutex(mouse_global->mutex);
}

int Mouse_Init()
{
	mouse_global = malloc(sizeof(*mouse_global));
	if (mouse_global)
	{
		memset(mouse_global, 0, sizeof(*mouse_global));

		mouse_global->mutex = Sys_Thread_CreateMutex();
		if (mouse_global->mutex)
		{
			Sys_Thread_LockMutex(mouse_global->mutex);

			mouse_global->sensitivity = sensitivity.value;
			mouse_global->m_pitch = m_pitch.value;
			mouse_global->m_yaw = m_yaw.value;
			mouse_global->m_accel = m_accel.value;
			mouse_global->m_filter = m_filter.value;

			Sys_Thread_UnlockMutex(mouse_global->mutex);

			return 1;
		}

		free(mouse_global);
	}

	return 0;
}

void Mouse_Shutdown()
{
	if (!mouse_global)
		return;

	Sys_Thread_DeleteMutex(mouse_global->mutex);
	free(mouse_global);
	mouse_global = 0;
}

void Mouse_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&sensitivity);
	Cvar_Register(&m_pitch);
	Cvar_Register(&m_yaw);
	Cvar_Register(&m_accel);
	Cvar_Register(&m_filter);
}

