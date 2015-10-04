/*
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

#include <dinput.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"
#include "sys_win.h"
#include "in_dinput8.h"

#define DINPUTNUMEVENTS 32
#define NUMBUTTONEVENTS 32

#warning Make this get the repeat rate from the registry
#define REPEATINITIALDELAY 200000
#define REPEATDELAY 60000

struct buttonevent
{
	unsigned char key;
	unsigned char down;
};

struct InputData
{
	CRITICAL_SECTION mutex;

	LPDIRECTINPUT8 di8;
	LPDIRECTINPUTDEVICE8 di8mouse;
	LPDIRECTINPUTDEVICE8 di8keyboard;

	int doreacquiremouse;
	int doreacquirekeyboard;

	int mousegrabbed;

	int mousex;
	int mousey;

	struct buttonevent buttonevents[NUMBUTTONEVENTS];
	unsigned int buttoneventhead;
	unsigned int buttoneventtail;

	unsigned char repeatkey;
	unsigned long long nextrepeattime;
};

static const unsigned char keytable[] =
{
	0, /* 0 */
	K_ESCAPE,
	'1',
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9', /* 10 */
	'0',
	'-',
	'=',
	K_BACKSPACE,
	K_TAB,
	'q',
	'w',
	'e',
	'r',
	't', /* 20 */
	'y',
	'u',
	'i',
	'o',
	'p',
	'[',
	']',
	K_ENTER,
	K_LCTRL,
	'a', /* 30 */
	's',
	'd',
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'', /* 40 */
	'`',
	K_LSHIFT,
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm', /* 50 */
	',',
	'.',
	'/',
	K_RSHIFT,
	KP_STAR,
	K_LALT,
	' ',
	0,
	K_F1,
	K_F2, /* 60 */
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	KP_NUMLOCK,
	K_SCRLCK, /* 70 */
	KP_HOME,
	KP_UPARROW,
	KP_PGUP,
	KP_MINUS,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_PLUS,
	KP_END,
	KP_DOWNARROW, /* 80 */
	KP_PGDN,
	KP_INS,
	KP_DEL,
	0,
	0,
	0,
	K_F11,
	K_F12,
	0,
	0, /* 90 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 100 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 110 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 120 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 130 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 140 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 150 */
	0,
	0,
	0,
	0,
	0,
	KP_ENTER,
	K_RCTRL,
	0,
	0,
	0, /* 160 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 170 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 180 */
	KP_SLASH,
	0,
	0,
	K_RALT,
	0,
	0,
	0,
	0,
	0,
	0, /* 190 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_HOME,
	K_UPARROW, /* 200 */
	K_PGUP,
	0,
	K_LEFTARROW,
	0,
	K_RIGHTARROW,
	0,
	K_END,
	K_DOWNARROW,
	K_PGDN,
	K_INS, /* 210 */
	K_DEL,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
};

struct InputData *Sys_Input_Init(HWND window)
{
	struct InputData *id;
	DIPROPDWORD dipdw;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = DINPUTNUMEVENTS;

	id = malloc(sizeof(*id));
	if (id)
	{
		if (DirectInput8Create(global_hInstance, DIRECTINPUT_VERSION, &IID_IDirectInput8A, (void **)&id->di8, 0) == DI_OK)
		{
			if (id->di8->lpVtbl->CreateDevice(id->di8, &GUID_SysMouse, &id->di8mouse, 0) == DI_OK)
			{
				if (id->di8mouse->lpVtbl->SetDataFormat(id->di8mouse, &c_dfDIMouse2) == DI_OK)
				{
					if (id->di8mouse->lpVtbl->SetCooperativeLevel(id->di8mouse, window, DISCL_FOREGROUND|DISCL_EXCLUSIVE) == DI_OK)
					{
						if (id->di8mouse->lpVtbl->SetProperty(id->di8mouse, DIPROP_BUFFERSIZE, &dipdw.diph) == DI_OK)
						{
							if (id->di8->lpVtbl->CreateDevice(id->di8, &GUID_SysKeyboard, &id->di8keyboard, 0) == DI_OK)
							{
								if (id->di8keyboard->lpVtbl->SetDataFormat(id->di8keyboard, &c_dfDIKeyboard) == DI_OK)
								{
									if (id->di8keyboard->lpVtbl->SetCooperativeLevel(id->di8keyboard, window, DISCL_FOREGROUND|DISCL_NONEXCLUSIVE|DISCL_NOWINKEY) == DI_OK)
									{
										if (id->di8keyboard->lpVtbl->SetProperty(id->di8keyboard, DIPROP_BUFFERSIZE, &dipdw.diph) == DI_OK)
										{
											InitializeCriticalSection(&id->mutex);

											id->di8keyboard->lpVtbl->Acquire(id->di8keyboard);

											id->doreacquiremouse = 0;
											id->doreacquirekeyboard = 0;

											id->mousegrabbed = 0;

											id->mousex = 0;
											id->mousey = 0;

											id->buttoneventhead = 0;
											id->buttoneventtail = 0;

											return id;
										}
									}
								}

								id->di8keyboard->lpVtbl->Release(id->di8keyboard);
							}
						}
					}
				}

				id->di8mouse->lpVtbl->Release(id->di8mouse);
			}

			id->di8->lpVtbl->Release(id->di8);
		}

		free(id);
	}

