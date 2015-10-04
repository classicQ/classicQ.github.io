/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2005-2010 Mark Olsen

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

#include <string.h>
#include <stdlib.h>

#include "quakedef.h"
#include "common.h"
#include "filesystem.h"

#include "strl.h"

#include "utils.h"
#include "context_sensitive_tab.h"
#include "tokenize_string.h"

extern cvar_t vid_fullscreen;
extern cvar_t vid_width;
extern cvar_t vid_height;
#ifdef GLQUAKE
extern cvar_t vid_conwidth;
extern cvar_t vid_conheight;
#endif
extern cvar_t s_nosound;

#ifndef SERVERONLY
qboolean CL_CheckServerCommand (void);
#endif

static void Cmd_ExecuteStringEx (cbuf_t *context, char *text);

static qboolean can_execute_functions;

cvar_t cl_warncmd = {"cl_warncmd", "0"};

cbuf_t *cbuf_main;
#ifndef SERVERONLY
cbuf_t *cbuf_svc;
cbuf_t *cbuf_safe, *cbuf_formatted_comms;
#endif
cbuf_t *cbuf_cmdsave;

cbuf_t *cbuf_current = NULL;

//=============================================================================

//Causes execution of the remainder of the command buffer to be delayed until next frame.
//This allows commands like: bind g "impulse 5 ; +attack ; wait ; -attack ; impulse 2"
void Cmd_Wait_f (void)
{
	if (cbuf_current)
		cbuf_current->wait = true;
}

/*
=============================================================================
						COMMAND BUFFER
=============================================================================
*/

void Cbuf_AddText (char *text)
{
	Cbuf_AddTextEx (cbuf_main, text);
}

void Cbuf_InsertText (char *text)
{
	Cbuf_InsertTextEx (cbuf_main, text);
}

void Cbuf_Execute (void)
{
	Cbuf_ExecuteEx (cbuf_main);
#ifndef SERVERONLY
	Cbuf_ExecuteEx (cbuf_safe);
	Cbuf_ExecuteEx (cbuf_formatted_comms);
#endif
}

void Cbuf_Init (void)
{
	cbuf_main = malloc(sizeof(*cbuf_main));

#ifndef SERVERONLY
	cbuf_svc = malloc(sizeof(*cbuf_svc));
	cbuf_safe = malloc(sizeof(*cbuf_safe));
	cbuf_formatted_comms = malloc(sizeof(*cbuf_formatted_comms));
#endif

	cbuf_cmdsave = malloc(sizeof(*cbuf_cmdsave));

	if (cbuf_main == 0
#ifndef SERVERONLY
	 || cbuf_svc == 0
	 || cbuf_safe == 0
	 || cbuf_formatted_comms == 0
#endif
	 || cbuf_cmdsave == 0)
	{
		Sys_Error("Cmd_Init: Out of memory\n");
	}

	cbuf_main->text_start = cbuf_main->text_end = MAXCMDBUF >> 1;
	cbuf_main->wait = false;
	cbuf_main->runAwayLoop = 0;
#ifndef SERVERONLY
	cbuf_safe->text_start = cbuf_safe->text_end = (MAXCMDBUF >> 1);
	cbuf_safe->wait = false;
	cbuf_safe->runAwayLoop = 0;

	cbuf_formatted_comms->text_start = cbuf_formatted_comms->text_end = (MAXCMDBUF >> 1);
	cbuf_formatted_comms->wait = false;
	cbuf_formatted_comms->runAwayLoop = 0;

	cbuf_svc->text_start = cbuf_svc->text_end = (MAXCMDBUF >> 1);
	cbuf_svc->wait = false;
	cbuf_svc->runAwayLoop = 0;
#endif

	cbuf_cmdsave->text_start = cbuf_cmdsave->text_end = (MAXCMDBUF >> 1);
	cbuf_cmdsave->wait = false;
	cbuf_cmdsave->runAwayLoop = 0;
}

void Cbuf_Shutdown()
{
	if (cbuf_main)
	{
		free(cbuf_main);
		cbuf_main = 0;
	}

#ifndef SERVERONLY
	if (cbuf_svc)
	{
		free(cbuf_svc);
		cbuf_svc = 0;
	}

	if (cbuf_safe)
	{
		free(cbuf_safe);
		cbuf_safe = 0;
	}

	if (cbuf_formatted_comms)
	{
		free(cbuf_formatted_comms);
		cbuf_formatted_comms = 0;
	}
#endif

	if (cbuf_cmdsave)
	{
		free(cbuf_cmdsave);
		cbuf_cmdsave = 0;
	}
}

//Adds command text at the end of the buffer
void Cbuf_AddTextEx (cbuf_t *cbuf, char *text)
{
	int len, new_start, new_bufsize;

	len = strlen (text);

	if (cbuf->text_end + len <= MAXCMDBUF)
	{
		memcpy (cbuf->text_buf + cbuf->text_end, text, len);
		cbuf->text_end += len;
		return;
	}

	new_bufsize = cbuf->text_end-cbuf->text_start+len;
	if (new_bufsize > MAXCMDBUF)
	{
		Com_Printf ("Cbuf_AddText: overflow\n");
		return;
	}

	// Calculate optimal position of text in buffer
	new_start = ((MAXCMDBUF - new_bufsize) >> 1);

	memcpy (cbuf->text_buf + new_start, cbuf->text_buf + cbuf->text_start, cbuf->text_end-cbuf->text_start);
	memcpy (cbuf->text_buf + new_start + cbuf->text_end-cbuf->text_start, text, len);
	cbuf->text_start = new_start;
	cbuf->text_end = cbuf->text_start + new_bufsize;
}

