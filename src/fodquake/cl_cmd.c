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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "quakedef.h"
#include "sys_io.h"
#include "menu.h"
#include "skin.h"
#include "teamplay.h"
#include "ignore.h"
#include "version.h"
#include "netqw.h"

#include "config_manager.h"
#include "ruleset.h"
#include "server_browser.h"

#include "strl.h"

void SCR_RSShot_f (void);
void CL_ProcessServerInfo (void);
void SV_Serverinfo_f (void);
void Key_WriteBindings (FILE *f);
void S_StopAllSounds (qboolean clear);


//adds the current command line as a clc_stringcmd to the client message.
//things like kill, say, etc, are commands directed to the server,
//so when they are typed in at the console, they will need to be forwarded.
void Cmd_ForwardToServer(void)
{
	char buf[1500];
	unsigned int i;
	char *s;

	if (cls.state == ca_disconnected
#ifdef NETQW
	 || cls.netqw == 0
#endif
	)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	// lowercase command
	for (s = Cmd_Argv(0); *s; s++)
		*s = (char) tolower(*s);

#ifdef NETQW
	if (Cmd_Argc() > 1)
	{
		i = snprintf(buf, sizeof(buf), "%c%s %s", clc_stringcmd, Cmd_Argv(0), Cmd_Args());
	}
	else
	{
		i = snprintf(buf, sizeof(buf), "%c%s", clc_stringcmd, Cmd_Argv(0));
	}

	if (i < sizeof(buf))
		NetQW_AppendReliableBuffer(cls.netqw, buf, i + 1);
#else
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, Cmd_Argv(0));
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
#endif
}

