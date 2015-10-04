/*
Copyright (C) 1996-1997 Id Software, Inc.
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

#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86dga.h>
#include <X11/extensions/XInput.h>

#include <dlfcn.h>

#include "quakedef.h"
#include "input.h"
#include "keys.h"

#include "sys_thread.h"
#include "in_x11.h"

#define XINPUTFLAGS (KeyPressMask|KeyReleaseMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask)

cvar_t cl_keypad = { "cl_keypad", "1" };
cvar_t in_dga_mouse = { "in_dga_mouse", "1" };

typedef struct
{
	int key;
	int down;
} keyq_t;

struct xpropertyrestore
{
	struct xpropertyrestore *next;
	XID deviceid;
	Atom property;
	char oldvalue;
};

struct inputdata
{
	struct SysMutex *mutex;
	Display *x_disp;
	Window x_win;
	int window_focused;
	unsigned int windowwidth;
	unsigned int windowheight;
	int fullscreen;

	const unsigned char *keytable;
	unsigned int keytablesize;

	int config_notify;
	int config_notify_width;
	int config_notify_height;

	keyq_t keyq[64];
	int keyq_head;
	int keyq_tail;

	unsigned int shift_down;

	int mousex;
	int mousey;

	int grab_mouse;
	int dga_mouse_enabled;
	int mouse_grabbed;

	void *libxi;
	XDevice *(*__XOpenDevice)(Display *display, XID device_id);
	void (*__XCloseDevice)(Display *display, XDevice *device);
	XDeviceInfo *(*__XListInputDevices)(Display *display, int *ndevices_return);
	int (*__XFreeDeviceList)(XDeviceInfo *list);
	Atom *(*__XListDeviceProperties)(Display *display, XDevice *device, int *nprops_return);
	int (*__XGetDeviceProperty)(Display *display, XDevice *device, Atom property, long offset, long length, Bool delete, Atom req_type, Atom *actual_type_return, int *actual_format_return, unsigned long *nitems_return, unsigned long *bytes_after_return, unsigned char **prop_return);
	void (*__XChangeDeviceProperty)(Display *display, XDevice *device, Atom property, Atom type, int format, int mode, const char *data, int nelements);

	struct xpropertyrestore *xpropertyrestore;
};

static void open_libxi(struct inputdata *id)
{
	/* libXi will unfortunately leak memory on each use... However there's no easy way around it really :( */

	id->libxi = dlopen("libXi.so", RTLD_NOW);
	if (id->libxi == 0)
		id->libxi = dlopen("libXi.so.6", RTLD_NOW); /* fffffffuuuuuuuuuuuuu */

	if (id->libxi)
	{
		id->__XOpenDevice = dlsym(id->libxi, "XOpenDevice");
		id->__XCloseDevice = dlsym(id->libxi, "XCloseDevice");
		id->__XListInputDevices = dlsym(id->libxi, "XListInputDevices");
		id->__XFreeDeviceList = dlsym(id->libxi, "XFreeDeviceList");
		id->__XListDeviceProperties = dlsym(id->libxi, "XListDeviceProperties");
		id->__XGetDeviceProperty = dlsym(id->libxi, "XGetDeviceProperty");
		id->__XChangeDeviceProperty = dlsym(id->libxi, "XChangeDeviceProperty");

		if (id->__XOpenDevice
		 && id->__XCloseDevice
		 && id->__XListInputDevices
		 && id->__XFreeDeviceList
		 && id->__XListDeviceProperties
		 && id->__XGetDeviceProperty
		 && id->__XChangeDeviceProperty)
		{
			return;
		}

		dlclose(id->libxi);
		id->libxi = 0;
	}
}

static void enable_middle_button_emulation(struct inputdata *id)
{
	struct xpropertyrestore *xpropertyrestore;
	struct xpropertyrestore *next;
	XDevice *xd;

	next = id->xpropertyrestore;
	while((xpropertyrestore = next))
	{
		xd = id->__XOpenDevice(id->x_disp, xpropertyrestore->deviceid);
		if (xd)
		{
			id->__XChangeDeviceProperty(id->x_disp, xd, xpropertyrestore->property, XA_INTEGER, 8, PropModeReplace, &xpropertyrestore->oldvalue, 1);

			id->__XCloseDevice(id->x_disp, xd);
		}

		next = xpropertyrestore->next;
		free(xpropertyrestore);
	}
}

