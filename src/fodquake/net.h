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
// net.h -- quake's interface to the networking layer

#ifndef __NET_H__
#define __NET_H__

#include "common.h"
#include "sys_net.h"

struct SysNetData;

#define	PORT_ANY	-1

enum netsrc
{
	NS_CLIENT,
	NS_SERVER
};

qboolean NET_OpenSocket(enum netsrc socknum, enum netaddrtype type);

void		NET_Init (void);
void		NET_Shutdown (void);
void		NET_ServerConfig (qboolean enable);	// open/close server socket

void		NET_ClearLoopback (void);
qboolean	NET_GetPacket(enum netsrc sock, sizebuf_t *message, struct netaddr *from);
void		NET_SendPacket(enum netsrc sock, int length, void *data, const struct netaddr *to);
void		NET_Sleep (int msec);

qboolean	NET_CompareAdr(const struct netaddr *a, const struct netaddr *b);
qboolean	NET_CompareBaseAdr(const struct netaddr *a, const struct netaddr *b);
qboolean	NET_IsLocalAddress(const struct netaddr *a);
char		*NET_AdrToString(const struct netaddr *a);
char		*NET_BaseAdrToString(const struct netaddr *a);
qboolean	NET_StringToAdr(struct SysNetData *sysnetdata, const char *s, struct netaddr *a);
char *NET_GetHostnameForAddress(const struct netaddr *addr);

#endif

