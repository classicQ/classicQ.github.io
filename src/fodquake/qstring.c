#include "qstring.h"

static inline char mytolower(char c)
{
	if (c >= 'A' && c <= 'Z')
		return c + ('a' - 'A');
	else
		return c;
}

int Q_strcasecmp(const char *s1, const char *s2)
{
	char c1;
	char c2;

	do
	{
		c1 = mytolower(*s1);
		c2 = mytolower(*s2);
		s1++;
		s2++;
	} while(c1 && c1 == c2);

	return c1 - c2;
}

int Q_strncasecmp(const char *s1, const char *s2, unsigned int n)
{
	char c1;
	char c2;

	c1 = 1;
	c2 = 1;

	while(n-- && c1 && c1 == c2)
	{
		c1 = mytolower(*s1);
		c2 = mytolower(*s2);
		s1++;
		s2++;
	}

	return c1 - c2;
}