//Adds command text at the beginning of the buffer
void Cbuf_InsertTextEx (cbuf_t *cbuf, char *text)
{
	int len, new_start, new_bufsize;

	len = strlen(text);

	if (len <= cbuf->text_start)
	{
		memcpy (cbuf->text_buf + (cbuf->text_start - len), text, len);
		cbuf->text_start -= len;
		return;
	}

	new_bufsize = cbuf->text_end - cbuf->text_start + len;
	if (new_bufsize > MAXCMDBUF)
	{
		Com_Printf ("Cbuf_InsertText: overflow\n");
		return;
	}

	// Calculate optimal position of text in buffer
	new_start = ((MAXCMDBUF - new_bufsize) >> 1);

	memmove (cbuf->text_buf + (new_start + len), cbuf->text_buf + cbuf->text_start,
		cbuf->text_end - cbuf->text_start);
	memcpy (cbuf->text_buf + new_start, text, len);
	cbuf->text_start = new_start;
	cbuf->text_end = cbuf->text_start + new_bufsize;
}

#define MAX_RUNAWAYLOOP 1000

void Cbuf_ExecuteEx (cbuf_t *cbuf)
{
	int i, j, cursize;
	char *text, line[1400], *src, *dest;
	qboolean comment, quotes;

	while (cbuf->text_end > cbuf->text_start)
	{
		// find a \n or ; line break
		text = (char *) cbuf->text_buf + cbuf->text_start;

		cursize = cbuf->text_end - cbuf->text_start;
		comment = quotes = false;
		for (i = 0; i < cursize; i++)
		{
			if (text[i] == '\n')
				break;
			if (text[i] == '"')
			{
				quotes = !quotes;
				continue;
			}
			if (comment || quotes)
				continue;

			if (text[i] == '/' && i + 1 < cursize && text[i + 1] == '/')
				comment = true;
			else if (text[i] == ';')
				break;
		}

		// don't execute lines without ending \n; this fixes problems with
		// partially stuffed aliases not being executed properly
#ifndef SERVERONLY
		if (cbuf == cbuf_svc && i == cursize)
			break;
#endif

		// Copy text to line, skipping carriage return chars
		src = text;
		dest = line;
		j = min (i, sizeof(line) - 1);
		for ( ; j; j--, src++)
		{
			if (*src != '\r')
				*dest++ = *src;
		}
		*dest = 0;

		// delete the text from the command buffer and move remaining commands down  This is necessary
		// because commands (exec, alias) can insert data at the beginning of the text buffer
		if (i == cursize)
		{
			cbuf->text_start = cbuf->text_end = (MAXCMDBUF >> 1);
		}
		else
		{
			i++;
			cbuf->text_start += i;
		}

		cursize = cbuf->text_end - cbuf->text_start;
		Cmd_ExecuteStringEx (cbuf, line);	// execute the command line

		if (cbuf->text_end - cbuf->text_start > cursize)
			cbuf->runAwayLoop++;


		if (cbuf->runAwayLoop > MAX_RUNAWAYLOOP)
		{
			Com_Printf("\x02" "A recursive alias has caused an infinite loop.");
			Com_Printf("\x02" " Clearing execution buffer to prevent lockup.\n");
			cbuf->text_start = cbuf->text_end = (MAXCMDBUF >> 1);
			cbuf->runAwayLoop = 0;
		}

		if (cbuf->wait)
		{
			// skip out while text still remains in buffer, leaving it for next frame
			cbuf->wait = false;
#ifndef SERVERONLY

			cbuf->runAwayLoop += Q_rint(0.5 * cls.frametime * MAX_RUNAWAYLOOP);
#endif
			return;
		}
	}

	cbuf->runAwayLoop = 0;
}

/*
==============================================================================
						SCRIPT COMMANDS
==============================================================================
*/

void Cmd_ParseLegacyCmdLineCmds()
{
	int i;

	for(i=1;i<com_argc;i++)
	{
		if (strcmp(com_argv[i], "-width") == 0 && i+1 < com_argc)
			Cvar_Set(&vid_width, com_argv[i+1]);
		else if (strcmp(com_argv[i], "-height") == 0 && i+1 < com_argc)
			Cvar_Set(&vid_height, com_argv[i+1]);
#ifdef GLQUAKE
		else if (strcmp(com_argv[i], "-conwidth") == 0 && i+1 < com_argc)
			Cvar_Set(&vid_conwidth, com_argv[i+1]);
		else if (strcmp(com_argv[i], "-conheight") == 0 && i+1 < com_argc)
			Cvar_Set(&vid_conheight, com_argv[i+1]);
#endif
		else if (strcmp(com_argv[i], "-fullscreen") == 0)
			Cvar_Set(&vid_fullscreen, "1");
		else if (strcmp(com_argv[i], "-window") == 0)
			Cvar_Set(&vid_fullscreen, "0");
		else if (strcmp(com_argv[i], "-nosound") == 0)
			Cvar_Set(&s_nosound, "1");
	}
}

/*
Adds command line parameters as script statements
Commands lead with a +, and continue until a - or another +
quake +prog jctest.qp +cmd amlev1
quake -nosound +cmd amlev1
*/
void Cmd_StuffCmds_f(void)
{
	int k, len;
	char *s, *text, *token;

	// build the combined string to parse from
	len = 0;
	for (k = 1; k < com_argc; k++)
		len += strlen (com_argv[k]) + 1;

	if (!len)
		return;

	text = Z_Malloc (len + 1);
	for (k = 1; k < com_argc; k++)
	{
		strcat (text, com_argv[k]);
		if (k != com_argc - 1)
			strcat (text, " ");
	}

	// pull out the commands
	token = Z_Malloc (len + 1);

	s = text;
	while (*s)
	{
		if (*s == '+')
		{
			k = 0;
			for (s = s + 1; s[0] && (s[0] != ' ' || (s[1] != '-' && s[1] != '+')); s++)
				token[k++] = s[0];
			token[k++] = '\n';
			token[k] = 0;
			Cbuf_AddText (token);
		}
		else if (*s == '-')
		{
			for (s = s + 1; s[0] && s[0] != ' '; s++)
				;
		}
		else
		{
			s++;
		}
	}

	Z_Free (text);
	Z_Free (token);
}

