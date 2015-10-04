/*
Copyright (C) 2010-2013 Mark Olsen

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

/* Fixme: Include less. */
#include "quakedef.h"
#include "strl.h"
#include "utils.h"
#include "console.h"

static qboolean con_parsecolors_callback(cvar_t *, char *);

static cvar_t con_notifylines = { "con_notifylines", "4" };
static cvar_t con_notifytime = { "con_notifytime", "3" };
static cvar_t con_parsecolors = { "con_parsecolors", "1", 0, con_parsecolors_callback };

static unsigned char *conbuf;
static unsigned int contail;
static unsigned int consize;
static unsigned int partiallinestart;

static unsigned int *lines;
static unsigned short *linestartcolours;
static unsigned int maxlines; /* Must be a power of 2 */
static unsigned int firstline;
static unsigned int lastline;
static unsigned int displayline;

static unsigned int textcolumns;

static char *scrollupmarker;

static unsigned int editlinepos;

static char *stitchbuffer; /* A buffer which contains a linear representation of the line that cross the end of the console buffer, if any */
unsigned int stitchbufferlength;

static const float con_cursorspeed = 4;

static unsigned int suppressed;

#define MAXNOTIFYLINES 16
static unsigned long long notifytimes[MAXNOTIFYLINES];
static unsigned int notifystart;

/* Ugly :( */
#define MAXCMDLINE 256
extern char key_lines[32][MAXCMDLINE];
extern int edit_line;
extern int key_linepos;

static void Con_CopyToBuffer(const void *buf, unsigned int size)
{
	if (contail + size <= consize)
	{
		memcpy(conbuf + contail, buf, size);
		contail += size;
		if (contail == consize)
			contail = 0;
	}
	else
	{
		memcpy(conbuf + contail, buf, consize - contail);
		memcpy(conbuf, buf + (consize - contail), size - (consize - contail));
		contail = size - (consize - contail);
	}
}

static unsigned int Con_BufferStringLength(unsigned int offset)
{
	unsigned int i;
	unsigned int len;

	i = offset;

	while(i < consize && conbuf[i] != 0)
	{
		i++;
	}

	len = i - offset;

	if (i == consize)
	{
		i = 0;

		while(i < offset && conbuf[i] != 0)
		{
			i++;
		}

		len += i;
	}

	return len;
}

static unsigned int Con_BufferColouredStringLength(unsigned int offset)
{
	unsigned int len;

	len = Con_BufferStringLength(offset);

	if (offset + len > consize)
	{
		if (stitchbuffer)
			return Colored_String_Length(stitchbuffer + strlen(stitchbuffer) - len);

		return 0;
	}
	else
	{
		return Colored_String_Length(conbuf + offset);
	}
}

static unsigned int Con_BufferColouredStringLengthOffset(unsigned int offset, unsigned int maxlen, unsigned short *lastcolour)
{
	unsigned int len;

	len = Con_BufferStringLength(offset);

	if (offset + len > consize)
	{
		if (stitchbuffer)
			return Colored_String_Offset(stitchbuffer + strlen(stitchbuffer) - len, maxlen, lastcolour);

		return len;
	}
	else
	{
		return Colored_String_Offset(conbuf + offset, maxlen, lastcolour);
	}
}

static unsigned int Con_BufferFindLinebreak(unsigned int offset, unsigned int max, unsigned short *lastcolour)
{
	unsigned int i;

	if (con_parsecolors.value)
	{
		max = Con_BufferColouredStringLength(offset);
		if (max <= textcolumns)
			return Con_BufferStringLength(offset);

		i = Con_BufferColouredStringLengthOffset(offset, textcolumns, lastcolour);
	}
	else
	{
		if (max <= textcolumns)
			return max;

		i = textcolumns;

		*lastcolour = 0x0fff;
	}

	while(i > 0 && conbuf[(offset + i) % consize] != ' ' && conbuf[(offset + i) % consize] != (' '|0x80))
		i--;

	if (i == 0)
		return textcolumns;

	while(conbuf[(offset + i) % consize] == ' ' || conbuf[(offset + i) % consize] == (' '|0x80))
		i++;

	return i;
}

