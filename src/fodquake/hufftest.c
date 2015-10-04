#if HUFFTEST
#include <stdio.h>
#include "common.h"

static unsigned int huffcounttable[256];

static int huffdocount;

void huff_countbytes(unsigned char *data, int len)
{
	while(len--)
	{
		huffcounttable[*data++]++;
	}
}

void huff_loadfile(char *name)
{
	unsigned int buf[256];
	FILE *f;
	int i;

	f = fopen(name, "rb");
	if (f)
	{
		i = fread(buf, sizeof(buf), 1, f);
		if (i != 1)
			Com_Printf("Failed to load huff data!\n");

		for(i=0;i<256;i++)
			huffcounttable[i] = BigLong(buf[i]);

		fclose(f);
	}
	else
		Com_Printf("Failed to load huff file!\n");
}

void huff_savefile(char *name)
{
	unsigned int buf[256];
	FILE *f;
	int i;

	huffdocount = 0;

	f = fopen(name, "wb");
	if (f)
	{
		for(i=0;i<256;i++)
			buf[i] = BigLong(huffcounttable[i]);

		i = fwrite(buf, sizeof(buf), 1, f);
		if (i != 1)
			Com_Printf("Failed to save huff data!\n");

		fclose(f);
	}
	else
		Com_Printf("Failed to open huff file!\n");
}
#endif
