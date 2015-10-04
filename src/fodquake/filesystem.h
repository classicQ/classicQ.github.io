#include <stdio.h>

#include "qtypes.h"

void FS_InitFilesystem(void);
void FS_ShutdownFilesystem(void);

int FS_FileOpenRead(const char *path, FILE **hndl);
void FS_CreatePath(char *path);
qboolean FS_WriteFile(const char *filename, void *data, int len);
void FS_SetGamedir(const char *dir);
int FS_FOpenFile(const char *filename, FILE **file);
int FS_FileExists(const char *filename);
void *FS_LoadZFile(const char *path);
void *FS_LoadMallocFile(const char *path);

void FS_Init(void);