static void disable_middle_button_emulation(struct inputdata *id)
{
	XDeviceInfo *xdi;
	XDevice *xd;
	Atom *xprops;
	char *name;
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *propdata;
	int numxdi;
	int numxprops;
	int i;
	int j;
	int ret;
	char oldval;
	static const char newval = 0;
	struct xpropertyrestore *xpropertyrestore;

	xdi = id->__XListInputDevices(id->x_disp, &numxdi);
	if (xdi)
	{
		for(i=0;i<numxdi;i++)
		{
			if (xdi[i].use != IsXExtensionPointer)
				continue;

			xd = id->__XOpenDevice(id->x_disp, xdi[i].id);
			if (xd)
			{
				xprops = id->__XListDeviceProperties(id->x_disp, xd, &numxprops);
				if (xprops)
				{
					for(j=0;j<numxprops;j++)
					{
						name = XGetAtomName(id->x_disp, xprops[j]);
						if (name)
						{
							if (strcmp(name, "Evdev Middle Button Emulation") == 0)
							{
								ret = id->__XGetDeviceProperty(id->x_disp, xd, xprops[j], 0, 1, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &propdata);
								if (ret == Success)
								{
									if (actual_format == 8 && actual_type == XA_INTEGER)
									{
										oldval = *(unsigned char *)propdata;
										if (oldval)
										{
											xpropertyrestore = malloc(sizeof(*xpropertyrestore));
											if (xpropertyrestore)
											{
												xpropertyrestore->next = id->xpropertyrestore;
												xpropertyrestore->deviceid = xdi[i].id;
												xpropertyrestore->property = xprops[j];
												xpropertyrestore->oldvalue = oldval;

												id->xpropertyrestore = xpropertyrestore;

												id->__XChangeDeviceProperty(id->x_disp, xd, xprops[j], XA_INTEGER, 8, PropModeReplace, &newval, 1);
											}
										}
									}

									XFree(propdata);
								}
							}

							XFree(name);
						}
					}

					XFree(xprops);
				}

				id->__XCloseDevice(id->x_disp, xd);
			}
		}

		id->__XFreeDeviceList(xdi);
	}
}

static int is_evdev_rules(struct inputdata *id)
{
	Window window;
	Atom actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes;
	unsigned char *data;
	int status;
	int ret;

	ret = 0;

	window = RootWindow(id->x_disp, 0);

	status = XGetWindowProperty(id->x_disp, window, XInternAtom(id->x_disp, "_XKB_RULES_NAMES", True), 0, ~0, False, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes, &data);
	if (status == Success)
	{
		if (strcmp((char *)data, "evdev") == 0)
			ret = 1;

		XFree(data);
	}

	return ret;
}

static void DoGrabMouse(struct inputdata *id)
{
	Window grab_win;
	int dgaflags;

	if (id->mouse_grabbed)
		return;

	XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS&(~PointerMotionMask));

	if (in_dga_mouse.value)
	{
		grab_win = DefaultRootWindow(id->x_disp);
	}
	else
	{
		grab_win = id->x_win;

		XWarpPointer(id->x_disp, None, id->x_win, 0, 0, 0, 0, id->windowwidth/2, id->windowheight/2);
	}

	XGrabPointer(id->x_disp, grab_win, True, PointerMotionMask | ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync, grab_win, None, CurrentTime);

	if (in_dga_mouse.value)
	{
		XF86DGAQueryDirectVideo(id->x_disp, DefaultScreen(id->x_disp), &dgaflags);

		if ((dgaflags&XF86DGADirectPresent))
		{
			if (XF86DGADirectVideo(id->x_disp, DefaultScreen(id->x_disp), XF86DGADirectMouse))
			{
				id->dga_mouse_enabled = 1;
			}
		}
	}

	if (id->dga_mouse_enabled)
		XSelectInput(id->x_disp, grab_win, PointerMotionMask);
	else
		XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS);

	id->mouse_grabbed = 1;
}

static void DoUngrabMouse(struct inputdata *id)
{
	if (!id->mouse_grabbed)
		return;

	if (id->dga_mouse_enabled)
	{
		XSelectInput(id->x_disp, DefaultRootWindow(id->x_disp), 0);
		id->dga_mouse_enabled = 0;
		XF86DGADirectVideo(id->x_disp, DefaultScreen(id->x_disp), 0);
	}
	XUngrabPointer(id->x_disp, CurrentTime);
	XSelectInput(id->x_disp, id->x_win, StructureNotifyMask | KeyPressMask | KeyReleaseMask | ExposureMask | ButtonPressMask | ButtonReleaseMask);

	id->mouse_grabbed = 0;
}