static int Con_ExpandMaxLines()
{
	unsigned int *newlines;
	unsigned short *newlinestartcolours;
	unsigned int i;
	unsigned int j;

	newlines = malloc(maxlines*2*sizeof(*lines));
	if (newlines)
	{
		newlinestartcolours = malloc(maxlines*2*sizeof(*linestartcolours));
		if (newlinestartcolours)
		{
			i = 0;
			j = firstline;
			while (j != ((lastline + 1) % maxlines))
			{
				newlines[i] = lines[j];
				newlinestartcolours[i] = linestartcolours[j];
				i++;
				j++;
				j %= maxlines;
			}

			displayline = (displayline - firstline) % maxlines;
			lastline = (lastline - firstline) % maxlines;
			firstline = 0;

			free(lines);
			free(linestartcolours);

			lines = newlines;
			linestartcolours = newlinestartcolours;
			maxlines *= 2;

			return 1;
		}

		free(newlines);
	}

	return 0;
}

static void Con_LayoutLine(unsigned int offset)
{
	unsigned int i;
	unsigned int j;
	unsigned short linestartcolour;
	unsigned short lastcolour;

	linestartcolour = 0x0fff;

	i = Con_BufferStringLength(offset);

	do
	{
		j = Con_BufferFindLinebreak(offset, i, &lastcolour);

		if ((lastline+2)%maxlines == firstline)
		{
			if (!Con_ExpandMaxLines())
			{
				firstline++;
				firstline = firstline % maxlines;
			}
		}

		if (displayline == lastline)
		{
			displayline++;
			displayline %= maxlines;
		}

		lastline++;
		lastline %= maxlines;

		lines[lastline] = offset;
		linestartcolours[lastline] = linestartcolour;

		notifytimes[notifystart] = Sys_IntTime();
		notifystart++;
		notifystart %= MAXNOTIFYLINES;

		offset += j;
		offset %= consize;
		i -= j;

		linestartcolour = lastcolour;
	} while(i);
}

static void Con_Relayout()
{
	unsigned int i;

	if (firstline != ((lastline + 1) % maxlines))
	{
		i = lines[firstline];

		firstline = 0;
		lastline = maxlines - 1;
		displayline = lastline;

		while(i != contail)
		{
			Con_LayoutLine(i);
			i += Con_BufferStringLength(i) + 1;
			i %= consize;
		}
	}
}

static void Con_Clear()
{
	contail = 0;
	partiallinestart = 0;
	firstline = 0;
	lastline = maxlines - 1;
	displayline = lastline;
}

void Con_Init(void)
{
	consize = 1024*256;
	conbuf = malloc(consize);
	if (conbuf)
	{
		lines = malloc(sizeof(*lines)*512);
		if (lines)
		{
			linestartcolours = malloc(sizeof(*linestartcolours)*512);
			if (lines)
			{
				memset(lines, 0, sizeof(*lines)*512);
				memset(linestartcolours, 0, sizeof(*linestartcolours)*512);
				maxlines = 512;
				lastline = maxlines - 1;
				displayline = lastline;

				textcolumns = 65536;
			}
		}
	}
}

void Con_Shutdown(void)
{
	free(conbuf);
	free(lines);
	free(scrollupmarker);

	conbuf = 0;
	lines = 0;
	scrollupmarker = 0;
}

void Con_CvarInit(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&con_notifylines);
	Cvar_Register(&con_notifytime);
	Cvar_Register(&con_parsecolors);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("clear", Con_Clear);
}

static qboolean con_parsecolors_callback(cvar_t *cvar, char *value)
{
	cvar->value = atof(value);

	Con_Relayout();

	return false;
}

void Con_CheckResize(unsigned int pixelwidth)
{
	unsigned int newtextcolumns;
	unsigned int i;

	newtextcolumns = pixelwidth / 8;
	if (newtextcolumns > 2)
		newtextcolumns -= 2;
	else
		newtextcolumns = 1;

	if (newtextcolumns != textcolumns)
	{
		textcolumns = newtextcolumns;

		free(scrollupmarker);
		if (textcolumns)
		{
			scrollupmarker = malloc(textcolumns + 1);
			for(i=0;i<textcolumns;i++)
				scrollupmarker[i] = i%4==0?'^':' ';

			scrollupmarker[i] = 0;
		}
		else
			scrollupmarker = 0;

		Con_Relayout();
	}
}

