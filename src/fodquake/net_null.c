/*
Copyright (C) 2008-2009 Mark Olsen

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

#include "quakedef.h"
#include "net.h"
#include "sys_net.h"

struct SysNetData *Sys_Net_Init()
{
	return 0;
}

void Sys_Net_Shutdown(struct SysNetData *netdata)
{
}

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address)
{
	return false;
}

qboolean Sys_Net_ResolveAddress(struct SysNetData *netdata, const struct netaddr *address, char *output, unsigned int outputsize)
{
	return false;
}

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype)
{
	return 0;
}

void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket)
{
}

qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port)
{
	return false;
}

int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address)
{
	return -1;
}

int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address)
{
	return -1;
}

void Sys_Net_Wait(struct SysNetData *netdata, struct SysSocket *socket, unsigned int timeout_us)
{
}

