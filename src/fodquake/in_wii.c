/*
Copyright (C) 2008 Mark Olsen

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

#include <ogc/ipc.h>
#include <ogc/lwp.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "in_wii.h"

#define true qtrue
#define false qfalse
#include "quakedef.h"
#undef true
#undef false
#include "keys.h"

#define USE_THREAD 1

#define EVENTQUEUESIZE 16

struct keyboardevent
{
	unsigned int type;
	unsigned int unknown1;
	unsigned char modifiers;
	unsigned char unknown2;
	unsigned char pressed[6];
};

struct eventlist
{
	unsigned int eventreceived;
	struct keyboardevent event;
};

struct inputdata
{
	int fd;

	unsigned char keystate[256];
	unsigned char modifierstate;

	struct eventlist eventlist[EVENTQUEUESIZE];
#if USE_THREAD
	volatile unsigned int eventlist_read_ptr;
	volatile unsigned int eventlist_write_ptr;

	lwp_t threadhandle;

	unsigned char threadstack[16384+15];
#endif
};

#if !USE_THREAD
static s32 myipccallback(s32 result, void *usrdata)
{
	struct eventlist *eventlist;

	eventlist = usrdata;

	eventlist->eventreceived = 1;

	return result;
}

static void SendEventAsync(struct inputdata *id, struct eventlist *eventlist)
{
	unsigned char random[32];
	int r;

	r = IOS_IoctlAsync(id->fd, 0, random, 32, (u8 *)&eventlist->event, sizeof(eventlist->event), myipccallback, eventlist);

	if (r != 0)
		printf("%s: IOS_IoctlAsync() failed: %d\n", __func__, r);
}

int setupasyncevents(struct inputdata *id)
{
		#if 0
			for(i=0;i<EVENTQUEUESIZE;i++)
				SendEventAsync(id, &id->eventlist[i]);
		#endif
	return 1;
}
#endif

#if USE_THREAD
static void *inputthread(void *inputdata)
{
	struct inputdata *id;
	unsigned int r;

	id = inputdata;

	while(1)
	{
		if ((id->eventlist_write_ptr + 1)%EVENTQUEUESIZE == id->eventlist_read_ptr)
		{
			Com_Printf("%s: You lose.\n", __func__);
		}

		r = IOS_Ioctl(id->fd, 0, 0, 0, (u8 *)&id->eventlist[id->eventlist_write_ptr].event, sizeof(id->eventlist[id->eventlist_write_ptr].event));
		if (r != 0)
		{
			Com_Printf("%s: IOS_Ioctl() failed: %d\n", __func__, r);
		}

		id->eventlist_write_ptr = (id->eventlist_write_ptr + 1) % EVENTQUEUESIZE;
	}
}

static int setupthread(struct inputdata *id)
{
	void *stackptr;

	stackptr = (void *)((((unsigned int)id->threadstack)+15)&~15);

	if(LWP_CreateThread(&id->threadhandle, inputthread, id, stackptr, 16384, LWP_PRIO_HIGHEST) != -1)
	{
		return 1;
	}

	return 0;
}
#endif

void *Sys_Input_Init()
{
	struct inputdata *id;
	unsigned int i;

	id = malloc(sizeof(*id));
	if (id)
	{
		memset(id, 0, sizeof(*id));
		id->fd = IOS_Open("/dev/usb/kbd", 0);
		if (id->fd >= 0)
		{
#if USE_THREAD
			if (setupthread(id))
#else
			if (setupasyncevents(id))
#endif
			{
				return id;
			}

			IOS_Close(id->fd);
		}

		free(id);
	}

	printf("%s() failed\n", __func__);

	return 0;
}

void Sys_Input_Shutdown(void *inputdata)
{
}

static unsigned char keymappingtable[] =
{
	0,
	0,
	0,
	0,
	'a',
	'b',
	'c',
	'd',
	'e',
	'f',
	'g', /* 10 */
	'h',
	'i',
	'j',
	'k',
	'l',
	'm',
	'n',
	'o',
	'p',
	'q', /* 20 */
	'r',
	's',
	't',
	'u',
	'v',
	'w',
	'x',
	'y',
	'z',
	'1', /* 30 */
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	K_ENTER, /* 40 */
	K_ESCAPE,
	K_BACKSPACE,
	K_TAB,
	K_SPACE,
	'-',
	'=',
	'[',
	']',
	0,
	'\\', /* 50 */
	';',
	'\'',
	'`',
	',',
	'.',
	'/',
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	K_PRINTSCR,
	K_SCRLCK,
	K_PAUSE,
	K_INS,
	K_HOME,
	K_PGUP,
	K_DEL,
	K_END,
	K_PGDN,
	K_RIGHTARROW,
	K_LEFTARROW,
	K_DOWNARROW,
	K_UPARROW,
};

