/*

Copyright (C) 2001-2002       A Nourai

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
#include "version.h"

#include "utils.h"
#include "fmod.h"
#include "modules.h"
#include "ruleset.h"

static float f_ruleset_reply_time, f_reply_time, f_mod_reply_time, f_version_reply_time, f_skins_reply_time, f_server_reply_time;
extern cvar_t r_fullbrightSkins;

extern int imitatedclientnum;
extern int imitatedosnum;

extern char *fversion_clientnames[];
extern char *fversion_osnames[];

void FChecks_VersionResponse(void) {
	Cbuf_AddText (va("say %s %s:" QW_RENDERER "\n", fversion_clientnames[imitatedclientnum], fversion_osnames[imitatedosnum]));
}

void FChecks_FServerResponse (void) {
	struct netaddr adr;

	if (!NET_StringToAdr(0, cls.servername, &adr))
		return;

	if (adr.type == NA_IPV4 && adr.addr.ipv4.port == 0)
		adr.addr.ipv4.port = BigShort (PORT_SERVER);
	else if (adr.type == NA_IPV6 && adr.addr.ipv6.port == 0)
		adr.addr.ipv6.port = BigShort (PORT_SERVER);

	Cbuf_AddText(va("say Fodquake f_server response: %s\n", NET_AdrToString(&adr)));
}

void FChecks_SkinsResponse(float fbskins) {
	Cbuf_AddText (va("say All skins %d%% fullbright\n", (int) (fbskins * 100)));	
}

qboolean FChecks_VersionRequest (char *s) {
	if (cl.spectator || (f_version_reply_time && cls.realtime - f_version_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_version") || Util_F_Match(s, "fuh_version")) {
		FChecks_VersionResponse();
		f_version_reply_time = cls.realtime;
		return true;
	}
	return false;
}

qboolean FChecks_SkinRequest (char *s) {
	float fbskins;		

	fbskins = bound(0, r_fullbrightSkins.value, cl.fbskins);	
	if (cl.spectator || !fbskins || (f_skins_reply_time && cls.realtime - f_skins_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_skins"))	{
		FChecks_SkinsResponse(fbskins);
		f_skins_reply_time = cls.realtime;
		return true;
	}
	return false;
}

qboolean FChecks_CheckFModRequest (char *s) {
	if (cl.spectator || (f_mod_reply_time && cls.realtime - f_mod_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_modified"))	{
		FMod_Response();
		f_mod_reply_time = cls.realtime;
		return true;
	}
	return false;
}

qboolean FChecks_CheckFServerRequest (char *s) {
	struct netaddr adr;

	if (cl.spectator || (f_server_reply_time && cls.realtime - f_server_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_server") && NET_StringToAdr(0, cls.servername, &adr))	{
		FChecks_FServerResponse();
		f_server_reply_time = cls.realtime;
		return true;
	}
	return false;
}

qboolean FChecks_CheckFRulesetRequest (char *s) {
	if (cl.spectator || (f_ruleset_reply_time && cls.realtime - f_ruleset_reply_time < 20))
		return false;

	if (Util_F_Match(s, "f_ruleset"))	{
		Cbuf_AddText(va("say Fodquake Ruleset: %s\n", Ruleset_GetName()));
		f_ruleset_reply_time = cls.realtime;
		return true;
	}
	return false;
}

void FChecks_CheckRequest(char *s) {
	qboolean fcheck = false;

	fcheck |= FChecks_VersionRequest (s);
	fcheck |= FChecks_SkinRequest (s);
	fcheck |= FChecks_CheckFModRequest (s);
	fcheck |= FChecks_CheckFServerRequest (s);
	fcheck |= FChecks_CheckFRulesetRequest (s);
	if (fcheck)
		f_reply_time = cls.realtime;
}

void FChecks_CvarInit(void)
{
	Cmd_AddCommand ("fuh_version", FChecks_VersionResponse);
	Cmd_AddCommand ("f_server", FChecks_FServerResponse);
}

void FChecks_Init(void)
{
}

