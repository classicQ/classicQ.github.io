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

#ifdef linux
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

#ifdef linux
#include <poll.h>
#endif

#include "sys_net.h"

struct SysSocket
{
	int s;
	int domain;
};

struct SysNetData *Sys_Net_Init()
{
	return (struct SysNetData *)-1;
}

void Sys_Net_Shutdown(struct SysNetData *netdata)
{
}

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address)
{
	int r;
	struct addrinfo *addr;
	struct addrinfo *origaddr;
	qboolean ret;

	ret = false;

	r = getaddrinfo(name, 0, 0, &origaddr);
	if (r == 0)
	{
		addr = origaddr;
		while(addr)
		{
			if (addr->ai_family == AF_INET)
			{
				address->type = NA_IPV4;
				memcpy(address->addr.ipv4.address, &((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr, 4);
				ret = true;
				break;
			}
			if (addr->ai_family == AF_INET6)
			{
				address->type = NA_IPV6;
				memcpy(address->addr.ipv6.address, &((struct sockaddr_in6 *)addr->ai_addr)->sin6_addr, sizeof(address->addr.ipv6.address));
				ret = true;
				break;
			}

			addr = addr->ai_next;
		}

		freeaddrinfo(origaddr);
	}

	return ret;
}

qboolean Sys_Net_ResolveAddress(struct SysNetData *netdata, const struct netaddr *address, char *output, unsigned int outputsize)
{
	int r;
	socklen_t addrsize;
	union
	{
		struct sockaddr_in addr;
		struct sockaddr_in6 addr6;
	} addr;

	if (address->type == NA_IPV4)
	{
		addr.addr.sin_family = AF_INET;
		addr.addr.sin_port = htons(address->addr.ipv4.port);
		memcpy(&addr.addr.sin_addr.s_addr, address->addr.ipv4.address, 4);
		addrsize = sizeof(addr.addr);
	}
	else if (address->type == NA_IPV6)
	{
		addr.addr6.sin6_family = AF_INET6;
		addr.addr6.sin6_port = htons(address->addr.ipv6.port);
		addr.addr6.sin6_flowinfo = 0;
		memcpy(&addr.addr6.sin6_addr, address->addr.ipv6.address, sizeof(addr.addr6.sin6_addr));
		addr.addr6.sin6_scope_id = 0;
		addrsize = sizeof(addr.addr6);
	}
	else
		return false;

	r = getnameinfo((struct sockaddr *)&addr, addrsize, output, outputsize, 0, 0, NI_NAMEREQD);
	if (r == 0)
		return true;
	
	return false;
}

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype)
{
	struct SysSocket *s;
	int domain;
	int r;
	int one;

	one = 1;

	if (addrtype == NA_IPV4)
		domain = AF_INET;
	else if (addrtype == NA_IPV6)
		domain = AF_INET6;
	else
		return 0;

	s = malloc(sizeof(*s));
	if (s)
	{
		s->s = socket(domain, SOCK_DGRAM, 0);
		if (s->s != -1)
		{
			r = ioctl(s->s, FIONBIO, &one);
			if (r == 0)
			{
				s->domain = domain;

				return s;
			}
		}

		free(s);
	}

	return 0;
}

void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket)
{
	close(socket->s);
	free(socket);
}

qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port)
{
	int r;
	socklen_t addrsize;
	union
	{
		struct sockaddr_in addr;
		struct sockaddr_in6 addr6;
	} addr;

	if (socket->domain == AF_INET)
	{
		addr.addr.sin_family = AF_INET;
		addr.addr.sin_port = htons(port);
		*(unsigned int *)&addr.addr.sin_addr.s_addr = 0;
		addrsize = sizeof(addr.addr);
	}
	else if (socket->domain == AF_INET6)
	{
		addr.addr6.sin6_family = AF_INET6;
		addr.addr6.sin6_port = htons(port);
		addr.addr6.sin6_flowinfo = 0;
		memset(&addr.addr6.sin6_addr, 0, sizeof(addr.addr6.sin6_addr));
		addr.addr6.sin6_scope_id = 0;
		addrsize = sizeof(addr.addr6);
	}
	else
		return false;

	r = bind(socket->s, (struct sockaddr *)&addr, addrsize);
	if (r == 0)
		return true;

	return false;
}