void Cmd_Exec_f(void)
{
	char *f, name[MAX_OSPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("exec <filename> : execute a script file\n");
		return;
	}

	Q_strncpyz(name, Cmd_Argv(1), sizeof(name) - 4);
	if (!(f = (char *)FS_LoadMallocFile(name)))
	{
		char *p;
		p = COM_SkipPath(name);
		if (!strchr (p, '.'))
		{
			// no extension, so try the default (.cfg)
			strcat (name, ".cfg");
			f = (char *) FS_LoadMallocFile(name);
		}
		if (!f)
		{
			Com_Printf("couldn't exec %s\n", Cmd_Argv(1));
			return;
		}
	}
	if (cl_warncmd.value || developer.value)
		Com_Printf("execing %s\n", name);

#ifndef SERVERONLY
	if (cbuf_current == cbuf_svc)
	{
		Cbuf_AddText("weight_disable\n");
		Cbuf_AddText(f);
		Cbuf_AddText("\n");
		Cbuf_AddText("weight_enable\n");
	}
	else
#endif
	{
		Cbuf_InsertText("weight_enable\n");
		Cbuf_InsertText("\n");
		Cbuf_InsertText(f);
		Cbuf_InsertText("weight_disable\n");
	}

	free(f);
}

//Just prints the rest of the line to the console
void Cmd_Echo_f (void)
{
	int i;

	for (i = 1; i < Cmd_Argc(); i++)
		Com_Printf ("%s ", Cmd_Argv(i));
	Com_Printf ("\n");
}

/*
=============================================================================
								ALIASES
=============================================================================
*/

static cmd_alias_t *cmd_alias_hash[32];
cmd_alias_t	*cmd_alias;

cmd_alias_t *Cmd_FindAlias (char *name)
{
	int key;
	cmd_alias_t *alias;

	key = Com_HashKey (name);
	for (alias = cmd_alias_hash[key]; alias; alias = alias->hash_next)
	{
		if (!Q_strcasecmp(name, alias->name))
			return alias;
	}
	return NULL;
}

char *Cmd_AliasString (char *name)
{
	int key;
	cmd_alias_t *alias;

	key = Com_HashKey (name);
	for (alias = cmd_alias_hash[key]; alias; alias = alias->hash_next)
	{
		if (!Q_strcasecmp(name, alias->name))
			return alias->value;
	}
	return NULL;
}

void Cmd_Viewalias_f (void)
{
	cmd_alias_t *alias;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("viewalias <aliasname> : view body of alias\n");
		return;
	}

	alias = Cmd_FindAlias(Cmd_Argv(1));

	if (alias)
		Com_Printf ("%s : \"%s\"\n", Cmd_Argv(1), alias->value);
	else
		Com_Printf ("No such alias: %s\n", Cmd_Argv(1));
}


int Cmd_AliasCompare (const void *p1, const void *p2)
{
	cmd_alias_t *a1, *a2;

	a1 = *((cmd_alias_t **) p1);
	a2 = *((cmd_alias_t **) p2);

	if (a1->name[0] == '+')
	{
		if (a2->name[0] == '+')
			return Q_strcasecmp(a1->name + 1, a2->name + 1);
		else
			return -1;
	}
	else if (a1->name[0] == '-')
	{
		if (a2->name[0] == '+')
			return 1;
		else if (a2->name[0] == '-')
			return Q_strcasecmp(a1->name + 1, a2->name + 1);
		else
			return -1;
	}
	else if (a2->name[0] == '+' || a2->name[0] == '-')
	{
		return 1;
	}
	else
	{
		return Q_strcasecmp(a1->name, a2->name);
	}
}

void Cmd_AliasList_f (void)
{
	cmd_alias_t	*a;
	int i, count;
	cmd_alias_t *sorted_aliases[512];

#define MAX_SORTED_ALIASES (sizeof(sorted_aliases) / sizeof(sorted_aliases[0]))

	for (a = cmd_alias, count = 0; a && count < MAX_SORTED_ALIASES; a = a->next, count++)
		sorted_aliases[count] = a;
	qsort(sorted_aliases, count, sizeof (cmd_alias_t *), Cmd_AliasCompare);

	for (i = 0; i < count; i++)
	{
		Com_Printf ("\x02%s :", sorted_aliases[i]->name);
		Com_Printf (" %s\n\n", sorted_aliases[i]->value);
	}

	Com_Printf ("----------\n%d aliases\n", count);
}

