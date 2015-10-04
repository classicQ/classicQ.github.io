/*
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

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define HEADERID (('P'<<24)|('A'<<16)|('C'<<8)|'K')

struct pakdirentry
{
	char name[56];
	unsigned int offset;
	unsigned int length;
};

static void WriteU32BE(void *p, unsigned int value)
{
	unsigned char *c;

	c = p;

	c[0] = value>>24;
	c[1] = value>>16;
	c[2] = value>>8;
	c[3] = value;
}

static void WriteU32LE(void *p, unsigned int value)
{
	unsigned char *c;

	c = p;

	c[0] = value;
	c[1] = value>>8;
	c[2] = value>>16;
	c[3] = value>>24;
}

static int scanpakdir(FILE *f, long *offset, const char *ospath, const char *pakpath, struct pakdirentry **dirlist, unsigned int *numfiles)
{
	DIR *dir;
	struct dirent *dent;
	struct stat st;
	char *fullpath;
	char *newpakpath;
	size_t s;
	int r;
	int ret;
	int appendslash;
	int error = 0;
	struct pakdirentry *pakdirentry;
	FILE *inf;
	char buf[4096];
	unsigned int bytesremaining;

	ret = 0;

	dir = opendir(ospath);
	if (dir)
	{
		if (*ospath && ospath[strlen(ospath)-1] != '/')
			appendslash = 1;
		else
			appendslash = 0;

		while((dent = readdir(dir)))
		{
			if (*dent->d_name == '.')
				continue;

			error = 1;
			fullpath = malloc(strlen(ospath)+1+strlen(dent->d_name)+1);
			if (fullpath)
			{
				sprintf(fullpath, "%s%s%s", ospath, appendslash?"/":"", dent->d_name);

				if (stat(fullpath, &st) == 0)
				{
					if (S_ISDIR(st.st_mode))
					{
						newpakpath = malloc(strlen(pakpath)+strlen(dent->d_name)+1+1);
						if (newpakpath)
						{
							sprintf(newpakpath, "%s%s/", pakpath, dent->d_name);

							if (scanpakdir(f, offset, fullpath, newpakpath, dirlist, numfiles))
								error = 0;

							free(newpakpath);
						}
					}
					else
					{
						if (*numfiles <= 65535)
						{
							pakdirentry = realloc(*dirlist, (*numfiles + 1) * sizeof(*pakdirentry));
							if (pakdirentry)
							{
								*dirlist = pakdirentry;
								pakdirentry += *numfiles;
								r = snprintf(pakdirentry->name, sizeof(pakdirentry->name), "%s%s", pakpath, dent->d_name);
								if (r >= sizeof(pakdirentry->name))
								{
									printf("The length of \"%s%s\" exceeds the maximum possible path length\n", pakpath, dent->d_name);
								}
								else
								{
									WriteU32LE(&pakdirentry->offset, *offset);
									WriteU32LE(&pakdirentry->length, st.st_size);
									(*numfiles)++;

									inf = fopen(fullpath, "rb");
									if (inf)
									{
										bytesremaining = st.st_size;
										while(bytesremaining)
										{
											s = fread(buf, 1, sizeof(buf), inf);
											if (s <= 0)
											{
												fprintf(stderr, "Unable to read from \"%s\"\n", fullpath);
												break;
											}

											if (fwrite(buf, 1, s, f) != s)
											{
												fprintf(stderr, "Unable to write to pak file\n");
												break;
											}

											bytesremaining -= s;
										}

										if (!bytesremaining)
										{
											error = 0;
											(*offset) += st.st_size;
										}

										fclose(inf);
									}
								}
							}
						}
					}
				}

				free(fullpath);
			}

			if (error)
				break;
		}

		if (dent == 0)
			ret = 1;

		closedir(dir);
	}

	return ret;
}

static struct pakdirentry *buildpakdirlist(FILE *f, const char *path, unsigned int *numfiles)
{
	struct pakdirentry *dirlist;
	long offset;

	dirlist = 0;
	*numfiles = 0;
	offset = ftell(f);

	if (offset == -1)
		return 0;

	if (!scanpakdir(f, &offset, path, "", &dirlist, numfiles))
		return 0;
	
	return dirlist;
}

int main(int argc, char **argv)
{
	struct pakdirentry *pakdirentry;
	unsigned int numfiles;
	unsigned char buf[12];
	FILE *f;
	long offset;
	int r;
	int ret;

	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s filename.pak directory\n", argv[0]);
		return 1;
	}

	ret = 1;

	f = fopen(argv[1], "wb");
	if (f)
	{
		WriteU32BE(buf, HEADERID);
		WriteU32LE(buf+4, 0);
		WriteU32LE(buf+8, 0);

		if (fwrite(buf, 1, 12, f) != 12)
		{
			printf("Unable to write to \"%s\"\n", argv[1]);
		}
		else
		{
			pakdirentry = buildpakdirlist(f, argv[2], &numfiles);
			if (pakdirentry)
			{
				offset = ftell(f);
				if (offset != -1)
				{
					r = fwrite(pakdirentry, 1, sizeof(*pakdirentry)*numfiles, f) != sizeof(*pakdirentry)*numfiles;
					if (r == 0)
						r = fseek(f, 4, SEEK_SET);
					if (r == 0)
					{
						WriteU32LE(buf+4, offset);
						WriteU32LE(buf+8, sizeof(*pakdirentry)*numfiles);
						r = fwrite(buf + 4, 1, 8, f) != 8;
					}

					if (r != 0)
						printf("Unable to write to \"%s\"\n", argv[1]);
					else
					{
						ret = 0;
					}
				}
			}
		}

		fclose(f);

		if (ret != 0)
			remove(argv[1]);
	}

	return ret;
}

