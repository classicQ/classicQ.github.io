/*
Copyright (C) 1996-1997 Id Software, Inc.
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

#include <windows.h>
#include <GL/gl.h>

#include "quakedef.h"
#include "sys_win.h"
#include "resource.h"
#include "in_dinput8.h"
#include "strl.h"
#include "vid_mode_win32.h"

struct display
{
	HWND window;
	HDC dc;
	int width;
	int height;
	HGLRC glctx;
	qboolean isfullscreen;
	qboolean focuschanged;
	qboolean windowactive;

	struct InputData *inputdata;

	char mode[256];

	DEVMODE gdevmode;

	int gammaworks;
	unsigned short currentgammaramps[3][256];
	unsigned short originalgammaramps[3][256];
};

/* Some lame hacks to support OpenGL 1.3 */

static void (APIENTRY *glMultiTexCoord2f_win)(GLenum target, GLfloat s, GLfloat t);
static void (APIENTRY *glDrawRangeElements_win)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
static void (APIENTRY *glClientActiveTexture_win)(GLenum texture);
static void (APIENTRY *glActiveTexture_win)(GLenum texture);

void glMultiTexCoord2f(GLenum target, GLfloat s, GLfloat t)
{
	glMultiTexCoord2f_win(target, s, t);
}

void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices)
{
	glDrawRangeElements_win(mode, start, end, count, type, indices);
}

void glClientActiveTexture(GLenum texture)
{
	glClientActiveTexture_win(texture);
}

void glActiveTexture(GLenum texture)
{
	glActiveTexture_win(texture);
}

