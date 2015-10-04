/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Mark Olsen

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

#include "common.h"
#include "sys_io.h"
#include "filesystem.h"
#include "draw.h"
#include "strl.h"
#include "context_sensitive_tab.h"
#include "utils.h"
#include "tokenize_string.h"

// in memory
struct packfile
{
	char name[MAX_QPATH];
	int filepos, filelen;
} packfile_t;

struct pack
{
	char filename[MAX_OSPATH];
	FILE *handle;
	int numfiles;
	struct packfile *files;
};

struct dpackheader
{
	char id[4];
	int dirofs;
	int dirlen;
};

struct dpackfile
{
	char name[56];
	int filepos, filelen;
};

// on disk
struct searchpath
{
	char filename[MAX_OSPATH];
	struct pack *pack;	// only one of filename / pack will be used
	struct searchpath *next;
};

static struct searchpath *com_searchpaths;
static struct searchpath *com_base_searchpaths;	// without gamedirs

static const char * const skins_endings[] = { ".pcx", ".png", NULL};	//endings for skins files

static int FS_FileLength(FILE * f)
{
	int pos, end;

	pos = ftell(f);
	fseek(f, 0, SEEK_END);
	end = ftell(f);
	fseek(f, pos, SEEK_SET);

	return end;
}

int FS_FileOpenRead(const char *path, FILE ** hndl)
{
	FILE *f;

	if (!(f = fopen(path, "rb")))
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return FS_FileLength(f);
}

//The filename will be prefixed by com_basedir
qboolean FS_WriteFile(const char *filename, void *data, int len)
{
	FILE *f;
	char name[MAX_OSPATH];

	snprintf(name, sizeof(name), "%s/%s", com_basedir, filename);

	if (!(f = fopen(name, "wb")))
	{
		FS_CreatePath(name);
		if (!(f = fopen(name, "wb")))
			return false;
	}
	Sys_Printf("FS_WriteFile: %s\n", name);
	fwrite(data, 1, len, f);
	fclose(f);
	return true;
}

//Only used for CopyFile and download


void FS_CreatePath(char *path)
{
	char *s, save;

	if (!*path)
		return;

	for (s = path + 1; *s; s++)
	{
#ifdef _WIN32
		if (*s == '/' || *s == '\\')
		{
#else
		if (*s == '/')
		{
#endif
			save = *s;
			*s = 0;
			Sys_IO_Create_Directory(path);
			*s = save;
		}
	}
}

//Finds the file in the search path.
//Sets com_filesize, com_netpath and one of handle or file
int file_from_pak;		// global indicating file came from pack file ZOID
int file_from_gamedir;

static int pakfile_compare(const void *a, const void *b)
{
	const struct packfile *paka, *pakb;

	paka = a;
	pakb = b;

	return strcmp(paka->name, pakb->name);
}

int FS_FOpenFile(const char *filename, FILE ** file)
{
	struct searchpath *search;
	struct pack *pak;
	struct packfile *pakfile;

	*file = NULL;
	file_from_pak = 0;
	file_from_gamedir = 1;
	com_filesize = -1;
	com_netpath[0] = 0;

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next)
	{
		if (search == com_base_searchpaths && com_searchpaths != com_base_searchpaths)
			file_from_gamedir = 0;

		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;

			pakfile = bsearch(filename, pak->files, pak->numfiles, sizeof(*pak->files), pakfile_compare);

			if (pakfile)
			{
				if (developer.value)
					Sys_Printf("PackFile: %s : %s\n", pak->filename, filename);
				// open a new file on the pakfile
				if (!(*file = fopen(pak->filename, "rb")))
					Sys_Error("Couldn't reopen %s", pak->filename);
				fseek(*file, pakfile->filepos, SEEK_SET);
				com_filesize = pakfile->filelen;

				file_from_pak = 1;
				snprintf(com_netpath, sizeof(com_netpath), "%s#%i", pak->filename, pakfile - pak->files);
				return com_filesize;
			}
		}
		else
		{
			snprintf(com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

			if (!(*file = fopen(com_netpath, "rb")))
				continue;

			if (developer.value)
				Sys_Printf("FindFile: %s\n", com_netpath);

			com_filesize = FS_FileLength(*file);
			return com_filesize;
		}
	}

	if (developer.value)
		Sys_Printf("FindFile: can't find %s\n", filename);

	return -1;
}