//Creates a new command that executes a command string (possibly ; separated)
void Cmd_Alias_f (void)
{
	cmd_alias_t	*a;
	char *s;
	int c, key;

	c = Cmd_Argc();
	if (c == 1)	{
		Com_Printf ("%s <name> <command> : create or modify an alias\n", Cmd_Argv(0));
		Com_Printf ("aliaslist : list all aliases\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	key = Com_HashKey(s);

	// if the alias already exists, reuse it
	for (a = cmd_alias_hash[key]; a; a = a->hash_next)
	{
		if (!Q_strcasecmp(a->name, s))
		{
			Z_Free (a->value);
			break;
		}
	}

	if (!a)	{
		a = Z_Malloc (sizeof(cmd_alias_t));
		a->next = cmd_alias;
		cmd_alias = a;
		a->hash_next = cmd_alias_hash[key];
		cmd_alias_hash[key] = a;
	}
	strcpy (a->name, s);

	a->flags = 0;
	if (!Q_strcasecmp(Cmd_Argv(0), "aliasa"))
		a->flags |= ALIAS_ARCHIVE;

#ifndef SERVERONLY
	if (cbuf_current == cbuf_svc)
		a->flags |= ALIAS_SERVER;
	if (!Q_strcasecmp(Cmd_Argv(0), "tempalias"))
		a->flags |= ALIAS_TEMP;
#endif


	// copy the rest of the command line
	a->value = CopyString (Cmd_MakeArgs(2));
}

qboolean Cmd_DeleteAlias (char *name)
{
	cmd_alias_t	*a, *prev;
	int key;

	key = Com_HashKey (name);

	prev = NULL;
	for (a = cmd_alias_hash[key]; a; a = a->hash_next)
	{
		if (!Q_strcasecmp(a->name, name))
		{
			// unlink from hash
			if (prev)
				prev->hash_next = a->hash_next;
			else
				cmd_alias_hash[key] = a->hash_next;
			break;
		}
		prev = a;
	}

	if (!a)
		return false;	// not found

	prev = NULL;
	for (a = cmd_alias; a; a = a->next)
	{
		if (!Q_strcasecmp(a->name, name))
		{
			// unlink from alias list
			if (prev)
				prev->next = a->next;
			else
				cmd_alias = a->next;

			// free
			Z_Free (a->value);
			Z_Free (a);
			return true;
		}
		prev = a;
	}

	Sys_Error("Cmd_DeleteAlias: alias list broken");
	return false;	// shut up compiler
}

void Cmd_UnAlias_f (void)
{
	char*s;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("unalias <alias>: erase an existing alias\n");
		return;
	}

	s = Cmd_Argv(1);
	if (strlen(s) >= MAX_ALIAS_NAME)
	{
		Com_Printf ("Alias name is too long\n");
		return;
	}

	Cmd_DeleteAlias (s);
}

// remove all aliases
void Cmd_UnAliasAll_f (void)
{
	cmd_alias_t	*a, *next;
	int i;

	for (a = cmd_alias; a ; a = next)
	{
		next = a->next;
		Z_Free (a->value);
		Z_Free (a);
	}
	cmd_alias = NULL;

	// clear hash
	for (i = 0; i < 32; i++)
		cmd_alias_hash[i] = NULL;
}




void DeleteServerAliases(void)
{
	extern cmd_alias_t *cmd_alias;
	cmd_alias_t	*a;

	for (a = cmd_alias; a; a = a->next)
	{
		if (a->flags & ALIAS_SERVER)
			Cmd_DeleteAlias(a->name);
	}
}



void Cmd_WriteAliases (FILE *f)
{
	cmd_alias_t	*a;

	for (a = cmd_alias ; a ; a=a->next)
		if (a->flags & ALIAS_ARCHIVE)
			fprintf (f, "aliasa %s \"%s\"\n", a->name, a->value);
}

/*
=============================================================================
					LEGACY COMMANDS
=============================================================================
*/

typedef struct legacycmd_s
{
	char *oldname, *newname;
	struct legacycmd_s *next;
} legacycmd_t;

static legacycmd_t *legacycmds = NULL;

void Cmd_AddLegacyCommand (char *oldname, char *newname)
{
	legacycmd_t *cmd;
	cmd = (legacycmd_t *) Q_Malloc (sizeof(legacycmd_t));
	cmd->next = legacycmds;
	legacycmds = cmd;

	cmd->oldname = CopyString(oldname);
	cmd->newname = CopyString(newname);
}

qboolean Cmd_IsLegacyCommand (char *oldname)
{
	legacycmd_t *cmd;

	for (cmd = legacycmds; cmd; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd->oldname, oldname))
			return true;
	}
	return false;
}

static qboolean Cmd_LegacyCommand (void)
{
	static qboolean recursive = false;
	legacycmd_t *cmd;
	char text[1024];

	for (cmd = legacycmds; cmd; cmd = cmd->next)
	{
		if (!Q_strcasecmp(cmd->oldname, Cmd_Argv(0)))
			break;
	}
	if (!cmd)
		return false;

	if (!cmd->newname[0])
		return true;		// just ignore this command

	// build new command string
	snprintf(text, sizeof(text), "%s %s", cmd->newname, Cmd_Args());

	if (recursive)
		Sys_Error("Cmd_LegacyCommand: Called recursively");

	recursive = true;
	Cmd_ExecuteString (text);
	recursive = false;

	return true;
}

/*
=============================================================================
					COMMAND EXECUTION
=============================================================================
*/

#define	MAX_ARGS		80

static	int		cmd_argc;
static	char	*cmd_argv[MAX_ARGS];
static	char	*cmd_null_string = "";
static	char	*cmd_args = NULL;

cmd_function_t	*cmd_hash_array[32];
/*static*/ cmd_function_t	*cmd_functions;		// possible commands to execute

int Cmd_Argc (void)
{
	return cmd_argc;
}

char *Cmd_Argv (int arg)
{
	if (arg >= cmd_argc)
		return cmd_null_string;
	return cmd_argv[arg];
}

//Returns a single string containing argv(1) to argv(argc() - 1)
char *Cmd_Args (void)
{
	if (!cmd_args)
		return "";
	return cmd_args;
}

//Returns a single string containing argv(start) to argv(argc() - 1)
//Unlike Cmd_Args, shrinks spaces between argvs
char *Cmd_MakeArgs (int start)
{
	int i, c;

	static char text[1024];

	text[0] = 0;
	c = Cmd_Argc();
	for (i = start; i < c; i++)
	{
		if (i > start)
			strlcat(text, " ", sizeof(text));

		strlcat(text, Cmd_Argv(i), sizeof(text));
	}

	return text;
}

