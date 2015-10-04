#include <winsock2.h>

#warning Fix this up.

struct ip6_scope_id
{
	union
	{
		struct
		{
			u_long  Zone : 28;
			u_long  Level : 4;
		};
		u_long  Value;
	};
};
struct in_addr6
{
	u_char	s6_addr[16];	/* IPv6 address */
};
typedef struct sockaddr_in6 {
	short  sin6_family;
	u_short  sin6_port;
	u_long  sin6_flowinfo;
	struct in_addr6  sin6_addr;
	union
	{
		u_long  sin6_scope_id;
		struct ip6_scope_id  sin6_scope_struct; 
	};
};

#include "quakedef.h"
#include "net.h"
#include "sys_net.h"

typedef unsigned int socklen_t;

struct addrinfo
{
  int ai_flags;
  int ai_family;
  int ai_socktype;
  int ai_protocol;
  size_t ai_addrlen;
  char* ai_canonname;
  struct sockaddr * ai_addr;
  struct addrinfo * ai_next;
};

struct SysNetData
{
	HANDLE dll_ws2_32;
	int (WSAAPI *pgetaddrinfo)(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res);
	int (WSAAPI *pfreeaddrinfo)(struct addrinfo *tofree);
};

struct SysSocket
{
	int s;
	int domain;
};


struct SysNetData *Winsock_Init()
{
	struct SysNetData *sdata;
	WSADATA winsockdata;
	WORD wVersionRequested;
	int r;

	wVersionRequested = MAKEWORD(1, 1);
	r = WSAStartup(wVersionRequested, &winsockdata);
	if (!r)	/* success == 0 */
	{
		sdata = malloc(sizeof(*sdata));

		sdata->dll_ws2_32 = LoadLibrary("ws2_32.dll");
		if (sdata->dll_ws2_32)
		{
			sdata->pgetaddrinfo = (void*)GetProcAddress(sdata->dll_ws2_32, "getaddrinfo");
			sdata->pfreeaddrinfo = (void*)GetProcAddress(sdata->dll_ws2_32, "freeaddrinfo");
		}
		return sdata;
	}

	return 0;
}

void Winsock_Shutdown(struct SysNetData *netdata)
{
	WSACleanup();
	FreeLibrary(netdata->dll_ws2_32);
	free(netdata);
}




struct SysNetData *Sys_Net_Init()
{
	return Winsock_Init();
}

void Sys_Net_Shutdown(struct SysNetData *netdata)
{
	Winsock_Shutdown(netdata);
}

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address)
{
	int r;
	struct addrinfo *addr;
	struct addrinfo *origaddr;
	qboolean ret;

	ret = false;
	if (netdata->pgetaddrinfo)
	{
		r = netdata->pgetaddrinfo(name, 0, 0, &origaddr);
		if (r == 0)
		{
			addr = origaddr;
			while(addr)
			{
				if (addr->ai_family == AF_INET)
				{
					address->type = NA_IPV4;
					*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)&((struct sockaddr_in *)addr->ai_addr)->sin_addr.s_addr;
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

			netdata->pfreeaddrinfo(origaddr);
		}
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
		*(unsigned int *)&addr.addr.sin_addr.s_addr = *(unsigned int *)address->addr.ipv4.address;
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

//	r = getnameinfo((struct sockaddr *)&addr, addrsize, output, outputsize, 0, 0, NI_NAMEREQD);
//	if (r == 0)
//		return true;
	
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
			r = ioctlsocket(s->s, FIONBIO, &one);
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
	closesocket(socket->s);
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
			*(unsigned int *)&addr.addr.sin_addr.s_addr = *(unsigned int *)address->addr.ipv4.address;
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

		r = sendto(socket->s, data, datalen, 0, (struct sockaddr *)&addr, addrsize);
	}
	else
		r = send(socket->s, data, datalen, 0);

	if (r == -1)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
			return 0;
	}

	return r;
}

int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address)
{
	int r;
	int err;

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
				*(unsigned int *)address->addr.ipv4.address = *(unsigned int *)&addr.addr.sin_addr.s_addr;
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
		err = WSAGetLastError();

		if (err == WSAEWOULDBLOCK)
			return 0;
		else if (err == WSAEINVAL) /* Windows returns this if you try to read from a socket before you write to it */
			return 0;
	}

	return r;
}

void Sys_Net_Wait(struct SysNetData *netdata, struct SysSocket *socket, unsigned int timeout_us)
{
	struct timeval tv;
	fd_set rfds;

	FD_ZERO(&rfds);
	FD_SET(socket->s, &rfds);

	tv.tv_sec = timeout_us / 1000000;
	tv.tv_usec = timeout_us % 1000000;

	select(socket->s + 1, &rfds, 0, 0, &tv);
}


