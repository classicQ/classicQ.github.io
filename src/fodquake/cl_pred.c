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

#include <math.h>

#include "quakedef.h"
#include "pmove.h"
#include "teamplay.h"

cvar_t	cl_nopred	= {"cl_nopred", "0"};
qboolean OnChange_cl_pushlatency(cvar_t *cvar, char *string);
cvar_t	cl_pushlatency	= {"pushlatency", "-999", 0, OnChange_cl_pushlatency};

qboolean OnChange_cl_pushlatency(cvar_t *cvar, char *string) {
	float newvalue = Q_atof(string);
	newvalue = min(newvalue, 0);
	Cvar_SetValue(cvar, newvalue);
	return true;
}

void CL_PredictUsercmd (player_state_t *from, player_state_t *to, usercmd_t *u) {
	// split up very long moves
	if (u->msec > 50) {
		player_state_t temp;
		usercmd_t split;

		split = *u;
		split.msec /= 2;

		CL_PredictUsercmd (from, &temp, &split);
		CL_PredictUsercmd (&temp, to, &split);
		return;
	}

	VectorCopy (from->origin, pmove.origin);
	VectorCopy (u->angles, pmove.angles);
	VectorCopy (from->velocity, pmove.velocity);

	pmove.jump_msec = (cl.z_ext & Z_EXT_PM_TYPE) ? 0 : from->jump_msec;
	pmove.jump_held = from->jump_held;
	pmove.waterjumptime = from->waterjumptime;
	pmove.pm_type = from->pm_type;

	pmove.cmd = *u;

	movevars.entgravity = cl.entgravity;
	movevars.maxspeed = cl.maxspeed;
	movevars.bunnyspeedcap = cl.bunnyspeedcap;

	PM_PlayerMove ();

	to->waterjumptime = pmove.waterjumptime;
	to->jump_held = pmove.jump_held;
	to->jump_msec = pmove.jump_msec;
	pmove.jump_msec = 0;

	VectorCopy (pmove.origin, to->origin);
	VectorCopy (pmove.angles, to->viewangles);
	VectorCopy (pmove.velocity, to->velocity);
	to->onground = pmove.onground;

	to->weaponframe = from->weaponframe;
	to->pm_type = from->pm_type;
}

//Used when cl_nopred is 1 to determine whether we are on ground, otherwise stepup smoothing code produces ugly jump physics
void CL_CategorizePosition (void) {
	if (cl.spectator && cl.playernum == cl.viewplayernum) {
		cl.onground = false;	// in air
		return;
	}
	VectorClear (pmove.velocity);
	VectorCopy (cl.simorg, pmove.origin);
	pmove.numtouch = 0;
	PM_CategorizePosition ();
	cl.onground = pmove.onground;
}

//Smooth out stair step ups.
//Called before CL_EmitEntities so that the player's lightning model origin is updated properly
void CL_CalcCrouch (void) {
	qboolean teleported;
	static vec3_t oldorigin = {0, 0, 0};
	static float oldz = 0, extracrouch = 0, crouchspeed = 100;

	teleported = !VectorL2Compare(cl.simorg, oldorigin, 48);
	VectorCopy (cl.simorg, oldorigin);

	if (teleported) {
		// possibly teleported or respawned
		oldz = cl.simorg[2];
		extracrouch = 0;
		crouchspeed = 100;
		cl.crouch = 0;
		VectorCopy (cl.simorg, oldorigin);
		return;
	}

	if (cl.onground && cl.simorg[2] - oldz > 0) {
		if (cl.simorg[2] - oldz > 20) {
			// if on steep stairs, increase speed
			if (crouchspeed < 160) {
				extracrouch = cl.simorg[2] - oldz - cls.frametime * 200 - 15;
				extracrouch = min(extracrouch, 5);
			}
			crouchspeed = 160;
		}

		oldz += cls.frametime * crouchspeed;
		if (oldz > cl.simorg[2])
			oldz = cl.simorg[2];

		if (cl.simorg[2] - oldz > 15 + extracrouch)
			oldz = cl.simorg[2] - 15 - extracrouch;
		extracrouch -= cls.frametime * 200;
		extracrouch = max(extracrouch, 0);

		cl.crouch = oldz - cl.simorg[2];
	} else {
		// in air or moving down
		oldz = cl.simorg[2];
		cl.crouch += cls.frametime * 150;
		if (cl.crouch > 0)
			cl.crouch = 0;
		crouchspeed = 100;
		extracrouch = 0;
	}
}

