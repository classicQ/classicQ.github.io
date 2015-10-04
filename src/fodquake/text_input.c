#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "qtypes.h"
#include "keys.h"
#include "strl.h"

extern qboolean keydown[256];

#define TI_DUPLICATED (1<<0)

struct input
{
	char *string;
	int size;
	int position;
	int flag;
	int length;
};

struct input *Text_Input_Create(char *string, int size, int position, int duplicate)
{

	struct input *input;

	if (duplicate)
		if (string == NULL)
			return NULL;

	input = calloc(1, sizeof(struct input));

	if (input == NULL)
		return NULL;

	input->size = size;
	input->position = position;

	if (duplicate == 0)
	{
		input->string = string;
	}
	else
	{
		input->string = malloc(size);

		if (input->string == NULL)
		{
			free(input);
			return NULL;
		}

		input->string[0] = '\0';
		strlcpy(input->string, string, input->size);

		input->flag |= TI_DUPLICATED;		
	}

	input->length = strlen(input->string);

	if (input->position > input->length)
		input->position = input->length;

	return input;
}

void Text_Input_Delete(struct input *input)
{
	if (input == NULL)
		return;

	if (input->flag & TI_DUPLICATED)
		free(input->string);

	free(input);
}

void Text_Input_Handle_Key(struct input *input, int key)
{
	int i, diff, position;

	if (key == K_LEFTARROW)
	{
		if (keydown[K_CTRL])
		{
			while (input->position > 0 && input->string[input->position] == ' ')
				input->position--;

			while (input->position > 0 && input->string[input->position] != ' ')
				input->position--;
			return;
		}


		input->position--;
		if (input->position < 0)
			input->position = 0;
		return;
	}

	if (key == K_RIGHTARROW)
	{
		if (keydown[K_CTRL])
		{
			while (input->position < input->length && input->string[input->position] == ' ')
				input->position++;

			while (input->position < input->length && input->string[input->position] != ' ')
				input->position++;
			return;
		}

		input->position++;
		if (input->position > input->length)
				input->position = input->length;
		return;
	}

	if (key == K_DEL)
	{

		if (input->length == 0)
			return;

		if (input->position >= input->length)
			return;

		for (i=input->position; i<input->length; i++)
			input->string[i] = input->string[i+1];

		input->length--;
		return;
	}

	if (key == K_BACKSPACE)
	{
		if (input->position == 0)
			return;

		for (i=input->position; i<=input->length+1; i++)
			input->string[i-1] = input->string[i];

		input->length--;
		input->position--;
	}

	if ((key == 'w'|| key == 'W') && keydown[K_CTRL])
	{
		diff = input->length - input->position;
		position = input->position;

		if (input->string[position] == '\0' && input->position > 1)
			position--;

		while (position > 0 && input->string[position] == ' ')
			position--;

		while (position > 0 && input->string[position] != ' ')
			position--;

		for (i=0; i < diff; i++)
			input->string[position + i] = input->string[input->position + i];

		diff = input->position - position;
		input->position = position;
		input->length -= diff;
		input->string[input->length] = '\0';

		return;

	}

	if ((key > 32 && key < 127) || key == K_SPACE)
	{
		if (input->length >= input->size - 1)
		{
			input->string[input->position] = key;
		}

		if (input->position == input->length)
		{
			if (input->position == input->size - 1)
			{
				return;
			}
		}

		input->string[input->position] = key;
		input->position++;
		input->length++;
		input->string[input->position] = '\0';
	}
}





