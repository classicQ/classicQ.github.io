#include "readablechars.h"

char readablechars[256] =
{
	'.', '_' , '_' , '_' , '_' , '.' , '_' , '_' , '_' , '_' , 10 , '_' , 10 , '>' , '.' , '.',
	'[', ']', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '_', '_', '_'
};


void ReadableChars_Init(void)
{
	int i;

	for (i = 32; i < 127; i++)
		readablechars[i] = readablechars[128 + i] = i;

	readablechars[127] = readablechars[128 + 127] = '_';

	for (i = 0; i < 32; i++)
		readablechars[128 + i] = readablechars[i];

	readablechars[128] = '_';
	readablechars[10 + 128] = '_';
	readablechars[12 + 128] = '_';
}

