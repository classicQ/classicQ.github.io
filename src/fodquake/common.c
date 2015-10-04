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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "crc.h"
#include "filesystem.h"
#include "console.h"
#include "sys_thread.h"

#include "strl.h"

#define MAX_NUM_ARGVS	50

static struct SysMutex *com_mutex;

usercmd_t nullcmd; // guaranteed to be zero

static char	*largv[MAX_NUM_ARGVS + 1];

cvar_t	developer = {"developer", "0"};
cvar_t	registered = {"registered", "0"};
cvar_t	mapname = {"mapname", "", CVAR_ROM};

qboolean com_serveractive = false;

char com_gamedirfile[MAX_QPATH];

/*
All of Quake's data access is through a hierarchic file system, but the contents of the file system can be transparently merged from several sources.

The "base directory" is the path to the directory holding the quake.exe and all game directories.  The sys_* files pass this to host_init in quakeparms_t->basedir.  This can be overridden with the "-basedir" command line parm to allow code debugging in a different directory.  The base directory is
only used during filesystem initialization.

The "game directory" is the first tree on the search path and directory that all generated files (savegames, screenshots, demos, config files) will be saved to.  This can be overridden with the "-game" command line parameter.  The game directory can never be changed while quake is executing.  This is a precacution against having a malicious server instruct clients to write files over areas they shouldn't.
*/

//============================================================================

// ClearLink is used for new headnodes
void ClearLink (link_t *l)
{
	l->prev = l->next = l;
}

void RemoveLink (link_t *l)
{
	l->next->prev = l->prev;
	l->prev->next = l->next;
}

void InsertLinkBefore (link_t *l, link_t *before)
{
	l->next = before;
	l->prev = before->prev;
	l->prev->next = l;
	l->next->prev = l;
}

void InsertLinkAfter (link_t *l, link_t *after)
{
	l->next = after->next;
	l->prev = after;
	l->prev->next = l;
	l->next->prev = l;
}

/*
============================================================================
					LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

int Q_atoi (char *str)
{
	int val, sign, c;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
	{
		sign = 1;
	}
		
	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val << 4) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val << 4) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val << 4) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	while (1)
	{
		c = *str++;
		if (c <'0' || c > '9')
			return val*sign;
		val = val * 10 + c - '0';
	}

	return 0;
}

float Q_atof (char *str)
{
	double val;
	int sign, c, decimal, total;
	
	if (*str == '-')
	{
		sign = -1;
		str++;
	}
	else
	{
		sign = 1;
	}
		
	val = 0;

	// check for hex
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X') )
	{
		str += 2;
		while (1)
		{
			c = *str++;
			if (c >= '0' && c <= '9')
				val = (val * 16) + c - '0';
			else if (c >= 'a' && c <= 'f')
				val = (val * 16) + c - 'a' + 10;
			else if (c >= 'A' && c <= 'F')
				val = (val * 16) + c - 'A' + 10;
			else
				return val*sign;
		}
	}

	// check for character
	if (str[0] == '\'')
		return sign * str[1];

	// assume decimal
	decimal = -1;
	total = 0;
	while (1)
	{
		c = *str++;
		if (c == '.')
		{
			decimal = total;
			continue;
		}

		if (c <'0' || c > '9')
			break;

		val = val*10 + c - '0';
		total++;
	}

	if (decimal == -1)
		return val * sign;

	while (total > decimal)
	{
		val /= 10;
		total--;
	}

	return val * sign;
}


void Q_strncpyz (char *dest, char *src, size_t size)
{
	strlcpy(dest, src, size);
}

int Q_snprintfz (char *dest, size_t size, char *fmt, ...)
{
	int i;
	va_list		argptr;

	va_start (argptr, fmt);
	i = vsnprintf (dest, size, fmt, argptr);
	va_end (argptr);

#ifdef _WIN32
	if (size > 0)
		dest[size - 1] = 0;
	if (i == -1)
		i = size-1;
#endif

	return i;
}

int Com_HashKey (char *name)
{
	unsigned int v;	
	unsigned char c; 

	v = 0; 
	while ( (c = *name++) != 0 ) 
		v += c &~ 32;   // make it case insensitive 

	return v % 32; 
} 


/*
============================================================================
					BYTE ORDER FUNCTIONS
============================================================================
*/

