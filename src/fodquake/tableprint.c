/*
Copyright (C) 2010 Mark Olsen

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "console.h"
#include "tableprint.h"

#warning "This file should either use common.c's console mutex or not use Con_Print() directly."

#define min(a, b) ((a) < (b) ? (a) : (b))

struct TablePrint
{
	int error;
	int dosort;
	const char **array;
	unsigned int arraysize;
	unsigned int numitems;
};

struct TablePrint *TablePrint_Begin(int dosort)
{
	struct TablePrint *tp;

	tp = malloc(sizeof(*tp));
	if (tp)
	{
		tp->error = 0;
		tp->dosort = dosort;
		tp->array = 0;
		tp->arraysize = 0;
		tp->numitems = 0;

		return tp;
	}

	return 0;
}

static void TablePrint_Do(struct TablePrint *tp)
{
	unsigned int i;
	unsigned int j;
	unsigned int k;
	unsigned int item;
	unsigned int *columns;
	unsigned int numcolumns;
	unsigned int *stringlengths;
	unsigned int itemspercolumn;
	unsigned int extraitems;
	unsigned int totalwidth;
	unsigned int textcolumns;
	char *tempstr;

	if (tp->numitems == 0)
		return;

	textcolumns = Con_GetColumns();

	columns = malloc(sizeof(*columns)*tp->numitems);
	if (columns == 0)
		tp->error = 1;
	else
	{
		stringlengths = malloc(sizeof(*stringlengths)*tp->numitems);
		if (stringlengths == 0)
			tp->error = 1;
		else
		{
			for(i=0;i<tp->numitems;i++)
			{
				stringlengths[i] = strlen(tp->array[i]);
			}

			for(numcolumns = tp->numitems; numcolumns > 0; numcolumns--)
			{
				itemspercolumn = tp->numitems / numcolumns;
				extraitems = tp->numitems - (itemspercolumn * numcolumns);

				totalwidth = 0;
				item = 0;

				for(i=0;i<numcolumns;i++)
				{
					columns[i] = 0;

					for(j=0;j<itemspercolumn+(i<extraitems?1:0);j++)
					{
						if (stringlengths[item] > columns[i])
							columns[i] = stringlengths[item];

						item++;
					}

					totalwidth += columns[i] + 2;
				}

				if (totalwidth <= textcolumns || numcolumns == 1)
					break;
			}

			tempstr = malloc(totalwidth + 1);
			if (tempstr == 0)
				tp->error = 1;
			else
			{
				item = 0;

				for(i=0;i<itemspercolumn + 1;i++)
				{
					k = 0;

					for(j=0;j<numcolumns && !(i == itemspercolumn && j >= extraitems);j++)
					{
						k += sprintf(tempstr + k, "%-*s  ", columns[j], tp->array[item + j * itemspercolumn + min(j, extraitems)]);
					}

					if (k)
					{
						tempstr[k - 2] = '\n';
						tempstr[k - 1] = 0;
						Con_Print(tempstr);
					}

					item++;
				}

				free(tempstr);
			}

			free(stringlengths);
		}

		free(columns);
	}

}

static int TablePrint_Sorter(const void *a, const void *b)
{
	return strcmp(*(char **)a, *(char **)b);
}

void TablePrint_End(struct TablePrint *tp)
{
	unsigned int i;

	if (tp && !tp->error)
	{
		if (tp->dosort)
			qsort(tp->array, tp->numitems, sizeof(tp->array), TablePrint_Sorter);

		TablePrint_Do(tp);
	}

	if (tp == 0 || tp->error)
		Con_Print("Out of memory.\n");

	for(i=0;i<tp->numitems;i++)
		free((void *)tp->array[i]);

	free(tp->array);
	free(tp);
}

void TablePrint_AddItem(struct TablePrint *tp, const char *txt)
{
	const char **newarray;

	if (tp == 0 || tp->error)
		return;

	if (tp->arraysize == tp->numitems)
	{
		tp->arraysize += 16;
		newarray = realloc(tp->array, sizeof(*tp->array) * tp->arraysize);
		if (newarray == 0)
		{
			tp->error = 1;
			return;
		}

		tp->array = newarray;
	}

	tp->array[tp->numitems] = strdup(txt);
	if (tp->array[tp->numitems] == 0)
	{
		tp->error = 1;
		return;
	}

	tp->numitems++;
}

