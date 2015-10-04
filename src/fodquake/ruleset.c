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

#include <string.h>

#include "qtypes.h"
#include "cvar.h"
#include "common.h"
#include "ruleset.h"

struct ruleset
{
	const char *name;
	void (*Init)(void);
	qboolean (*AllowRJScripts)(void);
	qboolean (*AllowTimeRefresh)(void);
	qboolean (*AllowPacketCmd)(void);
	qboolean (*ValidateCvarChange)(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue);
	qboolean (*AllowFTrigger)(const char *triggername);
	qboolean (*AllowMsgTriggers)(void);
	qboolean (*AllowMovementScripts)(void);
};

/* --- Default ruleset */

static void Ruleset_Default_Init()
{
}

static qboolean Ruleset_Default_AllowRJScripts()
{
	return true;
}

static qboolean Ruleset_Default_AllowTimeRefresh()
{
	return true;
}

static qboolean Ruleset_Default_AllowPacketCmd()
{
	return true;
}

static qboolean Ruleset_Default_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	return true;
}

static qboolean Ruleset_Default_AllowFTrigger(const char *triggername)
{
	return true;
}

static qboolean Ruleset_Default_AllowMsgTriggers()
{
	return true;
}

static qboolean Ruleset_Default_AllowMovementScripts()
{
	return true;
}

static const struct ruleset ruleset_default =
{
	"default",
	Ruleset_Default_Init,
	Ruleset_Default_AllowRJScripts,
	Ruleset_Default_AllowTimeRefresh,
	Ruleset_Default_AllowPacketCmd,
	Ruleset_Default_ValidateCvarChange,
	Ruleset_Default_AllowFTrigger,
	Ruleset_Default_AllowMsgTriggers,
	Ruleset_Default_AllowMovementScripts,
};

/* --- EQL ruleset */

static void Ruleset_EQL_Init()
{
#ifndef GLQUAKE
	extern cvar_t r_aliasstats;
#endif
	extern cvar_t cl_imitate_client;
	extern cvar_t cl_imitate_os;

#ifndef GLQUAKE
	Cvar_Set(&r_aliasstats, "0");
#endif
	Cvar_Set(&cl_imitate_client, "none");
	Cvar_Set(&cl_imitate_os, "none");
}

static qboolean Ruleset_EQL_AllowRJScripts()
{
	return false;
}

static qboolean Ruleset_EQL_AllowTimeRefresh()
{
	return false;
}

static qboolean Ruleset_EQL_AllowPacketCmd()
{
	return false;
}

static qboolean Ruleset_EQL_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	if (strcmp(cvar->name, "r_aliasstats") == 0
	 || strcmp(cvar->name, "cl_imitate_client") == 0
	 || strcmp(cvar->name, "cl_imitate_os") == 0)
	{
		return false;
	}

	return true;
}

static qboolean Ruleset_EQL_AllowFTrigger(const char *triggername)
{
	if (strcmp(triggername, "f_took") == 0
	 || strcmp(triggername, "f_respawn") == 0
	 || strcmp(triggername, "f_death") == 0
	 || strcmp(triggername, "f_flagdeath") == 0)
	{
		return false;
	}

	return true;
}

static qboolean Ruleset_EQL_AllowMsgTriggers()
{
	return false;
}

static qboolean Ruleset_EQL_AllowMovementScripts()
{
	return false;
}

static const struct ruleset ruleset_eql =
{
	"eql",
	Ruleset_EQL_Init,
	Ruleset_EQL_AllowRJScripts,
	Ruleset_EQL_AllowTimeRefresh,
	Ruleset_EQL_AllowPacketCmd,
	Ruleset_EQL_ValidateCvarChange,
	Ruleset_EQL_AllowFTrigger,
	Ruleset_EQL_AllowMsgTriggers,
	Ruleset_EQL_AllowMovementScripts,
};

/* --- */

static const struct ruleset *ruleset = &ruleset_default;

const char *Ruleset_GetName()
{
	return ruleset->name;
}

qboolean Ruleset_AllowRJScripts()
{
	return ruleset->AllowRJScripts();
}

qboolean Ruleset_AllowTimeRefresh()
{
	return ruleset->AllowTimeRefresh();
}

qboolean Ruleset_AllowPacketCmd()
{
	return ruleset->AllowPacketCmd();
}

qboolean Ruleset_ValidateCvarChange(const cvar_t *cvar, const char *newstringvalue, float newfloatvalue)
{
	return ruleset->ValidateCvarChange(cvar, newstringvalue, newfloatvalue);
}

qboolean Ruleset_AllowFTrigger(const char *triggername)
{
	return ruleset->AllowFTrigger(triggername);
}

qboolean Ruleset_AllowMsgTriggers()
{
	return ruleset->AllowMsgTriggers();
}

qboolean Ruleset_AllowMovementScripts()
{
	return ruleset->AllowMovementScripts();
}

/* --- */

static const struct ruleset *rulesets[] =
{
	&ruleset_default,
	&ruleset_eql,
	0
};

static qboolean cl_ruleset_callback(cvar_t *var, char *string)
{
	unsigned int i;

	for(i=0;rulesets[i];i++)
	{
		if (strcmp(rulesets[i]->name, string) == 0)
			break;
	}

	if (rulesets[i] == 0)
	{
		Com_Printf("Invalid ruleset name. Available rulesets are:\n");

		for(i=0;rulesets[i];i++)
		{
			Com_Printf("%s\n", rulesets[i]->name);
		}

		return true;
	}

	return false;
}

static cvar_t cl_ruleset = { "cl_ruleset", "default", 0, cl_ruleset_callback };

void Ruleset_Activate()
{
	unsigned int i;

	ruleset = &ruleset_default;

	for(i=0;rulesets[i];i++)
	{
		if (strcmp(rulesets[i]->name, cl_ruleset.string) == 0)
		{
			rulesets[i]->Init();
			ruleset = rulesets[i];
			break;
		}
	}
}

void Ruleset_CvarInit()
{
	Cvar_Register(&cl_ruleset);
}