static void Con_DrawTextLines(unsigned int y, unsigned int maxlinestodraw, unsigned int lastlinetodraw)
{
	unsigned int i;
	unsigned int j;
	unsigned int nextline;
	unsigned int linelength;

	if (firstline == ((lastline + 1) % maxlines))
		return;

	if (maxlinestodraw == 0)
		return;

	if (!con_parsecolors.value)
		Draw_BeginTextRendering();

	y += maxlinestodraw * 8;

	j = lastlinetodraw;

	if (firstline > lastlinetodraw)
		j += maxlines;

	if (firstline + maxlinestodraw - 1 <= j)
	{
		i = j - maxlinestodraw + 1;
	}
	else
	{
		i = firstline;
		maxlinestodraw = j - firstline + 1;
	}

	i %= maxlines;

	y -= maxlinestodraw * 8;

	while(i != ((lastline + 1) % maxlines) && maxlinestodraw)
	{
		if (maxlinestodraw == 1 && i != lastline)
		{
			Draw_String(8, y, scrollupmarker);
			break;
		}

		nextline = (i + 1) % maxlines;
		linelength = Con_BufferStringLength(lines[i]);
		if (nextline != ((lastline + 1) % maxlines))
		{
			if (lines[nextline] < lines[i])
				j = consize - lines[i] + lines[nextline];
			else
				j = lines[nextline] - lines[i];

			if (j < linelength)
				linelength = j;
		}

		if (lines[i] + linelength > consize)
		{
			if (stitchbuffer)
			{
				if (con_parsecolors.value)
					Draw_ColoredString_Length(8, y, stitchbuffer, 0, linelength, linestartcolours[i]);
				else
					Draw_String_Length(8, y, stitchbuffer, linelength);
			}
		}
		else
		{
			if (con_parsecolors.value)
				Draw_ColoredString_Length(8, y, conbuf + lines[i], 0, linelength, linestartcolours[i]);
			else
				Draw_String_Length(8, y, conbuf + lines[i], linelength);
		}

		y += 8;
		i = nextline;
		maxlinestodraw--;
	}

	if (!con_parsecolors.value)
		Draw_EndTextRendering();
}

void Con_DrawConsole(int pixellines)
{
	unsigned int linelength;
	unsigned char tmpline[2048];

	Draw_ConsoleBackground(pixellines);

	Con_DrawTextLines((pixellines+2)%8, (pixellines - 22) / 8, displayline);

	if (key_linepos && key_linepos - 1 < editlinepos)
		editlinepos = key_linepos - 1;
	else if (key_linepos >= editlinepos + textcolumns)
		editlinepos = key_linepos - textcolumns + 1;

	linelength = strlen(key_lines[edit_line] + editlinepos);
	if (linelength > textcolumns)
		linelength = textcolumns;
	if (linelength > sizeof(tmpline) - 1)
		linelength = sizeof(tmpline) - 1;

	strlcpy(tmpline, key_lines[edit_line] + editlinepos, linelength + 1);

	if ((int) (Sys_DoubleTime() * con_cursorspeed) & 1)
	{
		if (tmpline[key_linepos - editlinepos] == 0)
			linelength++;
		tmpline[key_linepos - editlinepos] = 11;
	}

	Draw_String_Length(8, pixellines - 22, tmpline, linelength);
}

unsigned int Con_DrawNotify(void)
{
	unsigned int maxnotifylines;
	unsigned int notifytime;
	unsigned long long now;
	unsigned int i;

	now = Sys_IntTime();

	notifytime = ((double)con_notifytime.value)*1000000;

	maxnotifylines = con_notifylines.value;
	if (maxnotifylines > MAXNOTIFYLINES)
		maxnotifylines = MAXNOTIFYLINES;

	i = (notifystart + (MAXNOTIFYLINES - maxnotifylines)) % MAXNOTIFYLINES;

	while(maxnotifylines && notifytimes[i] + notifytime < now)
	{
		i++;
		i %= MAXNOTIFYLINES;
		maxnotifylines--;
	}

	Con_DrawTextLines(0, maxnotifylines, lastline);

	return maxnotifylines;
}