	return 0;
}

void Sys_Input_Shutdown(struct InputData *inputdata)
{
	inputdata->di8keyboard->lpVtbl->Unacquire(inputdata->di8keyboard);
	inputdata->di8keyboard->lpVtbl->Release(inputdata->di8keyboard);

	inputdata->di8mouse->lpVtbl->Unacquire(inputdata->di8mouse);
	inputdata->di8mouse->lpVtbl->Release(inputdata->di8mouse);

	inputdata->di8->lpVtbl->Release(inputdata->di8);

	free(inputdata);
}

void Sys_Input_MainThreadFrameStart(struct InputData *inputdata)
{
	EnterCriticalSection(&inputdata->mutex);

	if (inputdata->doreacquiremouse)
	{
		inputdata->di8mouse->lpVtbl->Acquire(inputdata->di8mouse);
		inputdata->doreacquiremouse = 0;
	}

	if (inputdata->doreacquirekeyboard)
	{
		inputdata->di8keyboard->lpVtbl->Acquire(inputdata->di8keyboard);
		inputdata->doreacquirekeyboard = 0;
	}

	LeaveCriticalSection(&inputdata->mutex);
}

static void queuekey(struct InputData *inputdata, unsigned char key, unsigned char down)
{
	inputdata->buttonevents[inputdata->buttoneventhead].key = key;
	inputdata->buttonevents[inputdata->buttoneventhead].down = down;
	inputdata->buttoneventhead = (inputdata->buttoneventhead + 1) % NUMBUTTONEVENTS;
}

static void keyevent(struct InputData *inputdata, unsigned char rawkey, unsigned char down)
{
	if (rawkey < sizeof(keytable) && keytable[rawkey])
	{
		queuekey(inputdata, keytable[rawkey], down);
	}
	else if (developer.value)
		Com_Printf("No mapping for key %d\n", rawkey);
}

