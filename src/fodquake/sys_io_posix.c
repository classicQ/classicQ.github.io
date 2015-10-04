#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include "sys_io.h"

#include "dirent.h"
#include "sys/stat.h"

struct SysFile
{
	FILE *f;
	unsigned int length;
};

void Sys_IO_Create_Directory(const char *path)
{
	mkdir(path, 0777);
}

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque)
{
	DIR *dir;
	struct dirent dirent;
	struct dirent *posix_is_braindead;
	struct directory_entry de;
	struct stat st;
	char buf[4096];
	int r;
	int ret;

	ret = 0;

	snprintf(buf, sizeof(buf), "%s%s%s", basedir, subdir?"/":"", subdir?subdir:"");

	dir = opendir(buf);
	if (dir)
	{
		de.name = buf;

		while((r = readdir_r(dir, &dirent, &posix_is_braindead)) == 0 && posix_is_braindead)
		{
			if (strcmp(dirent.d_name, ".") == 0 || strcmp(dirent.d_name, "..") == 0)
				continue;

			snprintf(buf, sizeof(buf), "%s/%s%s%s", basedir, subdir?subdir:"", subdir?"/":"", dirent.d_name);
			if (stat(buf, &st) == -1)
				continue;

			snprintf(buf, sizeof(buf), "%s%s%s", subdir?subdir:"", subdir?"/":"", dirent.d_name);

			de.type = S_ISDIR(st.st_mode)?et_dir:et_file;
			de.size = st.st_size;

			if (!callback(opaque, &de))
			{
				r = 1;
				break;
			}
		}

		if (r == 0)
			ret = 1;

		closedir(dir);
	}

	return ret;
}

int Sys_IO_Path_Exists(const char *path)
{
	return access(path, F_OK) == 0;
}

int Sys_IO_Path_Writable(const char *path)
{
	return access(path, W_OK) == 0;
}

struct SysFile *Sys_IO_Open_File_Read(const char *path, enum Sys_IO_File_Status *filestatus)
{
	struct SysFile *sysfile;
	struct stat st;

	if (filestatus)
		*filestatus = SIOFS_NOT_FOUND;

	if (stat(path, &st) != 0)
		return 0;

	sysfile = malloc(sizeof(*sysfile));
	if (sysfile)
	{
		sysfile->f = fopen(path, "r");
		if (sysfile->f)
		{
			sysfile->length = st.st_size;

			if (filestatus)
				*filestatus = SIOFS_OK;

			return sysfile;
		}

		free(sysfile);
	}

	return 0;
}

void Sys_IO_Close_File(struct SysFile *sysfile)
{
	fclose(sysfile->f);
	free(sysfile);
}

int Sys_IO_Get_File_Length(struct SysFile *sysfile)
{
	return sysfile->length;
}

int Sys_IO_Get_File_Position(struct SysFile *sysfile)
{
	return ftell(sysfile->f);
}

void Sys_IO_Set_File_Position(struct SysFile *sysfile, int position)
{
	fseek(sysfile->f, position, SEEK_SET);
}

int Sys_IO_Read_File(struct SysFile *sysfile, void *buffer, int length)
{
	return fread(buffer, 1, length, sysfile->f);
}