short ShortSwap (short l)
{
	byte b1, b2;

	b1 = l & 255;
	b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

int LongSwap (int l)
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int) b1 << 24) + ((int) b2 << 16) + ((int) b3 << 8) + b4;
}

float FloatSwap (float f)
{
	union
	{
		float   f;
		byte    b[4];
	} dat1, dat2;

	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];
	return dat2.f;
}

//===========================================================================

void SZ_Init(sizebuf_t *buf, void *data, int length)
{
	memset(buf, 0, sizeof(*buf));
	buf->data = data;
	buf->maxsize = length;
}

void SZ_Clear(sizebuf_t *buf)
{
	buf->cursize = 0;
	buf->overflowed = false;
}

void *SZ_GetSpace(sizebuf_t *buf, int length)
{
	void *data;

	if (buf->cursize + length > buf->maxsize)
	{
		if (!buf->allowoverflow)
			Sys_Error ("SZ_GetSpace: overflow without allowoverflow set (%d)", buf->maxsize);

		if (length > buf->maxsize)
			Sys_Error ("SZ_GetSpace: %i is > full buffer size", length);

		Sys_Printf ("SZ_GetSpace: overflow\n");	// because Com_Printf may be redirected
		SZ_Clear (buf); 
		buf->overflowed = true;
	}

	data = buf->data + buf->cursize;
	buf->cursize += length;

	return data;
}

void SZ_Write(sizebuf_t *buf, void *data, int length)
{
	memcpy (SZ_GetSpace(buf,length),data,length);		
}

void SZ_Print(sizebuf_t *buf, char *data)
{
	int len;

	len = strlen(data) + 1;

	if (!buf->cursize || buf->data[buf->cursize-1])
		memcpy ((byte *)SZ_GetSpace(buf, len),data,len); // no trailing 0
	else
		memcpy ((byte *)SZ_GetSpace(buf, len-1)-1,data,len); // write over trailing 0
}

//============================================================================

char *COM_SkipPath (char *pathname)
{
	char *last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname == '/')
			last = pathname+1;
		pathname++;
	}

	return last;
}


void COM_StripExtension(char *s)
{
	char *dot;

	dot = strrchr(s, '.');
	if (dot)
		*dot = 0;
}

void COM_CopyAndStripExtension(const char *in, char *out, unsigned int maxlength)
{
	char *dot;

	dot = strrchr(in, '.');

	if (dot && (dot - in) + 1 < maxlength)
		maxlength = (dot - in) + 1;

	strlcpy(out, in, maxlength);
}


char *COM_FileExtension (char *in)
{
	static char exten[8];
	int i;

	if (!(in = strrchr(in, '.')))
		return "";

	in++;
	for (i = 0 ; i < 7 && *in ; i++,in++)
		exten[i] = *in;

	exten[i] = 0;
	return exten;
}


//If path doesn't have a .EXT, append extension (extension should include the .)
void COM_DefaultExtension (char *path, char *extension)
{
	char *src;

	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strlcat(path, extension, MAX_OSPATH);
}

//If path doesn't have an extension or has a different extension, append(!) specified extension
//Extension should include the .
void COM_ForceExtension (char *path, char *extension)
{
	char *src;

	src = path + strlen(path) - strlen(extension);
	if (src >= path && !strcmp(src, extension))
		return;

	strlcat(path, extension, MAX_OSPATH);
}

//============================================================================

#define MAX_COM_TOKEN	1024

char	com_token[MAX_COM_TOKEN];
int		com_argc;
char	**com_argv;

