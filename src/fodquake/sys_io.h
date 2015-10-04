#ifndef SYS_IO_H
#define SYS_IO_H

struct SysFile;

enum Sys_IO_File_Status
{
	SIOFS_OK,
	SIOFS_NOT_FOUND,
	SIOFS_TRY_AGAIN,
};

enum directory_entry_type
{
	et_file,
	et_dir,
};

struct directory_entry
{
	char *name;
	unsigned long long size;
	enum directory_entry_type type;
};


void Sys_IO_Create_Directory(const char *path);

int Sys_IO_Read_Dir(const char *basedir, const char *subdir, int (*callback)(void *opaque, struct directory_entry *de), void *opaque);

int Sys_IO_Path_Exists(const char *path);
int Sys_IO_Path_Writable(const char *path);

struct SysFile *Sys_IO_Open_File_Read(const char *path, enum Sys_IO_File_Status *filestatus);
struct SysFile *Sys_IO_Open_File_Write(const char *path);
void Sys_IO_Close_File(struct SysFile *);
int Sys_IO_Get_File_Length(struct SysFile *);
int Sys_IO_Get_File_Position(struct SysFile *);
void Sys_IO_Set_File_Position(struct SysFile *, int position);
int Sys_IO_Read_File(struct SysFile *, void *buffer, int length);
int Sys_IO_Write_File(struct SysFile *, const void *buffer, int length);

#endif /* SYS_IO_H */

