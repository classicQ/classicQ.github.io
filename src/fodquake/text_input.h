struct input
{
	char *string;
	int size;
	int position;
	int flags;
	int length;
};

struct input *Text_Input_Create(char *string, int size, int position, int duplicate);
void Text_Input_Delete(struct input *input);
void Text_Input_Handle_Key(struct input *input, int key);

