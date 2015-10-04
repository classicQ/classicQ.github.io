/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <exec/exec.h>
#include <intuition/intuition.h>
#include <intuition/extensions.h>
#include <intuition/intuitionbase.h>
#include <devices/input.h>

#include <proto/exec.h>
#include <proto/intuition.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#include "in_morphos.h"

#ifdef AROS
#include <devices/rawkeycodes.h>

#define NM_WHEEL_UP RAWKEY_NM_WHEEL_UP
#define NM_WHEEL_DOWN RAWKEY_NM_WHEEL_DOWN
#define NM_BUTTON_FOURTH RAWKEY_NM_BUTTON_FOURTH
#endif


struct inputdata
{
	struct Screen *screen;
	struct Window *window;

	struct Interrupt InputHandler;
	struct MsgPort *inputport;
	struct IOStdReq *inputreq;
	BYTE inputopen;

	struct InputEvent imsgs[MAXIMSGS];
	int imsglow;
	int imsghigh;

	int upkey;

	int mousebuf;
	int mousex[3], mousey[3];

	int grabmouse;
};

#ifdef AROS
static AROS_UFP2(struct InputEvent *, myinputhandler, AROS_UFHA(struct InputEvent *, moo, A0), AROS_UFHA(struct inputdata *, id, A1));
#else
static struct EmulLibEntry myinputhandler;
#endif

#ifndef SA_Displayed
#define SA_Displayed (SA_Dummy + 101)
#endif

extern struct IntuitionBase *IntuitionBase;

#define DEBUGRING(x)