static void pollstuff(struct InputData *inputdata)
{
	DIDEVICEOBJECTDATA events[DINPUTNUMEVENTS];
	DWORD elements;
	HRESULT res;
	unsigned int i;
	unsigned long long curtime;

	EnterCriticalSection(&inputdata->mutex);

	if (inputdata->mousegrabbed)
	{
		elements = DINPUTNUMEVENTS;
		res = inputdata->di8mouse->lpVtbl->GetDeviceData(inputdata->di8mouse, sizeof(*events), events, &elements, 0);
		if (res != DI_OK)
		{
#warning Should release all pressed buttons here.

			inputdata->doreacquiremouse = 1;
		}
		else
		{
			for(i=0;i<elements;i++)
			{
				switch(events[i].dwOfs)
				{
					case DIMOFS_X:
						inputdata->mousex += events[i].dwData;
						break;
					case DIMOFS_Y:
						inputdata->mousey += events[i].dwData;
						break;

					case DIMOFS_Z:
						if ((int)events[i].dwData > 0)
						{
							queuekey(inputdata, K_MWHEELUP, 1);
							queuekey(inputdata, K_MWHEELUP, 0);
						}
						else
						{
							queuekey(inputdata, K_MWHEELDOWN, 1);
							queuekey(inputdata, K_MWHEELDOWN, 0);
						}
						break;

					case DIMOFS_BUTTON0:
						queuekey(inputdata, K_MOUSE1, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON1:
						queuekey(inputdata, K_MOUSE2, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON2:
						queuekey(inputdata, K_MOUSE3, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON3:
						queuekey(inputdata, K_MOUSE4, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON4:
						queuekey(inputdata, K_MOUSE5, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON5:
						queuekey(inputdata, K_MOUSE6, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON6:
						queuekey(inputdata, K_MOUSE7, !!(events[i].dwData&0x80));
						break;

					case DIMOFS_BUTTON7:
						queuekey(inputdata, K_MOUSE8, !!(events[i].dwData&0x80));
						break;
				}
			}
		}
	}

	elements = DINPUTNUMEVENTS;
	res = inputdata->di8keyboard->lpVtbl->GetDeviceData(inputdata->di8keyboard, sizeof(*events), events, &elements, 0);
	if (res != DI_OK)
	{
#warning Should release all pressed buttons here.

		inputdata->doreacquirekeyboard = 1;
	}
	else
	{
		if (inputdata->repeatkey)
		{
			curtime = Sys_IntTime();

			while(inputdata->nextrepeattime <= curtime)
			{
				keyevent(inputdata, inputdata->repeatkey, 1);
				inputdata->nextrepeattime += REPEATDELAY;
			}
		}

		for(i=0;i<elements;i++)
		{
			if (events[i].dwOfs != DIK_LSHIFT
			 || events[i].dwOfs != DIK_RSHIFT
			 || events[i].dwOfs != DIK_LCONTROL
			 || events[i].dwOfs != DIK_RCONTROL
			 || events[i].dwOfs != DIK_LWIN
			 || events[i].dwOfs != DIK_RWIN
			 || events[i].dwOfs != DIK_LMENU
			 || events[i].dwOfs != DIK_RMENU)
			{
				if ((events[i].dwData&0x80))
				{
					inputdata->repeatkey = events[i].dwOfs;
					inputdata->nextrepeattime = Sys_IntTime() + REPEATINITIALDELAY;
				}
				else if (inputdata->repeatkey == events[i].dwOfs)
				{
					inputdata->repeatkey = 0;
					inputdata->nextrepeattime = 0;
				}
			}

			keyevent(inputdata, events[i].dwOfs, !!(events[i].dwData&0x80));
		}
	}

	LeaveCriticalSection(&inputdata->mutex);
}

int Sys_Input_GetKeyEvent(struct InputData *inputdata, keynum_t *keynum, qboolean *down)
{
	pollstuff(inputdata);

	if (inputdata->buttoneventhead != inputdata->buttoneventtail)
	{
		*keynum = inputdata->buttonevents[inputdata->buttoneventtail].key;
		*down = inputdata->buttonevents[inputdata->buttoneventtail].down;

		inputdata->buttoneventtail = (inputdata->buttoneventtail + 1) % NUMBUTTONEVENTS;

		return 1;
	}

	return 0;
}

void Sys_Input_GetMouseMovement(struct InputData *inputdata, int *mousex, int *mousey)
{
	pollstuff(inputdata);

	*mousex = inputdata->mousex;
	*mousey = inputdata->mousey;

	inputdata->mousex = 0;
	inputdata->mousey = 0;
}

void Sys_Input_GrabMouse(struct InputData *inputdata, int dograb)
{
	EnterCriticalSection(&inputdata->mutex);

	if (dograb && !inputdata->mousegrabbed)
	{
		inputdata->di8mouse->lpVtbl->Acquire(inputdata->di8mouse);
		inputdata->mousegrabbed = 1;
		inputdata->doreacquiremouse = 0;
	}
	else if (!dograb && inputdata->mousegrabbed)
	{
		inputdata->di8mouse->lpVtbl->Unacquire(inputdata->di8mouse);
		inputdata->mousegrabbed = 0;
		inputdata->doreacquiremouse = 0;
	}

	LeaveCriticalSection(&inputdata->mutex);
}

void Sys_Input_ClearRepeat(struct InputData *inputdata)
{
	inputdata->repeatkey = 0;
	inputdata->nextrepeattime = 0;
}

