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

#include <stdlib.h>
#include <string.h>

#include "sys_thread.h"
#include "sys_net.h"
#include "mouse.h"
#include "net.h"
#include "huffman.h"
#include "protocol.h"
#include "netqw.h"

#define PLPACKETHISTORYCOUNT 200

#warning TODO: Make this a cvar.
#define CLAMPTOINTEGERMS 1

#warning TODO: Network error checking.
#warning TODO: Add network timeout.

static const usercmd_t zerocmd;

enum state
{
	state_uninitialised,
	state_sendchallenge,
	state_sendconnection,
	state_connected
};

struct ReliableBuffer
{
	struct ReliableBuffer *next;
	unsigned int length;

	/* data follows :) */
};

struct NetPacket
{
	struct NetPacket *clientnext;
	struct NetPacket *internalnext;
	unsigned long long delayuntil;
	unsigned int length;
	unsigned int usecount;

	/* data follows :) */
};

struct SendPacket
{
	/* Like above, but for sent packets */
	struct SendPacket *next;
	unsigned long long delayuntil;
	unsigned int length;

	/* Data follows */
};

struct NetQW
{
#warning Not really used for anything.
	int error;
	unsigned short qport;
	int challenge;
	unsigned long long resendtime;
	unsigned char packetloss;
	char *hoststring;
	char *userinfo;
	struct netaddr addr;
	struct SysThread *thread;
	struct SysMutex *mutex;
	struct SysNetData *netdata;
	struct SysSocket *socket;

	/* QW connection state info */
	unsigned int current_outgoing_sequence_number;
	unsigned int last_incoming_sequence_number;

	unsigned int reliable_buffers_sent;
	unsigned int reliable_sequence_number;

	unsigned int outgoing_reliable_xor;
	unsigned int incoming_reliable_xor;

	unsigned char replyreceived[PLPACKETHISTORYCOUNT];

	struct HuffContext *huffcontext;
	unsigned int huffcrc;

	struct NetPacket *internalnetpackethead;
	struct NetPacket *internalnetpackettail;
	struct SendPacket *sendpackethead;
	struct SendPacket *sendpackettail;

	/* Shared between threads */
	volatile int quit;
	enum state state;
	unsigned int microsecondsperframe;
	unsigned long long lastmovesendtime;
	unsigned long long movetimecounter;
	int send_tmove;
	int movement_locked;
	unsigned char tmove_buffer[6];
	float forwardspeed;
	float sidespeed;
	float upspeed;
	unsigned char oncebuttons;
	unsigned char currentbuttons;
	unsigned char priorityimpulse;
	unsigned char normalimpulse;
	unsigned int lag;
	int lag_ezcheat;
	struct ReliableBuffer *reliablebufferhead;
	struct ReliableBuffer *reliablebuffertail;
	struct NetPacket *clientnetpackethead;
	struct NetPacket *clientnetpackettail;
	unsigned long long lastserverpackettime;

	struct
	{
		usercmd_t cmd;
		double senttime;
		unsigned int delta_sequence;
	} frames[UPDATE_BACKUP];
	unsigned int lastsentframe;
	unsigned int framestosend;
	unsigned int lastcopiedframe;
	unsigned int framestocopy;
	int last_received_entity_update_frame;
};

/*****/

static void WriteAngle16(unsigned char *b, float f)
{
	unsigned short v;

	v = Q_rint(f * 65536.0 / 360.0) & 65535;

	b[0] = v;
	b[1] = v>>8;
}

static unsigned int WriteDeltaUsercmd(unsigned char *buf, const usercmd_t *from, const usercmd_t *cmd)
{
	unsigned int i;
	int bits;

	i = 0;

	// send the movement message
	bits = 0;
	if (cmd->angles[0] != from->angles[0])
		bits |= CM_ANGLE1;
	if (cmd->angles[1] != from->angles[1])
		bits |= CM_ANGLE2;
	if (cmd->angles[2] != from->angles[2])
		bits |= CM_ANGLE3;
	if (cmd->forwardmove != from->forwardmove)
		bits |= CM_FORWARD;
	if (cmd->sidemove != from->sidemove)
		bits |= CM_SIDE;
	if (cmd->upmove != from->upmove)
		bits |= CM_UP;
	if (cmd->buttons != from->buttons)
		bits |= CM_BUTTONS;
	if (cmd->impulse != from->impulse)
		bits |= CM_IMPULSE;

	buf[i++] = bits;

	if (bits & CM_ANGLE1)
	{
		WriteAngle16(buf + i, cmd->angles[0]);
		i += 2;
	}
	if (bits & CM_ANGLE2)
	{
		WriteAngle16(buf + i, cmd->angles[1]);
		i += 2;
	}
	if (bits & CM_ANGLE3)
	{
		WriteAngle16(buf + i, cmd->angles[2]);
		i += 2;
	}
	
	if (bits & CM_FORWARD)
	{
		buf[i++] = cmd->forwardmove;
		buf[i++] = cmd->forwardmove>>8;
	}
	if (bits & CM_SIDE)
	{
		buf[i++] = cmd->sidemove;
		buf[i++] = cmd->sidemove>>8;
	}
	if (bits & CM_UP)
	{
		buf[i++] = cmd->upmove;
		buf[i++] = cmd->upmove>>8;
	}

 	if (bits & CM_BUTTONS)
		buf[i++] = cmd->buttons;
 	if (bits & CM_IMPULSE)
		buf[i++] = cmd->impulse;

	buf[i++] = cmd->msec;

	return i;
}