static char specialkeys[8] =
{
	K_LCTRL,
	K_LSHIFT,
	K_LALT,
	K_LWIN,
	K_RCTRL,
	K_RSHIFT,
	K_RALT,
	K_RWIN
};

static void handleevent(struct inputdata *id, struct keyboardevent *event)
{
	unsigned int i;
	unsigned int j;
	unsigned char translatedkey;

	if (event->type == 0)
		Com_Printf("Keyboard connected\n");
	else if (event->type == 1)
		Com_Printf("Keyboard disconnected\n");
	else if (event->type == 2)
	{
		/* Check the special keys */
		for(i=0;i<8;i++)
		{
			if ((event->modifiers&(1<<i))^(id->modifierstate&(1<<i)))
				Key_Event(specialkeys[i], !!(event->modifiers&(1<<i)));
		}

		id->modifierstate = event->modifiers;

		/* First check for new down events */
		for(i=0;i<6;i++)
		{
			if (id->keystate[event->pressed[i]] == 0)
			{
				if (event->pressed[i] < sizeof(keymappingtable))
					translatedkey = keymappingtable[event->pressed[i]];
				else
					translatedkey = 0;

				if (translatedkey)
					Key_Event(translatedkey, 1);
				else
					Com_Printf("%s: Unknown key %d\n", __func__, event->pressed[i]);

				id->keystate[event->pressed[i]] = 1;
			}
		}

		/* Now check for keys which are no longer pressed... Sign... */
		for(i=1;i<sizeof(keymappingtable);i++)
		{
			if (id->keystate[i])
			{
				for(j=0;j<6 && event->pressed[j] != i;j++);

				if (j == 6)
				{
					translatedkey = keymappingtable[i];
					if (translatedkey)
						Key_Event(translatedkey, 0);

					id->keystate[i] = 0;
				}
			}
		}
	}
	else
	{
		Com_Printf("%s: Unknown event\n", __func__);
	}
}

void Sys_Input_GetEvents(void *inputdata)
{
	struct inputdata *id;
	unsigned int i;

	id = inputdata;

#if USE_THREAD
	while (id->eventlist_read_ptr != id->eventlist_write_ptr)
	{
		handleevent(id, &id->eventlist[id->eventlist_read_ptr].event);
		id->eventlist_read_ptr = (id->eventlist_read_ptr + 1) % EVENTQUEUESIZE;
	}
#else
	IOS_Ioctl(id->fd, 0, 0, 0, (u8 *)&id->eventlist[0].event, sizeof(id->eventlist[0].event));

	handleevent(id, &id->eventlist[0].event);

	return;

	for(i=0;i<EVENTQUEUESIZE;i++)
	{
		if (id->eventlist[i].eventreceived)
		{
			printf("Received event\n");
			id->eventlist[i].eventreceived = 0;
			SendEventAsync(id, &id->eventlist[i]);
		}
	}
#endif
}

void Sys_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey)
{
	*mousex = 0;
	*mousey = 0;
}

void Sys_Input_GrabMouse(void *inputdata, int dograb)
{
}

