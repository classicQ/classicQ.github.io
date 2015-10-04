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

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#ifdef __MORPHOS__
#include <sys/filio.h>
#elif defined(AROS)
#include <sys/ioctl.h>
#endif

#include <devices/timer.h>

#include <proto/exec.h>
#include <proto/socket.h>

#include <string.h>

#include "quakedef.h"
#include "sys_net.h"

struct SysSocket
{
	int s;
};

struct SysNetData
{
	struct Library *SocketBase;
	struct MsgPort *timerport;
	struct timerequest *timerrequest;
};

#define SocketBase netdata->SocketBase

struct SysNetData *Sys_Net_Init()
{
	struct SysNetData *netdata;

	netdata = AllocVec(sizeof(*netdata), MEMF_ANY);
	if (netdata)
	{
		SocketBase = OpenLibrary("bsdsocket.library", 0);
		if (SocketBase)
		{
			netdata->timerport = CreateMsgPort();
			if (netdata->timerport)
			{
				netdata->timerrequest = (struct timerequest *)CreateIORequest(netdata->timerport, sizeof(*netdata->timerrequest));
				if (netdata->timerrequest)
				{
					if (OpenDevice(TIMERNAME, UNIT_MICROHZ, (struct IORequest *)netdata->timerrequest, 0) == 0)
					{
						netdata->timerrequest->tr_node.io_Command = TR_ADDREQUEST;
						netdata->timerrequest->tr_time.tv_secs = 1;
						netdata->timerrequest->tr_time.tv_micro = 0;
						SendIO((struct IORequest *)netdata->timerrequest);
						AbortIO((struct IORequest *)netdata->timerrequest);

						return netdata;
					}

					DeleteIORequest((struct IORequest *)netdata->timerrequest);
				}

				DeleteMsgPort(netdata->timerport);
			}

			CloseLibrary(SocketBase);
		}

		FreeVec(netdata);
	}

	return 0;
}

void Sys_Net_Shutdown(struct SysNetData *netdata)
{
	WaitIO((struct IORequest *)netdata->timerrequest);
	CloseDevice((struct IORequest *)netdata->timerrequest);
	DeleteIORequest((struct IORequest *)netdata->timerrequest);
	DeleteMsgPort(netdata->timerport);
	CloseLibrary(SocketBase);
	FreeVec(netdata);
}

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address)
{
	struct hostent *remote;

	remote = gethostbyname(name);
	if (remote)
	{
		address->type = NA_IPV4;
		*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)remote->h_addr;

		return true;
	}

	return false;
}

qboolean Sys_Net_ResolveAddress(struct SysNetData *netdata, const struct netaddr *address, char *output, unsigned int outputsize)
{
	struct hostent *remote;
	struct in_addr addr;

	if (address->type != NA_IPV4)
		return false;

	addr.s_addr = (address->addr.ipv4.address[0]<<24)|(address->addr.ipv4.address[1]<<16)|(address->addr.ipv4.address[2]<<8)|address->addr.ipv4.address[3];

	remote = gethostbyaddr((void *)&addr, sizeof(addr), AF_INET);
	if (remote)
	{
		strlcpy(output, remote->h_name, outputsize);

		return true;
	}

	return false;
}

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype)
{
	struct SysSocket *s;
	int r;
	int one;

	one = 1;

	if (addrtype != NA_IPV4)
		return 0;

	s = AllocVec(sizeof(*s), MEMF_ANY);
	if (s)
	{
		s->s = socket(AF_INET, SOCK_DGRAM, 0);
		if (s->s != -1)
		{
			r = IoctlSocket(s->s, FIONBIO, (void *)&one);
			if (r == 0)
			{
				return s;
			}

			CloseSocket(s->s);
		}

		FreeVec(s);
	}

	return 0;
}

void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket)
{
	CloseSocket(socket->s);
	FreeVec(socket);
}

qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port)
{
	int r;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	*(unsigned int *)&addr.sin_addr.s_addr = 0;

	r = bind(socket->s, (struct sockaddr *)&addr, sizeof(addr));
	if (r == 0)
		return true;

	return false;
}

int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address)
{
	int r;

	if (address)
	{
		struct sockaddr_in addr;

		addr.sin_family = AF_INET;
		addr.sin_port = htons(address->addr.ipv4.port);
		*(unsigned int *)&addr.sin_addr.s_addr = *(unsigned int *)address->addr.ipv4.address;

		r = sendto(socket->s, data, datalen, 0, (struct sockaddr *)&addr, sizeof(addr));
	}
	else
		r = send(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (Errno() == EWOULDBLOCK)
			return 0;
	}

	return r;
}

int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address)
{
	int r;

	if (address)
	{
		LONG fromlen;
		struct sockaddr_in addr;

		fromlen = sizeof(addr);

		r = recvfrom(socket->s, data, datalen, 0, (struct sockaddr *)&addr, &fromlen);

		if (fromlen != sizeof(addr))
			return -1;

		address->type = NA_IPV4;
		address->addr.ipv4.port = htons(addr.sin_port);
		*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)&addr.sin_addr.s_addr;
	}
	else
		r = recv(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (Errno() == EWOULDBLOCK)
			return 0;
	}

	return r;
}

void Sys_Net_Wait(struct SysNetData *netdata, struct SysSocket *socket, unsigned int timeout_us)
{
	fd_set rfds;
	ULONG sigmask;

	WaitIO((struct IORequest *)netdata->timerrequest);

	if (SetSignal(0, 0) & (1<<netdata->timerport->mp_SigBit))
		Wait(1<<netdata->timerport->mp_SigBit);

	FD_ZERO(&rfds);
	FD_SET(socket->s, &rfds);

	netdata->timerrequest->tr_node.io_Command = TR_ADDREQUEST;
	netdata->timerrequest->tr_time.tv_secs = timeout_us / 1000000;
	netdata->timerrequest->tr_time.tv_micro = timeout_us % 1000000;

	SendIO((struct IORequest *)netdata->timerrequest);

	sigmask = 1<<netdata->timerport->mp_SigBit;

	WaitSelect(socket->s + 1, &rfds, 0, 0, 0, &sigmask);

	AbortIO((struct IORequest *)netdata->timerrequest);
}