#ifdef NETQW
#define INTERPOLATEDPHYSICS 1
#else
#define INTERPOLATEDPHYSICS 0
#endif

void CL_PredictMove (void) {
	int i, oldphysent;
	frame_t *from = NULL, *to;
	float lerpfrac;
	double playertime;

	if (cl.paused)
		return;

	if (cl.intermission) {
		cl.crouch = 0;
		return;
	}

	if (!cl.validsequence)
		return;

	if (cls.netchan.outgoing_sequence - cl.validsequence >= UPDATE_BACKUP - 1)
		return;

	VectorCopy (cl.viewangles, cl.simangles);

	// this is the last valid frame received from the server
	to = &cl.frames[cl.validsequence & UPDATE_MASK];

	// FIXME...
	if (cls.demoplayback && cl.spectator && cl.viewplayernum != cl.playernum) {
		VectorCopy (to->playerstate[cl.viewplayernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.viewplayernum].origin, cl.simorg);
		VectorCopy (to->playerstate[cl.viewplayernum].viewangles, cl.simangles);
		CL_CategorizePosition ();
		goto out;
	}

	if ((cl_nopred.value && !cls.mvdplayback) || (!INTERPOLATEDPHYSICS && cl.validsequence + 1 >= cls.netchan.outgoing_sequence))
	{
		VectorCopy (to->playerstate[cl.playernum].velocity, cl.simvel);
		VectorCopy (to->playerstate[cl.playernum].origin, cl.simorg);
		CL_CategorizePosition ();
		goto out;
	}

	oldphysent = pmove.numphysent;
	CL_SetSolidPlayers (cl.playernum);

	playertime = cls.realtime - cls.latency - cl_pushlatency.value * 0.001;
	playertime = min(playertime, cls.realtime);

	// run frames until to->senttime >= playertime

	if (INTERPOLATEDPHYSICS)
	{
		double curtime;

		if (cl.validsequence >= 0)
		{
			for(i=cl.validsequence;i<cls.netchan.outgoing_sequence-1;i++)
			{
				from = &cl.frames[i & UPDATE_MASK];
				to = &cl.frames[(i + 1) & UPDATE_MASK];
				CL_PredictUsercmd(&from->playerstate[cl.playernum], &to->playerstate[cl.playernum], &to->cmd);
			}
		}
	
		curtime = Sys_DoubleTime();

		from = &cl.frames[(cls.netchan.outgoing_sequence - 2) & UPDATE_MASK];
		to = &cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK];

		playertime = from->senttime + (curtime - to->senttime);
	}
	else
	{
		for (i = 1; i < cls.netchan.outgoing_sequence - cl.validsequence; i++) {
			from = to;
			to = &cl.frames[(cl.validsequence + i) & UPDATE_MASK];
			CL_PredictUsercmd(&from->playerstate[cl.playernum], &to->playerstate[cl.playernum], &to->cmd);
			cl.onground = pmove.onground;
			if (to->senttime >= playertime)
				break;
		}
	}

	if (from == 0)
	{
		VectorCopy(to->playerstate[cl.playernum].origin, cl.simorg);
		VectorCopy(to->playerstate[cl.playernum].velocity, cl.simvel);
		goto out;
	}

	pmove.numphysent = oldphysent;

	// now interpolate some fraction of the final frame
	if (!VectorSupCompare(from->playerstate[cl.playernum].origin, to->playerstate[cl.playernum].origin, 128)) {
		lerpfrac = 1;	// teleported, so don't lerp
	} else if (to->senttime == from->senttime) {
		lerpfrac = 0;
	} else {
		lerpfrac = (playertime - from->senttime) / (to->senttime - from->senttime);
		lerpfrac = bound(0, lerpfrac, 1);
	}

	VectorInterpolate(from->playerstate[cl.playernum].origin, lerpfrac, to->playerstate[cl.playernum].origin, cl.simorg);
	VectorInterpolate(from->playerstate[cl.playernum].velocity, lerpfrac, to->playerstate[cl.playernum].velocity, cl.simvel);

out:
	if (INTERPOLATEDPHYSICS)
		CL_CategorizePosition ();

	CL_CalcCrouch ();
	cl.waterlevel = pmove.waterlevel;
}

void CL_CvarInitPrediction(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_NETWORK);
	Cvar_Register(&cl_nopred);
	Cvar_Register(&cl_pushlatency);

	Cvar_ResetCurrentGroup();
}

