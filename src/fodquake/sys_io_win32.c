#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "sys_io.h"

#include "windows.h"

void Sys_IO_Create_Directory(const char *path)
{
	CreateDirectory(path, 0);
}

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque)
{
	WIN32_FIND_DATA fd;
	HANDLE h;
	struct directory_entry de;
	char buf[4096];
	int ret;

	snprintf(buf, sizeof(buf), "%s%s%s/*", basedir, subdir?"/":"", subdir?subdir:"");

	h = FindFirstFile(buf, &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
			return 1;
		else
			return 0;
	}

	de.name = buf;

	ret = 0;
	while(1)
	{
		if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0)
		{
			snprintf(buf, sizeof(buf), "%s%s%s", subdir?subdir:"", subdir?"/":"", fd.cFileName);

			de.type = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)?et_dir:et_file;
			de.size = (((unsigned long long)fd.nFileSizeHigh)<<32)|fd.nFileSizeLow;

			if (!callback(opaque, &de))
				break;
		}

		if (FindNextFile(h, &fd) == 0)
		{
			if (GetLastError() == ERROR_NO_MORE_FILES)
				ret = 1;

			break;
		}
	}

	FindClose(h);

	return ret;
}

int Sys_IO_Path_Exists(const char *path)
{
	DWORD attributes;

	attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES;
}

int Sys_IO_Path_Writable(const char *path)
{
	DWORD attributes;

	attributes = GetFileAttributesA(path);

	return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_READONLY);
}