static unsigned char keyconv[] =
{
	'`',			/* 0 */
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',			/* 10 */
	'-',
	'=',
	0,
	0,
	KP_INS,
	'q',
	'w',
	'e',
	'r',
	't',			/* 20 */
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	0,
	KP_END,
	KP_DOWNARROW,		/* 30 */
	KP_PGDN,
	'a',
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',			/* 40 */
	';',
	'\'',
	'\\',
	0,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	'<',
	'z',
	'x',			/* 50 */
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.',
	'/',
	0,
	KP_DEL,			/* 60 */
	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	' ',
	K_BACKSPACE,
	K_TAB,
	KP_ENTER,
	K_ENTER,
	K_ESCAPE,
	K_DEL,			/* 70 */
	K_INS,
	K_PGUP,
	K_PGDN,
	KP_MINUS,
	K_F11,
	K_UPARROW,
	K_DOWNARROW,
	K_RIGHTARROW,
	K_LEFTARROW,
	K_F1,			/* 80 */
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	0,			/* 90 */
	0,
	KP_SLASH,
	0,
	KP_PLUS,
	0,
	K_LSHIFT,
	K_RSHIFT,
	K_CAPSLOCK,
	K_RCTRL,
	K_LALT,			/* 100 */
	K_RALT,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_PAUSE,		/* 110 */
	K_F12,
	K_HOME,
	K_END,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 120 */
	0,
	K_MWHEELUP,
	K_MWHEELDOWN,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 150 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 180 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 220 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 230 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,			/* 250 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0
};

void Sys_Input_Shutdown(void *inputdata)
{
	struct inputdata *id = inputdata;

	if (id->inputopen)
	{
		id->inputreq->io_Data = (void *)&id->InputHandler;
		id->inputreq->io_Command = IND_REMHANDLER;
		DoIO((struct IORequest *)id->inputreq);

		CloseDevice((struct IORequest *)id->inputreq);
	}

	if (id->inputreq)
		DeleteIORequest(id->inputreq);

	if (id->inputport)
		DeleteMsgPort(id->inputport);

	FreeVec(id);
}

void *Sys_Input_Init(struct Screen *screen, struct Window *window)
{
	struct inputdata *id;

	id = AllocVec(sizeof(*id), MEMF_ANY);
	if (id)
	{
		id->imsglow = id->imsghigh = id->mousebuf = id->mousex[0] = id->mousex[1] = id->mousex[2] = id->mousey[0] = id->mousey[1] = id->mousey[2] = 0;

		id->grabmouse = 1;

		id->screen = screen;
		id->window = window;

		id->upkey = -1;

		id->inputport = CreateMsgPort();
		if (id->inputport == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to create message port");
		}

		id->inputreq = CreateIORequest(id->inputport, sizeof(*id->inputreq));
		if (id->inputreq == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to create IO request");
		}

		id->inputopen = !OpenDevice("input.device", 0, (struct IORequest *)id->inputreq, 0);
		if (id->inputopen == 0)
		{
			Sys_Input_Shutdown(id);
			Sys_Error("Unable to open input.device");
		}

		id->InputHandler.is_Node.ln_Type = NT_INTERRUPT;
		id->InputHandler.is_Node.ln_Pri = 100;
		id->InputHandler.is_Node.ln_Name = "Fodquake input handler";
		id->InputHandler.is_Data = id;
		id->InputHandler.is_Code = (void (*)())&myinputhandler;
		id->inputreq->io_Data = (void *)&id->InputHandler;
		id->inputreq->io_Command = IND_ADDHANDLER;
		DoIO((struct IORequest *)id->inputreq);
	}

	return id;
}

int Sys_Input_GetKeyEvent(void *inputdata, keynum_t *keynum, qboolean *down)
{
	struct inputdata *id = inputdata;
	int key;
	qboolean dodown;
	int i;

	if (id->upkey != -1)
	{
		*keynum = id->upkey;
		*down = false;
		id->upkey = -1;
		return 1;
	}

	key = 0;
	dodown = 0;

	if (id->imsglow != id->imsghigh)
	{
		i = id->imsglow;

		DEBUGRING(dprintf("%d %d\n", i, id->imsghigh));
		if (id->imsgs[i].ie_Class == IECLASS_NEWMOUSE)
		{
			if (id->imsgs[i].ie_Code == NM_WHEEL_UP)
			{
				key = K_MWHEELUP;
			}
			else if (id->imsgs[i].ie_Code == NM_WHEEL_DOWN)
			{
				key = K_MWHEELDOWN;
			}

			if (id->imsgs[i].ie_Code == NM_BUTTON_FOURTH)
			{
				key = K_MOUSE4;
				dodown = true;
			}
			else if (id->imsgs[i].ie_Code == (NM_BUTTON_FOURTH | IECODE_UP_PREFIX))
			{
				key = K_MOUSE4;
				dodown = false;
			}

			if (key)
			{
				id->upkey = key;
				dodown = 1;
			}
		}
		else if (id->imsgs[i].ie_Class == IECLASS_RAWKEY)
		{
			dodown = !(id->imsgs[i].ie_Code & IECODE_UP_PREFIX);
			id->imsgs[i].ie_Code &= ~IECODE_UP_PREFIX;

			if (id->imsgs[i].ie_Code <= 255)
				key = keyconv[id->imsgs[i].ie_Code];

			if (!key && developer.value)
			{
				Com_Printf("Unknown key %d\n", id->imsgs[i].ie_Code);
			}
		}
		else if (id->imsgs[i].ie_Class == IECLASS_RAWMOUSE)
		{
			dodown = !(id->imsgs[i].ie_Code&IECODE_UP_PREFIX);
			id->imsgs[i].ie_Code &= ~IECODE_UP_PREFIX;

			if (id->imsgs[i].ie_Code == IECODE_LBUTTON)
				key = K_MOUSE1;
			else if (id->imsgs[i].ie_Code == IECODE_RBUTTON)
				key = K_MOUSE2;
			else if (id->imsgs[i].ie_Code == IECODE_MBUTTON)
				key = K_MOUSE3;
		}

		id->imsglow++;
		id->imsglow %= MAXIMSGS;
	}

	if (key)
	{
		*keynum = key;
		*down = dodown;
		return 1;
	}

	return 0;
}

void Sys_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey)
{
	struct inputdata *id = inputdata;
	unsigned int mbuf;

	mbuf = id->mousebuf;
	id->mousebuf = (id->mousebuf+1)%3;

	*mousex = id->mousex[mbuf];
	*mousey = id->mousey[mbuf];
	id->mousex[mbuf] = 0;
	id->mousey[mbuf] = 0;
}

#ifndef AROS
static struct InputEvent *myinputhandler_real(void);

static struct EmulLibEntry myinputhandler =
{
	TRAP_LIB, 0, (void (*)(void))myinputhandler_real
};
#endif

#ifdef AROS
static AROS_UFH2(struct InputEvent *, myinputhandler, AROS_UFHA(struct InputEvent *, moo, A0), AROS_UFHA(struct inputdata *, id, A1))
{
	AROS_USERFUNC_INIT	
#else
static struct InputEvent *myinputhandler_real()
{
	struct InputEvent *moo = (struct InputEvent *)REG_A0;
	struct inputdata *id = (struct inputdata *)REG_A1;
#endif

	struct InputEvent *coin;

	ULONG screeninfront;

	if (!(id->window->Flags & WFLG_WINDOWACTIVE))
		return moo;

	coin = moo;

	if (id->screen)
	{
#ifdef __MORPHOS__
		if (IntuitionBase->LibNode.lib_Version > 50 || (IntuitionBase->LibNode.lib_Version == 50 && IntuitionBase->LibNode.lib_Revision >= 56))
			GetAttr(SA_Displayed, id->screen, &screeninfront);
		else
#endif
			screeninfront = id->screen == IntuitionBase->FirstScreen;
	}
	else
		screeninfront = 1;

	do
	{
		if (coin->ie_Class == IECLASS_RAWMOUSE)
		{
			id->mousex[id->mousebuf] += coin->ie_position.ie_xy.ie_x;
			id->mousey[id->mousebuf] += coin->ie_position.ie_xy.ie_y;
		}
		if ((coin->ie_Class == IECLASS_RAWMOUSE && coin->ie_Code != IECODE_NOBUTTON) || coin->ie_Class == IECLASS_RAWKEY || coin->ie_Class == IECLASS_NEWMOUSE)
		{
			if ((id->imsghigh > id->imsglow && !(id->imsghigh == MAXIMSGS - 1 && id->imsglow == 0)) || (id->imsghigh < id->imsglow && id->imsghigh != id->imsglow - 1) || id->imsglow == id->imsghigh)
			{
				CopyMem(coin, &id->imsgs[id->imsghigh], sizeof(id->imsgs[0]));
				id->imsghigh++;
				id->imsghigh %= MAXIMSGS;
			}
			else
			{
				DEBUGRING(kprintf("Fodquake: message dropped, imsglow = %d, imsghigh = %d\n", id->imsglow, id->imsghigh));
			}
		}

		if ((id->window->Flags & WFLG_WINDOWACTIVE) && coin->ie_Class == IECLASS_RAWMOUSE && screeninfront && id->window->MouseX > 0 && id->window->MouseY > 0)
		{
			if (id->grabmouse)
			{
				coin->ie_position.ie_xy.ie_x = 0;
				coin->ie_position.ie_xy.ie_y = 0;
			}
		}
	
		coin = coin->ie_NextEvent;
	} while (coin);

	return moo;
	
#ifdef AROS
	AROS_USERFUNC_EXIT
#endif
}

void Sys_Input_GrabMouse(void *inputdata, int dograb)
{
	struct inputdata *id = inputdata;

	id->grabmouse = dograb;	
}