static const unsigned char keytable_xorg[] =
{
	0, /* 0 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_ESCAPE,
	'1', /* 10 */
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	'-', /* 20 */
	'=',
	K_BACKSPACE,
	K_TAB,
	'q',
	'w',
	'e',
	'r',
	't',
	'y',
	'u', /* 30 */
	'i',
	'o',
	'p',
	'[',
	']',
	K_ENTER,
	K_LCTRL,
	'a',
	's',
	'd', /* 40 */
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'',
	'`',
	K_LSHIFT, /* 50 */
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.', /* 60 */
	'/',
	K_RSHIFT,
	KP_STAR,
	K_LALT,
	' ',
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3,
	K_F4, /* 70 */
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	KP_NUMLOCK,
	0,
	KP_HOME,
	KP_UPARROW, /* 80 */
	KP_PGUP,
	KP_MINUS,
	KP_LEFTARROW,
	KP_5,
	KP_RIGHTARROW,
	KP_PLUS,
	KP_END,
	KP_DOWNARROW,
	KP_PGDN,
	KP_INS, /* 90 */
	KP_DEL,
	0,
	0,
	'<',
	K_F11,
	K_F12,
	K_HOME,
	K_UPARROW,
	K_PGUP,
	K_LEFTARROW, /* 100 */
	0,
	K_RIGHTARROW,
	K_END,
	K_DOWNARROW,
	K_PGDN,
	K_INS,
	K_DEL,
	KP_ENTER,
	K_RCTRL,
	0, /* 110 */
	0,
	KP_SLASH,
	K_RALT,
	0,
	K_LWIN,
	K_RWIN,
	K_MENU,
};

static const unsigned char keytable_evdev[] =
{
	0, /* 0 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	K_ESCAPE,
	'1', /* 10 */
	'2',
	'3',
	'4',
	'5',
	'6',
	'7',
	'8',
	'9',
	'0',
	'-', /* 20 */
	'=',
	K_BACKSPACE,
	K_TAB,
	'q',
	'w',
	'e',
	'r',
	't',
	'y',
	'u', /* 30 */
	'i',
	'o',
	'p',
	'[',
	']',
	K_ENTER,
	K_LCTRL,
	'a',
	's',
	'd', /* 40 */
	'f',
	'g',
	'h',
	'j',
	'k',
	'l',
	';',
	'\'',
	'`',
	K_LSHIFT, /* 50 */
	'\\',
	'z',
	'x',
	'c',
	'v',
	'b',
	'n',
	'm',
	',',
	'.', /* 60 */
	'/',
	K_RSHIFT,
	0,
	K_LALT,
	' ',
	K_CAPSLOCK,
	K_F1,
	K_F2,
	K_F3,
	K_F4, /* 70 */
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	0,
	0,
	0,
	0, /* 80 */
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0, /* 90 */
	0,
	0,
	0,
	'<',
	K_F11,
	K_F12,
	0,
	0,
	0,
	0, /* 100 */
	0,
	0,
	0,
	0,
	K_RCTRL,
	0,
	0,
	K_RALT,
	0,
	K_HOME, /* 110 */
	K_UPARROW,
	K_PGUP,
	K_LEFTARROW,
	K_RIGHTARROW,
	K_END,
	K_DOWNARROW,
	K_PGDN,
	K_INS,
	K_DEL,
	0, /* 120 */
	0,
	0,
	0,
	0,
	0,
	0,
	K_PAUSE,
	0,
	0,
	0, /* 130 */
	0,
	0,
	K_LWIN,
	K_RWIN,
	K_MENU,
	0,
	0,
	0,
	0,
};