//Parses the given string into command line tokens.
void Cmd_TokenizeString (char *text)
{
	int idx;
	static char argv_buf[1024];

	idx = 0;

	cmd_argc = 0;
	cmd_args = NULL;

	while (1)
	{
		// skip whitespace
		while (*text == ' ' || *text == '\t' || *text == '\r')
			text++;

		if (*text == '\n')
		{
			/* a newline separates commands in the buffer */
			break;
		}

		if (!*text)
			return;

		if (cmd_argc == 1)
			 cmd_args = text;

		text = COM_Parse (text);
		if (!text)
			return;

		if (cmd_argc < MAX_ARGS)
		{
			if (idx+strlen(com_token) >= sizeof(argv_buf))
				break;
			cmd_argv[cmd_argc] = argv_buf + idx;
			strcpy(cmd_argv[cmd_argc], com_token);
			idx += strlen(com_token) + 1;
			cmd_argc++;
		}
	}
}

extern int cvarsregged;
void Cmd_AddCommand (char *cmd_name, xcommand_t function)
{
	cmd_function_t *cmd;
	int	key;

	if (cvarsregged)
	{
		printf("Command \"%s\" registered too late\n", cmd_name);
	}

	// fail if the command is a variable name
	if (Cvar_FindVar(cmd_name))
	{
		Com_Printf ("Cmd_AddCommand: %s already defined as a var\n", cmd_name);
		return;
	}

	key = Com_HashKey (cmd_name);

	// fail if the command already exists
	for (cmd = cmd_hash_array[key]; cmd; cmd=cmd->hash_next)
	{
		if (!Q_strcasecmp (cmd_name, cmd->name))
		{
			Com_Printf ("Cmd_AddCommand: %s already defined\n", cmd_name);
			return;
		}
	}

	cmd = malloc(sizeof(*cmd));
	if (cmd == 0)
		Sys_Error("Cmd_AddCommand: Unable to allocate memory for command\n");

	cmd->name = cmd_name;
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd_functions = cmd;
	cmd->hash_next = cmd_hash_array[key];
	cmd_hash_array[key] = cmd;
	cmd->weight = 0;
}

qboolean Cmd_Exists (char *cmd_name)
{
	int	key;
	cmd_function_t	*cmd;

	key = Com_HashKey (cmd_name);
	for (cmd=cmd_hash_array[key]; cmd; cmd = cmd->hash_next)
	{
		if (!Q_strcasecmp (cmd_name, cmd->name))
			return true;
	}
	return false;
}

cmd_function_t *Cmd_FindCommand (char *cmd_name)
{
	int	key;
	cmd_function_t *cmd;

	key = Com_HashKey (cmd_name);
	for (cmd = cmd_hash_array[key]; cmd; cmd = cmd->hash_next)
	{
		if (!Q_strcasecmp (cmd_name, cmd->name))
			return cmd;
	}
	return NULL;
}

char *Cmd_CompleteCommand (char *partial)
{
	cmd_function_t *cmd;
	int len;
	cmd_alias_t *alias;

	len = strlen(partial);

	if (!len)
		return NULL;

	// check for exact match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strcasecmp (partial, cmd->name))
			return cmd->name;
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strcasecmp (partial, alias->name))
			return alias->name;

	// check for partial match
	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strncasecmp (partial, cmd->name, len))
			return cmd->name;
	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strncasecmp (partial, alias->name, len))
			return alias->name;

	return NULL;
}

int Cmd_CompleteCountPossible (char *partial)
{
	cmd_function_t *cmd;
	int len, c = 0;

	len = strlen(partial);
	if (!len)
		return 0;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
		if (!Q_strncasecmp (partial, cmd->name, len))
			c++;

	return c;
}

int Cmd_AliasCompleteCountPossible (char *partial)
{
	cmd_alias_t *alias;
	int len, c = 0;

	len = strlen(partial);
	if (!len)
		return 0;

	for (alias = cmd_alias; alias; alias = alias->next)
		if (!Q_strncasecmp (partial, alias->name, len))
			c++;

	return c;
}

int Cmd_CommandCompare (const void *p1, const void *p2)
{
	return strcmp((*((cmd_function_t **) p1))->name, (*((cmd_function_t **) p2))->name);
}

void Cmd_CmdList_f (void)
{
	cmd_function_t *cmd;
	int i;
	int count;
	cmd_function_t **sorted_cmds;

	for (cmd = cmd_functions, count = 0; cmd; cmd = cmd->next, count++);

	sorted_cmds = malloc(count * sizeof(*sorted_cmds));
	if (sorted_cmds == 0)
	{
		Com_ErrorPrintf("cmdlist: out of memory\n");
		return;
	}

	for (cmd = cmd_functions, count = 0; cmd; cmd = cmd->next, count++)
		sorted_cmds[count] = cmd;

	qsort(sorted_cmds, count, sizeof (cmd_function_t *), Cmd_CommandCompare);

	for (i = 0; i < count; i++)
		Com_Printf ("%s\n", sorted_cmds[i]->name);

	Com_Printf ("------------\n%d commands\n", count);

	free(sorted_cmds);
}



#define MAX_MACROS 64

typedef struct
{
	char		name[32];
	const char		*(*func) (void);
	qboolean	teamplay;
} macro_command_t;

static macro_command_t macro_commands[MAX_MACROS];
static int macro_count = 0;

void Cmd_AddMacroEx(char *s, const char *(*f)(void), qboolean teamplay)
{
	if (macro_count == MAX_MACROS)
		Sys_Error("Cmd_AddMacro: macro_count == MAX_MACROS");
	Q_strncpyz(macro_commands[macro_count].name, s, sizeof(macro_commands[macro_count].name));
	macro_commands[macro_count].func = f;
	macro_commands[macro_count].teamplay = teamplay;
	macro_count++;
}