// don't forward the first argument
void CL_ForwardToServer_f (void)
{
	char buf[1500];
	unsigned int i;

	if (cls.state == ca_disconnected
#ifdef NETQW
	 || cls.netqw == 0
#endif
	)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (Q_strcasecmp(Cmd_Argv(1), "snap") == 0) {
		SCR_RSShot_f ();
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	if (Cmd_Argc() > 1)
	{
#ifdef NETQW
		i = snprintf(buf, sizeof(buf), "%c%s", clc_stringcmd, Cmd_Args());
		if (i < sizeof(buf))
			NetQW_AppendReliableBuffer(cls.netqw, buf, i + 1);
#else
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
#endif
	}
}

//Handles both say and say_team
void CL_Say_f (void) {
	char buf[1500];
	unsigned int i;
	char *s;
	int tmp;
	qboolean qizmo = false;

	if (Cmd_Argc() < 2)
		return;

	if (cls.state == ca_disconnected
#ifdef NETQW
	 || cls.netqw == 0
#endif
	)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	// lowercase command
	for (s = Cmd_Argv(0); *s; s++)
		*s = (char) tolower(*s);

	if (CL_ConnectedToProxy()) {	
		for (s = Cmd_Argv(1); *s == ' '; s++)
			;
		if (!strncmp(s, ".stuff", 6) || !strncmp(s, ",stuff", 6) || strstr(s, ":stuff"))
			return;		

		qizmo = (!strncmp(s, "proxy:", 6) || s[0] == ',' || s[0] == '.');
	}

	
	if (!qizmo && cl_floodprot.value && cl_fp_messages.value > 0 && cl_fp_persecond.value > 0) {
		tmp = cl.whensaidhead - min(cl_fp_messages.value, 10) + 1;
		if (tmp < 0)
			tmp += 10;
		if (cl.whensaid[tmp] && (cls.realtime - cl.whensaid[tmp]) < (1.02 * cl_fp_persecond.value)) {
			Com_Printf("Flood Protection\n");
			return;
		}
	}
	
	s = TP_ParseMacroString (Cmd_Args());
	s = TP_ParseFunChars (s, true);

#ifdef NETQW
	if (*s && *s < 32)
	{
		i = snprintf(buf, sizeof(buf), "%c%s \"%s\"", clc_stringcmd, Cmd_Argv(0), s);
	}
	else
	{
		i = snprintf(buf, sizeof(buf), "%c%s %s", clc_stringcmd, Cmd_Argv(0), s);
	}

	if (i < sizeof(buf))
		NetQW_AppendReliableBuffer(cls.netqw, buf, i + 1);
#else
	MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
	SZ_Print(&cls.netchan.message, Cmd_Argv(0));
	SZ_Print(&cls.netchan.message, " ");
	if (*s && *s < 32)
	{
		SZ_Print(&cls.netchan.message, "\"");
		SZ_Print(&cls.netchan.message, s);
		SZ_Print(&cls.netchan.message, "\"");
	}
	else
	{
		SZ_Print(&cls.netchan.message, s);
	}
#endif
	
	if (!qizmo) {
		cl.whensaidhead++;
		if (cl.whensaidhead > 9)
			cl.whensaidhead = 0;
		cl.whensaid[cl.whensaidhead] = cls.realtime;
	}
	
}

void CL_Pause_f (void) {
	if (cls.demoplayback)
		cl.paused ^= PAUSED_DEMO;
	else
		Cmd_ForwardToServer();
}

//packet <destination> <contents>
//Contents allows \n escape character
void CL_Packet_f(void) {
	struct netaddr adr;
	char send[2048], *in, *out;

	if (Cmd_Argc() != 3) {
		Com_Printf("packet <destination> <contents>\n");
		return;
	}

	if (cbuf_current && cbuf_current != cbuf_svc && !Ruleset_AllowPacketCmd()) {
		Com_Printf("Packet commands is disabled during match\n");
		return;
	}

	if (!NET_StringToAdr(0, Cmd_Argv(1), &adr)) {
		Com_Printf("Bad address\n");
		return;
	}

	if (adr.type == NA_IPV4 && adr.addr.ipv4.port == 0)
		adr.addr.ipv4.port = BigShort(PORT_SERVER);
	else if (adr.type == NA_IPV6 && adr.addr.ipv4.port == 0)
		adr.addr.ipv6.port = BigShort(PORT_SERVER);

	send[0] = send[1] = send[2] = send[3] = 0xFF;

	in = Cmd_Argv(2);
	out = send + 4;


	while (*in && out - send < sizeof(send) - 2) {
		if (in[0] == '\\' && in[1]) {
			switch(in[1]) {
				case 'n' : *out++ = '\n'; break;
				case 't' : *out++ = '\t'; break;
				case '\\' : *out++ = '\\'; break;
				default : *out++ = in[0]; *out++ = in[1]; break;
			}
			in += 2;
		} else {
			*out++ = *in++;
		}
	}
	*out = 0;

	NET_SendPacket(NS_CLIENT, out - send, send, &adr);
}

//Send the rest of the command line over as an unconnected command.
void CL_Rcon_f (void)
{
	char message[1024] = {0};
	int i;
	struct netaddr to;
	extern cvar_t rcon_password, rcon_address;

	message[0] = 255;
	message[1] = 255;
	message[2] = 255;
	message[3] = 255;
	message[4] = 0;

	strlcat(message, "rcon ", sizeof(message));

	if (rcon_password.string[0])
	{
		strlcat(message, rcon_password.string, sizeof(message));
		strlcat(message, " ", sizeof(message));
	}

	for (i = 1; i < Cmd_Argc(); i++)
	{
		strlcat(message, Cmd_Argv(i), sizeof(message));
		strlcat(message, " ", sizeof(message));
	}

	if (cls.state >= ca_connected)
	{
#ifdef NETQW
		to = cls.server_adr;
#else
		to = cls.netchan.remote_address;
#endif
	}
	else
	{
		if (!strlen(rcon_address.string))
		{
			Com_Printf ("You must either be connected or set 'rcon_address' to issue rcon commands\n");
			return;
		}
		NET_StringToAdr(0, rcon_address.string, &to);
		if (to.type == NA_IPV4 && to.addr.ipv4.port == 0)
			to.addr.ipv4.port = BigShort (PORT_SERVER);
		else if (to.type == NA_IPV6 && to.addr.ipv6.port == 0)
			to.addr.ipv6.port = BigShort(PORT_SERVER);
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, &to);
}

void CL_Download_f (void)
{
	char buf[1500];
	unsigned int i;
	char *p, *q;

	if (cls.state == ca_disconnected
#ifdef NETQW
	 || cls.netqw == 0
#endif
	)
	{
		Com_Printf ("Must be connected.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: %s <datafile>\n", Cmd_Argv(0));
		return;
	}

	snprintf(cls.downloadname, sizeof(cls.downloadname), "%s", Cmd_Argv(1));

	p = cls.downloadname;
	while (1)
	{
		if ((q = strchr(p, '/')) != NULL)
		{
			*q = 0;
			Sys_IO_Create_Directory(cls.downloadname);
			*q = '/';
			p = q + 1;
		}
		else
			break;
	}

	strcpy(cls.downloadtempname, cls.downloadname);
	cls.downloadtype = dl_single;

#ifdef NETQW
	i = snprintf(buf, sizeof(buf), "%cdownload %s", clc_stringcmd, Cmd_Argv(1));
	if (i < sizeof(buf))
	{
		NetQW_AppendReliableBuffer(cls.netqw, buf, i + 1);
	}
#else
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, va("download %s\n",Cmd_Argv(1)));
#endif
}

void CL_User_f (void) {
	int uid, i;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <username / userid>\n", Cmd_Argv(0));
		return;
	}

	uid = atoi(Cmd_Argv(1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid	|| !strcmp(cl.players[i].name, Cmd_Argv(1)) ) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Com_Printf ("User not in server.\n");
}

