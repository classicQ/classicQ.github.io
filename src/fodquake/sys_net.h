#ifndef SYS_NET_H
#define SYS_NET_H

#include "qtypes.h"

enum netaddrtype
{
	NA_LOOPBACK,  /* Only used internally */
	NA_IPV4,
	NA_IPV6,
	NA_NUMTYPES   /* Not a real type */
};

struct netaddr_ipv4
{
	unsigned char address[4];
	unsigned short port;
};

struct netaddr_ipv6
{
	unsigned char address[16];
	unsigned short port;
};

struct netaddr
{
	enum netaddrtype type;

	union
	{
		struct netaddr_ipv4 ipv4;
		struct netaddr_ipv6 ipv6;
	} addr;
};

struct SysNetData;
struct SysSocket;

struct SysNetData *Sys_Net_Init(void);
void Sys_Net_Shutdown(struct SysNetData *netdata);

qboolean Sys_Net_ResolveName(struct SysNetData *netdata, const char *name, struct netaddr *address);
qboolean Sys_Net_ResolveAddress(struct SysNetData *netdata, const struct netaddr *address, char *output, unsigned int outputsize);

struct SysSocket *Sys_Net_CreateSocket(struct SysNetData *netdata, enum netaddrtype addrtype);
void Sys_Net_DeleteSocket(struct SysNetData *netdata, struct SysSocket *socket);
qboolean Sys_Net_Bind(struct SysNetData *netdata, struct SysSocket *socket, unsigned short port);
int Sys_Net_Send(struct SysNetData *netdata, struct SysSocket *socket, const void *data, int datalen, const struct netaddr *address);
int Sys_Net_Receive(struct SysNetData *netdata, struct SysSocket *socket, void *data, int datalen, struct netaddr *address);
void Sys_Net_Wait(struct SysNetData *netdata, struct SysSocket *socket, unsigned int timeout_us);

#endif