/*****/

void NetQW_GenerateFrames(struct NetQW *netqw)
{
	usercmd_t hej;
	unsigned int framenum;
	unsigned long long curtime;
	int i;

	curtime = Sys_IntTime();

	if (netqw->state != state_connected)
		return;

	Sys_Thread_LockMutex(netqw->mutex);

	while(netqw->lastmovesendtime + netqw->microsecondsperframe <= curtime)
	{
		netqw->lastmovesendtime += netqw->microsecondsperframe;
		netqw->movetimecounter += netqw->microsecondsperframe;

		memset(&hej, 0, sizeof(hej));

#warning I should MakeShort() here... Maybe...
		hej.forwardmove = netqw->forwardspeed;
		hej.sidemove = netqw->sidespeed;
		hej.upmove = netqw->upspeed;

		hej.buttons = netqw->oncebuttons | netqw->currentbuttons;
		netqw->oncebuttons = 0;

		if (netqw->priorityimpulse)
		{
			hej.impulse = netqw->priorityimpulse;
			netqw->priorityimpulse = 0;
		}
		else if (netqw->normalimpulse)
		{
			hej.impulse = netqw->normalimpulse;
			netqw->normalimpulse = 0;
		}

		for (i = 0; i < 3; i++)
			hej.angles[i] = (Q_rint(hej.angles[i] * 65536.0 / 360.0) & 65535) * (360.0 / 65536.0);

		hej.msec = netqw->movetimecounter/1000;
		netqw->movetimecounter %= 1000;
		Mouse_GetViewAngles(hej.angles);

		framenum = (netqw->lastsentframe + netqw->framestosend + 1) & UPDATE_MASK;

		if (netqw->movement_locked)
		{
			hej.forwardmove = 0;
			hej.sidemove = 0;
			hej.upmove = 0;
		}

		netqw->frames[framenum].cmd = hej;
		netqw->frames[framenum].senttime = Sys_DoubleTime();
		netqw->frames[framenum].delta_sequence = -1;

		if (netqw->last_received_entity_update_frame != -1 && netqw->current_outgoing_sequence_number - netqw->last_received_entity_update_frame < UPDATE_BACKUP)
			netqw->frames[netqw->current_outgoing_sequence_number & UPDATE_MASK].delta_sequence = netqw->last_received_entity_update_frame;

		netqw->framestosend++;
		netqw->framestocopy++;

#if 0
		printf("Sent frame %d at %f\n", netqw->current_outgoing_sequence_number & UPDATE_MASK, netqw->frames[netqw->current_outgoing_sequence_number & UPDATE_MASK].senttime);
#endif
	}

	Sys_Thread_UnlockMutex(netqw->mutex);
}

static int NetQW_Thread_Init(struct NetQW *netqw)
{
	qboolean b;

	netqw->netdata = Sys_Net_Init();
	if (netqw->netdata)
	{
		b = NET_StringToAdr(netqw->netdata, netqw->hoststring, &netqw->addr);
		if (!b)
		{
			printf("Unable to parse/resolve hoststring\n");
		}
		else
		{
			netqw->socket = Sys_Net_CreateSocket(netqw->netdata, netqw->addr.type);
			if (netqw->socket)
			{
				netqw->resendtime = 0;
				netqw->state = state_sendchallenge;
#warning "Can't we randomise this a bit? Say 1-65536 as starting number?"
#warning "Spike says we can, but as he also points out, there needs to be an upper bound on acceptable incoming sequence numbers then."
				netqw->current_outgoing_sequence_number = 1;
				netqw->last_incoming_sequence_number = 0;
				netqw->incoming_reliable_xor = 0;
				netqw->outgoing_reliable_xor = 0;

				return 1;
			}
		}

		Sys_Net_Shutdown(netqw->netdata);
	}

	return 0;
}

static void NetQW_Thread_Deinit(struct NetQW *netqw)
{
	Sys_Net_DeleteSocket(netqw->netdata, netqw->socket);
	Sys_Net_Shutdown(netqw->netdata);
}

void NetQW_Thread_QueuePacket(struct NetQW *netqw, const void *buf, unsigned int buflen)
{
	struct SendPacket *sendpacket;

	sendpacket = malloc(sizeof(*sendpacket) + buflen);
	if (sendpacket)
	{
		sendpacket->next = 0;
		sendpacket->length = buflen;
		memcpy(sendpacket + 1, buf, buflen);
		if (netqw->lag_ezcheat)
			sendpacket->delayuntil = Sys_IntTime() + netqw->lag;
		else
			sendpacket->delayuntil = Sys_IntTime() + netqw->lag / 2;

		if (netqw->sendpackettail)
		{
			netqw->sendpackettail->next = sendpacket;
			netqw->sendpackettail = sendpacket;
		}
		else
		{
			netqw->sendpackethead = sendpacket;
			netqw->sendpackettail = sendpacket;
		}
	}
}

