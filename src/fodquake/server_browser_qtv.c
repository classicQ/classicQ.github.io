/*
Copyright (C) 2010 Jürgen Legler
Copyright (C) 2010 Mark Olsen

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

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "sys_thread.h"
#include "sys_net.h"
#include "server_browser_qtv.h"

enum QTVR_Status
{
	QTVR_WAITING_FOR_REPLY,
	QTVR_REPLY_RECIEVED,
	QTVR_ERROR
};

struct qtvr
{
	struct SysThread *thread;
	struct SysMutex *mutex;

	enum QTVR_Status status;

	char *retval;
	char *request;
	char *server;

	volatile int quit;
};

char *QTVR_Get_Retval(struct qtvr *qtvr)
{
	char *ret;

	ret = 0;

	Sys_Thread_LockMutex(qtvr->mutex);
	if (qtvr->status == QTVR_REPLY_RECIEVED)
	{
		ret = qtvr->retval;
	}
	Sys_Thread_UnlockMutex(qtvr->mutex);

	return ret;
}

int QTVR_Waiting(struct qtvr *qtvr)
{
	return qtvr->status == QTVR_WAITING_FOR_REPLY;
}

static void QTVR_Thread(void *arg)
{
	struct qtvr *qtvr;
	int r;
	char buf[128];
	struct netaddr serveraddr;
	struct netaddr addr;

	struct SysNetData *netdata;
	struct SysSocket *socket;

	qtvr = arg;

	netdata = Sys_Net_Init();

	if (netdata == NULL)
	{
		Com_Printf("QTVR_Thread: Sys_Net_Init failed.\n");
	}
	else
	{
		if (!NET_StringToAdr(netdata, qtvr->server, &serveraddr))
		{
			Com_Printf("QTVR_Thread: Unable to parse/resolve hoststring.\n");
		}
		else
		{
			socket = Sys_Net_CreateSocket(netdata, serveraddr.type);

			if (socket == NULL)
			{
				Com_Printf("QTVR_Create: could not create socket.\n");
			}
			else
			{
				Sys_Net_Send(netdata, socket, qtvr->request, strlen(qtvr->request), &serveraddr);

				while (!qtvr->quit)
				{
					Sys_Net_Wait(netdata, socket, 50000);

					if ((r = Sys_Net_Receive(netdata, socket, buf, sizeof(buf) - 1, &addr)) > 0)
					{
						buf[r] = 0;

						Sys_Thread_LockMutex(qtvr->mutex);
						qtvr->retval = strdup(buf);
						if (qtvr->retval == NULL)
							qtvr->status = QTVR_ERROR;
						else
							qtvr->status = QTVR_REPLY_RECIEVED;
						Sys_Thread_UnlockMutex(qtvr->mutex);

						break;
					}
				}

				Sys_Net_DeleteSocket(netdata, socket);
			}
		}

		Sys_Net_Shutdown(netdata);
	}

	if (qtvr->status != QTVR_REPLY_RECIEVED)
		qtvr->status = QTVR_ERROR;
}

struct qtvr *QTVR_Create(const char *server, const char *request)
{
	struct qtvr *qtvr;

	if (server == 0 || request == 0)
		return 0;

	qtvr = calloc(1, sizeof(*qtvr));
	if (qtvr == NULL)
	{
		Com_Printf("QTVR_Create: could not allocate qtvr.\n");
	}
	else
	{
		qtvr->server = strdup(server);
		qtvr->request = strdup(request);
		if (qtvr->server == NULL || qtvr->request == NULL)
		{
			Com_Printf("QTVR_Create: could not strdup server.\n");
		}
		else
		{
			qtvr->mutex = Sys_Thread_CreateMutex();
			if (qtvr->mutex == NULL)
			{
				Com_Printf("QTVR_Create: could not create mutex.\n");
			}
			else
			{
				qtvr->status = QTVR_WAITING_FOR_REPLY;

				qtvr->thread = Sys_Thread_CreateThread(QTVR_Thread, qtvr);
				if (qtvr->thread == NULL)
				{
					Com_Printf("QTVR_Create: could not create thread.\n");
				}
				else
				{
					return qtvr;
				}

				Sys_Thread_DeleteMutex(qtvr->mutex);
			}
		}

		free(qtvr->server);
		free(qtvr->request);

		free(qtvr);
	}

	return NULL;
}

void QTVR_Destroy(struct qtvr *qtvr)
{
	qtvr->quit = 1;
	Sys_Thread_DeleteThread(qtvr->thread);
	Sys_Thread_DeleteMutex(qtvr->mutex);
	free(qtvr->retval);
	free(qtvr->request);
	free(qtvr->server);
	free(qtvr);
}
