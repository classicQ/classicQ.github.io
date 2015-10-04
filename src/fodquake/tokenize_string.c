#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tokenize_string.h"

/*
 * String Tokenizing
 */

struct tokenized_string_temp
{
	struct tokenized_string_temp *next;
	char *token;
};

static void delete_token_temp(struct tokenized_string_temp *list, int delete_tokens)
{
	struct tokenized_string_temp *tlist;

	while (list)
	{
		tlist = list->next;

		if (delete_tokens)
			free(list->token);
		free(list);

		list = tlist;
	}
}

static struct tokenized_string *create_tokenized_string(struct tokenized_string_temp *tst, int count)
{
	struct tokenized_string *ts;
	struct tokenized_string_temp *tstt;
	int i;

	ts = calloc(1, sizeof(struct tokenized_string));

	if (ts == NULL)
	{
		delete_token_temp(tst, 1);
		return NULL;
	}

	ts->tokens = calloc(count, sizeof(char *));

	if (ts->tokens == NULL)
	{
		delete_token_temp(tst, 1);
		free(ts);
		return NULL;
	}

	tstt = tst;
	for (i=0; i<count; i++)
	{
		ts->tokens[i] = tstt->token;
		tstt = tstt->next;
	}

	delete_token_temp(tst, 0);

	ts->count = count;

	return ts;
}

static int add_token_to_temp(struct tokenized_string_temp **list, char *token)
{
	struct tokenized_string_temp *temp, *temp1;;

	temp = calloc(1, sizeof(struct tokenized_string_temp));

	if (temp == NULL)
		return 1;

	temp->token = token;

	if (*list == NULL)
		*list = temp;
	else
	{
		temp1 = *list;
		while (temp1->next)
			temp1 = temp1->next;
		temp1->next = temp;
	}

	return 0;
}

/*
 * These two functions should probably be merged into one to avoid code
 * duplication. Also there's lots of opportunities for doing things more
 * efficiently in this code. Instead of using a list, realloc could be used,
 * or maybe even a two-pass scan. Also some conditional checks in the code are
 * unnecessary, but nevermind :)
 *
 * As far as I can tell, this code will create an empty token if the string
 * ends with a space, and space is not the delimiter, or in the case of space
 * being the delimiter, if the string ends with two spaces.
 *
 * The token pointer is leaked in case add_token_to_temp() fails.
 *
 * - bigfoot
 */

struct tokenized_string *Tokenize_String_Delimiter(char *string, char delimiter)
{
	char *p_start, *p_end;
	struct tokenized_string_temp *tt = NULL;
	char *token;
	int len, i, count;

	p_start = string;

	p_end = p_start;

	len = strlen(p_start);

	count = 0;
	i = 0;

	while (*p_start && i < len)
	{
		while (*p_start == ' ')
		{
			p_start++;
			i++;
		}

		if (*p_start == delimiter)
		{
			p_start++;
			i++;
		}

		p_end = p_start;
		while (*p_end != delimiter && i < len)
		{
			p_end++;
			i++;
		}

		token = calloc(p_end - p_start + 1, sizeof(char));

		if (token == NULL)
		{
			delete_token_temp(tt, 1);
			return NULL;
		}

		memcpy(token, p_start, p_end - p_start);
		token[p_end - p_start] = '\0';

		if (add_token_to_temp(&tt, token))
		{
			delete_token_temp(tt, 1);
			return NULL;
		}

		p_start = p_end;
		p_start++;
		i++;

		count++;
	}

	return create_tokenized_string(tt, count);
}


struct tokenized_string *Tokenize_String(char *string)
{
	char *p_start, *p_end;
	struct tokenized_string_temp *tt = NULL;
	char *token;
	int len, i, count;
	char end_token;

	p_start = string;

	p_end = p_start;

	len = strlen(p_start);

	count = 0;
	i = 0;

	while (*p_start && i < len)
	{
		while (*p_start == ' ')
		{
			p_start++;
			i++;
		}

		if (*p_start == '"')
		{
			end_token = '"';
			p_start++;
			i++;
		}
		else
			end_token = ' ';

		p_end = p_start;
		while (*p_end != end_token && i < len)
		{
			p_end++;
			i++;
		}

		token = calloc(p_end - p_start + 1, sizeof(char));

		if (token == NULL)
		{
			delete_token_temp(tt, 1);
			return NULL;
		}

		memcpy(token, p_start, p_end - p_start);
		token[p_end - p_start] = '\0';

		if (add_token_to_temp(&tt, token))
		{
			delete_token_temp(tt, 1);
			return NULL;
		}

		p_start = p_end;

		if (end_token == '"')
		{
			p_start++;
			p_end++;
			i++;
		}

		count++;
	}

	return create_tokenized_string(tt, count);
}

void Tokenize_String_Delete(struct tokenized_string *ts)
{
	int i;

	for (i=0; i<ts->count; i++)
	{
		free(ts->tokens[i]);
	}

	free(ts->tokens);
	free(ts);
}