void NetQW_Thread_SendPacket(struct NetQW *netqw, const void *buf, unsigned int buflen, const struct netaddr *to, int force)
{
	unsigned char compressedbuf[1500];
	unsigned int i;

	if (buflen >= sizeof(compressedbuf))
		return;

	if (netqw->huffcontext)
	{
		memcpy(compressedbuf, buf, 10);
		i = Huff_CompressPacket(netqw->huffcontext, buf + 10, buflen - 10, compressedbuf + 10, sizeof(compressedbuf) - 10);
		if (i == 0)
		{
			Com_Printf("Huffman coding failed\n");
		}
		else
		{
			if (netqw->lag && !force)
				NetQW_Thread_QueuePacket(netqw, compressedbuf, i+10);
			else
				Sys_Net_Send(netqw->netdata, netqw->socket, compressedbuf, i + 10, &netqw->addr);
		}
	}
	else
	{
		if (netqw->lag && !force)
			NetQW_Thread_QueuePacket(netqw, buf, buflen);
		else
			Sys_Net_Send(netqw->netdata, netqw->socket, buf, buflen, &netqw->addr);
	}
}

static void NetQW_Thread_Disconnect(struct NetQW *netqw)
{
	unsigned char disconnectmessage[10+6];
	unsigned int i;

	if (netqw->state == state_connected)
	{
		for(i=0;i<3;i++)
		{
			disconnectmessage[0] = netqw->current_outgoing_sequence_number;
			disconnectmessage[1] = netqw->current_outgoing_sequence_number>>8;
			disconnectmessage[2] = netqw->current_outgoing_sequence_number>>16;
			disconnectmessage[3] = netqw->current_outgoing_sequence_number>>24;

			disconnectmessage[4] = netqw->last_incoming_sequence_number;
			disconnectmessage[5] = netqw->last_incoming_sequence_number>>8;
			disconnectmessage[6] = netqw->last_incoming_sequence_number>>16;
			disconnectmessage[7] = netqw->last_incoming_sequence_number>>24;

			disconnectmessage[8] = netqw->qport;
			disconnectmessage[9] = netqw->qport>>8;

			disconnectmessage[10] = clc_stringcmd;
			strcpy((char *)(disconnectmessage + 11), "drop");

			NetQW_Thread_SendPacket(netqw, disconnectmessage, 16, &netqw->addr, 1);

			netqw->current_outgoing_sequence_number++;
		}
	}
}

#if 1
void NetQW_Thread_HandleReceivedPacket(struct NetQW *netqw, struct NetPacket *netpacket)
{
	struct ReliableBuffer *reliablebuffer;
	unsigned int sequence_number;
	unsigned int i;
	unsigned int j;
	unsigned int lostpackets;
	unsigned char *buf;

	buf = (unsigned char *)(netpacket + 1);

	sequence_number = buf[0];
	sequence_number |= buf[1]<<8;
	sequence_number |= buf[2]<<16;
	sequence_number |= (buf[3]&0x7f)<<24;

#if 0
	printf("Incoming sequence number: %d\n", sequence_number);
#endif

	netqw->last_incoming_sequence_number = sequence_number;

	if ((buf[3]&0x80))
		netqw->outgoing_reliable_xor ^= 1;

	if (sequence_number + PLPACKETHISTORYCOUNT > netqw->current_outgoing_sequence_number)
	{
		netqw->replyreceived[sequence_number%PLPACKETHISTORYCOUNT] = 1;

		lostpackets = 0;
		j = netqw->current_outgoing_sequence_number;
		if (j > PLPACKETHISTORYCOUNT)
			j = PLPACKETHISTORYCOUNT;

		j -= netqw->current_outgoing_sequence_number - sequence_number + 1;

		for(i=0;i<j;i++)
		{
			if (netqw->replyreceived[(sequence_number-i)%PLPACKETHISTORYCOUNT] == 0)
				lostpackets++;
		}

#warning Not taking svc_chokecount into account
		netqw->packetloss = ((lostpackets * 100) + (PLPACKETHISTORYCOUNT - 1))/PLPACKETHISTORYCOUNT;
	}

	if (netqw->reliable_buffers_sent && sequence_number >= netqw->reliable_sequence_number)
	{
		/* The server should have seen our reliable packet by now... */

		if ((!netqw->incoming_reliable_xor) ^ (!(buf[7]&0x80)))
		{
			/* ... but it hasn't :/ */

			netqw->reliable_buffers_sent = 0;
			netqw->incoming_reliable_xor ^= 1;
		}
		else
		{
			/* And it has! Great, free up the buffers then. */

			for(i=0;i<netqw->reliable_buffers_sent;i++)
			{
				reliablebuffer = netqw->reliablebufferhead;
				netqw->reliablebufferhead = reliablebuffer->next;
				free(reliablebuffer);
			}

			if (netqw->reliablebufferhead == 0)
				netqw->reliablebuffertail = 0;

			netqw->reliable_buffers_sent = 0;
		}
	}
}
#endif