//Parse a token out of a string
char *COM_Parse (char *data)
{
	unsigned char c;
	int len;

	len = 0;
	com_token[0] = 0;

	if (!data)
		return NULL;

// skip whitespace
skipwhite:
	while ( (c = *data) == ' ' || c == '\t' || c == '\r' || c == '\n')
		data++;

	if (c == 0)
		return NULL;			// end of file;

	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;
		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while(len < sizeof(com_token)-1)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				com_token[len] = 0;
				if (!c)
					data--;

				return data;
			}

			com_token[len] = c;
			len++;
		}
	}

	if (len < sizeof(com_token)-1)
	{
		// parse a regular word
		do
		{
			com_token[len] = c;
			data++;
			len++;
			if (len >= MAX_COM_TOKEN - 1)
				break;

			c = *data;
		} while (c && c != ' ' && c != '\t' && c != '\n' && c != '\r');
	}

	com_token[len] = 0;

	return data;
}

void COM_CheckRegistered(void)
{
	if (FS_FileExists("gfx/pop.lmp"))
	{
		Cvar_Set(&registered, "1");
	}
}

//Adds the given string at the end of the current argument list
void COM_AddParm (char *parm)
{
	if (com_argc >= MAX_NUM_ARGVS)
		return;

	largv[com_argc++] = parm;
}

//Returns the position (1 to argc-1) in the program's argument list
//where the given parameter appears, or 0 if not present
int COM_CheckParm (char *parm)
{
	int		i;

	for (i = 1; i < com_argc; i++)
	{
		if (!strcmp(parm, com_argv[i]))
			return i;
	}

	return 0;
}

int COM_Argc (void)
{
	return com_argc;
}

char *COM_Argv (int arg)
{
	if (arg < 0 || arg >= com_argc)
		return "";

	return com_argv[arg];
}

void COM_ClearArgv (int arg)
{
	if (arg < 0 || arg >= com_argc)
		return;

	com_argv[arg] = "";
}

void COM_InitArgv(int argc, char **argv)
{
	for (com_argc = 0; com_argc < MAX_NUM_ARGVS && com_argc < argc; com_argc++)
	{
		if (argv[com_argc])
			largv[com_argc] = argv[com_argc];
		else
			largv[com_argc] = "";
	}

	largv[com_argc] = "";
	com_argv = largv;
}

int COM_Init(void)
{
	com_mutex = Sys_Thread_CreateMutex();
	if (com_mutex)
	{
		Cvar_SetCurrentGroup(CVAR_GROUP_NO_GROUP);
		Cvar_Register (&developer);
		Cvar_Register (&registered);
		Cvar_Register (&mapname);

		Cvar_ResetCurrentGroup();

		return 1;
	}

	return 0;
}

void COM_Shutdown(void)
{
	Sys_Thread_DeleteMutex(com_mutex);
}

//does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
char *va (char *format, ...)
{
	va_list argptr;
	static char string[8][2048];
	static int idx = 0;

	idx++;
	if (idx == 8)
		idx = 0;

	va_start (argptr, format);
	vsnprintf (string[idx], sizeof(string[idx]), format, argptr);
	va_end (argptr);

	return string[idx];
}

char *CopyString (char *in)
{
	char *out;

	out = Z_Malloc (strlen(in) + 1);
	strcpy (out, in);
	return out;
}

/*
=============================================================================
QUAKE FILESYSTEM
=============================================================================
*/

int		com_filesize;
char	com_netpath[MAX_OSPATH];	

#define	MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];
char	com_basedir[MAX_OSPATH];

/*
=====================================================================
  INFO STRINGS
=====================================================================
*/

