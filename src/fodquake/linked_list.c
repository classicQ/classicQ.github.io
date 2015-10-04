/*
Copyright (C) 2009 Jürgen Legler

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

#include "stdlib.h"

struct linked_list_node
{
	struct linked_list_node *next, *prev;
};

struct linked_list
{
	struct linked_list *next, *prev;
	int (*compare)(void *data1, void *data2);
	void (*free_data)(void *data);
	int sorted;
	int entries;
	struct linked_list_node *node;
};

void *List_Delete_Node(struct linked_list *list, struct linked_list_node *node)
{
	if (node->next && node->prev)
	{
		node->next->prev = node->prev;
		node->prev->next = node->next;
	}
	else if (node->next && !node->prev)
	{
		node->next->prev = NULL;
		list->node = node->next;
	}
	else if (!node->next && node->prev)
	{
		node->prev->next = NULL;
	}
	else if (!node->next && !node->prev)
	{
		list->node = NULL;
	}
	return(node);
}

int List_Insert_Node(struct linked_list *list, struct linked_list_node *anchor, struct linked_list_node *node, int before)
{
	if (!list || !anchor || !node)
		return -1;

	if (before == 0)
	{
		if (anchor->next)
		{
			node->next = anchor->next;
			node->next->prev = node;
			anchor->next = node;
		}
		else
		{
			anchor->next = node;
			node->next = NULL;
		}
		node->prev = anchor;
		return 1;
	}
	else if (before == 1)
	{
		if (anchor->prev)
		{
			anchor->prev->next = node;
			node->prev = anchor->prev;
			anchor->prev = node;
			node->next = anchor;
		}
		else
		{
			list->node = node;
			anchor->prev = node;
			node->next = anchor;
		}

		return 1;
	}
	return -1;
}

int List_Add_Node(struct linked_list *list, void *input_node)
{
	struct linked_list_node *node, *node_search;
	int compare;

	if (!list)
		return 0;

	if (!input_node)
		return 0;

	node = (struct linked_list_node *)input_node;

	if (!list->node)
	{
		list->node = node;
		return 1;
	}
	else
	{
		if (list->sorted)
		{
			node_search = list->node;
			while (node_search->next)
			{
				compare = list->compare(input_node, node_search);
				if (compare == 1)
				{
					return List_Insert_Node(list, node_search, node, 0);					
				}
				else if (compare == 2)
				{
					return List_Insert_Node(list, node_search, node, 1);					
				}
				else if (compare == 3)
				{
					return -1;
				}

				node_search = node_search->next;
			}
			node_search->next = node;
			node->prev = node_search;
			return 1;
		}
		else
		{
			node_search = list->node;

			while (node_search->next)
				node_search = node_search->next;

			node->prev = node_search;
			node_search->next = node;
			return 1;
		}
	}
	return -1;
}

struct linked_list *List_Add(int sorted, int (*compare)(void *data1, void *data2), void (*free_data)(void *data))
{
	struct linked_list *list, *list_search;

	list = calloc(1, sizeof(struct linked_list));

	if (!list)
		return NULL;

	list->sorted = sorted;
	list->compare = compare;
	list->free_data = free_data;

	return list;
}


void List_Cleanup(struct linked_list *list)
{
	struct linked_list_node *node, *onode;

	if (!list)
		return;

	node = list->node;

	while (node)
	{
		onode = node;
		node = node->next;
		if (list->free_data)
			list->free_data(List_Delete_Node(list, onode));
		else
			free(onode);
	}
}

void List_Remove(struct linked_list *list)
{
	List_Cleanup(list);
	free(list);
}

int List_Node_Count(struct linked_list *list)
{
	struct linked_list_node *node;
	int i;

	if (!list)
		return -1;
	
	if (!list->node)
		return 0;

	i = 0;

	node = list->node;

	while(node)
	{
		i++;
		node = node->next;
	}
	
	return i;
}

struct linked_list_node *List_Get_Node_Ny_Number(struct linked_list *list, int num)
{
	struct linked_list_node *node;
	int i;

	if (!list || num < 0)
		return NULL;

	if (num == 0)
	{
		return list->node;
	}
	else
	{
		node = list->node;
		i = num;
		while (node && i > 0)
		{
			node = node->next;
			i--;
		}
		return node;
	}
}

void *List_Remove_Node(struct linked_list *list, int num, int delete)
{
	struct linked_list_node *node;

	node = List_Get_Node_Ny_Number(list, num);

	if (!node)
		return NULL;

	node = List_Delete_Node(list, node);
	if (delete)
	{
		free(node);
		return NULL;
	}

	return node;
}

void *List_Get_Node(struct linked_list *list, int num)
{
	struct linked_list_node *node;

	node = List_Get_Node_Ny_Number(list, num);

	if (!node)
		return NULL;

	return node;
}

void *List_Find_Node(struct linked_list *list, int (*compare_function)(void *match, void *against), void *match)
{
	struct linked_list_node *node;

	if (!list || !compare_function || !match)
		return NULL;

	if (!list->node)
		return NULL;

	node = list->node;

	while (node)
	{
		if (compare_function(match, node))
			return node;
		node = node->next;
	}

	return NULL;
}

struct linked_list_node *needs_sorting(struct linked_list *list, int (*compare_function)(void *match, void *against))
{
	struct linked_list_node *node;

	node = list->node;
	while (node)
	{
		if (compare_function(node, NULL))
		{
			return node;
		}

		node = node->next;
	}

	return NULL;
}

void Reinsert_Node(struct linked_list *list, struct linked_list_node *node, int (*compare_function)(void *match, void *against))
{
	struct linked_list_node *anchor;
	int i;

	anchor = list->node;
	while (anchor)
	{
		if ((i = compare_function(node, anchor)))
		{
			if (i == 2)
			{
				List_Insert_Node(list, anchor, node, 0);
				return;
			}
			if (i == 3)
			{
				List_Insert_Node(list, anchor, node, 1);
				return;
			}
		}
		anchor = anchor->next;
	}
}

/* 
 * compare function should return
 * 0 - leave in place
 * 1 - needs sorting
 * 2 - insert after
 * 3 - insert before
 */
void List_Resort(struct linked_list *list, int (*compare_function)(void *match, void *against))
{
	struct linked_list_node *node;

	if (!list || !compare_function)
		return;

	while ((node = needs_sorting(list, compare_function)))
	{
		List_Delete_Node(list,node);
		Reinsert_Node(list, node, compare_function);
	}
}