static void ProcessMessages()
{
	MSG msg;

	while(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static int bChosePixelFormat(HDC hDC, PIXELFORMATDESCRIPTOR *pfd, PIXELFORMATDESCRIPTOR *retpfd)
{
	int pixelformat;

	if (!(pixelformat = ChoosePixelFormat(hDC, pfd)))
	{
		MessageBox(NULL, "ChoosePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	if (!(DescribePixelFormat(hDC, pixelformat, sizeof(PIXELFORMATDESCRIPTOR), retpfd)))
	{
		MessageBox(NULL, "DescribePixelFormat failed", "Error", MB_OK);
		return 0;
	}

	return pixelformat;
}

static BOOL bSetupPixelFormat(HDC hDC)
{
	int pixelformat;
	PIXELFORMATDESCRIPTOR retpfd, pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),	// size of this pfd
		1,								// version number
		PFD_DRAW_TO_WINDOW 				// support window
		|  PFD_SUPPORT_OPENGL 			// support OpenGL
		|  PFD_DOUBLEBUFFER ,			// double buffered
		PFD_TYPE_RGBA,					// RGBA type
		24,								// 24-bit color depth
		0, 0, 0, 0, 0, 0,				// color bits ignored
		0,								// no alpha buffer
		0,								// shift bit ignored
		0,								// no accumulation buffer
		0, 0, 0, 0, 					// accum bits ignored
		32,								// 32-bit z-buffer
		0,								// no stencil buffer
		0,								// no auxiliary buffer
		PFD_MAIN_PLANE,					// main layer
		0,								// reserved
		0, 0, 0							// layer masks ignored
	};

	if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
		return FALSE;

	if (retpfd.cDepthBits < 24)
	{
		pfd.cDepthBits = 24;
		if (!(pixelformat = bChosePixelFormat(hDC, &pfd, &retpfd)))
			return FALSE;
	}

	if (!SetPixelFormat(hDC, pixelformat, &retpfd))
	{
		MessageBox(NULL, "SetPixelFormat failed", "Error", MB_OK);
		return FALSE;
	}

	return TRUE;
}


#if 0
LONG CDAudio_MessageHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

/* main window procedure */
static LONG WINAPI MainWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LONG lRet = 1;
	int fActive, fMinimized;

	struct display *d;

	d = (struct display *)GetWindowLong(hWnd, GWL_USERDATA);

	switch (uMsg)
	{
		case WM_KILLFOCUS:
			if (d->isfullscreen)
				ShowWindow(hWnd, SW_SHOWMINNOACTIVE);
			break;

		case WM_CREATE:
			{
				LPCREATESTRUCT cs;
				cs = (LPCREATESTRUCT)lParam;
				SetWindowLong(hWnd, GWL_USERDATA, (LONG)cs->lpCreateParams);
			}
			break;

		case WM_MOVE:
			break;

		case WM_SYSCHAR:
			// keep Alt-Space from happening
			break;

		case WM_SIZE:
			break;

		case WM_CLOSE:
			if (MessageBox (d->window, "Are you sure you want to quit?", "Fodquake : Confirm Exit",
						MB_YESNO | MB_SETFOREGROUND | MB_ICONQUESTION) == IDYES)
			{
				Host_Quit ();
			}
			break;

		case WM_ACTIVATE:
			if (d->isfullscreen)
			{
				if (wParam == WA_INACTIVE)
				{
					ChangeDisplaySettings (NULL, 0);

					if (d->gammaworks)
					{
						SetDeviceGammaRamp(d->dc, d->originalgammaramps);
					}
				}
				else
				{
					if (ChangeDisplaySettings(&d->gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
						Com_ErrorPrintf("Unable to reset fullscreen mode\n");
					else
					{
						ShowWindow (d->window, SW_SHOWNORMAL);
						MoveWindow(d->window, 0, 0, d->width, d->height, false);
					}

					if (d->gammaworks)
					{
						SetDeviceGammaRamp(d->dc, d->currentgammaramps);
					}
				}
			}
			d->windowactive = wParam != WA_INACTIVE;
			d->focuschanged = 1;
			break;

		case WM_DESTROY:
			DestroyWindow (hWnd);
			break;

#if 0
		case MM_MCINOTIFY:
			lRet = CDAudio_MessageHandler (hWnd, uMsg, wParam, lParam);
			break;
#endif

		default:
			/* pass all unhandled messages to DefWindowProc */
			lRet = DefWindowProc (hWnd, uMsg, wParam, lParam);
			break;
	}

	/* return 1 if handled message, 0 if not */
	return lRet;
}






/*
static cvar_t		vid_vsync = {"vid_vsync", "", CVAR_INIT};
static cvar_t		vid_hwgammacontrol = {"vid_hwgammacontrol", "1"};
*/
void Sys_Video_SetWindowTitle(void *display, const char *text)
{
	struct display *d = display;

	SetWindowText (d->window, text);
}

void Sys_Video_GetMouseMovement(void *display, int *mousex, int *mousey)
{
	struct display *d = display;

#if 0
	ProcessMessages();
#endif

	Sys_Input_GetMouseMovement(d->inputdata, mousex, mousey);
}

void Sys_Video_GrabMouse(void *display, int dograb)
{
	struct display *d = display;

	Sys_Input_GrabMouse(d->inputdata, dograb);
}

qboolean Sys_Video_HWGammaSupported(void *display)
{
	struct display *d;

	d = display;

	return d->gammaworks;
}

void *Sys_Video_GetProcAddress(void *display, const char *name)
{
	return wglGetProcAddress(name);
}

void Sys_Video_SetGamma(void *display, unsigned short *ramps)
{
	struct display *d;
	int i, j;

	d = display;

	if (!d->gammaworks)
		return;

	for(i=0;i<128;i++)
	{
		for(j=0;j<3;j++)
		{
			ramps[j * 256 + i] = min(ramps[j * 256 + i], (i + 0x80) << 8);
		}
	}

	for(j=0;j<3;j++)
	{
		ramps[j * 256 + 128] = min(ramps[j * 256 + 128], 0xFE00);
	}

	memcpy(d->currentgammaramps, ramps, sizeof(d->currentgammaramps));

	if (d->windowactive)
		SetDeviceGammaRamp(d->dc, ramps);
}

void Sys_Video_BeginFrame(void *display)
{
	struct display *d = display;

	ProcessMessages();

	Sys_Input_MainThreadFrameStart(d->inputdata);
}

unsigned int Sys_Video_GetNumBuffers(void *display)
{
	return 3;
}

void Sys_Video_Update(void *display, vrect_t *rects)
{
	struct display *d = display;
	ProcessMessages();
	SwapBuffers(d->dc);
	ProcessMessages();

	/* In a hope that it helps the networking thread on a uniprocessor system. */
	Sleep(0);
}

void Sys_Video_CvarInit()
{
}

int Sys_Video_Init()
{
	return 1;
}

void Sys_Video_Shutdown()
{
}

void *Sys_Video_Open(const char *mode, unsigned int width, unsigned int height, int fullscreen, unsigned char *palette)
{
	RECT		rect;
	int WindowStyle, ExWindowStyle;
	struct display *d;
	HANDLE hdc;
	WNDCLASS wc;
	MSG msg;
	int displacement;
	int error;

	d = malloc(sizeof(*d));
	if (d)
	{
		error = 0;

		memset(d, 0, sizeof(*d));

		if (fullscreen)	//first step is to set up the video res that we want
		{
			if (mode == 0 || *mode == 0 || !modeline_to_devmode(mode, &d->gdevmode))
			{
				EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &d->gdevmode);

				snprintf(d->mode, sizeof(d->mode), "%d,%d,%d,%d,%d", d->gdevmode.dmPelsWidth, d->gdevmode.dmPelsHeight, d->gdevmode.dmBitsPerPel, d->gdevmode.dmDisplayFlags, d->gdevmode.dmDisplayFrequency);
			}
			else
				strlcpy(d->mode, mode, sizeof(d->mode));

			d->gdevmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
			d->gdevmode.dmSize = sizeof(d->gdevmode);

			if (ChangeDisplaySettings(&d->gdevmode, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
			{
				Com_Printf ("Couldn't set fullscreen DIB mode\n");

				fullscreen = false;
			}
			else
			{
				width = d->gdevmode.dmPelsWidth;
				height = d->gdevmode.dmPelsHeight;
			}
		}

		if (!fullscreen)
		{
			hdc = GetDC (NULL);
			if (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE)
			{
				Com_Printf ("You probably don't want opengl paletted. Switch to 16bit or 32bit colour or something.\n");
				error = 1;
			}
			ReleaseDC (NULL, hdc);
		}

		if (!error)
		{
			d->width = width;
			d->height = height;

			d->isfullscreen = fullscreen;
			d->windowactive = 1;

			/* Register the frame class */
			wc.style         = 0;
			wc.lpfnWndProc   = (WNDPROC)MainWndProc;
			wc.cbClsExtra    = 0;
			wc.cbWndExtra    = 0;
			wc.hInstance     = global_hInstance;
			wc.hIcon         = LoadIcon (global_hInstance, MAKEINTRESOURCE (IDI_APPICON));
			wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
			wc.hbrBackground = NULL;
			wc.lpszMenuName  = 0;
			wc.lpszClassName = "Fodquake";

			RegisterClass (&wc);	//assume that any failures are due to it still being registered
							//we'll fail on the create instead.


			rect.top = rect.left = 0;
			rect.right = d->width;
			rect.bottom = d->height;

			if (d->isfullscreen)
			{
				WindowStyle = WS_POPUP;
				ExWindowStyle = 0;
			}
			else
			{
				WindowStyle = WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
				ExWindowStyle = 0;
			}

			AdjustWindowRectEx(&rect, WindowStyle, FALSE, 0);

			//ajust the rect so that it appears in the center of the screen
			displacement = (GetSystemMetrics(SM_CYSCREEN) - (rect.top+rect.bottom)) / 2;
			rect.top += displacement;
			rect.bottom += displacement;
			displacement = (GetSystemMetrics(SM_CXSCREEN) - (rect.left+rect.right)) / 2;
			rect.left += displacement;
			rect.right += displacement;

			// Create the DIB window
			d->window = CreateWindowEx (
				 ExWindowStyle,
				 "Fodquake",
				 "Fodquake",
				 WindowStyle,
				 rect.left, rect.top,
				 rect.right - rect.left,
				 rect.bottom - rect.top,
				 NULL,
				 NULL,
				 global_hInstance,
				 d);

			if (!d->window)
			{
				Com_Printf("Couldn't create a window\n");
			}
			else
			{
				ShowWindow (d->window, SW_SHOWDEFAULT);
				UpdateWindow (d->window);

				SetForegroundWindow(d->window);
				ProcessMessages();
				SetWindowPos(d->window, HWND_TOP, 0, 0, 0, 0, SWP_DRAWFRAME | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
				SetForegroundWindow(d->window);

				d->dc = GetDC(d->window);
				if (!bSetupPixelFormat(d->dc))
				{
					Com_Printf ("bSetupPixelFormat failed\n");
					return NULL;
				}

				if (fullscreen)
					d->gammaworks = GetDeviceGammaRamp(d->dc, d->originalgammaramps);
				else
					d->gammaworks = 0;

				if (!(d->glctx = wglCreateContext(d->dc)))
				{
					Com_Printf ("Could not initialize GL (wglCreateContext failed).\n\nMake sure you in are 65535 color mode, and try running -window.\n");
				}
				else
				{
					if (!wglMakeCurrent(d->dc, d->glctx))
					{
						Com_Printf ("wglMakeCurrent failed\n");
					}
					else
					{
						BOOL (APIENTRY *swapinterval)(int);

						glMultiTexCoord2f_win = (void *)wglGetProcAddress("glMultiTexCoord2f");
						glDrawRangeElements_win = (void *)wglGetProcAddress("glDrawRangeElements");
						glClientActiveTexture_win = (void *)wglGetProcAddress("glClientActiveTexture");
						glActiveTexture_win = (void *)wglGetProcAddress("glActiveTexture");

						if (glMultiTexCoord2f_win && glDrawRangeElements_win && glClientActiveTexture_win && glActiveTexture_win)
						{
							/* Vsync is broken on Windows, no point in ever enabling it. */
							swapinterval = wglGetProcAddress("wglSwapIntervalEXT");
							if (swapinterval)
							{
								Com_Printf("Disabled vsync\n");
								swapinterval(0);
							}

							d->inputdata = Sys_Input_Init(d->window);
							if (d->inputdata)
								return d;

							wglMakeCurrent(0, 0);
						}
					}

					wglDeleteContext(d->glctx);
				}

				DestroyWindow(d->window);
			}
		}

		if (d->isfullscreen)
			ChangeDisplaySettings (NULL, 0);

		free(d);
	}

	return 0;
}

void Sys_Video_Close(void *display)
{
	struct display *d = display;

	wglMakeCurrent(0, 0);

	wglDeleteContext(d->glctx);

	if (d->gammaworks)
		SetDeviceGammaRamp(d->dc, d->originalgammaramps);

	ReleaseDC(d->window, d->dc);

	DestroyWindow(d->window);

	if (d->isfullscreen)
		ChangeDisplaySettings (NULL, 0);

	free(d);
}

unsigned int Sys_Video_GetWidth(void *display)
{
	struct display *d;

	d = display;

	return d->width;
}

unsigned int Sys_Video_GetHeight(void *display)
{
	struct display *d;

	d = display;

	return d->height;
}

qboolean Sys_Video_GetFullscreen(void *display)
{
	struct display *d;

	d = display;

	return d->isfullscreen;
}

const char *Sys_Video_GetMode(void *display)
{
	struct display *d;

	d = display;

	return d->mode;
}

int Sys_Video_GetKeyEvent(void *display, keynum_t *keynum, qboolean *down)
{
	struct display *d;

	d = display;

	ProcessMessages();

	return Sys_Input_GetKeyEvent(d->inputdata, keynum, down);
}

int Sys_Video_FocusChanged(void *display)
{
	struct display *d;

	d = display;

	ProcessMessages();

	if (d->focuschanged)
	{
		d->focuschanged = 0;
		Sys_Input_ClearRepeat(d->inputdata);
		return 1;
	}

	return 0;
}