int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address)
{
	int r;

	if (address)
	{
		unsigned int addrsize;
		union
		{
			struct sockaddr_in addr;
			struct sockaddr_in6 addr6;
		} addr;

		if (socket->domain == AF_INET)
		{
			addr.addr.sin_family = AF_INET;
			addr.addr.sin_port = htons(address->addr.ipv4.port);
			memcpy(&addr.addr.sin_addr.s_addr, address->addr.ipv4.address, 4);
			addrsize = sizeof(addr.addr);
		}
		else if (socket->domain == AF_INET6)
		{
			addr.addr6.sin6_family = AF_INET6;
			addr.addr6.sin6_port = htons(address->addr.ipv6.port);
			memcpy(&addr.addr6.sin6_addr, address->addr.ipv6.address, sizeof(addr.addr6.sin6_addr));
			addr.addr6.sin6_flowinfo = 0;
			addr.addr6.sin6_scope_id = 0;
			addrsize = sizeof(addr.addr6);
		}
		else
			return -1;

		r = sendto(socket->s, data, datalen, 0, (struct sockaddr *)&addr, addrsize);
	}
	else
		r = send(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (errno == EWOULDBLOCK)
			return 0;
	}

	return r;
}

int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address)
{
	int r;

	if (address)
	{
		socklen_t fromlen;
		union
		{
			struct sockaddr_in addr;
			struct sockaddr_in6 addr6;
		} addr;

		if (socket->domain == AF_INET)
			fromlen = sizeof(addr.addr);
		else if (socket->domain == AF_INET6)
			fromlen = sizeof(addr.addr6);

		r = recvfrom(socket->s, data, datalen, 0, (struct sockaddr *)&addr, &fromlen);

		if (r >= 0)
		{
			if (socket->domain == AF_INET)
			{
				if (fromlen != sizeof(addr.addr))
					return -1;

				address->type = NA_IPV4;
				address->addr.ipv4.port = htons(addr.addr.sin_port);
				memcpy(address->addr.ipv4.address, &addr.addr.sin_addr.s_addr, 4);
			}
			else if (socket->domain == AF_INET6)
			{
				if (fromlen != sizeof(addr.addr6))
					return -1;

				address->type = NA_IPV6;
				address->addr.ipv6.port = htons(addr.addr6.sin6_port);
				memcpy(address->addr.ipv6.address, &addr.addr6.sin6_addr, sizeof(address->addr.ipv6.address));
			}
		}
	}
	else
		r = recv(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (errno == EWOULDBLOCK)
			return 0;
	}

	return r;
}

void Sys_Net_Wait(struct SysNetData *netdata, struct SysSocket *socket, unsigned int timeout_us)
{
#ifndef linux
	/* On Linux, select() rounds up the sleeping time to the nearest X ms
	 * even if its <sarcasm>hyperadvanced</sarcasm> NO_HZ option is set, so
	 * this won't be useful. However, on such configurations, usleep()
	 * works as advertised, so see below. */

	struct timeval tv;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(socket->s, &rfds);

	tv.tv_sec = timeout_us / 1000000;
	tv.tv_usec = timeout_us % 1000000;

	select(socket->s + 1, &rfds, 0, 0, &tv);
#else
#if 0
	/* Here we trade off socket receive accuracy for sleep accuracy. */

	struct timeval stv;
	struct timeval tv;
	fd_set rfds;
	unsigned long long x;
	int r;

	gettimeofday(&stv, 0);

	while(1)
	{
		gettimeofday(&tv, 0);

		x = (tv.tv_sec * 1000000 + tv.tv_usec) - (stv.tv_sec * 1000000 + stv.tv_usec);
		if (x >= timeout_us)
			break;

		FD_ZERO(&rfds);
		FD_SET(socket->s, &rfds);

		tv.tv_sec = 0;
		tv.tv_usec = 0;

		r = select(socket->s + 1, &rfds, 0, 0, &tv);
		if (r > 0)
			break;

		x -= timeout_us;

		if (x > 2500)
			x = 2500;

		usleep(x);
	}
#else
	struct pollfd pfd;
	struct timespec ts;

	pfd.fd = socket->s;
	pfd.events = POLLIN;
	ts.tv_sec = timeout_us/1000000;
	ts.tv_nsec = (timeout_us%1000000)*1000;
	ppoll(&pfd, 1, &ts, 0);
#endif
#endif
}