int FS_FileExists(const char *filename)
{
	struct searchpath *search;
	struct pack *pak;
	struct packfile *pakfile;
	FILE *f;

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next)
	{
		// is the element a pak file?
		if (search->pack)
		{
			// look through all the pak file elements
			pak = search->pack;

			pakfile = bsearch(filename, pak->files, pak->numfiles, sizeof(*pak->files), pakfile_compare);

			if (pakfile)
			{
				return 1;
			}
		}
		else
		{
			snprintf(com_netpath, sizeof(com_netpath), "%s/%s", search->filename, filename);

			if (!(f = fopen(com_netpath, "rb")))
				continue;

			fclose(f);

			return 1;
		}
	}

	return 0;
}

//Filename are relative to the quake directory.
//Always appends a 0 byte to the loaded data.
static byte *FS_LoadFile(const char *path, int usehunk)
{
	FILE *h;
	byte *buf;
	int len;
	int r;

	buf = NULL;		// quiet compiler warning

	// look for it in the filesystem or pack files
	len = com_filesize = FS_FOpenFile(path, &h);
	if (!h)
		return NULL;

	if (usehunk == 0)
	{
		buf = Z_Malloc(len + 1);
	}
	else if (usehunk == 5)
	{
		buf = malloc(len + 1);
	}
	else
	{
		Sys_Error("FS_LoadFile: bad usehunk");
	}

	if (!buf)
		Sys_Error("FS_LoadFile: not enough space for %s", path);

	((byte *) buf)[len] = 0;

	r = fread(buf, 1, len, h);
	fclose(h);
	if (r != len)
		Sys_Error("FS_LoadFile: Error while reading file %s", path);

	return buf;
}

void *FS_LoadZFile(const char *path)
{
	return FS_LoadFile(path, 0);
}

void *FS_LoadMallocFile(const char *path)
{
	return FS_LoadFile(path, 5);
}

static int packfile_name_compare(const void *pack1, const void *pack2)
{
	return strcmp(((const struct packfile *)pack1)->name, ((const struct packfile *)pack2)->name);
}

/*
Takes an explicit (not game tree related) path to a pak file.

Loads the header and directory, adding the files at the beginning
of the list so they override previous pack files.
*/
static struct pack *FS_LoadPackFile(char *packfile)
{

	struct dpackheader header;
	int i;
	int j;
	int filelen;
	struct packfile *newfiles;
	struct pack *pack;
	FILE *packhandle;
	struct dpackfile *info;

	if ((filelen = FS_FileOpenRead(packfile, &packhandle)) == -1)
		return NULL;

