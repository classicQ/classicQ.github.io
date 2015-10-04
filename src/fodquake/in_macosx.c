/*
Copyright (C) 2011-2014 Florian Zwoch
Copyright (C) 2011-2014 Mark Olsen

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

#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hidsystem/event_status_driver.h>

#undef true
#undef false

#include <pthread.h>

#include "keys.h"
#include "in_macosx.h"
#include "vid.h"
#include "sys.h"

#define NUMBUTTONEVENTS 32

static const unsigned char keytable[] =
{
	0, /* 0 */
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
	'\\',
	'\\', /* 50 */
	';',
	'\'',
	'~',
	',',
	'.',
	'/',
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3, /* 60 */
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,
	0, /* 70 */
	0,
	0,
	0,
	K_HOME,
	K_PGUP,
	K_DEL,
	K_END,
	K_PGDN,
	K_RIGHTARROW,
	K_LEFTARROW, /* 80 */
	K_DOWNARROW,
	K_UPARROW,
	KP_NUMLOCK,
	KP_SLASH,
	KP_STAR,
	KP_MINUS,
	KP_PLUS,
	KP_ENTER,
	KP_END,
	KP_DOWNARROW, /* 90 */
	KP_PGDN,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_END,
	KP_UPARROW,
	KP_PGUP,
	KP_INS,
	KP_DEL,
	'~', /* 100 */
	0,
	0,
	'=',
	0, //K_F13
	0, //K_F14
	0, //K_F15
	0, //K_F16
	0, //K_F17
	0, //K_F18
	0, //K_F19 /* 110 */
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
	0,
	0,
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
	0,
	0,
	0,
	0,
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
	0,
	0, /* 200 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 210 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 220 */
	0,
	0,
	0,
	K_LCTRL,
	K_LSHIFT,
	K_LALT,
	K_LWIN,
	0,
	K_RSHIFT,
	K_RALT, /* 230 */
	K_RWIN,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 240 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 250 */
	0,
	0,
	0,
	0,
	0
};

struct buttonevent
{
	keynum_t key;
	qboolean down;
};

struct input_data
{
	pthread_t thread;
	pthread_mutex_t mouse_mutex;
	pthread_mutex_t key_mutex;

	CFRunLoopRef threadrunloop;
	pthread_mutex_t thread_mutex;
	pthread_cond_t thread_has_spawned;
	qboolean thread_shutdown;

	IOHIDManagerRef hid_manager;

	int ignore_mouse;
	qboolean left_cmd_key_active;
	qboolean right_cmd_key_active;
	qboolean fn_key_active;
	int fn_key_behavior;

	int mouse_x;
	int mouse_y;

	struct buttonevent buttonevents[NUMBUTTONEVENTS];
	unsigned int buttoneventhead;
	unsigned int buttoneventtail;

	unsigned char repeatkey;
	unsigned long long nextrepeattime;

	int key_repeat_initial_delay;
	int key_repeat_delay;
};

static void sequencepointkthx()
{
}

static void add_to_event_queue(struct input_data *input, keynum_t key, qboolean down)
{
	if ((input->buttoneventhead + 1) % NUMBUTTONEVENTS == input->buttoneventtail)
	{
		return;
	}

	if ((input->fn_key_behavior && input->fn_key_active) || (!input->fn_key_behavior && !input->fn_key_active))
	{
		if (key >= K_F1 && key <= K_F12)
		{
			return;
		}
	}

	input->buttonevents[input->buttoneventhead].key = key;
	input->buttonevents[input->buttoneventhead].down = down;

	sequencepointkthx();

	input->buttoneventhead = (input->buttoneventhead + 1) % NUMBUTTONEVENTS;
}

