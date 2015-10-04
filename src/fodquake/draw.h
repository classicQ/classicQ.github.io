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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

#ifndef DRAW_H
#define DRAW_H

enum Draw_LoadPicture_Fallback
{
	DRAW_LOADPICTURE_NOFALLBACK,
	DRAW_LOADPICTURE_DUMMYFALLBACK,
};

void Draw_CvarInit(void);
int Draw_Init(void);
void Draw_Shutdown(void);
void Draw_SetSize(unsigned int width, unsigned int height);
void Draw_BeginTextRendering(void);
void Draw_EndTextRendering(void);
void Draw_BeginColoredTextRendering(void);
void Draw_EndColoredTextRendering(void);
void Draw_Character(int x, int y, unsigned char num);
void Draw_ConsoleBackground(int lines);
void Draw_TileClear(int x, int y, int w, int h);
void Draw_Fill(int x, int y, int w, int h, int c);
void Draw_Line(int x1, int y1, int x2, int y2, float width, float r, float g, float b, float alpha);
void Draw_AlphaFill(int x, int y, int w, int h, int c, float alpha);
void Draw_AlphaFillRGB(int x, int y, int w, int h, float r, float g, float b, float alpha);
void Draw_FadeScreen(void);
void Draw_String(int x, int y, const char *str);
void Draw_String_Length(int x, int y, const char *str, int len);
void Draw_Alt_String(int x, int y, const char *str);
void Draw_ColoredString(int x, int y, char *str, int red);
void Draw_ColoredString_Length(int x, int y, char *text, int red, int len, unsigned short startcolour);
void Draw_Crosshair(void);
void Draw_RecalcCrosshair(void);
void Draw_TextBox(int x, int y, int width, int lines);


struct Picture *Draw_LoadPicture(const char *name, enum Draw_LoadPicture_Fallback fallback);
void Draw_FreePicture(struct Picture *);
unsigned int Draw_GetPictureWidth(struct Picture *);
unsigned int Draw_GetPictureHeight(struct Picture *);
void Draw_DrawPicture(struct Picture *, int x, int y, unsigned int width, unsigned int height);
void Draw_DrawPictureModulated(struct Picture *picture, int x, int y, unsigned int width, unsigned int height, float r, float g, float b, float alpha);
void Draw_DrawSubPicture(struct Picture *picture, float sx, float sy, float swidth, float sheight, int x, int y, unsigned int width, unsigned int height);



void DrawImp_CvarInit(void);
void DrawImp_Init(void);
void DrawImp_Shutdown(void);

void DrawImp_Character(int x, int y, unsigned char num);
void DrawImp_SetTextColor(int r, int g, int b);

#endif