	fread(&header, 1, sizeof(header), packhandle);
	if (filelen < 12 || header.id[0] != 'P' || header.id[1] != 'A' || header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error("%s is not a packfile", packfile);
	header.dirofs = LittleLong(header.dirofs);
	header.dirlen = LittleLong(header.dirlen);

	if (header.dirofs < 0 || header.dirlen < 0 || header.dirofs > filelen || header.dirofs + header.dirlen > filelen || header.dirofs + header.dirlen < header.dirofs)
		Sys_Error("%s is a malformed packfile", packfile);

	pack = Q_Malloc(sizeof(*pack));
	strcpy(pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = header.dirlen / sizeof(struct dpackfile);

	pack->files = newfiles = Q_Malloc(pack->numfiles * sizeof(struct packfile));
	info = Q_Malloc(header.dirlen);

	fseek(packhandle, header.dirofs, SEEK_SET);
	fread(info, 1, header.dirlen, packhandle);

	// parse the directory
	for (i = 0; i < pack->numfiles; i++)
	{
		for(j=0;info[i].name[j]&&j<sizeof(info[i].name);j++);

		if (j == sizeof(info[i].name))
			Sys_Error("%s is a malformed packfile", packfile);

		strcpy(newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);

		if (newfiles[i].filepos < 0 || newfiles[i].filelen < 0 || newfiles[i].filepos > filelen || newfiles[i].filepos + newfiles[i].filelen > filelen || newfiles[i].filepos + newfiles[i].filelen < newfiles[i].filepos)
			Sys_Error("%s is a malformed packfile", packfile);

	}

	free(info);

	/* Sort the entries by name to make it easier to search */
	qsort(newfiles, pack->numfiles, sizeof(*newfiles), packfile_name_compare);

	return pack;
}

static void FS_FreePackFile(struct pack *pack)
{
	fclose(pack->handle);
	free(pack->files);
	free(pack);
}

static void FS_FreeSearchPaths(struct searchpath *searchpaths)
{
	struct searchpath *t;

	while((t = searchpaths))
	{
		searchpaths = searchpaths->next;

		if (t->pack)
			FS_FreePackFile(t->pack);

		free(t);
	}
}

static const char *fs_basedirs[5];

//Sets com_gamedir, adds the directory to the head of the path, then loads and adds pak1.pak pak2.pak ... 
static void FS_AddGameDirectory_NoReally(const char *dir)
{
	int i;
	struct searchpath *firstsearch;
	struct searchpath *search;
	struct pack *pak;
	char pakfile[MAX_OSPATH], *p;
	int error;

	if (strlen(dir) + 1 >= sizeof(com_gamedir))
	{
		Sys_Error("Game directory path too long\n");
		return;
	}

	p = strrchr(dir, '/');
	if (p == 0)
		return;

	if (strlen(p) + 1 >= sizeof(com_gamedirfile))
	{
		Sys_Error("Game directory path too long\n");
		return;
	}

	strcpy(com_gamedirfile, p + 1);
	strcpy(com_gamedir, dir);

	search = com_searchpaths;
	while(search)
	{
		if (strcmp(search->filename, dir) == 0)
			break;

		search = search->next;
	}

	/* Path already exists */
	if (search)
		return;

	// add the directory to the search path
	search = malloc(sizeof(*search));
	if (search)
	{
		strcpy(search->filename, dir);
		search->pack = NULL;
		search->next = com_searchpaths;
		firstsearch = search;

		error = 0;
		// add any pak files in the format pak0.pak pak1.pak, ...
		for (i = 0;; i++)
		{
			search = malloc(sizeof(*search));
			if (search == 0)
			{
				error = 1;
				break;
			}

			snprintf(pakfile, sizeof(pakfile), "%s/pak%i.pak", dir, i);
			if (!(pak = FS_LoadPackFile(pakfile)))
			{
				free(search);
				break;
			}
			strlcpy(search->filename, pakfile, sizeof(search->filename));
			search->pack = pak;
			search->next = firstsearch;
			firstsearch = search;
		}

		if (!error)
		{
			com_searchpaths = firstsearch;
			return;
		}

		FS_FreeSearchPaths(firstsearch);
		com_searchpaths = 0;
	}

	Sys_Error("FS_AddGameDirectory: Failed to add \"%s\"\n", dir);
}

static void FS_AddGameDirectory(const char *dir)
{
	unsigned int i;

	i = 0;

	while(fs_basedirs[i])
	{
		FS_AddGameDirectory_NoReally(va("%s/%s", fs_basedirs[i], dir));
		i++;
	}
}

//Sets the gamedir and path to a different directory.
void FS_SetGamedir(const char *dir)
{
	struct searchpath *search;

	if (strstr(dir, "..") || strstr(dir, "/") || strstr(dir, "\\") || strstr(dir, ":"))
	{
		Com_Printf("Gamedir should be a single filename, not a path\n");
		return;
	}

	if (!strcmp(com_gamedirfile, dir))
		return;		// still the same

	// free up any current game dir info
	if (com_searchpaths != com_base_searchpaths)
	{
		search = com_searchpaths;

		while(search->next != com_base_searchpaths)
			search = search->next;

		search->next = 0;
		FS_FreeSearchPaths(com_searchpaths);
		com_searchpaths = com_base_searchpaths;
	}

	FS_AddGameDirectory(dir);
}

const char *ro_data_path;
const char *user_data_path;
const char *legacy_data_path;

static int FS_CheckSubpathExist(const char *path, const char *subpath)
{
	char *p;
	int ret;

	p = malloc(strlen(path) + 1 + strlen(subpath) + 1);
	if (p == 0)
		Sys_Error("Cry, cry");

	sprintf(p, "%s/%s", path, subpath);

	ret = Sys_IO_Path_Exists(p);

	free(p);

	return ret;
}

void FS_InitFilesystem(void)
{
	int i;
	int preferlegacy;
	int legacywritable;
	int userwritable;
	unsigned int dircount;

	ro_data_path = Sys_GetRODataPath();
	user_data_path = Sys_GetUserDataPath();
	legacy_data_path = Sys_GetLegacyDataPath();

	preferlegacy = 0;
	legacywritable = 0;

	if (legacy_data_path)
	{
		if (Sys_IO_Path_Exists(legacy_data_path) && (FS_CheckSubpathExist(legacy_data_path, "id1") || FS_CheckSubpathExist(legacy_data_path, "fodquake") || FS_CheckSubpathExist(legacy_data_path, "qw")))
		{
			legacywritable = Sys_IO_Path_Writable(legacy_data_path);
			if (legacywritable)
			{
				preferlegacy = FS_CheckSubpathExist(legacy_data_path, "fodquake/temp"); /* That directory is created by MT_Init() */
			}
		}
		else
		{
			Sys_FreePathString(legacy_data_path);
			legacy_data_path = 0;
		}
	}

	userwritable = 0;

	if (user_data_path)
	{
		Sys_IO_Create_Directory(user_data_path);

		if (Sys_IO_Path_Exists(user_data_path))
		{
			userwritable = Sys_IO_Path_Writable(user_data_path);
		}
		else
		{
			Sys_FreePathString(user_data_path);
			user_data_path = 0;
		}
	}

	if (!legacywritable && !userwritable)
	{
		Sys_Error("Nowhere to write data. Check that either \"%s\" or \"%s\" is writable.\n", legacy_data_path?legacy_data_path:"<none>", user_data_path?user_data_path:"<none>");
	}

	dircount = 0;

	if (ro_data_path)
	{
		fs_basedirs[dircount++] = ro_data_path;
	}

	if (user_data_path && preferlegacy)
	{
		fs_basedirs[dircount++] = user_data_path;
	}

	if (legacy_data_path)
	{
		fs_basedirs[dircount++] = legacy_data_path;
	}

	if (user_data_path && !preferlegacy)
	{
		fs_basedirs[dircount++] = user_data_path;
	}

	if ((i = COM_CheckParm("-basedir")) && i < com_argc - 1)
	{
#warning Create a Sys_ function for converting paths.
		for (i = 0; i < strlen(com_basedir); i++)
			if (com_basedir[i] == '\\')
				com_basedir[i] = '/';

		fs_basedirs[dircount++] = user_data_path;
	}

	strlcpy(com_basedir, fs_basedirs[dircount - 1], sizeof(com_basedir));

	fs_basedirs[dircount] = 0;

	FS_AddGameDirectory("id1");
	FS_AddGameDirectory("fodquake");
	FS_AddGameDirectory("qw");

	i = strlen(com_basedir) - 1;
	if (i >= 0 && com_basedir[i] == '/')
		com_basedir[i] = 0;

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;

	// the user might want to override default game directory
	if (!(i = COM_CheckParm("-game")))
		i = COM_CheckParm("+gamedir");
	if (i && i < com_argc - 1)
		FS_SetGamedir(com_argv[i + 1]);
}

void FS_ShutdownFilesystem(void)
{
	FS_FreeSearchPaths(com_searchpaths);
	com_searchpaths = 0;

	if (ro_data_path)
		Sys_FreePathString(ro_data_path);

	if (user_data_path)
		Sys_FreePathString(user_data_path);

	if (legacy_data_path)
		Sys_FreePathString(legacy_data_path);
}

static void FS_Path_f(void)
{
	struct searchpath *s;

	Com_Printf("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next)
	{
		if (s == com_base_searchpaths)
			Com_Printf("----------\n");
		if (s->pack)
			Com_Printf("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Com_Printf("%s\n", s->filename);
	}
}

struct cstc_skindata
{
	qboolean initialized;
	qboolean *checked;
	struct directory_list *dl;
	struct Picture *picture;
};

static int cstc_skins_get_data(struct cst_info *self, int remove)
{
	struct cstc_skindata *data;

	if (!self)
		return 1;

	if (self->data)
	{
		data = (struct cstc_skindata *)self->data;
		Util_Dir_Delete(data->dl);
		if (data->picture)
			Draw_FreePicture(data->picture);
		free(data);
		self->data = NULL;
	}

	if (remove)
		return 0;

	if ((data = calloc(1, sizeof(*data))))
	{
		if ((data->dl = Util_Dir_Read(va("%s/qw/skins/", com_basedir), 1, 1, skins_endings)))
		{
			if (data->dl->entry_count != 0)
			{
				self->data = (void *)data;
				return 0;
			}
			else
			{
				Com_Printf(va("%s/qw/skins/ has no skin files in it.\n", com_basedir));
			}
			Util_Dir_Delete(data->dl);
		}
		free(data);
	}
	return 1;
}

static int cstc_skins_check(char *entry, struct tokenized_string *ts)
{
	int i;

	for (i=0; i<ts->count; i++)
	{
		if (Util_strcasestr(entry, ts->tokens[i]) == NULL)
			return 0;
	}
	return 1;
}

static int cstc_skins_get_results(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	struct cstc_skindata *data;
	int count, i;

	if (self->data == NULL)
		return 1;

	data = (struct cstc_skindata *)self->data;

	if (results || data->initialized == false)
	{
		if (data->checked)
			free(data->checked);
		if ((data->checked = calloc(data->dl->entry_count, sizeof(qboolean))) == NULL)
			return 1;

		for (i=0, count=0; i<data->dl->entry_count; i++)
		{
			if (cstc_skins_check(data->dl->entries[i].name, self->tokenized_input))
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

static int cstc_skins_condition(void)
{
	struct directory_list *data;
	int i;

	data = Util_Dir_Read(va("%s/qw/skins/", com_basedir), 1, 1, skins_endings);

	if (data == NULL)
		return 0;

	if (data->entry_count == 0)
	{
		Com_Printf(va("no skins found in \"%s/qw/skins/\".\n", com_basedir));
		Util_Dir_Delete(data);
		i = 0;
	}
	else
		i = 1;

	Util_Dir_Delete(data);

	return i;
}

static void cstc_skins_draw(struct cst_info *self)
{
	struct cstc_skindata *data;
	int x, y;
	int i, count;
	char *s;


	if (self->data == NULL)
		return;

	data = (struct cstc_skindata *) self->data;

	if (data->picture && ( self->toggleables_changed || self->selection_changed))
	{
		Draw_FreePicture(data->picture);
		data->picture = NULL;
	}

	if (data->picture == NULL)
	{
		for (i=0, count=-1; i<data->dl->entry_count; i++)
		{
			if (data->checked[i] == true)
				count++;
			if (count == self->selection)
				break;
		}
		if (i == data->dl->entry_count)
			return;

		data->picture = Draw_LoadPicture(va("skins/%s", data->dl->entries[i].name), DRAW_LOADPICTURE_NOFALLBACK);
	}

	if (data->picture == NULL)
	{
		s = va("not a valid skin.");
		x = vid.conwidth - strlen(s) * 8;
		y = self->offset_y;
		Draw_Fill(x, y, 8 * strlen(s), 8, 0);
		Draw_String(x, y, s);
		return;
	}

	y = self->offset_y - (self->direction == -1 ? 200: 0);
	x = vid.conwidth - 320;

	Draw_DrawPicture(data->picture, x, y, 320, 200);//, Draw_GetPictureWidth(self->picture), Draw_GetPictureHeight(self->picture));
	return;
}

void FS_Init()
{
	CSTC_Add("enemyskin enemyquadskin enemypentskin enemybothskin teamskin teamquadskin teampentskin teambothskin", &cstc_skins_condition, &cstc_skins_get_results, &cstc_skins_get_data, &cstc_skins_draw, CSTC_MULTI_COMMAND| CSTC_NO_INPUT| CSTC_EXECUTE, "arrow up/down to navigate");
	Cmd_AddCommand("path", FS_Path_f);
}