void Cmd_AddMacro(char *s, const char *(*f)(void))
{
	Cmd_AddMacroEx(s, f, false);
}

char *Cmd_MacroString (char *s, int *macro_length)
{
	int i;
	macro_command_t	*macro;

	for (i = 0; i < macro_count; i++)
	{
		macro = &macro_commands[i];
		if (!Q_strncasecmp(s, macro->name, strlen(macro->name)))
		{
#ifndef SERVERONLY
			if (cbuf_current == cbuf_main && macro->teamplay)
				cbuf_current = cbuf_formatted_comms;
#endif
			*macro_length = strlen(macro->name);
			return macro->func();
		}
	}
	*macro_length = 0;
	return NULL;
}

int Cmd_MacroCompare (const void *p1, const void *p2)
{
	return strcmp((*((macro_command_t **) p1))->name, (*((macro_command_t **) p2))->name);
}

void Cmd_MacroList_f (void)
{
	int	i;
	static qboolean sorted = false;
	static macro_command_t *sorted_macros[MAX_MACROS];

	if (!macro_count)
	{
		Com_Printf("No macros!");
		return;
	}

	if (!sorted)
	{
		for (i = 0; i < macro_count; i++)
			sorted_macros[i] = &macro_commands[i];
		sorted = true;
		qsort(sorted_macros, macro_count, sizeof (macro_command_t *), Cmd_MacroCompare);
	}

	for (i = 0; i < macro_count; i++)
		Com_Printf ("$%s\n", sorted_macros[i]->name);

	Com_Printf ("---------\n%d macros\n", macro_count);
}



//Expands all $cvar expressions to cvar values
//If not SERVERONLY, also expands $macro expressions
void Cmd_ExpandString(char *data, char *dest, unsigned int maxlen)
{
	unsigned int c;
	char buf[255], *str;
	int i, len, quotes = 0, name_length;
	cvar_t	*var, *bestvar;
#ifndef SERVERONLY
	int macro_length;
#endif

	len = 0;

	while ((c = *data))
	{
		if (c == '"')
			quotes++;

		if (c == '$' && !(quotes&1))
		{
			data++;

			/* Copy the texter after '$' to a temporary buffer and
			 * look for the longest cvar match in the process */
			i = 0;
			buf[0] = 0;
			bestvar = NULL;
			while ((c = *data) > 32 && i < sizeof(buf)-1)
			{
				if (c == '$')
					break;
				data++;
				buf[i++] = c;
				buf[i] = 0;
				if ((var = Cvar_FindVar(buf)))
				{
					bestvar = var;
				}
			}

#ifndef SERVERONLY
			if (!dedicated)
			{
				str = Cmd_MacroString (buf, &macro_length);
				name_length = macro_length;

				if (bestvar && (!str || (strlen(bestvar->name) > macro_length)))
				{
					str = bestvar->string;
					name_length = strlen(bestvar->name);
				}
			}
			else
#endif
			{
				if (bestvar)
				{
					str = bestvar->string;
					name_length = strlen(bestvar->name);
				}
				else
				{
					str = NULL;
				}
			}

			if (str)
			{
				// check buffer size
				if (len + strlen(str) + strlen(buf+name_length) >= maxlen - 1)
					break;

				strcpy(&dest[len], str);
				len += strlen(str);
				i = name_length;
				while (buf[i])
					dest[len++] = buf[i++];
			}
			else
			{
				// no matching cvar or macro
				dest[len++] = '$';
				if (len + strlen(buf) >= maxlen - 1)
					break;
				strcpy (&dest[len], buf);
				len += strlen(buf);
			}
		}
		else
		{
			dest[len] = c;
			data++;
			len++;
			if (len >= maxlen - 1)
				break;
		}
	}

	dest[len] = 0;
}

char *msgtrigger_commands[] =
{
	"play", "playvol", "stopsound", "set", "echo", "say", "say_team",
		"alias", "unalias", "msg_trigger", "inc", "bind", "unbind", "record",
		"easyrecord", "stop", "if", "wait", "log", "match_forcestart",
		NULL
};

char *formatted_comms_commands[] =
{
	"if", "wait", "echo", "say", "say_team",
	"tp_point", "tp_pickup", "tp_took",
	NULL
};