void CL_Users_f (void) {
	int i, c;

	c = 0;
	Com_Printf ("userid frags name\n");
	Com_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Com_Printf ("%6i %4i %s\n", cl.players[i].userid, cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Com_Printf ("%i total users\n", c);
}

void CL_Color_f (void) {
	extern cvar_t topcolor, bottomcolor;
	int top, bottom;

	if (Cmd_Argc() == 1) {
		Com_Printf ("\"color\" is \"%s %s\"\n",
			Info_ValueForKey (cls.userinfo, "topcolor"),
			Info_ValueForKey (cls.userinfo, "bottomcolor") );
		Com_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc() == 2) {
		top = bottom = atoi(Cmd_Argv(1));
	} else {
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	top = min(top, 13);
	bottom &= 15;
	bottom = min(bottom, 13);

	Cvar_SetValue (&topcolor, top);
	Cvar_SetValue (&bottomcolor, bottom);

	cl.players[cl.playernum].real_topcolor = top;
	cl.players[cl.playernum].real_bottomcolor = bottom;

	TP_CalculateColoursForPlayer(cl.playernum);
}

//usage: fullinfo \name\unnamed\topcolor\0\bottomcolor\1, etc
void CL_FullInfo_f (void) {
	char key[512], value[512], *o, *s;

	if (Cmd_Argc() != 2) {
		Com_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s) {
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (!Q_strcasecmp(key, pmodel_name) || !Q_strcasecmp(key, emodel_name))
			continue;

		Info_SetValueForKey (cls.userinfo, key, value, MAX_INFO_STRING);
	}
}

//Allow clients to change userinfo
void CL_SetInfo_f (void) {
	if (Cmd_Argc() == 1) {
		Info_Print (cls.userinfo);
		return;
	}
	if (Cmd_Argc() != 3) {
		Com_Printf ("Usage: %s [ <key> <value> ]\n", Cmd_Argv(0));
		return;
	}
	if (!Q_strcasecmp(Cmd_Argv(1), pmodel_name) || !strcmp(Cmd_Argv(1), emodel_name))
		return;

	Info_SetValueForKey (cls.userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
	if (cls.state >= ca_connected)
		Cmd_ForwardToServer ();
}


void CL_UserInfo_f (void) {
	if (Cmd_Argc() != 1) {
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		return;
	}
	Info_Print (cls.userinfo);
}

void SV_Quit_f (void);

void CL_Quit_f (void) {
	extern cvar_t cl_confirmquit;

#ifndef CLIENTONLY
	if (dedicated)
		SV_Quit_f ();
	else
#endif
	{
		if (cl_confirmquit.value)
			M_Menu_Quit_f ();
		else
			Host_Quit ();
	}
}

void CL_Serverinfo_f (void) {
#ifndef CLIENTONLY
	if (cls.state < ca_connected || com_serveractive) {
		SV_Serverinfo_f();
		return;
	}
#endif

	if (cls.state >= ca_onserver && cl.serverinfo)
		Info_Print (cl.serverinfo);
	else		
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
}


//============================================================================

void CL_WriteConfig (char *name) {
	FILE *f;

	if (!(f = fopen (va("%s/%s", cls.gamedir, name), "w"))) {
		Com_Printf ("Couldn't write %s.\n", name);
		return;
	}

	fprintf (f, "// Generated by Fodquake\n");
	fprintf (f, "\n// Key bindings\n");
	Key_WriteBindings (f);
	fprintf (f, "\n// Variables\n");
	Cvar_WriteVariables (f);
	fprintf (f, "\n// Aliases\n");
	Cmd_WriteAliases (f);

	fclose (f);
}

//Writes key bindings and archived cvars to config.cfg
void CL_WriteConfiguration (void) {
	if (host_initialized && cfg_legacy_write.value)
		CL_WriteConfig ("config.cfg");
}

//Writes key bindings and archived cvars to a custom config file
void CL_WriteConfig_f (void) {
	char name[MAX_OSPATH];

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <filename>\n", Cmd_Argv(0));
		return;
	}

	Q_strncpyz (name, Cmd_Argv(1), sizeof(name));
	COM_ForceExtension (name, ".cfg");

	Com_Printf ("Writing %s\n", name);

	CL_WriteConfig (name);
}

void Skin_Skins_f(void)
{
	char buf[128];
	int i;

	if (cls.state == ca_onserver && cbuf_current != cbuf_main)	//only download when connecting
	{
#ifdef NETQW
		if (cls.netqw)
		{
			i = snprintf(buf, sizeof(buf), "%cbegin %i", clc_stringcmd, cl.servercount);
			if (i < sizeof(buf))
				NetQW_AppendReliableBuffer(cls.netqw, buf, i + 1);
		}
#else
		MSG_WriteByte(&cls.netchan.message, clc_stringcmd);
		MSG_WriteString(&cls.netchan.message, va("begin %i", cl.servercount));
#endif
	}
	else
	{
		Skin_FreeAll();
	}
}

void CL_InitCommands (void) {
	// general commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("download", CL_Download_f);
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("quit", CL_Quit_f);
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("say", CL_Say_f);
	Cmd_AddCommand ("say_team", CL_Say_f);
	Cmd_AddCommand ("serverinfo", CL_Serverinfo_f);
	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("user", CL_User_f);
	Cmd_AddCommand ("users", CL_Users_f);
	Cmd_AddCommand ("version", CL_Version_f);
	Cmd_AddCommand ("writeconfig", CL_WriteConfig_f);

	// client info setting
	Cmd_AddCommand ("color", CL_Color_f);
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f);
	Cmd_AddCommand ("setinfo", CL_SetInfo_f);
	Cmd_AddCommand ("userinfo", CL_UserInfo_f);

	// forward to server commands
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("fly", NULL);
}

/*
==============================================================================
SERVER COMMANDS

Server commands are commands stuffed by server into client's cbuf
We use a separate command buffer for them -- there are several
reasons for that:
1. So that partially stuffed commands are always executed properly
2. Not to let players cheat in TF (v_cshift etc don't work in console)
3. To hide some commands the user doesn't need to know about, like
changing, fullserverinfo, nextul, stopul
==============================================================================
*/

//Just sent as a hint to the client that they should drop to full console
void CL_Changing_f (void) {
	cl.intermission = 0;

	if (cls.download)  // don't change when downloading
		return;

	S_StopAllSounds (true);
	cls.state = ca_connected;	// not active anymore, but not disconnected

	Ignore_PreNewMap();

#ifdef NETQW
	if (cls.netqw)
		NetQW_SetDeltaPoint(cls.netqw, -1);
#endif

	Com_Printf ("\nChanging map...\n");
}

//Sent by server when serverinfo changes
void CL_FullServerinfo_f (void) {
	char *p;

	if (Cmd_Argc() != 2)
		return;

	Q_strncpyz (cl.serverinfo, Cmd_Argv(1), sizeof(cl.serverinfo));

	p = Info_ValueForKey (cl.serverinfo, "*cheats");
	if (*p)
		Com_Printf ("== Cheats are enabled ==\n");

	if (!cls.demoplayback)
	{
		p = Info_ValueForKey (cl.serverinfo, "*version");
		if (Q_strcasecmp(p, "mvdsv 0.26") == 0)
			Com_Printf ("== BROKEN SERVER ==\nDownloads will not work! Please report download bugs to the MVDSV project.\n");
		else if (Q_strncasecmp(p, "mvdsv", 5) == 0)
			Com_Printf ("== BROKEN SERVER ==\nDownloads will probably not work! Please report problems with downloading to the MVDSV project.\n");
	}

	CL_ProcessServerInfo ();
}

void CL_Fov_f (void) {
	extern cvar_t scr_fov, default_fov;

	if (Cmd_Argc() == 1) {
		Com_Printf ("\"fov\" is \"%s\"\n", scr_fov.string);
		return;
	}

	if (Q_atof(Cmd_Argv(1)) == 90.0 && default_fov.value)
		Cvar_SetValue (&scr_fov, default_fov.value);
	else
		Cvar_Set (&scr_fov, Cmd_Argv(1));
}

void CL_R_DrawViewModel_f (void) {
	extern cvar_t cl_filterdrawviewmodel;

	if (cl_filterdrawviewmodel.value)
		return;
	Cvar_Command ();
}

void InterceptServerRateSet()
{
	if (Cmd_Argc() == 2)
	{
		CL_UserinfoChanged("rate", Cmd_Argv(1));
	}
}

typedef struct {
	char	*name;
	void	(*func) (void);
} svcmd_t;

svcmd_t svcmds[] = {
	{"changing", CL_Changing_f},
	{"fullserverinfo", CL_FullServerinfo_f},
	{"nextul", CL_NextUpload},
	{"stopul", CL_StopUpload},
	{"fov", CL_Fov_f},
	{"r_drawviewmodel", CL_R_DrawViewModel_f},
	{"rate", InterceptServerRateSet},
	{NULL, NULL}
};

//Called by Cmd_ExecuteString if cbuf_current == cbuf_svc
qboolean CL_CheckServerCommand ()
{
	svcmd_t	*cmd;
	char *s;

	s = Cmd_Argv (0);

	for (cmd = svcmds; cmd->name; cmd++)
	{
		if (!strcmp (s, cmd->name) )
		{
			if (cmd->func)
				cmd->func();

			return true;
		}
	}

	return false;
}

