/*
Copyright (C) 2010 Jürgen Legler

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

#include "quakedef.h"
#include "linked_list.h"
#include <stdlib.h>
#include <string.h>
#include "strl.h"
#include "hud_new.h"
#include "hud_functions.h"

static int Draw_Big_Number(int extx, int exty, int number, int digits, int color);
static int Draw_Big_Number_Width(int extx, int exty, int number, int digits, int color);

extern char psfix_dummy;

static void print_general_info (struct hud_item *self)
{
	Com_Printf("Name: %s - %i %i\n", self->name, self->x, self->y);
	if (self->linked_name)
	{
		if (self->linked == NULL)
			Com_Printf(" linked to: %s, unresolved!\n", self->linked_name);
		else
			Com_Printf(" linked to: %s\n", self->linked_name);
	}
	Com_Printf(" selected variable: \"%s\"\n", self->vartype_string->name);
	if (self->vartype_string->type == 0)
	{
		Com_Printf("  will be drawn as an integer with width: %i\n", self->integer_width);
	}
	else if (self->vartype_string->type == 1)
	{
		Com_Printf("  will be drawn as a float with width: %i - precision: %i\n", self->float_width, self->float_precision);
	}

	if (self->prefix == &psfix_dummy )
		Com_Printf("  no prefix has been set\n");
	else
		if (self->prefix)
			Com_Printf("  prefix: \"%s\"\n", self->prefix);

	if (self->suffix == &psfix_dummy )
		Com_Printf("  no suffix has been set\n");
	else
		if (self->prefix)
			Com_Printf("  suffix: \"%s\"\n", self->prefix);

	Com_Printf("  anchor: %i\n", self->anchor);
	Com_Printf("  anchor_orientation: %i\n", self->anchor_orientation);
	Com_Printf("  orientation: %i\n", self->orientation);
}


static void print_color_table (struct hud_item *self)
{
	struct color_table *entry;

	entry = List_Get_Node(self->color_table, 0);

	while (entry)
	{
		Com_Printf("%5.5f: %f %f %f\n", entry->value, entry->color[0], entry->color[1], entry->color[2]);
		entry = (struct color_table *) entry->node.next;
	}
}

static void print_picture_table (struct hud_item *self)
{
	struct picture_table *entry;

	entry = List_Get_Node(self->picture_table, 0);

	while (entry)
	{
		Com_Printf("%5.5f: %s %i %i\n", entry->value, entry->name, entry->width, entry->height);
		entry = (struct picture_table *) entry->node.next;
	}
}

static void print_char_table (struct hud_item *self)
{
	struct char_table *entry;

	entry = List_Get_Node(self->char_table, 0);

	while (entry)
	{
		Com_Printf("%5.5f: %s\n", entry->value, entry->entry);
		entry = (struct char_table *)entry->node.next;
	}
}

static void color_to_colorstring(float r, float g, float b, char buffer[6])
{
	float color1;

	buffer[0] = '&';
	buffer[1] = 'c';
	buffer[5] = '\0';

	r = bound(0, r, 1);
	color1 = r * 15.0f;

	if (color1 < 10)
		buffer[2] = '0' + (int)color1;
	else
		buffer[2] = 'a' + (int)color1 - 10;

	g = bound(0, g, 1);
	color1 = g * 15.0f;
	if (color1 < 10)
		buffer[3] = '0' + (int)color1;
	else
		buffer[3] = 'a' + (int)color1 - 10;
	
	b = bound(0, b, 1);
	color1 = b * 15.0f;
	if (color1 < 10)
		buffer[4] = '0' + (int)color1;
	else
		buffer[4] = 'a' + (int)color1 - 10;

	return ;
}

static void Get_Color_Code_From_Table(struct hud_item *hud, float value, char code[6])
{
	struct color_table *entry, *next;
	int count;
	float value1, value2, value3, color[3];

	static const char white[] = "&cfff";

	entry = List_Get_Node(hud->color_table, 0);

	count = List_Node_Count(hud->color_table);

	memcpy(code, white, 6);

	if (count == 0)
	{
		return;
	}
	if (count == 1)
	{
		color_to_colorstring(entry->color[0], entry->color[1], entry->color[2], code);
		return;
	}

	for ( ; entry->node.next && entry->value < value; entry = (struct color_table *)entry->node.next);

	// if equal
	if (entry->value == value)
	{
		color_to_colorstring(entry->color[0], entry->color[1], entry->color[2], code);
		return;
	}

	// if greater and no next node
	if (!entry->node.next && entry->value < value)
	{
		color_to_colorstring(entry->color[0], entry->color[1], entry->color[2], code);
		return;
	}

	// if smaller and no prev node
	
	if (!entry->node.prev && entry->value > value)
	{
		color_to_colorstring(entry->color[0], entry->color[1], entry->color[2], code);
		return;
	}

	// if inbetween two nodes
	if (entry->node.prev)
	{
		next = entry;
		entry = (struct color_table *)entry->node.prev;
		value1 = next->value - entry->value;
		value2 = value - entry->value;
		value3 = value2/value1;
		color[0] =  entry->color[0] + (next->color[0] - entry->color[0]) *value3;
		color[1] =  entry->color[1] + (next->color[1] - entry->color[1]) *value3;
		color[2] =  entry->color[2] + (next->color[2] - entry->color[2]) *value3;
		color_to_colorstring(color[0], color[1], color[2], code);
		return;
	}
}


/*
 * normal drawing option:
 * this will just draw the info without any
 */