//Searches the string for the given key and returns the associated value, or an empty string.
char *Info_ValueForKey (char *s, char *key)
{
	char pkey[512];
	static char value[4][512];	// use two buffers so compares work without stomping on each other
	static int	valueindex;
	char *o;

	valueindex = (valueindex + 1) % 4;
	if (*s == '\\')
		s++;

	while (1)
	{
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return "";

			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";

			*o++ = *s++;
		}

		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";
		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char *start, pkey[512], value[512], *o;

	if (strstr (key, "\\"))
	{
		Com_Printf ("Can't use a key with a \\\n");
		return;
	}

	while (1)
	{
		start = s;
		if (*s == '\\')
			s++;
		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
		{
			/* Remove this part */
			memmove(start, s, strlen(s)+1);
			return;
		}

		if (!*s)
			return;
	}

}

void Info_RemovePrefixedKeys (char *start, char prefix)
{
	char *s, pkey[512], value[512], *o;

	s = start;

	while (1)
	{
		if (*s == '\\')
			s++;

		o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;

			*o++ = *s++;
		}

		*o = 0;
		s++;

		o = value;
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;

		if (pkey[0] == prefix)
		{
			Info_RemoveKey (start, pkey);
			s = start;
		}

		if (!*s)
			return;
	}

}