static void input_callback(void *context, IOReturn result, void *sender, IOHIDValueRef value)
{
	struct input_data *input = (struct input_data*)context;
	IOHIDElementRef elem = IOHIDValueGetElement(value);
	uint32_t page = IOHIDElementGetUsagePage(elem);
	uint32_t usage = IOHIDElementGetUsage(elem);
	uint32_t val = IOHIDValueGetIntegerValue(value);

	if (page == kHIDPage_GenericDesktop)
	{
		if (input->ignore_mouse)
		{
			return;
		}

		switch (usage)
		{
			case kHIDUsage_GD_X:
				pthread_mutex_lock(&input->mouse_mutex);
				input->mouse_x += val;
				pthread_mutex_unlock(&input->mouse_mutex);
				break;
			case kHIDUsage_GD_Y:
				pthread_mutex_lock(&input->mouse_mutex);
				input->mouse_y += val;
				pthread_mutex_unlock(&input->mouse_mutex);
				break;
			case kHIDUsage_GD_Wheel:
				if ((int32_t)val > 0)
				{
					add_to_event_queue(input, K_MWHEELUP, true);
					add_to_event_queue(input, K_MWHEELUP, false);
				}
				else if ((int32_t)val < 0)
				{
					add_to_event_queue(input, K_MWHEELDOWN, true);
					add_to_event_queue(input, K_MWHEELDOWN, false);
				}
				break;
			default:
				break;
		}
	}
	else if (page == kHIDPage_Button)
	{
		if (input->ignore_mouse)
		{
			return;
		}

		if (usage < 1 || usage > 10)
		{
			usage = 10;
		}

		add_to_event_queue(input, K_MOUSE1 + usage - 1, val ? true : false);
	}
	else if (page == kHIDPage_KeyboardOrKeypad)
	{
		if (usage == kHIDUsage_KeyboardLeftGUI)
		{
			input->left_cmd_key_active = val ? true : false;
		}
		else if (usage == kHIDUsage_KeyboardRightGUI)
		{
			input->right_cmd_key_active = val ? true : false;
		}

		if (usage < sizeof(keytable) && (input->left_cmd_key_active || input->right_cmd_key_active))
		{
			if (keytable[usage] == 'c' && val)
			{
				add_to_event_queue(input, K_COPY, true);
				add_to_event_queue(input, K_COPY, false);
			}
			else if (keytable[usage] == 'v' && val)
			{
				add_to_event_queue(input, K_PASTE, true);
				add_to_event_queue(input, K_PASTE, false);
			}

			return;
		}

		if (usage < sizeof(keytable))
		{
			add_to_event_queue(input, keytable[usage], val ? true : false);

			pthread_mutex_lock(&input->key_mutex);

			if (val)
			{
				input->repeatkey = keytable[usage];
				input->nextrepeattime = Sys_IntTime() + input->key_repeat_initial_delay;
			}
			else
			{
				input->repeatkey = 0;
				input->nextrepeattime = 0;
			}

			pthread_mutex_unlock(&input->key_mutex);
		}
	}
	else if (page == 0xFF)
	{
		if (usage == kHIDUsage_KeyboardErrorUndefined)
		{
			input->fn_key_active = val ? true : false;
		}
	}
}