static void draw_normal(struct hud_item *self)
{
	Draw_String(self->real_x, self->real_y, self->value_string);
}

static void setup_normal(struct hud_item *self)
{
	if (self->vartype_string->type == 0)
	{
		snprintf(self->value_string, sizeof(self->value_string), "%s%*i%s", self->prefix, self->integer_width, self->ivalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
	else if (self->vartype_string->type == 1)
	{
		snprintf(self->value_string, sizeof(self->value_string), "%s%*.*f%s", self->prefix, self->float_width, self->float_precision, self->dvalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
	else if (self->vartype_string->type == 2)
	{
		snprintf(self->value_string, sizeof(self->value_string), "%s%*s%s", self->prefix, self->char_width, self->cvalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
}

static void info_normal(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: normal, draws as plain uncolored text\n");
}

/*
 * colored
 */
static void draw_colored(struct hud_item *self)
{
	Draw_ColoredString(self->real_x, self->real_y, self->value_string, 1);
}

static void setup_colored(struct hud_item *self)
{
	char color[6];

	if (self->vartype_string->type == 0)
	{
		Get_Color_Code_From_Table(self, self->ivalue_color_table , color);
		snprintf(self->value_string, sizeof(self->value_string), "%s%s%*i%s", color, self->prefix, self->integer_width, self->ivalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
	else if (self->vartype_string->type == 1)
	{
		Get_Color_Code_From_Table(self, self->dvalue_color_table, color);
		snprintf(self->value_string, sizeof(self->value_string), "%s%s%*.*f%s",color, self->prefix, self->float_width, self->float_precision, self->dvalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
	else if (self->vartype_string->type == 2)
	{
		Get_Color_Code_From_Table(self, self->dvalue_color_table, color);
		snprintf(self->value_string, sizeof(self->value_string), "%s%s%*s%s",color, self->prefix, self->char_width, self->cvalue, self->suffix);
		self->width = Colored_String_Length(self->value_string) *8;
		self->height = 8;
	}
}

static void info_colored(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: colored,  draws as colored text\n" );
	if (List_Node_Count(self->color_table))
	{
		Com_Printf(" color_table_entries: %i\n", List_Node_Count(self->color_table));
		print_color_table(self);
	}
}

static void draw_picture(struct hud_item *self)
{
	if (!self->picture)
		return;
	
	Draw_Pic(self->real_x + self->picture_static_x, self->real_y + self->picture_static_y, self->picture);
}

static void setup_picture(struct hud_item *self)
{
	struct picture_table *entry;
	float value = 0;

	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;

	self->picture = NULL;
	self->width = 2;
	self->height = 2;
	self->picture_static_x = 0;
	self->picture_static_y = 0;

	if (List_Node_Count(self->picture_table) == 0)
	{
		return;
	}

	entry = (struct picture_table *)List_Get_Node(self->picture_table, 0);

	if (!entry)
		return;

	for ( ;entry->node.next && entry->value < value; entry = (struct picture_table *)entry->node.next);

	if (!entry)
		return;

	self->width = entry->width;
	self->height = entry->height;
	if (entry->is_dummy)
		self->picture = NULL;
	else
		self->picture = entry->picture;



	if (self->picture_static_size != 0)
	{
		self->width = self->picture_static_width;
		self->height = self->picture_static_height;

		if (self->picture_static_valign == 1)
			self->picture_static_x = (self->width - entry->width) / 2;
		else if (self->picture_static_valign == 2)
			self->picture_static_x = (self->width - entry->width);

		if (self->picture_static_halign == 1)
			self->picture_static_y = (self->height - entry->height) /2;
		else if (self->picture_static_halign == 2)
			self->picture_static_y = (self->height - entry->height);
	}
}


static void info_picture(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: picture,  draws as picture\n" );
	if (List_Node_Count(self->picture_table))
	{
		Com_Printf(" picture_table_entries: %i\n", List_Node_Count(self->picture_table));
		print_picture_table(self);
	}
}


static void draw_big(struct hud_item *self)
{
	int width;
	if (self->integer_width < 1)
		width = 4;
	else
		width = self->integer_width;

	if (self->vartype_string->type == 0)
		Draw_Big_Number(self->real_x, self->real_y, self->ivalue, width, 0);
}

static void setup_big(struct hud_item *self)
{
	int width;
	if (self->integer_width < 1)
		width = 4;
	else
		width = self->integer_width;

	self->width = 24;
	self->height= 24;

	if (self->vartype_string->type == 0)
		self->width = Draw_Big_Number_Width(self->real_x, self->real_y, self->ivalue, width, 0);

}

static void info_big(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: big,  draws int as big numbers\n" );
}


/* normal text */
static void draw_normal_text(struct hud_item *self)
{
	Draw_String(self->real_x, self->real_y, self->value_string);
}

static void setup_normal_text(struct hud_item *self)
{
	struct char_table *entry;
	float value = 0;

	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;

	self->height = 8;

	if (List_Node_Count(self->char_table) == 0)
	{
		snprintf(self->value_string, sizeof(self->value_string), "no entry in the char table!\n");
		self->width = Colored_String_Length(self->value_string) * 8;
		return;
	}

	entry = (struct char_table *)List_Get_Node(self->char_table, 0);

	if (!entry)
	{
		snprintf(self->value_string, sizeof(self->value_string), "no entry in the char table!\n");
		self->width = Colored_String_Length(self->value_string) * 8;
		return;
	}

	while(entry)
	{
		if (entry->value == value)
		{
			snprintf(self->value_string, sizeof(self->value_string), "%s%*s%s", self->prefix, self->char_width, entry->entry, self->suffix);
			self->width = Colored_String_Length(self->value_string) * 8;
		}
		entry = (struct char_table *)entry->node.next;
	}
}

static void info_normal_text(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: normal, looks up the variables value in the char table\n");
	if (List_Node_Count(self->char_table))
	{
		Com_Printf(" Char table entries: %i\n", List_Node_Count(self->char_table));
		print_char_table(self);
	}
}

static void draw_graph(struct hud_item *self)
{
	struct graph_table *entry, *entry_1;
	int i,j;
	float x1,y1,x2,y2,h,w,sv;
	x2 = y2 = 0;

	if (!self->graph_table)
		return;
	i = List_Node_Count(self->graph_table);

	h = self->graph_highest - self->graph_lowest;
	if (h < 0)
		h *= -1;

	w = self->width / i;

	entry_1 = entry = List_Get_Node(self->graph_table, 0);
	for (j = 0; j < i-1; j++)
	{
		if (j == 0)
		{
			entry_1 = (struct graph_table *)entry->node.next;
		}
		else
		{
			entry = entry_1;
			entry_1 = (struct graph_table *)entry->node.next;
		}

		if (j == 0)
		{
			x1 = w * j;
			if (entry->value < 0)
				sv = entry->value * -1;
			else 
				sv = entry->value;
			if (h == 0)
				y1 = 0;
			else
				y1 = self->height * (entry->value/h);
		}
		else
		{
			x1 = x2;
			y1 = y2;
		}

		x2 = w * j;
		if (entry_1->value < 0)
			sv = entry_1->value * -1;
		else 
			sv = entry_1->value;
		if (h == 0)
			y2 = 0;
		else
			y2 = self->height * (entry_1->value/h);

#ifdef GLQUAKE
		Draw_Line_RGBA(self->real_x + x1, self->real_y + self->height - y1, self->real_x  + x2, self->real_y + self->height - y2, 1, self->graph_color[0], self->graph_color[1], self->graph_color[2], 1);
#else
		Draw_Line(self->real_x + x1, self->real_y + self->height - y1, self->real_x  + x2, self->real_y + self->height - y2, 1, self->graph_color_software & 0xFF);
#endif
	}
}

static void setup_graph(struct hud_item *self)
{
	struct graph_table *entry, *entry_1;
	int i;

	if (self->graph_update_time + self->graph_update_interval > cls.realtime)
		return;

	self->graph_update_time = cls.realtime;

	entry = calloc(1, sizeof(struct graph_table));
	if (!entry)
		return;

	if (self->vartype_string->type == 0)
		entry->value = self->ivalue;
	else if (self->vartype_string->type == 1)
		entry->value = self->dvalue;
	else
	{
		free(entry);
		return;
	}

	if (self->graph_highest < entry->value)
		self->graph_highest = entry->value;

	if (self->graph_lowest > entry->value)
		self->graph_lowest = entry->value;

	List_Add_Node(self->graph_table, entry);


	while ((i = List_Node_Count(self->graph_table) > self->graph_retain))
	{
		entry = List_Get_Node(self->graph_table, 0);
		entry_1 = List_Get_Node(self->graph_table, 1);
		if (!entry || !entry_1)
			return;
		entry->value = (entry->value + entry_1->value)/2;
		List_Remove_Node(self->graph_table, 1, 1);
	}

	self->width = self->graph_width;
	self->height = self->graph_height;
}

static void info_graph(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: graph, displays variable info as graph\n");
	Com_Printf("  %10s : %11i - %s\n", "width", self->graph_width, "width in pixels.");
	Com_Printf("  %10s : %11i - %s\n", "height", self->graph_height, "height in pixels.");
	Com_Printf("  %10s : %11i - %s\n", "retain", self->graph_retain, "how many values will be saved.");
	Com_Printf("  %10s : %5.5f - %s\n", "retain", self->graph_update_interval, "time interval between the info is stored in seconds.");
}

static void draw_normal_text_combination(struct hud_item *self)
{
	Draw_String(self->real_x, self->real_y, self->value_string);
}


static void setup_normal_text_combination(struct hud_item *self)
{
	struct char_table *entry;
	float value = 0;
	char temp_string[512];
	int first = 1;

	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;

	self->height = 8;

	if (List_Node_Count(self->char_table) == 0)
	{
		snprintf(self->value_string, sizeof(self->value_string), "no entry in the char table!\n");
		self->width = Colored_String_Length(self->value_string) * 8;
		return;
	}


	entry = (struct char_table *)List_Get_Node(self->char_table, 0);

	if (!entry)
	{
		snprintf(self->value_string, sizeof(self->value_string), "no entry in the char table!\n");
		self->width = Colored_String_Length(self->value_string) * 8;
		return;
	}

	temp_string[0] = '\0';
	while(entry)
	{
		if ((int)value & (1 << (int)entry->value))
		{
			if (first == 1)
			{
				first = 0;
				snprintf(temp_string, sizeof(temp_string), "%s", entry->entry);
			}
			else
			{
				strlcat(temp_string, " ", sizeof(temp_string));
				strlcat(temp_string, entry->entry, sizeof(temp_string));
			}
		}
		entry = (struct char_table *)entry->node.next;
	}

	snprintf(self->value_string, sizeof(self->value_string), "%s%*s%s", self->prefix, self->char_width, temp_string, self->suffix);
	self->width = Colored_String_Length(self->value_string) * 8;
}



static void info_normal_text_combination(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: combined text, bit compares the entries in the char table to the variable value\n");
	if (List_Node_Count(self->char_table))
	{
		Com_Printf(" Char table entries: %i\n", List_Node_Count(self->char_table));
		print_char_table(self);
	}
}


/* picture combination */
static void draw_picture_combination(struct hud_item *self)
{
	struct picture_table *entry;
	int x, y;
	float value;



	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;
	else
		value = 0;
	



	entry = (struct picture_table *)List_Get_Node(self->picture_table, 0);

	if (!entry)
		return;

	x = y = 0;
	while(entry)
	{
		if (self->combined_static_size == 0)
		{
			if ((int)value & (1 << (int)entry->value))
			{
				if (self->combined_alignment != 0)
				{
					x = self->width - entry->width;
				}

				Draw_Pic(self->real_x + x, self->real_y + y, entry->picture);
				if (self->combined_orientation == 0)
				{
					x += entry->width;
				}
				else
				{
					y += entry->height;
				}

			}
		}
		else
		{
			if (self->combined_alignment!= 0)
			{
				x = self->width - entry->width;
			}

			if ((int)value & (1 << (int)entry->value))
				Draw_Pic(self->real_x + x, self->real_y + y, entry->picture);
			if (self->combined_orientation == 0)
			{
				x += entry->width;
			}
			else
			{
				y += entry->height;
			}
		}
		entry = (struct picture_table *)entry->node.next;
	}
}

static void setup_picture_combination(struct hud_item *self)
{
	
	struct picture_table *entry;
	float value = 0;


	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;


	self->picture = NULL;
	self->width = 2;
	self->height = 2;

	if (List_Node_Count(self->picture_table) == 0)
	{
		return;
	}


	entry = (struct picture_table *)List_Get_Node(self->picture_table, 0);

	if (!entry)
		return;

	self->width = 0;
	self->height = 0;
	while(entry)
	{
		if (self->combined_static_size == 0)
		{
			if ((int)value & (1 << (int)entry->value))
			{
				if (self->combined_orientation == 0)
				{
					self->width += entry->width;
					self->height = entry->height;
				}
				else
				{
					if (entry->width > self->width)
						self->width = entry->width;
					self->height += entry->height;
				}
			}
		}
		else
		{
			if (self->combined_orientation == 0)
			{
				self->width += entry->width;
				self->height = entry->height;
			}
			else
			{
				if (entry->width > self->width)
					self->width = entry->width;
				self->height += entry->height;
			}
		}
		entry = (struct picture_table *)entry->node.next;
	}
}

static void info_picture_combination(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: combined pictures, bit compares the entries in the picture table to the variable value\n");
	Com_Printf("  %14s : %11i - %s\n", "alignment", self->combined_alignment, "0 = left aligned, 1 = right aligned.");
	Com_Printf("  %14s : %11i - %s\n", "orientation", self->combined_orientation, "0 = left to right, 1 = top to bottom.");
	Com_Printf("  %14s : %11i - %s\n", "static_size", self->combined_static_size, "0 = size will change dynamicaly, 1 = will always be maximum sized.");

	if (List_Node_Count(self->picture_table))
	{
		Com_Printf(" picture_table_entries: %i\n", List_Node_Count(self->picture_table));
		print_picture_table(self);
	}
}

/* progess bar */

static void setup_progress_bar(struct hud_item *self)
{

	float value = 0;

	if (self->vartype_string->type == 0)
		value = self->ivalue;
	else if (self->vartype_string->type == 1)
		value = self->dvalue;
	else
		return;

	if (self->progress_bar_dynamic == 1)
	{
		if (self->progress_bar_max < value)
			self->progress_bar_max = value;
		if (self->progress_bar_min > value)
			self->progress_bar_min = value;
	}

	if (self->progress_bar_orientation == 0)
	{
		self->width = self->progress_bar_width;
		self->height = self->progress_bar_height;
	}
	else
	{
		self->width = self->progress_bar_height;
		self->height = self->progress_bar_width;
	}

	self->progress_bar_ratio = value /(self->progress_bar_max - self->progress_bar_min);
}

static void draw_progress_bar(struct hud_item *self)
{
	int x1, x2, y1, y2, t;

	if (self->progress_bar_orientation == 0)
	{
		x1 = self->real_x;
		y1 = self->real_y + self->height/2;

		x2 = x1+  self->width * self->progress_bar_ratio;
		y2 = self->real_y + self->height/2;

		t = self->height/2;
	}
	else if (self->progress_bar_orientation == 1)
	{
		x1 = self->real_x + self->height/2;
		y1 = self->real_y + self->width;

		x2 = x1 ;
		y2 = y1 - self->width * self->progress_bar_ratio;

		t = self->height/2;

	}
	else
		return;


		
#ifdef GLQUAKE
	Draw_Line_RGBA(x1, y1, x2, y2, t, 1, 1, 1, 1);
#endif

}

static void info_progress_bar(struct hud_item *self)
{
	print_general_info(self);
	Com_Printf(" type: progress_bar, draw a progress bar\n");
	Com_Printf("  %14s : %11i - %s\n", "width", self->progress_bar_width, "width in pixels.");
	Com_Printf("  %14s : %11i - %s\n", "height", self->progress_bar_height, "height in pixels.");
	Com_Printf("  %14s : %11i - %s\n", "orientation", self->progress_bar_orientation, "0 = horizontal, 1 = vertical.");
	Com_Printf("  %14s : %11i - %s\n", "dynamic", self->progress_bar_dynamic, "0 = static limits, 1 = values will be set by the variable values.");
	Com_Printf("  %14s : %11i - %s\n", "min", self->progress_bar_min, "minimum value if dynamic = 0.");
	Com_Printf("  %14s : %11i - %s\n", "max", self->progress_bar_max, "maximum value if dynamic = 0.");
}

static void draw_team_hud(struct hud_item *self)
{
	HUD_Draw_Team(self);	
}


static void setup_team_hud(struct hud_item *self)
{
	HUD_Setup_Team(self);	
}
static void info_team_hud(struct hud_item *self)
{
}


static const struct hud_function hud_functions[] ={
			{ &draw_normal, &setup_normal, &info_normal, "normal", "will draw the info in normal uncolored text"},
			{ &draw_colored, &setup_colored, &info_colored, "colored", "will draw the info colored by a provided color table"},
			{ &draw_picture, &setup_picture, &info_picture, "picture", "will draw the info as pictures by a provided picture table"},
			{ &draw_picture_combination, &setup_picture_combination, &info_picture_combination, "picture_combination", "will draw the info as a combination of pictures provided by a picture table"},
			{ &draw_big, &setup_big, &info_big, "big", "this will only draw int type info as big numbers"},
			{ &draw_normal_text, &setup_normal_text, &info_normal_text, "normal_text", "this will draw the info as strings from the char table"},
			{ &draw_normal_text_combination, &setup_normal_text_combination, &info_normal_text_combination, "normal_text_combination", "this will draw a combination of strings from the char table the value of the variable is & compared against the char table"},
			{ &draw_graph, &setup_graph, &info_graph, "graph", "this will draw the info as a time/value graph"},
			{ &draw_progress_bar, &setup_progress_bar, &info_progress_bar, "progress_bar", "this will draw a progress bar"},
			{ &draw_team_hud, &setup_team_hud, &info_team_hud, "team", "will draw teams for scoreboard like display"}
			};



struct hud_function *HUD_Find_Functions(char *name)
{
	int i;
	for (i=0; i<(sizeof(hud_functions)/sizeof(*hud_functions)); i++)
		if (strcmp(hud_functions[i].name, name) == 0)
			return &hud_functions[i];
	return NULL;
}


void HUD_List_Types_f(void)
{
	int i;
	for (i=0; i<(sizeof(hud_functions)/sizeof(*hud_functions)); i++)
		Com_Printf("%10s : %s\n", hud_functions[i].name, hud_functions[i].info_string);
	
}






static int Sbar_itoa (int num, char *buf) {
	char *str;
	int pow10, dig;
	
	str = buf;
	
	if (num < 0) {
		*str++ = '-';
		num = -num;
	}
	
	for (pow10 = 10 ; num >= pow10 ; pow10 *= 10)
		;
	
	do {
		pow10 /= 10;
		dig = num / pow10;
		*str++ = '0' + dig;
		num -= dig * pow10;
	} while (pow10 != 1);
	
	*str = 0;
	
	return str - buf;
}

#define STAT_MINUS 10
static int Draw_Big_Number(int extx, int exty, int number, int digits, int color)
{
	char str[12], *ptr;
	int l, frame;
	int x,y;
	extern mpic_t *sb_nums[2][11];

	x = extx;
	y = exty;

	l = Sbar_itoa (number, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';

		Draw_Pic(x, y, sb_nums[color][frame]);
		x += 24;
		ptr++;
	}

	return x - extx;
}


static int Draw_Big_Number_Width(int extx, int exty, int number, int digits, int color)
{
	char str[12], *ptr;
	int l, frame;
	int x,y;
	extern mpic_t *sb_nums[2][11];

	x = extx;
	y = exty;

	l = Sbar_itoa (number, str);
	ptr = str;
	if (l > digits)
		ptr += (l - digits);
	if (l < digits)
		x += (digits - l) * 24;

	while (*ptr) {
		frame = (*ptr == '-') ? STAT_MINUS : *ptr -'0';
		x += 24;
		ptr++;
	}

	return x - extx;
}