void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize)
{
	char new[1024], *v;
	int c;
#ifndef CLIENTONLY
	extern cvar_t sv_highchars;
#endif

	if (strstr (key, "\\") || strstr (value, "\\") )
	{
		Com_Printf ("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr (key, "\"") || strstr (value, "\"") )
	{
		Com_Printf ("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > 63 || strlen(value) > 63)
	{
		Com_Printf ("Keys and values must be < 64 characters.\n");
		return;
	}

	// this next line is kinda trippy
	if (*(v = Info_ValueForKey(s, key)))
	{
		// key exists, make sure we have enough room for new value, if we don't,
		// don't change it!
		if (strlen(value) - strlen(v) + strlen(s) >= maxsize)
		{
			Com_Printf ("Info string length exceeded\n");
			return;
		}
	}

	Info_RemoveKey (s, key);
	if (!value || !strlen(value))
		return;

	sprintf (new, "\\%s\\%s", key, value);

	if ((strlen(new) + strlen(s)) >= maxsize)
	{
		Com_Printf ("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	v = new;
#ifndef CLIENTONLY
	if (!sv_highchars.value)
	{
		while (*v)
		{
			c = (unsigned char)*v++;
			if (c == ('\\'|128))
				continue;

			c &= 127;
			if (c >= 32)
				*s++ = c;
		}
	}
	else
#endif
	{
		while (*v)
		{
			c = (unsigned char)*v++;
			if (c > 13)
				*s++ = c;
		}

		*s = 0;
	}
}

void Info_SetValueForKey (char *s, char *key, char *value, int maxsize)
{
	if (key[0] == '*')
	{
		Com_Printf ("Can't set * keys\n");
		return;
	}

	Info_SetValueForStarKey (s, key, value, maxsize);
}

#define INFO_PRINT_FIRST_COLUMN_WIDTH 20
void Info_Print (char *s)
{
	char key[512], value[512], *o;
	int l;

	if (*s == '\\')
		s++;
	while (*s)
	{
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;

		l = o - key;
		if (l < INFO_PRINT_FIRST_COLUMN_WIDTH)
		{
			memset (o, ' ', INFO_PRINT_FIRST_COLUMN_WIDTH - l);
			key[INFO_PRINT_FIRST_COLUMN_WIDTH] = 0;
		}
		else
		{
			*o = 0;
		}

		Com_Printf ("%s", key);

		if (!*s)
		{
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;

		*o = 0;

		if (*s)
			s++;

		Com_Printf ("%s\n", value);
	}
}

//============================================================================

static byte chktbl[1024] = {
0x78,0xd2,0x94,0xe3,0x41,0xec,0xd6,0xd5,0xcb,0xfc,0xdb,0x8a,0x4b,0xcc,0x85,0x01,
0x23,0xd2,0xe5,0xf2,0x29,0xa7,0x45,0x94,0x4a,0x62,0xe3,0xa5,0x6f,0x3f,0xe1,0x7a,
0x64,0xed,0x5c,0x99,0x29,0x87,0xa8,0x78,0x59,0x0d,0xaa,0x0f,0x25,0x0a,0x5c,0x58,
0xfb,0x00,0xa7,0xa8,0x8a,0x1d,0x86,0x80,0xc5,0x1f,0xd2,0x28,0x69,0x71,0x58,0xc3,
0x51,0x90,0xe1,0xf8,0x6a,0xf3,0x8f,0xb0,0x68,0xdf,0x95,0x40,0x5c,0xe4,0x24,0x6b,
0x29,0x19,0x71,0x3f,0x42,0x63,0x6c,0x48,0xe7,0xad,0xa8,0x4b,0x91,0x8f,0x42,0x36,
0x34,0xe7,0x32,0x55,0x59,0x2d,0x36,0x38,0x38,0x59,0x9b,0x08,0x16,0x4d,0x8d,0xf8,
0x0a,0xa4,0x52,0x01,0xbb,0x52,0xa9,0xfd,0x40,0x18,0x97,0x37,0xff,0xc9,0x82,0x27,
0xb2,0x64,0x60,0xce,0x00,0xd9,0x04,0xf0,0x9e,0x99,0xbd,0xce,0x8f,0x90,0x4a,0xdd,
0xe1,0xec,0x19,0x14,0xb1,0xfb,0xca,0x1e,0x98,0x0f,0xd4,0xcb,0x80,0xd6,0x05,0x63,
0xfd,0xa0,0x74,0xa6,0x86,0xf6,0x19,0x98,0x76,0x27,0x68,0xf7,0xe9,0x09,0x9a,0xf2,
0x2e,0x42,0xe1,0xbe,0x64,0x48,0x2a,0x74,0x30,0xbb,0x07,0xcc,0x1f,0xd4,0x91,0x9d,
0xac,0x55,0x53,0x25,0xb9,0x64,0xf7,0x58,0x4c,0x34,0x16,0xbc,0xf6,0x12,0x2b,0x65,
0x68,0x25,0x2e,0x29,0x1f,0xbb,0xb9,0xee,0x6d,0x0c,0x8e,0xbb,0xd2,0x5f,0x1d,0x8f,
0xc1,0x39,0xf9,0x8d,0xc0,0x39,0x75,0xcf,0x25,0x17,0xbe,0x96,0xaf,0x98,0x9f,0x5f,
0x65,0x15,0xc4,0x62,0xf8,0x55,0xfc,0xab,0x54,0xcf,0xdc,0x14,0x06,0xc8,0xfc,0x42,
0xd3,0xf0,0xad,0x10,0x08,0xcd,0xd4,0x11,0xbb,0xca,0x67,0xc6,0x48,0x5f,0x9d,0x59,
0xe3,0xe8,0x53,0x67,0x27,0x2d,0x34,0x9e,0x9e,0x24,0x29,0xdb,0x69,0x99,0x86,0xf9,
0x20,0xb5,0xbb,0x5b,0xb0,0xf9,0xc3,0x67,0xad,0x1c,0x9c,0xf7,0xcc,0xef,0xce,0x69,
0xe0,0x26,0x8f,0x79,0xbd,0xca,0x10,0x17,0xda,0xa9,0x88,0x57,0x9b,0x15,0x24,0xba,
0x84,0xd0,0xeb,0x4d,0x14,0xf5,0xfc,0xe6,0x51,0x6c,0x6f,0x64,0x6b,0x73,0xec,0x85,
0xf1,0x6f,0xe1,0x67,0x25,0x10,0x77,0x32,0x9e,0x85,0x6e,0x69,0xb1,0x83,0x00,0xe4,
0x13,0xa4,0x45,0x34,0x3b,0x40,0xff,0x41,0x82,0x89,0x79,0x57,0xfd,0xd2,0x8e,0xe8,
0xfc,0x1d,0x19,0x21,0x12,0x00,0xd7,0x66,0xe5,0xc7,0x10,0x1d,0xcb,0x75,0xe8,0xfa,
0xb6,0xee,0x7b,0x2f,0x1a,0x25,0x24,0xb9,0x9f,0x1d,0x78,0xfb,0x84,0xd0,0x17,0x05,
0x71,0xb3,0xc8,0x18,0xff,0x62,0xee,0xed,0x53,0xab,0x78,0xd3,0x65,0x2d,0xbb,0xc7,
0xc1,0xe7,0x70,0xa2,0x43,0x2c,0x7c,0xc7,0x16,0x04,0xd2,0x45,0xd5,0x6b,0x6c,0x7a,
0x5e,0xa1,0x50,0x2e,0x31,0x5b,0xcc,0xe8,0x65,0x8b,0x16,0x85,0xbf,0x82,0x83,0xfb,
0xde,0x9f,0x36,0x48,0x32,0x79,0xd6,0x9b,0xfb,0x52,0x45,0xbf,0x43,0xf7,0x0b,0x0b,
0x19,0x19,0x31,0xc3,0x85,0xec,0x1d,0x8c,0x20,0xf0,0x3a,0xfa,0x80,0x4d,0x2c,0x7d,
0xac,0x60,0x09,0xc0,0x40,0xee,0xb9,0xeb,0x13,0x5b,0xe8,0x2b,0xb1,0x20,0xf0,0xce,
0x4c,0xbd,0xc6,0x04,0x86,0x70,0xc6,0x33,0xc3,0x15,0x0f,0x65,0x19,0xfd,0xc2,0xd3,

// Only the first 512 bytes of the table are initialized, the rest
// is just zeros.
// This is an idiocy in QW but we can't change this, or checksums
// will not match.
};

//For proxy protecting
byte COM_BlockSequenceCRCByte (byte *base, int length, int sequence)
{
	unsigned short crc;
	byte *p;
	byte chkb[60 + 4];

	p = chktbl + ((unsigned int)sequence % (sizeof(chktbl) - 4));

	if (length > 60)
		length = 60;
	memcpy (chkb, base, length);

	chkb[length] = (sequence & 0xff) ^ p[0];
	chkb[length+1] = p[1];
	chkb[length+2] = ((sequence>>8) & 0xff) ^ p[2];
	chkb[length+3] = p[3];

	length += 4;

	crc = CRC_Block (chkb, length);

	crc &= 0xff;

	return crc;
}


//=====================================================================

#define	MAXPRINTMSG	4096

static void (*rd_print) (char *) = NULL;

void Com_BeginRedirect (void (*RedirectedPrint) (char *))
{
	Sys_Thread_LockMutex(com_mutex);
	rd_print = RedirectedPrint;
}

void Com_EndRedirect (void)
{
	rd_print = NULL;
	Sys_Thread_UnlockMutex(com_mutex);
}

//All console printing must go through this in order to be logged to disk
void Com_Printf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	
	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	Sys_Thread_LockMutex(com_mutex);

	if (rd_print)
	{
		// add to redirected message
		rd_print (msg);
		return;
	}

	// also echo to debugging console
	Sys_Printf ("%s", msg);

	// write it to the scrollable buffer
	Con_Print (msg);

	Sys_Thread_UnlockMutex(com_mutex);
}

void Com_ErrorPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];
	
	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	Sys_Thread_LockMutex(com_mutex);

	if (rd_print)
	{
		// add to redirected message
		rd_print(msg);
		return;
	}

	// also echo to debugging console
	Sys_Printf("Error: %s", msg);

	// write it to the scrollable buffer
	Con_Print("&cf00Error: ");
	Con_Print(msg);

	Sys_Thread_UnlockMutex(com_mutex);
}

//A Com_Printf that only shows up if the "developer" cvar is set
void Com_DPrintf(const char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	if (!developer.value)
		return;			// don't confuse non-developers with techie stuff...

	va_start (argptr,fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);
	
	Com_Printf ("%s", msg);
}