static int XLateKey(struct inputdata *id, XKeyEvent * ev)
{
	int key, kp;
	char buf[64];
	KeySym keysym;

	if (ev->keycode < id->keytablesize)
	{
		key = id->keytable[ev->keycode];
		if (key)
			return key;
	}

	key = 0;
	kp = (int) cl_keypad.value;

	XLookupString(ev, buf, sizeof buf, &keysym, 0);

	switch (keysym)
	{
		case XK_Scroll_Lock:
			key = K_SCRLCK;
			break;

		case XK_Caps_Lock:
			key = K_CAPSLOCK;
			break;

		case XK_Num_Lock:
			key = kp ? KP_NUMLOCK : K_PAUSE;
			break;

		case XK_KP_Page_Up:
			key = kp ? KP_PGUP : K_PGUP;
			break;
		case XK_Page_Up:
			key = K_PGUP;
			break;

		case XK_KP_Page_Down:
			key = kp ? KP_PGDN : K_PGDN;
			break;
		case XK_Page_Down:
			key = K_PGDN;
			break;

		case XK_KP_Home:
			key = kp ? KP_HOME : K_HOME;
			break;
		case XK_Home:
			key = K_HOME;
			break;

		case XK_KP_End:
			key = kp ? KP_END : K_END;
			break;
		case XK_End:
			key = K_END;
			break;

		case XK_KP_Left:
			key = kp ? KP_LEFTARROW : K_LEFTARROW;
			break;
		case XK_Left:
			key = K_LEFTARROW;
			break;

		case XK_KP_Right:
			key = kp ? KP_RIGHTARROW : K_RIGHTARROW;
			break;
		case XK_Right:
			key = K_RIGHTARROW;
			break;

		case XK_KP_Down:
			key = kp ? KP_DOWNARROW : K_DOWNARROW;
			break;

		case XK_Down:
			key = K_DOWNARROW;
			break;

		case XK_KP_Up:
			key = kp ? KP_UPARROW : K_UPARROW;
			break;

		case XK_Up:
			key = K_UPARROW;
			break;

		case XK_Escape:
			key = K_ESCAPE;
			break;

		case XK_KP_Enter:
			key = kp ? KP_ENTER : K_ENTER;
			break;

		case XK_Return:
			key = K_ENTER;
			break;

		case XK_Tab:
			key = K_TAB;
			break;

		case XK_F1:
			key = K_F1;
			break;

		case XK_F2:
			key = K_F2;
			break;

		case XK_F3:
			key = K_F3;
			break;

		case XK_F4:
			key = K_F4;
			break;

		case XK_F5:
			key = K_F5;
			break;

		case XK_F6:
			key = K_F6;
			break;

		case XK_F7:
			key = K_F7;
			break;

		case XK_F8:
			key = K_F8;
			break;

		case XK_F9:
			key = K_F9;
			break;

		case XK_F10:
			key = K_F10;
			break;

		case XK_F11:
			key = K_F11;
			break;

		case XK_F12:
			key = K_F12;
			break;

		case XK_BackSpace:
			key = K_BACKSPACE;
			break;

		case XK_KP_Delete:
			key = kp ? KP_DEL : K_DEL;
			break;
		case XK_Delete:
			key = K_DEL;
			break;

		case XK_Pause:
			key = K_PAUSE;
			break;

		case XK_Shift_L:
			key = K_LSHIFT;
			break;
		case XK_Shift_R:
			key = K_RSHIFT;
			break;

		case XK_Execute:
		case XK_Control_L:
			key = K_LCTRL;
			break;
		case XK_Control_R:
			key = K_RCTRL;
			break;

		case XK_Alt_L:
		case XK_Meta_L:
			key = K_LALT;
			break;
		case XK_Alt_R:
		case XK_Meta_R:
			key = K_RALT;
			break;

		case XK_Super_L:
			key = K_LWIN;
			break;
		case XK_Super_R:
			key = K_RWIN;
			break;
		case XK_Menu:
			key = K_MENU;
			break;

		case XK_KP_Begin:
			key = kp ? KP_5 : '5';
			break;

		case XK_KP_Insert:
			key = kp ? KP_INS : K_INS;
			break;
		case XK_Insert:
			key = K_INS;
			break;

		case XK_KP_Multiply:
			key = kp ? KP_STAR : '*';
			break;

		case XK_KP_Add:
			key = kp ? KP_PLUS : '+';
			break;

		case XK_KP_Subtract:
			key = kp ? KP_MINUS : '-';
			break;

		case XK_KP_Divide:
			key = kp ? KP_SLASH : '/';
			break;

		default:
			key = *(unsigned char *) buf;
			if (key >= 'A' && key <= 'Z')
				key = key - 'A' + 'a';

			break;
	}
	return key;
}