static void NetQW_Thread_DoReceive(struct NetQW *netqw)
{
	struct netaddr addr;
	unsigned char buf[1500];
	unsigned char compressedbuf[1500];
	unsigned char challengebuf[16];
	struct NetPacket *netpacket;
	unsigned int sequence_number;
	unsigned char *p;
	int r;
	unsigned int i;
	unsigned int len;
	unsigned int extension;
	unsigned int value;

	do
	{
		if (netqw->state == state_connected && netqw->huffcontext)
		{
			r = Sys_Net_Receive(netqw->netdata, netqw->socket, compressedbuf, sizeof(compressedbuf) - 1, &addr);
			memcpy(buf, compressedbuf, 8);
			if (r > 8)
			{
				r = Huff_DecompressPacket(netqw->huffcontext, compressedbuf + 8, r - 8, buf + 8, sizeof(buf) - 8 - 1);
				r += 8;
			}
		}
		else
			r = Sys_Net_Receive(netqw->netdata, netqw->socket, buf, sizeof(buf) - 1, &addr);

		if (r > 0)
		{
			if (!NET_CompareAdr(&addr, &netqw->addr))
				continue;

			netqw->lastserverpackettime = Sys_IntTime();

			if (netqw->state == state_sendchallenge)
			{
				if (r < 5 || buf[0] != 255 || buf[1] != 255 || buf[2] != 255 || buf[3] != 255 || buf[4] != S2C_CHALLENGE)
					continue;

				len = r - 5;

				p = buf + 5;
				i = 0;
				while(*p && len)
				{
					p++;
					i++;
					len--;
				}

				if (i < 16)
				{
					memcpy(challengebuf, buf + 5, i);
					challengebuf[i] = 0;

					netqw->challenge = atoi((char *)challengebuf);

					if (len)
					{
						p++;
						len--;

						while(len >= 8)
						{
							extension = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
							value = (p[7] << 24) | (p[6] << 16) | (p[5] << 8) | p[4];

							if (extension == QW_PROTOEXT_HUFF)
							{
								Com_Printf("Server supports Huffman compression\n");

								netqw->huffcontext = Huff_Init(value);
								if (netqw->huffcontext == 0)
								{
									Com_Printf("Unable to initialise Huffman coding\n");
								}
								else
								{
									netqw->huffcrc = value;
								}
							}
							else
							{
								Com_Printf("Unknown protocol extension: %08x\n", extension);
							}

							p += 8;
							len -= 8;
						}
					}

					netqw->state = state_sendconnection;
					netqw->resendtime = 0;
				}
			}
			else if (netqw->state == state_sendconnection)
			{
				if (r < 5 || buf[0] != 255 || buf[1] != 255 || buf[2] != 255 || buf[3] != 255)
					continue;

				if (buf[4] == A2C_PRINT)
				{
					len = r - 5;
					p = buf + 5;
					while(*p && len)
					{
						p++;
						len--;
					}

					*p = 0;

					Com_Printf("Connectionless message: %s\n", buf + 5);
				}
				else if (buf[4] == S2C_CONNECTION)
				{
					Com_Printf("Connected.\n");

					buf[0] = clc_stringcmd;
					strcpy((char *)(buf + 1), "new");
					NetQW_AppendReliableBuffer(netqw, buf, 5);

					netqw->lastmovesendtime = Sys_IntTime() - netqw->microsecondsperframe;
					netqw->movetimecounter = 0;
					netqw->state = state_connected;
				}
				else
				{
					printf("Unknown packet from server: %d\n", buf[4]);
				}
			}
			else
			{
				if (r < 8)
					continue;

				sequence_number = buf[0];
				sequence_number |= buf[1]<<8;
				sequence_number |= buf[2]<<16;
				sequence_number |= (buf[3]&0x7f)<<24;

#if 0
				printf("Incoming sequence number: %d\n", sequence_number);
#endif

				if (sequence_number <= netqw->last_incoming_sequence_number)
					continue;

				sequence_number = buf[4];
				sequence_number |= buf[5]<<8;
				sequence_number |= buf[6]<<16;
				sequence_number |= (buf[7]&0x7f)<<24;

				if (sequence_number >= netqw->current_outgoing_sequence_number)
					continue;

				netpacket = malloc(sizeof(*netpacket) + r);
				if (netpacket)
				{
					netpacket->clientnext = 0;
					netpacket->internalnext = 0;
					if (netqw->lag && !netqw->lag_ezcheat)
						netpacket->delayuntil = Sys_IntTime() + netqw->lag / 2;
					else
						netpacket->delayuntil = 0;
					netpacket->length = r;
					netpacket->usecount = 1;
					memcpy(netpacket + 1, buf, r);

					if (netpacket->delayuntil == 0)
					{
						NetQW_Thread_HandleReceivedPacket(netqw, netpacket);
					}
					else
					{
						if (netqw->internalnetpackettail)
						{
							netqw->internalnetpackettail->internalnext = netpacket;
							netqw->internalnetpackettail = netpacket;
						}
						else
						{
							netqw->internalnetpackethead = netpacket;
							netqw->internalnetpackettail = netpacket;
						}

						netpacket->usecount++;
					}

					Sys_Thread_LockMutex(netqw->mutex);
					if (netqw->clientnetpackettail)
					{
						netqw->clientnetpackettail->clientnext = netpacket;
						netqw->clientnetpackettail = netpacket;
					}
					else
					{
						netqw->clientnetpackethead = netpacket;
						netqw->clientnetpackettail = netpacket;
					}
					Sys_Thread_UnlockMutex(netqw->mutex);
				}
			}
		}
	} while(r > 0);
}