static void *Sys_Input_Thread(void *inarg)
{
	struct input_data *input;
	IOReturn tIOReturn;

	input = inarg;

	pthread_mutex_lock(&input->thread_mutex);
	input->threadrunloop = CFRunLoopGetCurrent();
	pthread_cond_signal(&input->thread_has_spawned);
	pthread_mutex_unlock(&input->thread_mutex);

	input->hid_manager = IOHIDManagerCreate(kCFAllocatorSystemDefault, kIOHIDOptionsTypeNone);
	if (input->hid_manager)
	{
		IOHIDManagerSetDeviceMatching(input->hid_manager, NULL);
		IOHIDManagerRegisterInputValueCallback(input->hid_manager, input_callback, input);
		IOHIDManagerScheduleWithRunLoop(input->hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

		tIOReturn = IOHIDManagerOpen(input->hid_manager, kIOHIDOptionsTypeNone);
		if (tIOReturn == kIOReturnSuccess)
		{
			do
			{
				CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, true);
			}
			while (!input->thread_shutdown);
		}

		IOHIDManagerUnscheduleFromRunLoop(input->hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

	}

	CFRelease(input->hid_manager);

	return 0;
}

struct input_data *Sys_Input_Init()
{
	struct input_data *input;

	input = (struct input_data*)malloc(sizeof(struct input_data));
	if (input)
	{
		memset(input, 0, sizeof(struct input_data));

		input->ignore_mouse = 1;
		input->key_repeat_initial_delay = 500000;
		input->key_repeat_delay = 100000;
		
		NXEventHandle handle = NXOpenEventStatus();
		if (handle)
		{
			input->key_repeat_initial_delay = NXKeyRepeatThreshold(handle) * 1000000; 
			input->key_repeat_delay = NXKeyRepeatInterval(handle) * 1000000;
			
			NXCloseEventStatus(handle);
		}

		if (pthread_mutex_init(&input->mouse_mutex, 0) == 0)
		{
			if (pthread_mutex_init(&input->key_mutex, 0) == 0)
			{
				if (pthread_mutex_init(&input->thread_mutex, 0) == 0)
				{
					if (pthread_cond_init(&input->thread_has_spawned, 0) == 0)
					{
						pthread_mutex_lock(&input->thread_mutex);

						if (pthread_create(&input->thread, 0, Sys_Input_Thread, input) == 0)
						{
							pthread_cond_wait(&input->thread_has_spawned, &input->thread_mutex);
							pthread_mutex_unlock(&input->thread_mutex);

							return input;
						}

						pthread_mutex_unlock(&input->thread_mutex);
						pthread_cond_destroy(&input->thread_has_spawned);
					}

					pthread_mutex_destroy(&input->thread_mutex);
				}

				pthread_mutex_destroy(&input->key_mutex);
			}

			pthread_mutex_destroy(&input->mouse_mutex);
		}

		free(input);
	}

	return NULL;
}

void Sys_Input_Shutdown(struct input_data *input)
{
	input->thread_shutdown = true;
	CFRunLoopStop(input->threadrunloop);

	pthread_join(input->thread, 0);

	pthread_mutex_destroy(&input->mouse_mutex);
	pthread_mutex_destroy(&input->key_mutex);
	pthread_mutex_destroy(&input->thread_mutex);
	pthread_cond_destroy(&input->thread_has_spawned);

	free(input);
}

int Sys_Input_GetKeyEvent(struct input_data *input, keynum_t *keynum, qboolean *down)
{
	pthread_mutex_lock(&input->key_mutex);
	
	if (input->repeatkey)
	{
		long long curtime = Sys_IntTime();

		while (input->nextrepeattime <= curtime)
		{
			add_to_event_queue(input, input->repeatkey, true);
			input->nextrepeattime += input->key_repeat_delay;
		}
	}
	
	pthread_mutex_unlock(&input->key_mutex);

	if (input->buttoneventhead == input->buttoneventtail)
	{
		return 0;
	}

	*keynum = input->buttonevents[input->buttoneventtail].key;
	*down = input->buttonevents[input->buttoneventtail].down;

	sequencepointkthx();

	input->buttoneventtail = (input->buttoneventtail + 1) % NUMBUTTONEVENTS;

	return 1;
}

void Sys_Input_GetMouseMovement(struct input_data *input, int *mouse_x, int *mouse_y)
{
	pthread_mutex_lock(&input->mouse_mutex);

	*mouse_x = input->mouse_x;
	*mouse_y = input->mouse_y;

	input->mouse_x = 0;
	input->mouse_y = 0;

	pthread_mutex_unlock(&input->mouse_mutex);
}

void Sys_Input_GrabMouse(struct input_data *input, int dograb)
{
	if (dograb)
	{
		input->ignore_mouse = 0;
	}
	else
	{
		input->ignore_mouse = 1;
	}
}

void Sys_Input_SetFnKeyBehavior(struct input_data *input, int behavior)
{
	input->fn_key_behavior = behavior;
}
