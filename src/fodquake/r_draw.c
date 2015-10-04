/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2005-2007, 2009-2011 Mark Olsen

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

// r_draw.c

#include <string.h>
#include <ctype.h>

#include "quakedef.h"
#include "sound.h"
#include "version.h"

cvar_t	scr_conalpha	= {"scr_conalpha", "0.8"};
cvar_t	scr_menualpha	= {"scr_menualpha", "0.7"};

extern cvar_t scr_coloredText;

static struct Picture *conbackpic;

static struct Picture *backtilepic;

static struct Picture *box_tlpic;
static struct Picture *box_mlpic;
static struct Picture *box_blpic;
static struct Picture *box_tmpic;
static struct Picture *box_mmpic;
static struct Picture *box_mm2pic;
static struct Picture *box_bmpic;
static struct Picture *box_trpic;
static struct Picture *box_mrpic;
static struct Picture *box_brpic;

//=============================================================================
/* Support Routines */

void Draw_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&scr_conalpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register(&scr_menualpha);

	Cvar_ResetCurrentGroup();

	DrawImp_CvarInit();
}

int Draw_Init(void)
{
	DrawImp_Init();

	conbackpic = Draw_LoadPicture("gfx/conback.lmp", DRAW_LOADPICTURE_NOFALLBACK);
	if (conbackpic)
	{
		backtilepic = Draw_LoadPicture("wad:backtile", DRAW_LOADPICTURE_DUMMYFALLBACK);

		box_tlpic = Draw_LoadPicture("gfx/box_tl.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_mlpic = Draw_LoadPicture("gfx/box_ml.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_blpic = Draw_LoadPicture("gfx/box_bl.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_tmpic = Draw_LoadPicture("gfx/box_tm.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_mmpic = Draw_LoadPicture("gfx/box_mm.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_mm2pic = Draw_LoadPicture("gfx/box_mm2.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_bmpic = Draw_LoadPicture("gfx/box_bm.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_trpic = Draw_LoadPicture("gfx/box_tr.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_mrpic = Draw_LoadPicture("gfx/box_mr.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
		box_brpic = Draw_LoadPicture("gfx/box_br.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

		return 1;
	}

	DrawImp_Shutdown();

	return 0;
}

void Draw_Shutdown()
{
	Draw_FreePicture(box_tlpic);
	Draw_FreePicture(box_mlpic);
	Draw_FreePicture(box_blpic);
	Draw_FreePicture(box_tmpic);
	Draw_FreePicture(box_mmpic);
	Draw_FreePicture(box_mm2pic);
	Draw_FreePicture(box_bmpic);
	Draw_FreePicture(box_trpic);
	Draw_FreePicture(box_mrpic);
	Draw_FreePicture(box_brpic);

	Draw_FreePicture(backtilepic);

	Draw_FreePicture(conbackpic);

	DrawImp_Shutdown();
}

void Draw_Character(int x, int y, unsigned char num)
{
	if (y <= -8)
		return;

	if (num == 32 || num == (32|128))
		return;

	DrawImp_Character(x, y, num);
}

void Draw_String(int x, int y, const char *str)
{
	if (str[0] == 0)
		return;

	Draw_BeginTextRendering();

	while (*str)
	{
		Draw_Character (x, y, *str);
		str++;
		x += 8;
	}

	Draw_EndTextRendering();
}

void Draw_String_Length(int x, int y, const char *str, int len)
{
	if (len == 0)
		return;

	Draw_BeginTextRendering();

	while(len--)
	{
		Draw_Character(x, y, *str);
		str++;
		x += 8;
	}

	Draw_EndTextRendering();
}

void Draw_Alt_String(int x, int y, const char *str)
{
	unsigned char num;

	if (str[0] == 0)
		return;

	Draw_BeginTextRendering();

	while (*str)
	{
		num = *str++;
		num |= 0x80;
		Draw_Character (x, y, num);
		x += 8;
	}

	Draw_EndTextRendering();
}

static int HexToInt(char c)
{
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString(int x, int y, char *text, int red)
{
	int r, g, b;
	int num;

	Draw_BeginColoredTextRendering();

	for ( ; *text; text++)
	{
		if (*text == '&')
		{
			if (text[1] == 'c' && text[2] && text[3] && text[4])
			{
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0)
				{
					if (scr_coloredText.value)
					{
						DrawImp_SetTextColor(r, g, b);
					}
					text += 4;
					continue;
				}
			}
			else if (text[1] == 'r')
			{
				if (scr_coloredText.value)
				{
					DrawImp_SetTextColor(15, 15, 15);
				}
				text += 1;
				continue;
			}
		}

		num = *text & 255;
		if (!scr_coloredText.value && red)
			num |= 128;

		Draw_Character(x, y, num);
		x += 8;
	}

	Draw_EndColoredTextRendering();
}

void Draw_ColoredString_Length(int x, int y, char *text, int red, int len, unsigned short startcolour)
{
	int r, g, b;
	int num;

	Draw_BeginColoredTextRendering();

	DrawImp_SetTextColor((startcolour>>8)&15, (startcolour>>4)&15, startcolour&15);

	while(len)
	{
		if (*text == '&')
		{
			if (text[1] == 'c' && text[2] && text[3] && text[4])
			{
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0)
				{
					if (scr_coloredText.value)
					{
						DrawImp_SetTextColor(r, g, b);
					}
					text += 5;
					len -= 5;
					continue;
				}
			}
			else if (text[1] == 'r')
			{
				if (scr_coloredText.value)
				{
					DrawImp_SetTextColor(15, 15, 15);
				}
				text += 2;
				len -= 2;
				continue;
			}
		}

		num = *text & 255;
		if (!scr_coloredText.value && red)
			num |= 128;

		Draw_Character(x, y, num);
		x += 8;

		text++;
		len--;
	}

	Draw_EndColoredTextRendering();
}

void Draw_TextBox(int x, int y, int width, int lines)
{
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	Draw_DrawPicture(box_tlpic, cx, cy, 8, 8);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_DrawPicture(box_mlpic, cx, cy, 8, 8);
	}
	Draw_DrawPicture(box_blpic, cx, cy + 8, 8, 8);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		Draw_DrawPicture(box_tmpic, cx, cy, 16, 8);
		Draw_DrawPicture(box_mmpic, cx, cy + 8, 16, 8);
		cy += 16;
		for (n = 1; n < lines; n++)
		{
			Draw_DrawPicture(box_mm2pic, cx, cy, 16, 8);
			cy += 8;
		}
		Draw_DrawPicture(box_bmpic, cx, cy, 16, 8);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	Draw_DrawPicture(box_trpic, cx, cy, 8, 8);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_DrawPicture(box_mrpic, cx, cy, 8, 8);
	}
	Draw_DrawPicture(box_brpic, cx, cy + 8, 8, 8);
}

void Draw_ConsoleBackground(int lines)
{
	char ver[80];

	if (SCR_NEED_CONSOLE_BACKGROUND)
	{
		Draw_DrawPicture(conbackpic, 0, lines - vid.conheight, vid.conwidth, vid.conheight);
	}
	else
	{
		Draw_DrawPictureModulated(conbackpic, 0, lines - vid.conheight, vid.conwidth, vid.conheight, 1, 1, 1, scr_conalpha.value);
	}

	sprintf(ver, "Fodquake %s", FODQUAKE_VERSION);
	Draw_Alt_String(vid.conwidth - strlen(ver) * 8 - 8, lines - 10, ver);
}

//This repeats a 64*64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear(int x, int y, int width, int height)
{
	int xoffset;
	int yoffset;
	int dstx;
	int dstwidth;
	int dsty;
	int dstheight;
	int tilewidth;
	int tileheight;

	yoffset = y % 64;
	dsty = y;
	dstheight = height;
	tileheight = 64 - yoffset;
	while(dstheight)
	{
		if (tileheight > dstheight)
			tileheight = dstheight;

		xoffset = x % 64;
		dstx = x;
		dstwidth = width;
		tilewidth = 64 - xoffset;
		while(dstwidth)
		{
			if (tilewidth > dstwidth)
				tilewidth = dstwidth;

			Draw_DrawSubPicture(backtilepic, ((double)xoffset)/64, ((double)yoffset)/64, ((double)tilewidth/64), ((double)tileheight)/64, dstx, dsty, tilewidth, tileheight);

			dstx += tilewidth;
			dstwidth -= tilewidth;
			xoffset = 0;
			tilewidth = 64;
		}

		dsty += tileheight;
		dstheight -= tileheight;
		yoffset = 0;
		tileheight = 64;
	}
}

//=============================================================================