void Con_ClearNotify(void)
{
	memset(notifytimes, 0, sizeof(notifytimes));
}

void Con_Suppress(void)
{
	suppressed = 1;
}

void Con_Unsuppress(void)
{
	suppressed = 0;
}

unsigned int Con_GetColumns()
{
	return textcolumns;
}

static void Con_ClearBufferSpace(unsigned int size)
{
	while(firstline != ((lastline + 1) % maxlines) && ((lines[firstline] >= contail && lines[firstline] < contail + size) || (contail + size >= consize && lines[firstline] < (((contail + size) % consize)))))
	{
		if (displayline == firstline)
		{
			displayline++;
			displayline %= maxlines;
		}

		lines[firstline++] = 0;
		firstline %= maxlines;
	}
}

void Con_Print(const char *txt)
{
	unsigned int i;
	unsigned int j;
	unsigned int linebegin;
	const char *newline;
	char *tmp;
	char colourbuf[256];
	void *freethis;

	if (suppressed)
		return;

	if (lines == 0)
	{
		printf("%s\n", txt);
		return;
	}

	if (strlen(txt) >= consize)
		return;

	freethis = 0;

	if (*txt == 1 || *txt == 2)
	{
		/* OMGWTFBBQ */

		txt++;

		if (strlen(txt) + 1 < sizeof(colourbuf))
		{
			strcpy(colourbuf, txt);
			tmp = colourbuf;
		}
		else
		{
			freethis = malloc(strlen(txt) + 1);
			if (freethis == 0)
			{
				Con_Print("Out of memory\n");
				return;
			}

			strcpy(freethis, txt);
			tmp = freethis;
		}

		txt = tmp;
		tmp--;

		while(*++tmp)
		{
			if (*tmp != '\n')
				*tmp |= 128;
		}
	}

	while((newline = strchr(txt, '\n')))
	{
		if ((tmp = strchr(txt, '\r')) && tmp < newline)
			txt = tmp + 1;

		i = newline - txt + 1;

		if (i + partiallinestart < consize)
		{
			Con_ClearBufferSpace(i);

			if (partiallinestart)
				linebegin = partiallinestart;
			else
				linebegin = contail;

			Con_CopyToBuffer(txt, i);

			if (contail == 0)
				j = consize - 1;
			else
				j = contail - 1;

			conbuf[j] = 0;

			if (contail < linebegin)
			{
				j = consize - linebegin + contail;
				if (j > stitchbufferlength)
				{
					free(stitchbuffer);
					stitchbuffer = malloc(j);
				}

				if (stitchbuffer)
				{
					memcpy(stitchbuffer, conbuf + linebegin, consize - linebegin);
					memcpy(stitchbuffer + (consize - linebegin), conbuf, contail);
				}
			}

			Con_LayoutLine(linebegin);
		}

		partiallinestart = 0;
		txt += i;
	}

	if (*txt)
	{
		i = strlen(txt);
		if (i + partiallinestart < consize)
		{
			if (!partiallinestart)
				partiallinestart = contail;

			Con_ClearBufferSpace(i + 1);
			Con_CopyToBuffer(txt, i);
			conbuf[contail] = 0;
		}
	}

	free(freethis);
}

void Con_ScrollUp(unsigned int numlines)
{
	int distance;

	distance = displayline - firstline;
	if (distance < 0)
		distance += maxlines;

	if (numlines > distance)
		numlines = distance;

	displayline -= numlines;
	displayline %= maxlines;
}

void Con_ScrollDown(unsigned int numlines)
{
	int distance;

	distance = lastline - displayline;
	if (distance < 0)
		distance += maxlines;

	if (numlines > distance)
		numlines = distance;

	displayline += numlines;
	displayline %= maxlines;
}

void Con_Home(void)
{
	displayline = firstline;
}

void Con_End(void)
{
	displayline = lastline;
}