static void GetEvents(struct inputdata *id)
{
	XEvent event;

	int newmousex;
	int newmousey;
	
	newmousex = id->windowwidth/2;
	newmousey = id->windowheight/2;

	Sys_Thread_LockMutex(id->mutex);

	XSync(id->x_disp, 0);

	while(XPending(id->x_disp))
	{
		XNextEvent(id->x_disp, &event);
		switch (event.type)
		{
			case FocusIn:
				id->window_focused = 1;
				if (id->grab_mouse)
					DoGrabMouse(id);
				break;
			case FocusOut:
				id->window_focused = 0;
				DoUngrabMouse(id);
				break;
			case KeyPress:
				id->keyq[id->keyq_head].key = XLateKey(id, &event.xkey);
				id->keyq[id->keyq_head].down = true;
				if (id->keyq[id->keyq_head].key == K_LSHIFT)
					id->shift_down |= 1;
				else if (id->keyq[id->keyq_head].key == K_RSHIFT)
					id->shift_down |= 2;

				if (id->keyq[id->keyq_head].key == K_INS && id->shift_down)
				{
					id->keyq[id->keyq_head].key = K_PASTE;
					id->keyq_head = (id->keyq_head + 1) & 63;
					id->keyq[id->keyq_head].key = K_PASTE;
					id->keyq[id->keyq_head].down = false;
					id->keyq_head = (id->keyq_head + 1) & 63;
					id->keyq[id->keyq_head].key = K_INS;
					id->keyq[id->keyq_head].down = true;
				}

				if (id->keyq_tail != id->keyq_head && id->keyq[id->keyq_head].key == id->keyq[(id->keyq_head - 1) & 63].key && !id->keyq[(id->keyq_head - 1) & 63].down)
				{
					id->keyq[(id->keyq_head - 1) & 63].key = id->keyq[id->keyq_head].key;
					id->keyq[(id->keyq_head - 1) & 63].down = true;
				}
				else
				{
					id->keyq_head = (id->keyq_head + 1) & 63;
				}
				break;
			case KeyRelease:
				id->keyq[id->keyq_head].key = XLateKey(id, &event.xkey);
				if (id->keyq[id->keyq_head].key == K_LSHIFT)
					id->shift_down &= ~1;
				else if (id->keyq[id->keyq_head].key == K_RSHIFT)
					id->shift_down &= ~2;

				id->keyq[id->keyq_head].down = false;
				id->keyq_head = (id->keyq_head + 1) & 63;
				break;
			case MotionNotify:
				if (id->dga_mouse_enabled)
				{
					id->mousex+= event.xmotion.x;
					id->mousey+= event.xmotion.y;
				}
				else
				{
					newmousex = event.xmotion.x;
					newmousey = event.xmotion.y;
				}
				break;

			case ButtonPress:
			case ButtonRelease:
				switch (event.xbutton.button)
				{
					case 1:
						id->keyq[id->keyq_head].key = K_MOUSE1;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 2:
						id->keyq[id->keyq_head].key = K_MOUSE3;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 3:
						id->keyq[id->keyq_head].key = K_MOUSE2;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 4:
						id->keyq[id->keyq_head].key = K_MWHEELUP;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 5:
						id->keyq[id->keyq_head].key = K_MWHEELDOWN;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 6:
						id->keyq[id->keyq_head].key = K_MOUSE4;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 7:
						id->keyq[id->keyq_head].key = K_MOUSE5;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 8:
						id->keyq[id->keyq_head].key = K_MOUSE6;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 9:
						id->keyq[id->keyq_head].key = K_MOUSE7;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 10:
						id->keyq[id->keyq_head].key = K_MOUSE8;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 11:
						id->keyq[id->keyq_head].key = K_MOUSE9;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
					case 12:
						id->keyq[id->keyq_head].key = K_MOUSE10;
						id->keyq[id->keyq_head].down = event.type == ButtonPress;
						id->keyq_head = (id->keyq_head + 1) & 63;
						break;
				}
				break;

			case ConfigureNotify:
				id->config_notify_width = event.xconfigure.width;
				id->config_notify_height = event.xconfigure.height;
				id->config_notify = 1;
				break;

			default:
				break;
		}
	}

	if (!id->dga_mouse_enabled && (newmousex != id->windowwidth/2 || newmousey != id->windowheight/2) && id->grab_mouse)
	{
		newmousex-= id->windowwidth/2;
		newmousey-= id->windowheight/2;
		id->mousex+= newmousex;
		id->mousey+= newmousey;
	
		if (id->grab_mouse)
		{
			/* move the mouse to the window center again */
			XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS&(~PointerMotionMask));
			XWarpPointer(id->x_disp, None, None, 0, 0, 0, 0, -newmousex, -newmousey);
			XSelectInput(id->x_disp, id->x_win, XINPUTFLAGS);
			XFlush(id->x_disp);
		}
	}

	Sys_Thread_UnlockMutex(id->mutex);
}