//A complete command line has been parsed, so try to execute it
static void Cmd_ExecuteStringEx (cbuf_t *context, char *text)
{
	cvar_t *v;
	cmd_function_t *cmd;
	cmd_alias_t *a;
	static char buf[2048];
	cbuf_t *inserttarget, *oldcontext;
	extern int weight_disable;

	oldcontext = cbuf_current;
	cbuf_current = context;

	Cmd_ExpandString(text, buf, sizeof(buf));
	Cmd_TokenizeString (buf);

	if (!Cmd_Argc())
		goto done;		// no tokens

#ifndef SERVERONLY
	if (cbuf_current == cbuf_svc)
	{
		if (CL_CheckServerCommand())
			goto done;
	}
#endif

	// check functions
	if ((cmd = Cmd_FindCommand(cmd_argv[0])))
	{
#ifndef SERVERONLY
		if (can_execute_functions || strcmp(cmd_argv[0], "cfg_load") == 0 || strcmp(cmd_argv[0], "exec") == 0 || strcmp(cmd_argv[0], "alias") == 0)
		{
			char **s;

			if (cbuf_current == cbuf_safe)
			{
				for (s = msgtrigger_commands; *s; s++)
				{
					if (!Q_strcasecmp(cmd_argv[0], *s))
						break;
				}
				if (!*s)
				{
					Com_Printf ("\"%s\" cannot be used in message triggers\n", cmd_argv[0]);
					goto done;
				}
			}
			else if (cbuf_current == cbuf_formatted_comms)
			{
				for (s = formatted_comms_commands; *s; s++)
				{
					if (!Q_strcasecmp(cmd_argv[0], *s))
						break;
				}
				if (!*s)
				{
					Com_Printf("\"%s\" cannot be used in combination with teamplay $macros\n", cmd_argv[0]);
					goto done;
				}
			}
#endif

			if (weight_disable == 0)
				cmd->weight++;

			if (cmd->function)
				cmd->function();
			else
				Cmd_ForwardToServer ();

			goto done;
		}
		else
		{
			Cbuf_AddTextEx(cbuf_cmdsave, text);
			Cbuf_AddTextEx(cbuf_cmdsave, "\n");
			goto done;
		}
	}

	// some bright guy decided to use "skill" as a mod command in Custom TF, sigh
	if (!strcmp(Cmd_Argv(0), "skill") && cmd_argc == 1 && Cmd_FindAlias("skill"))
		goto checkaliases;

	// check cvars
	if ((v = Cvar_FindVar (Cmd_Argv(0))))
	{
		if (weight_disable == 0)
			v->weight++;
#ifndef SERVERONLY
		if (cbuf_current == cbuf_formatted_comms)
		{
			Com_Printf("\"%s\" cannot be used in combination with teamplay $macros\n", cmd_argv[0]);
			goto done;
		}
#endif
		if (Cvar_Command())
			goto done;
	}

	// check aliases
checkaliases:
	if ((a = Cmd_FindAlias(cmd_argv[0])))
	{
		if (weight_disable == 0)
			a->weight++;
#ifndef SERVERONLY
		if (cbuf_current == cbuf_svc)
		{
			Cbuf_AddText (a->value);
			Cbuf_AddText ("\n");
		}
		else
#endif
		{

#ifdef SERVERONLY
			inserttarget = cbuf_main;
#else
			inserttarget = cbuf_current ? cbuf_current : cbuf_main;
#endif

			Cbuf_InsertTextEx (inserttarget, "\n");

			// if the alias value is a command or cvar and
			// the alias is called with parameters, add them
			if (Cmd_Argc() > 1 && !strchr(a->value, ' ') && !strchr(a->value, '\t')	&&
				(Cvar_FindVar(a->value) || (Cmd_FindCommand(a->value) && a->value[0] != '+' && a->value[0] != '-'))
				)
				{
					Cbuf_InsertTextEx (inserttarget, Cmd_Args());
					Cbuf_InsertTextEx (inserttarget, " ");
				}
				Cbuf_InsertTextEx (inserttarget, a->value);
		}
		goto done;
	}

#ifndef SERVERONLY
	if (Cmd_LegacyCommand())
		goto done;
#endif

#ifndef SERVERONLY
	if (cbuf_current != cbuf_svc)
#endif
	{
		if (cl_warncmd.value || developer.value)
			Com_Printf ("Unknown command \"%s\"\n", Cmd_Argv(0));
	}

done:
	cbuf_current = oldcontext;
}

void Cmd_ExecuteString (char *text)
{
	Cmd_ExecuteStringEx (NULL, text);
}

static qboolean is_numeric (char *c)
{
	return (*c >= '0' && *c <= '9') ||
		((*c == '-' || *c == '+') && (c[1] == '.' || (c[1]>='0' && c[1]<='9'))) ||
		(*c == '.' && (c[1]>='0' && c[1]<='9'));
}

void Cmd_If_f (void)
{
	int	i, c;
	char *op, buf[1024] = {0};
	qboolean result;

	if ((c = Cmd_Argc()) < 5)
	{
		Com_Printf ("Usage: if <expr1> <op> <expr2> <command> [else <command>]\n");
		return;
	}

	op = Cmd_Argv(2);
	if (!strcmp(op, "==") || !strcmp(op, "=") || !strcmp(op, "!=") || !strcmp(op, "<>"))
	{
		if (is_numeric(Cmd_Argv(1)) && is_numeric(Cmd_Argv(3)))
			result = Q_atof(Cmd_Argv(1)) == Q_atof(Cmd_Argv(3));
		else
			result = !strcmp(Cmd_Argv(1), Cmd_Argv(3));

		if (op[0] != '=')
			result = !result;
	}
	else if (!strcmp(op, ">"))
	{
		result = Q_atof(Cmd_Argv(1)) > Q_atof(Cmd_Argv(3));
	}
	else if (!strcmp(op, "<"))
	{
		result = Q_atof(Cmd_Argv(1)) < Q_atof(Cmd_Argv(3));
	}
	else if (!strcmp(op, ">="))
	{
		result = Q_atof(Cmd_Argv(1)) >= Q_atof(Cmd_Argv(3));
	}
	else if (!strcmp(op, "<="))
	{
		result = Q_atof(Cmd_Argv(1)) <= Q_atof(Cmd_Argv(3));

	}
	else if (!strcmp(op, "isin"))
	{
		result = (strstr(Cmd_Argv(3), Cmd_Argv(1)) ? 1 : 0);
	}
	else if (!strcmp(op, "!isin"))
	{
		result = (strstr(Cmd_Argv(3), Cmd_Argv(1)) ? 0 : 1);

	}
	else
	{
		Com_Printf ("unknown operator: %s\n", op);
		Com_Printf ("valid operators are ==, =, !=, <>, >, <, >=, <=, isin, !isin\n");
		return;
	}

	if (result)
	{
		for (i = 4; i < c; i++)
		{
			if ((i == 4) && !Q_strcasecmp(Cmd_Argv(i), "then"))
				continue;

			if (!Q_strcasecmp(Cmd_Argv(i), "else"))
				break;

			if (buf[0])
				strlcat(buf, " ", sizeof(buf));

			strlcat(buf, Cmd_Argv(i), sizeof(buf));
		}
	}
	else
	{
		for (i = 4; i < c ; i++)
		{
			if (!Q_strcasecmp(Cmd_Argv(i), "else"))
				break;
		}

		if (i == c)
			return;

		for (i++; i < c; i++)
		{
			if (buf[0])
				strlcat(buf, " ", sizeof(buf));

			strlcat(buf, Cmd_Argv(i), sizeof(buf));
		}
	}

	strlcat(buf, "\n", sizeof(buf));
	Cbuf_InsertTextEx (cbuf_current ? cbuf_current : cbuf_main, buf);
}

