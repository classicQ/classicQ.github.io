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

#include "net.h"

enum ServerScannerStatus
{
	SSS_SCANNING,
	SSS_PINGING,
	SSS_IDLE,
	SSS_ERROR
};

enum QWServerStatus
{
	QWSS_WAITING,
	QWSS_REQUESTSENT,
	QWSS_DONE,
	QWSS_FAILED
};

struct QWPlayer
{
	const char *name; /* Can be 0 */
	const char *team; /* Ditto */
	unsigned int frags;
	unsigned int time;
	unsigned int ping;
	unsigned int topcolor;
	unsigned int bottomcolor;
};

struct QWSpectator
{
	const char *name; /* Can be 0 */
	const char *team; /* Ditto */
	unsigned int time;
	unsigned int ping;
};

struct QWServer
{
	struct netaddr addr;
	enum QWServerStatus status;
	unsigned int pingtime; /* In microseconds */
	const char *hostname; /* Note, this is the _QW_ text 'hostname' */
	const char *gamedir;
	unsigned int maxclients;
	unsigned int maxspectators;
	unsigned int teamplay;
	const char *map; /* Can be 0 */
	const struct QWPlayer *players;
	unsigned int numplayers;
	const struct QWSpectator *spectators;
	unsigned int numspectators;
};

struct ServerScanner *ServerScanner_Create(const char *masters);
void ServerScanner_Delete(struct ServerScanner *serverscanner);

void ServerScanner_DoStuff(struct ServerScanner *serverscanner);
int ServerScanner_DataUpdated(struct ServerScanner *serverscanner);
const struct QWServer **ServerScanner_GetServers(struct ServerScanner *serverscanner, unsigned int *numservers);
void ServerScanner_FreeServers(struct ServerScanner *serverscanner, const struct QWServer **servers);
void ServerScanner_RescanServer(struct ServerScanner *serverscanner, const struct QWServer *server);
enum ServerScannerStatus ServerScanner_GetStatus(struct ServerScanner *serverscanner);



