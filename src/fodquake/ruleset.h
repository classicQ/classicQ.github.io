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

const char *Ruleset_GetName(void);
qboolean Ruleset_AllowRJScripts(void);
qboolean Ruleset_AllowTimeRefresh(void);
qboolean Ruleset_AllowPacketCmd(void);
qboolean Ruleset_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue);
qboolean Ruleset_AllowFTrigger(const char *triggername);
qboolean Ruleset_AllowMsgTriggers(void);
qboolean Ruleset_AllowMovementScripts(void);

void Ruleset_Activate(void);
void Ruleset_CvarInit(void);