void X11_Input_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_MOUSE);
	Cvar_Register(&in_dga_mouse);
	Cvar_SetCurrentGroup(CVAR_GROUP_INPUT_KEYBOARD);
	Cvar_Register(&cl_keypad);
	Cvar_ResetCurrentGroup();
}

void *X11_Input_Init(Window x_win, unsigned int windowwidth, unsigned int windowheight, int fullscreen)
{
	struct inputdata *id;
	XSetWindowAttributes attr;
	Window focuswindow;
	int revertto;

	id = malloc(sizeof(*id));
	if (id)
	{
		id->mutex = Sys_Thread_CreateMutex();
		if (id->mutex)
		{
			id->x_disp = XOpenDisplay(0);
			if (id->x_disp)
			{
				id->xpropertyrestore = 0;

				open_libxi(id);

				if (id->libxi)
					disable_middle_button_emulation(id);

				if (is_evdev_rules(id))
				{
					id->keytable = keytable_evdev;
					id->keytablesize = sizeof(keytable_evdev);
				}
				else
				{
					id->keytable = keytable_xorg;
					id->keytablesize = sizeof(keytable_xorg);
				}

				attr.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | FocusChangeMask;
				XChangeWindowAttributes(id->x_disp, x_win, CWEventMask, &attr);

				XGetInputFocus(id->x_disp, &focuswindow, &revertto);

				if (focuswindow == x_win)
					id->window_focused = 1;
				else
					id->window_focused = 0;

				if (fullscreen)
					XGrabKeyboard(id->x_disp, x_win, False, GrabModeAsync, GrabModeAsync, CurrentTime);

				id->x_win = x_win;
				id->windowwidth = windowwidth;
				id->windowheight = windowheight;
				id->fullscreen = 1;
				id->config_notify = 0;
				id->keyq_head = 0;
				id->keyq_tail = 0;
				id->shift_down = 0;
				id->mousex = 0;
				id->mousey = 0;
				id->grab_mouse = 0;
				id->dga_mouse_enabled = 0;
				id->mouse_grabbed = 0;
	
				return id;
			}

			Sys_Thread_DeleteMutex(id->mutex);
		}

		free(id);
	}

	return 0;
}

void X11_Input_Shutdown(void *inputdata)
{
	struct inputdata *id;
	XSetWindowAttributes attr;

	id = inputdata;

	DoUngrabMouse(id);
	if (id->fullscreen)
		XUngrabKeyboard(id->x_disp, CurrentTime);

	attr.event_mask = 0;
	XChangeWindowAttributes(id->x_disp, id->x_win, CWEventMask, &attr);

	if (id->libxi)
		enable_middle_button_emulation(id);

	XCloseDisplay(id->x_disp);

	if (id->libxi)
		dlclose(id->libxi);

	Sys_Thread_DeleteMutex(id->mutex);

	free(id);
}

int X11_Input_GetKeyEvent(void *inputdata, keynum_t *key, qboolean *down)
{
	struct inputdata *id = inputdata;

	if (id->keyq_head == id->keyq_tail)
		GetEvents(id);

	if (id->keyq_head != id->keyq_tail)
	{
		*key = id->keyq[id->keyq_tail].key;
		*down = id->keyq[id->keyq_tail].down;
		id->keyq_tail = (id->keyq_tail + 1) & 63;

		return 1;
	}

	return 0;
}

void X11_Input_GetMouseMovement(void *inputdata, int *mousex, int *mousey)
{
	struct inputdata *id = inputdata;

	GetEvents(id);

	*mousex = id->mousex;
	*mousey = id->mousey;
	id->mousex = 0;
	id->mousey = 0;
}

void X11_Input_GetConfigNotify(void *inputdata, int *config_notify, int *config_notify_width, int *config_notify_height)
{
	struct inputdata *id = inputdata;

	*config_notify = id->config_notify;
	*config_notify_width = id->config_notify_width;
	*config_notify_height = id->config_notify_height;
	id->config_notify = 0;
}

void X11_Input_GrabMouse(void *inputdata, int dograb)
{
	struct inputdata *id = inputdata;

	Sys_Thread_LockMutex(id->mutex);

	if (id->window_focused)
	{
		if (dograb)
		{
			DoGrabMouse(id);
		}
		else if (!dograb)
		{
			DoUngrabMouse(id);
		}
	}

	id->grab_mouse = dograb;

	Sys_Thread_UnlockMutex(id->mutex);
}