static void NetQW_Thread_DoSend(struct NetQW *netqw)
{
	static const char *getchallenge = "\xff\xff\xff\xff" "getchallenge\n";
	unsigned char buf[1500];
	struct ReliableBuffer *reliablebuffer;
	unsigned long long curtime;
	unsigned int i;
	unsigned int j;

	curtime = Sys_IntTime();

	/* Just to avoid catastrophy with buggy Sys_IntTime() implementations */
	if (netqw->lastmovesendtime + 1000000 < curtime)
		netqw->lastmovesendtime = curtime;

	if (netqw->state == state_connected)
	{
		NetQW_GenerateFrames(netqw);

		while(1)
		{
			if (!netqw->framestosend)
				break;

#if 0
			if (netqw->framestosend > 1)
				printf("Have to send %d frames!\n", netqw->framestosend);
#endif

			i = 0;

			buf[i++] = netqw->current_outgoing_sequence_number;
			buf[i++] = netqw->current_outgoing_sequence_number>>8;
			buf[i++] = netqw->current_outgoing_sequence_number>>16;
			buf[i++] = netqw->current_outgoing_sequence_number>>24;

			buf[i++] = netqw->last_incoming_sequence_number;
			buf[i++] = netqw->last_incoming_sequence_number>>8;
			buf[i++] = netqw->last_incoming_sequence_number>>16;
			buf[i++] = netqw->last_incoming_sequence_number>>24;

			buf[7] |= netqw->outgoing_reliable_xor<<7;

			buf[i++] = netqw->qport;
			buf[i++] = netqw->qport>>8;

			buf[i++] = clc_move;
			buf[i++] = 0;
			buf[i++] = netqw->packetloss;

			i += WriteDeltaUsercmd(buf + i, &zerocmd, &netqw->frames[(netqw->current_outgoing_sequence_number - 2) & UPDATE_MASK].cmd);
			i += WriteDeltaUsercmd(buf + i, &netqw->frames[(netqw->current_outgoing_sequence_number - 2) & UPDATE_MASK].cmd, &netqw->frames[(netqw->current_outgoing_sequence_number - 1) & UPDATE_MASK].cmd);
			i += WriteDeltaUsercmd(buf + i, &netqw->frames[(netqw->current_outgoing_sequence_number - 1) & UPDATE_MASK].cmd, &netqw->frames[netqw->current_outgoing_sequence_number & UPDATE_MASK].cmd);

			buf[11] = COM_BlockSequenceCRCByte(buf + 12, i - 12, netqw->current_outgoing_sequence_number);

#warning Make this automatically tune to a given ping time.
			Sys_Thread_LockMutex(netqw->mutex);
			if (netqw->frames[netqw->current_outgoing_sequence_number & UPDATE_MASK].delta_sequence != -1)
			{
				buf[i++] = clc_delta;
				buf[i++] = netqw->frames[netqw->current_outgoing_sequence_number & UPDATE_MASK].delta_sequence;
			}

			Sys_Thread_UnlockMutex(netqw->mutex);

			if (netqw->send_tmove)
			{
				buf[i++] = clc_tmove;
				memcpy(buf + i, netqw->tmove_buffer, 6);
				i += 6;

				netqw->send_tmove = 0;
			}

#warning Implement c2spps
#warning "Downloads? (probably shouldn't go here anyway)"

			if (!netqw->reliable_buffers_sent)
			{
				/* Now append as many reliable buffers as we can fit */
				Sys_Thread_LockMutex(netqw->mutex);

				j = 0;
				reliablebuffer = netqw->reliablebufferhead;
				while(reliablebuffer && i + reliablebuffer->length < 1450)
				{
					memcpy(buf + i, reliablebuffer + 1, reliablebuffer->length);
					i += reliablebuffer->length;
					reliablebuffer = reliablebuffer->next;
					j++;
				}

				Sys_Thread_UnlockMutex(netqw->mutex);

				if (j)
				{
					netqw->reliable_buffers_sent = j;
					netqw->reliable_sequence_number = netqw->current_outgoing_sequence_number;
					netqw->incoming_reliable_xor ^= 1;

					buf[3] |= 0x80;
				}
			}

			NetQW_Thread_SendPacket(netqw, buf, i, &netqw->addr, 0);

			netqw->replyreceived[netqw->current_outgoing_sequence_number%PLPACKETHISTORYCOUNT] = 0;

			Sys_Thread_LockMutex(netqw->mutex);
			netqw->lastsentframe++;
			netqw->framestosend--;
			netqw->current_outgoing_sequence_number++;
			Sys_Thread_UnlockMutex(netqw->mutex);

		}
	}
	else
	{
		if (netqw->resendtime <= curtime)
		{
#if 0
			printf("Resending!\n");
#endif

			if (netqw->state == state_sendchallenge)
			{
				Sys_Net_Send(netqw->netdata, netqw->socket, getchallenge, strlen(getchallenge), &netqw->addr);
			}
			else
			{
				i = snprintf((char *)buf, sizeof(buf), "\xff\xff\xff\xff" "connect %d %d %d \"%s\"\n", PROTOCOL_VERSION, netqw->qport, netqw->challenge, netqw->userinfo);

				if (netqw->huffcontext)
				{
					i += snprintf((char *)(buf + i), sizeof(buf) - i, "0x%08x 0x%08x\n", QW_PROTOEXT_HUFF, netqw->huffcrc);
				}

				if (i < sizeof(buf))
				{
					Sys_Net_Send(netqw->netdata, netqw->socket, buf, i, &netqw->addr);
				}
			}

			netqw->resendtime = curtime + 3000000;
		}
	}
}

