/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// gl_ngraph.c

#include "quakedef.h"
#include "gl_local.h"
#include "gl_state.h"

extern byte *draw_chars;   // 8 * 8 graphic characters

int netgraphtexture;       // netgraph texture

#define NET_GRAPHHEIGHT 32

static byte ngraph_texels[NET_GRAPHHEIGHT][NET_TIMINGS];

static void R_LineGraph(int x, int h)
{
	int i, s, color;

	s = NET_GRAPHHEIGHT;

	if (h == 10000)
		color = 0x6f;   // yellow
	else if (h == 9999)
		color = 0x4f;   // red
	else if (h == 9998)
		color = 0xd0;   // blue
	else
		color = 0xfe;   // white

	h = min(h, s);

	for (i = 0; i < h; i++)
	{
		if (i & 1)
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = 0xff;
		else
			ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)color;
	}

	for ( ; i < s; i++)
		ngraph_texels[NET_GRAPHHEIGHT - i - 1][x] = (byte)0xff;
}

void Draw_CharToNetGraph(int x, int y, int num)
{
	byte *source;
	int row, col, drawline, nx;

	row = num >> 4;
	col = num & 15;
	source = draw_chars + (row<<10) + (col<<3);

	for (drawline = 8; drawline; drawline--, y++)
	{
		for (nx = 0; nx < 8; nx++)
			if (source[nx] != 255)
				ngraph_texels[y][nx + x] = 0x60 + source[nx];
		source += 128;
	}
}

void R_NetGraph(void)
{
	int a, x, i, y, lost;
	char st[80];
	unsigned ngraph_pixels[NET_GRAPHHEIGHT][NET_TIMINGS];
	float coords[4*2];
	static const float texcoords[4*2] =
	{
		0, 0,
		1, 0,
		1, 1,
		0, 1,
	};

	x = 0;
	lost = CL_CalcNet();
	for (a = 0; a < NET_TIMINGS; a++)
	{
		i = (cls.netchan.outgoing_sequence-a) & NET_TIMINGSMASK;
		R_LineGraph(NET_TIMINGS-1-a, packet_latency[i]);
	}

	// now load the netgraph texture into gl and draw it
	for (y = 0; y < NET_GRAPHHEIGHT; y++)
		for (x = 0; x < NET_TIMINGS; x++)
			ngraph_pixels[y][x] = d_8to24table[ngraph_texels[y][x]];

	x = 0;
	y = vid.conheight - sb_lines - 24 - NET_GRAPHHEIGHT - 1;

	if (r_netgraph.value != 2 && r_netgraph.value != 3)
		Draw_TextBox(x, y, NET_TIMINGS / 8, NET_GRAPHHEIGHT / 8 + 1);

	if (r_netgraph.value != 3)
	{
		sprintf(st, "%3i%% packet loss", lost);
		Draw_String(8, y + 8, st);
	}

	x = 8;
	y += 16;

	GL_Bind(netgraphtexture);

	glTexImage2D(GL_TEXTURE_2D, 0, 4, NET_TIMINGS, NET_GRAPHHEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, ngraph_pixels);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	coords[0*2 + 0] = x;
	coords[0*2 + 1] = y;
	coords[1*2 + 0] = x + NET_TIMINGS;
	coords[1*2 + 1] = y;
	coords[2*2 + 0] = x + NET_TIMINGS;
	coords[2*2 + 1] = y + NET_GRAPHHEIGHT;
	coords[3*2 + 0] = x;
	coords[3*2 + 1] = y + NET_GRAPHHEIGHT;

	GL_SetArrays(FQ_GL_VERTEX_ARRAY | FQ_GL_TEXTURE_COORD_ARRAY);
	glVertexPointer(2, GL_FLOAT, 0, coords);
	glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
	glDrawArrays(GL_QUADS, 0, 4);
}