//Returns the position (1 to argc - 1) in the command's argument list where the given parameter apears, or 0 if not present
int Cmd_CheckParm (char *parm)
{
	int i, c;

	c = Cmd_Argc();
	for (i = 1; i < c; i++)
		if (! Q_strcasecmp (parm, Cmd_Argv (i)))
			return i;

	return 0;
}

void Cmd_EnableFunctionExecution()
{
	can_execute_functions = 1;

	Cbuf_ExecuteEx(cbuf_cmdsave);
}

static int cstc_alias_check(char *string, struct tokenized_string *ts)
{
	int i;

	for (i=0; i<ts->count; i++)
	{
		if (strstr(string, ts->tokens[i]) == NULL)
			return 0;
	}

	return 1;
}


static int cstc_alias_conditions(void)
{
	if (cmd_alias == NULL)
		return 0;
	return 1;
}

static int cstc_alias_get_results(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	int count;
	cmd_alias_t *a;
	extern cmd_alias_t	*cmd_alias;

	count = 0;
	if (results)
	{
		a = cmd_alias;
		while (a)
		{
			if (cstc_alias_check(a->name, self->tokenized_input))
				count++;
			a = a->next;
		}
		*results = count;
		return 0;
	}

	if (result == NULL)
		return 0;

	count = -1;

	a = cmd_alias;

	while (a)
	{
		if (cstc_alias_check(a->name, self->tokenized_input))
			count++;

		if (count == get_result)
		{
			if (a->value)
				*result = va("%s \"%s\"", a->name, a->value);
			else
				*result = va("%s \"\"", a->name);
			return 0;
		}

		a = a->next;
	}

	return 1;
}

struct cstc_cfginfo
{
	struct directory_list *dl;
	qboolean *checked;
	qboolean initialized;
};


static int cstc_exec_get_data(struct cst_info *self, int remove)
{
	struct cstc_cfginfo *data;
	const char * const cfg_endings[] = { ".cfg", NULL};

	if (!self)
		return 1;

	if (self->data)
	{
		data = (struct cstc_cfginfo *)self->data;
		Util_Dir_Delete(data->dl);
		free(data->checked);
		free(data);
		self->data = NULL;
	}

	if (remove)
		return 0;

	if ((data = calloc(1, sizeof(*data))))
	{
		if ((data->dl = Util_Dir_Read(va("%s/qw/", com_basedir), 1, 1, cfg_endings)))
		{
			if (data->dl->entries == 0)
			{
				cstc_exec_get_data(self, 1);
				return 1;
			}
			self->data = data;
			return 0;
		}
		free(data);
	}

	return 1;
}

static int cstc_exec_check(char *entry, struct tokenized_string *ts)
{
	int i;

	for (i=0; i<ts->count; i++)
	{
		if (Util_strcasestr(entry, ts->tokens[i]) == NULL)
			return 0;
	}
	return 1;
}

static int cstc_exec_get_results(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	struct cstc_cfginfo *data;
	int count, i;

	if (self->data == NULL)
		return 1;

	data = (struct cstc_cfginfo *)self->data;

	if (results || data->initialized == false)
	{
		if (data->checked)
			free(data->checked);
		if (!(data->checked= calloc(data->dl->entry_count, sizeof(qboolean))))
			return 1;

		for (i=0, count=0; i<data->dl->entry_count; i++)
		{
			if (cstc_exec_check(data->dl->entries[i].name, self->tokenized_input))
			{
				data->checked[i] = true;
				count++;
			}
		}
		if (results)
			*results = count;

		data->initialized = true;
		return 0;
	}

	if (result == NULL)
		return 0;

	for (i=0, count=-1; i<data->dl->entry_count; i++)
	{
		if (data->checked[i] == true)
			count++;
		if (count == get_result)
		{
			*result = data->dl->entries[i].name;
			return 0;
		}
	}
	return 1;
}

void Cmd_Init (void)
{
	// register our commands
	Cmd_AddCommand("exec", Cmd_Exec_f);
	Cmd_AddCommand("echo", Cmd_Echo_f);
	Cmd_AddCommand("aliaslist", Cmd_AliasList_f);
	//Cmd_AddCommand("aliasa", Cmd_Alias_f);
	Cmd_AddCommand("alias", Cmd_Alias_f);
	Cmd_AddCommand("tempalias", Cmd_Alias_f);
	Cmd_AddCommand("viewalias", Cmd_Viewalias_f);
	Cmd_AddCommand("unaliasall", Cmd_UnAliasAll_f);
	Cmd_AddCommand("unalias", Cmd_UnAlias_f);
	Cmd_AddCommand("wait", Cmd_Wait_f);
	Cmd_AddCommand("cmdlist", Cmd_CmdList_f);
	Cmd_AddCommand("if", Cmd_If_f);
	Cmd_AddCommand("macrolist", Cmd_MacroList_f);

	CSTC_Add("alias", &cstc_alias_conditions, &cstc_alias_get_results, NULL, NULL, 0, "arrow up/down to navigate");
	CSTC_Add("exec", NULL, &cstc_exec_get_results, &cstc_exec_get_data, NULL, CSTC_EXECUTE, "arrow up/down to navigate");
}

void Cmd_Shutdown()
{
	cmd_function_t *func;

	while((func = cmd_functions))
	{
		cmd_functions = func->next;
		free(func);
	}
}