static void NetQW_Thread(void *arg)
{
	struct NetQW *netqw;
	struct NetPacket *netpacket;
	struct SendPacket *sendpacket;
	int r;
	unsigned long long curtime;
	unsigned int waittime;

	netqw = arg;

	r = NetQW_Thread_Init(netqw);
	if (!r)
	{
		netqw->error = 1;
	}
	else
	{
		while(!netqw->quit)
		{
			curtime = Sys_IntTime();

			if (netqw->state == state_connected)
			{
				Sys_Thread_LockMutex(netqw->mutex);

				if (netqw->lastmovesendtime + netqw->microsecondsperframe > curtime)
					waittime = (netqw->lastmovesendtime + netqw->microsecondsperframe) - curtime;
				else
					waittime = 0;

				Sys_Thread_UnlockMutex(netqw->mutex);
			}
			else
			{
				if (netqw->resendtime > curtime)
					waittime = netqw->resendtime - curtime;
				else
					waittime = 0;
			}

			/* Mmm, sanity checks... */
			if ((netpacket = netqw->internalnetpackethead) && netpacket->delayuntil > curtime + 1000000)
			{
				while(netpacket)
				{
					netpacket->delayuntil = curtime;
					netpacket = netpacket->internalnext;
				}
			}

			while((netpacket = netqw->internalnetpackethead) && netpacket->delayuntil <= curtime)
			{
				NetQW_Thread_HandleReceivedPacket(netqw, netpacket);

				netqw->internalnetpackethead = netpacket->internalnext;
				if (netqw->internalnetpackethead == 0)
					netqw->internalnetpackettail = 0;

				Sys_Thread_LockMutex(netqw->mutex);

				netpacket->usecount--;
				if (netpacket->usecount == 0)
					free(netpacket);

				Sys_Thread_UnlockMutex(netqw->mutex);
			}

			if ((sendpacket = netqw->sendpackethead) && sendpacket->delayuntil > curtime + 1000000)
			{
				while(sendpacket)
				{
					sendpacket->delayuntil = curtime;
					sendpacket = sendpacket->next;
				}
			}

			while((sendpacket = netqw->sendpackethead) && sendpacket->delayuntil <= curtime)
			{
				Sys_Net_Send(netqw->netdata, netqw->socket, sendpacket + 1, sendpacket->length, &netqw->addr);

				netqw->sendpackethead = sendpacket->next;
				if (netqw->sendpackethead == 0)
					netqw->sendpackettail = 0;

				free(sendpacket);
			}

			if ((netpacket = netqw->internalnetpackethead))
			{
				if (netpacket->delayuntil - curtime < waittime)
					waittime = netpacket->delayuntil - curtime;
			}

			if ((sendpacket = netqw->sendpackethead))
			{
				if (sendpacket->delayuntil - curtime < waittime)
					waittime = sendpacket->delayuntil - curtime;
			}

			if (waittime)
			{
				/* Because we don't quit from signalling, but
				 * polling, and the latency would be too high
				 * otherwise */
				if (waittime > 50000)
				{
					waittime = 50000;
				}

				Sys_Net_Wait(netqw->netdata, netqw->socket, waittime);
			}

			NetQW_Thread_DoReceive(netqw);
			NetQW_Thread_DoSend(netqw);
		}

		NetQW_Thread_Disconnect(netqw);

		NetQW_Thread_Deinit(netqw);
	}
}

struct NetQW *NetQW_Create(const char *hoststring, const char *userinfo, unsigned short qport)
{
	struct NetQW *netqw;
	int r;

