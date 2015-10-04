struct tokenized_string *Tokenize_String(char *string);
struct tokenized_string *Tokenize_String_Delimiter(char *string, char delimiter);
void Tokenize_String_Delete(struct tokenized_string *ts);

struct tokenized_string
{
	int count;
	char **tokens;
};


