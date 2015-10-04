#include <proto/dos.h>

#include "sys_io.h"

void Sys_IO_Create_Directory(const char *path)
{
	BPTR lock;

	lock = CreateDir(path);
	if (lock)
	{
		UnLock(lock);
	}
}

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque)
{
	return 0;
}

int Sys_IO_Path_Exists(const char *path)
{
	BPTR lock;

	lock = Lock(path, ACCESS_READ);
	if (lock)
		UnLock(lock);

	return !!lock;
}

int Sys_IO_Path_Writable(const char *path)
{
	struct InfoData id;
	BPTR lock;
	int ret;

	ret = 0;

	lock = Lock(path, ACCESS_READ);
	if (lock)
	{
		if (Info(lock, &id))
		{
			if (id.id_DiskState == ID_VALIDATED)
				ret = 1;
		}

		UnLock(lock);
	}

	return !!lock;
}

struct SysFile *Sys_IO_Open_File_Read(const char *path, enum Sys_IO_File_Status *filestatus)
{
	BPTR fh;

	fh = Open(path, MODE_OLDFILE);

	if (filestatus)
	{
		if (fh)
			*filestatus = SIOFS_OK;
		else
			*filestatus = SIOFS_NOT_FOUND;
	}

	return (struct SysFile *)fh;
}

struct SysFile *Sys_IO_Open_File_Write(const char *path)
{
	return 0;
}

void Sys_IO_Close_File(struct SysFile *sysfile)
{
	Close((BPTR)sysfile);
}

int Sys_IO_Get_File_Length(struct SysFile *sysfile)
{
	struct FileInfoBlock fib __attribute__((aligned(4)));

	if (!ExamineFH((BPTR)sysfile, &fib))
		return -1;

	return fib.fib_Size;
}

int Sys_IO_Get_File_Position(struct SysFile *sysfile)
{
	return Seek((BPTR)sysfile, 0, OFFSET_CURRENT);
}

void Sys_IO_Set_File_Position(struct SysFile *sysfile, int position)
{
	Seek((BPTR)sysfile, position, OFFSET_BEGINNING);
}

int Sys_IO_Read_File(struct SysFile *sysfile, void *buffer, int length)
{
	return Read((BPTR)sysfile, buffer, length);
}

int Sys_IO_Write_File(struct SysFile *sysfile, const void *buffer, int length)
{
	return -1;
}