	netqw = malloc(sizeof(*netqw));
	if (netqw)
	{
		netqw->quit = 0;
		netqw->qport = qport;
		netqw->mutex = 0;
		netqw->thread = 0;
		netqw->reliable_buffers_sent = 0;
		netqw->reliablebufferhead = 0;
		netqw->reliablebuffertail = 0;
		netqw->clientnetpackethead = 0;
		netqw->clientnetpackettail = 0;
		netqw->internalnetpackethead = 0;
		netqw->internalnetpackettail = 0;
		netqw->sendpackethead = 0;
		netqw->sendpackettail = 0;
		netqw->packetloss = 0;
		netqw->state = state_uninitialised;

		netqw->lastserverpackettime = 0;

		netqw->lastsentframe = 0;
		netqw->framestosend = 0;

		netqw->lastcopiedframe = 0;
		netqw->framestocopy = 0;

		netqw->last_received_entity_update_frame = -1;
		memset(netqw->frames, 0, sizeof(netqw->frames));

		NetQW_SetFPS(netqw, 72);

		netqw->huffcontext = 0;

		netqw->send_tmove = 0;
		netqw->movement_locked = 0;

		netqw->forwardspeed = 0;
		netqw->sidespeed = 0;
		netqw->upspeed = 0;
		netqw->oncebuttons = 0;
		netqw->currentbuttons = 0;
		netqw->priorityimpulse = 0;
		netqw->normalimpulse = 0;

		netqw->lag = 0;
		netqw->lag_ezcheat = 0;

		netqw->hoststring = malloc(strlen(hoststring)+1);
		if (netqw->hoststring)
		{
			strcpy(netqw->hoststring, hoststring);

			netqw->userinfo = malloc(strlen(userinfo)+1);
			if (netqw->userinfo)
			{
				strcpy(netqw->userinfo, userinfo);

				netqw->mutex = Sys_Thread_CreateMutex();
				if (netqw->mutex)
				{
					netqw->thread = Sys_Thread_CreateThread(NetQW_Thread, netqw);
					if (netqw->thread)
					{
						Sys_Thread_SetThreadPriority(netqw->thread, SYSTHREAD_PRIORITY_HIGH);

						return netqw;
					}

					Sys_Thread_DeleteMutex(netqw->mutex);
				}

				netqw->mutex = 0;
				netqw->thread = 0;

				r = NetQW_Thread_Init(netqw);
				if (r)
				{
					return netqw;
				}

				free(netqw->userinfo);
			}

			free(netqw->hoststring);
		}

		free(netqw);
	}

	return 0;
}

void NetQW_Delete(struct NetQW *netqw)
{
	struct ReliableBuffer *reliablebuffer;
	struct NetPacket *netpacket;

	/* Yup, should use proper signalling */
	netqw->quit = 1;

	if (netqw->thread)
		Sys_Thread_DeleteThread(netqw->thread);
	else
	{
		NetQW_Thread_Disconnect(netqw);
		NetQW_Thread_Deinit(netqw);
	}

	if (netqw->mutex)
		Sys_Thread_DeleteMutex(netqw->mutex);

	while((reliablebuffer = netqw->reliablebufferhead))
	{
		netqw->reliablebufferhead = reliablebuffer->next;
		free(reliablebuffer);
	}

	while((netpacket = netqw->clientnetpackethead))
	{
		netqw->clientnetpackethead = netpacket->clientnext;
		netpacket->usecount--;
		if (netpacket->usecount == 0)
			free(netpacket);
	}

	while((netpacket = netqw->internalnetpackethead))
	{
		netqw->internalnetpackethead = netpacket->internalnext;
		netpacket->usecount--;
		if (netpacket->usecount == 0)
			free(netpacket);
	}

	free(netqw->userinfo);
	free(netqw->hoststring);
	free(netqw);
}

void NetQW_Receive(struct NetQW *netqw)
{
}

void NetQW_Send(struct NetQW *netqw)
{
}

void NetQW_SetFPS(struct NetQW *netqw, unsigned int fps)
{
#if 0
	fps = 10;
#endif

	if (fps != 0)
	{
		if (netqw->mutex)
			Sys_Thread_LockMutex(netqw->mutex);

		netqw->microsecondsperframe = 1000000/fps;
		if (netqw->microsecondsperframe < 1000)
			netqw->microsecondsperframe = 1000;

		if (CLAMPTOINTEGERMS)
		{
			netqw->microsecondsperframe += 999;
			netqw->microsecondsperframe -= netqw->microsecondsperframe % 1000;
		}

#if 0
		printf("us per frame: %d\n", netqw->microsecondsperframe);
#endif

		if (netqw->mutex)
			Sys_Thread_UnlockMutex(netqw->mutex);
	}
}

unsigned long long NetQW_GetFrameTime(struct NetQW *netqw)
{
	return netqw->microsecondsperframe;
}

int NetQW_AppendReliableBuffer(struct NetQW *netqw, const void *buffer, unsigned int bufferlen)
{
	struct ReliableBuffer *reliablebuffer;

	reliablebuffer = malloc(sizeof(*reliablebuffer) + bufferlen);
	if (reliablebuffer)
	{
		reliablebuffer->next = 0;
		reliablebuffer->length = bufferlen;
		memcpy(reliablebuffer + 1, buffer, bufferlen);

		Sys_Thread_LockMutex(netqw->mutex);

		if (netqw->reliablebuffertail)
		{
			netqw->reliablebuffertail->next = reliablebuffer;
			netqw->reliablebuffertail = reliablebuffer;
		}
		else
		{
			netqw->reliablebufferhead = reliablebuffer;
			netqw->reliablebuffertail = reliablebuffer;
		}

		Sys_Thread_UnlockMutex(netqw->mutex);

		return 1;
	}

	return 0;
}

unsigned int NetQW_GetPacketLength(struct NetQW *netqw)
{
	unsigned int ret;

	Sys_Thread_LockMutex(netqw->mutex);

	if (netqw->clientnetpackethead && netqw->clientnetpackethead->delayuntil <= Sys_IntTime())
		ret = netqw->clientnetpackethead->length;
	else
		ret = 0;

	Sys_Thread_UnlockMutex(netqw->mutex);

	return ret;
}

