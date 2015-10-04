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

struct linked_list
{
	void *nil;
};

struct linked_list_node
{
	struct linked_list_node *next, *prev;
};


void *List_Remove(struct linked_list *list);
void List_Cleanup(struct linked_list *list);
struct linked_list *List_Add(int sorted,int(compare)(void *data1,void *data2),void(*free_data)(void *data));
int List_Add_Node(struct linked_list *list,void *input_node);
int List_Node_Count(struct linked_list *list);
void *List_Remove_Node(struct linked_list *list, int num, int delete);
void *List_Get_Node(struct linked_list *list, int num);
void *List_Find_Node(struct linked_list *list, int (*compare_function)(void *match, void *against), void *match);
void List_Resort(struct linked_list *list, int (*compare_function)(void *match, void *against));