void *NetQW_GetPacketData(struct NetQW *netqw)
{
	void *ret;

	Sys_Thread_LockMutex(netqw->mutex);

	if (netqw->clientnetpackethead && netqw->clientnetpackethead->delayuntil <= Sys_IntTime())
		ret = netqw->clientnetpackethead + 1;
	else
		ret = 0;

	Sys_Thread_UnlockMutex(netqw->mutex);

	return ret;
}

void NetQW_FreePacket(struct NetQW *netqw)
{
	struct NetPacket *netpacket;

	Sys_Thread_LockMutex(netqw->mutex);

	netpacket = netqw->clientnetpackethead;
	netqw->clientnetpackethead = netpacket->clientnext;
	netpacket->usecount--;
	if (netpacket->usecount == 0)
		free(netpacket);
	if (netqw->clientnetpackethead == 0)
		netqw->clientnetpackettail = 0;

	Sys_Thread_UnlockMutex(netqw->mutex);
}

void NetQW_CopyFrames(struct NetQW *netqw, frame_t *frames, unsigned int *newseqnr, unsigned int *startframe, unsigned int *numframes)
{
	unsigned int framecount;
	Sys_Thread_LockMutex(netqw->mutex);

	framecount = 0;
	if (startframe)
		*startframe = (netqw->lastcopiedframe + 1) & UPDATE_MASK;

	while(netqw->framestocopy)
	{
		netqw->lastcopiedframe = (netqw->lastcopiedframe + 1) & UPDATE_MASK;
		frames[netqw->lastcopiedframe].cmd = netqw->frames[netqw->lastcopiedframe].cmd;
		frames[netqw->lastcopiedframe].senttime = netqw->frames[netqw->lastcopiedframe].senttime;
		frames[netqw->lastcopiedframe].delta_sequence = netqw->frames[netqw->lastcopiedframe].delta_sequence;

#if 0
		printf("Copied time %f into position %d\n", frames[netqw->lastcopiedframe].senttime, netqw->lastcopiedframe);
#endif

		netqw->framestocopy--;

		framecount++;
	}

	if (newseqnr)
		*newseqnr = netqw->current_outgoing_sequence_number + netqw->framestosend;
	if (numframes)
		*numframes = framecount;

	Sys_Thread_UnlockMutex(netqw->mutex);
}

void NetQW_SetDeltaPoint(struct NetQW *netqw, int delta_sequence_number)
{
	Sys_Thread_LockMutex(netqw->mutex);

	netqw->last_received_entity_update_frame = delta_sequence_number;

	Sys_Thread_UnlockMutex(netqw->mutex);
}

void NetQW_SetTeleport(struct NetQW *netqw, float *position)
{
	unsigned int temp;

	Sys_Thread_LockMutex(netqw->mutex);

	netqw->send_tmove = 1;

	temp = (int)(position[0] * 8);
	netqw->tmove_buffer[0] = temp;
	netqw->tmove_buffer[1] = temp>>8;

	temp = (int)(position[1] * 8);
	netqw->tmove_buffer[2] = temp;
	netqw->tmove_buffer[3] = temp>>8;

	temp = (int)(position[2] * 8);
	netqw->tmove_buffer[4] = temp;
	netqw->tmove_buffer[5] = temp>>8;

	Sys_Thread_UnlockMutex(netqw->mutex);
}

void NetQW_LockMovement(struct NetQW *netqw)
{
	netqw->movement_locked = 1;
}

void NetQW_UnlockMovement(struct NetQW *netqw)
{
	netqw->movement_locked = 0;
}

void NetQW_SetLag(struct NetQW *netqw, unsigned int microseconds)
{
	if (microseconds > 500000)
		microseconds = 500000;

	netqw->lag = microseconds;
}

void NetQW_SetLagEzcheat(struct NetQW *netqw, int enabled)
{
	netqw->lag_ezcheat = enabled;
}

unsigned long long NetQW_GetTimeSinceLastPacketFromServer(struct NetQW *netqw)
{
	return Sys_IntTime() - netqw->lastserverpackettime;
}

void NetQW_SetForwardSpeed(struct NetQW *netqw, float value)
{
	netqw->forwardspeed = value;
}

void NetQW_SetSideSpeed(struct NetQW *netqw, float value)
{
	netqw->sidespeed = value;
}

void NetQW_SetUpSpeed(struct NetQW *netqw, float value)
{
	netqw->upspeed = value;
}

unsigned int NetQW_ButtonDown(struct NetQW *netqw, int button, int impulse)
{
	unsigned int ret;

	if (button < 0 || button > 2)
		return 0;

	Sys_Thread_LockMutex(netqw->mutex);
	if (impulse)
		netqw->priorityimpulse = impulse;

	netqw->oncebuttons |= (1<<button);

	ret = netqw->current_outgoing_sequence_number;

	netqw->currentbuttons |= (1<<button);

	Sys_Thread_UnlockMutex(netqw->mutex);

	return ret;
}

void NetQW_ButtonUp(struct NetQW *netqw, int button)
{
	if (button < 0 || button > 2)
		return;

	netqw->currentbuttons &= ~(1<<button);
}

void NetQW_SetImpulse(struct NetQW *netqw, int impulse)
{
	Sys_Thread_LockMutex(netqw->mutex);

	netqw->normalimpulse = impulse;

	Sys_Thread_UnlockMutex(netqw->mutex);
}

