#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "context_sensitive_tab.h"
#include "tokenize_string.h"
#include "text_input.h"

#define SLIDER_LOWER_LIMIT				0
#define SLIDER_UPPER_LIMIT				1
#define SLIDER_ORIGINAL_VALUE			2
#define SLIDER_VALUE					3

#define COLOR_COLOR1	0
#define COLOR_COLOR2	1
#define COLOR_ROW		2

#define CC_INITALIZED	0

enum CSTC_Pictures
{
	cstcp_magnifying,
	cstcp_arrow_up,
	cstcp_arrow_down,
	cstcp_arrow_left,
	cstcp_arrow_right,
	cstcp_textbox_left,
	cstcp_textbox_center,
	cstcp_textbox_right,
	cstcp_border_top_left,
	cstcp_border_top_right,
	cstcp_border_bottom_left,
	cstcp_border_bottom_right,
	cstcp_border_bottom,
	cstcp_border_left,
	cstcp_border_top,
	cstcp_border_right,
	cstcp_bubble,
	cstcp_slider_knob,
	cstcp_slider_vertical_top,
	cstcp_slider_vertical_center,
	cstcp_slider_vertical_bottom,
	cstcp_cstc_icon
};

struct cst_commands
{
	struct cst_commands *next;
	char *name;
	struct tokenized_string *commands;
	int (*conditions)(void);
	int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result);
	int (*get_data)(struct cst_info *self, int remove);
	void (*draw)(struct cst_info *self);
	int flags;
	char *tooltip;
};

struct cst_commands Command_Completion;
struct Picture *cstc_pictures;

#define MAXCMDLINE 256
extern int key_linepos;
extern int edit_line;
extern char key_lines[32][MAXCMDLINE];

int context_sensitive_tab_completion_active = 0;

cvar_t	context_sensitive_tab_completion = {"context_sensitive_tab_completion", "1"};
cvar_t	context_sensitive_tab_completion_show_notification = {"context_sensitive_tab_completion_show_notification", "1"};
cvar_t	context_sensitive_tab_completion_use_pictures = {"context_sensitive_tab_completion_use_pictures", "1"};
cvar_t	context_sensitive_tab_completion_command_only_on_ctrl_tab = {"context_sensitive_tab_completion_command_only_on_ctrl_tab", "1"};
cvar_t	context_sensitive_tab_completion_color_coded_types = {"context_sensitive_tab_completion_color_coded_types", "1"};
cvar_t	context_sensitive_tab_completion_close_on_tab = {"context_sensitive_tab_completion_close_on_tab", "1"};
cvar_t	context_sensitive_tab_completion_sorting_method = {"context_sensitive_tab_completion_sorting_method", "2"};
cvar_t	context_sensitive_tab_completion_show_results = {"context_sensitive_tab_completion_show_results", "1"};
cvar_t	context_sensitive_tab_completion_ignore_alt_tab = {"context_sensitive_tab_completion_ignore_alt_tab", "1"};
cvar_t	context_sensitive_tab_completion_background_color = {"context_sensitive_tab_completion_background_color", "4"};
cvar_t	context_sensitive_tab_completion_tooltip_color = {"context_sensitive_tab_completion_tooltip_color", "14"};
cvar_t	context_sensitive_tab_completion_inputbox_color = {"context_sensitive_tab_completion_inputbox_color", "4"};
cvar_t	context_sensitive_tab_completion_selected_color = {"context_sensitive_tab_completion_selected_color", "40"};
cvar_t	context_sensitive_tab_completion_highlight_color = {"context_sensitive_tab_completion_highlight_color", "186"};
cvar_t	context_sensitive_tab_completion_insert_slash = {"context_sensitive_tab_completion_insert_slash", "1"};
cvar_t	context_sensitive_tab_completion_slider_no_offset = {"context_sensitive_tab_completion_slider_no_offset", "1"};
cvar_t	context_sensitive_tab_completion_slider_border_color = {"context_sensitive_tab_completion_slider_border_color", "0"};
cvar_t	context_sensitive_tab_completion_slider_background_color = {"context_sensitive_tab_completion_slider_background_color", "4"};
cvar_t	context_sensitive_tab_completion_slider_color = {"context_sensitive_tab_completion_slider_color", "10"};
cvar_t	context_sensitive_tab_completion_slider_variables = {"context_sensitive_tab_completion_slider_variables", "gl_gamma gl_contrast volume"};
cvar_t	context_sensitive_tab_completion_execute_on_enter = {"context_sensitive_tab_completion_execute_on_enter", "quit sb_activate"};

char *context_sensitive_tab_completion_color_variables = "context_sensitive_tab_completion_inputbox_color context_sensitive_tab_completion_selected_color context_sensitive_tab_completion_background_color sb_color_bg sb_color_bg_empty sb_color_bg_free sb_color_bg_specable sb_color_bg_full sb_highlight_sort_column_color topcolor bottomcolor r_skycolor context_sensitive_tab_completion_slider_border_color context_sensitive_tab_completion_slider_background_color context_sensitive_tab_completion_slider_color context_sensitive_tab_completion_tooltip_color context_sensitive_tab_completion_highlight_color";
char *context_sensitive_tab_completion_player_color_variables = "teamcolor enemycolor color";

char *cstc_slider_tooltip = "arrow up/down will add/remove 0.1, arrow left/right will add/remove 0.01";
char *cstc_player_color_tooltip = "arrow keys to navigate, space to select the color, enter to finalize";
char *cstc_color_tooltip = "arror keys to navigate, enter to select";

static void cleanup_cst(struct cst_info *info)
{
	if (info == NULL)
		return;

	if (info->tokenized_input)
		Tokenize_String_Delete(info->tokenized_input);

	if (info->get_data)
		info->get_data(info, 1);

	if (info->new_input)
		Text_Input_Delete(info->new_input);
}

static struct cst_info cst_info_static;
static struct cst_info *cst_info = &cst_info_static;

static struct cst_commands *commands;
static struct cst_commands CC_Slider;
static struct cst_commands CC_Player_Color;
static struct cst_commands CC_Color;

static void CSTC_Cleanup(struct cst_info *self)
{
	context_sensitive_tab_completion_active = 0;
	cleanup_cst(self);
	memset(self, 0, sizeof(struct cst_info));
}

static void CSTC_DrawPicture(int x, int y, int width, int height, enum CSTC_Pictures pic)
{
	int index_x, index_y;
	float sx, sy;

	if (cstc_pictures == NULL)
		return;

	for (index_x = pic, index_y = 0; index_x > 15; index_x -= 16, index_y++);

	sx = (1.0f/16.0f) * index_x;
	sy = (1.0f/16.0f) * index_y;
	Draw_DrawSubPicture(cstc_pictures, (1.0f/16.0f) * index_x, (1.0f/16.0f) * index_y, 1.0f/16.0f, 1.0f/16.0f, x, y, width ? width : 16, height ? height : 16);
}

static qboolean CSTC_PictureCheck(void)
{
	if (cstc_pictures && context_sensitive_tab_completion_use_pictures.value == 1)
		return true;

	return false;
}

/*
 * x and y are the positions of the internal lining of the border
 * width and height are the dimensions of the internal lining of the border
 * if fill_color is >= 0 the internal space of the border will be filled with that color
 */
static void CSTC_DrawBorder(int x, int y, int width, int height, int border_width, int fill_color)
{
	int pos_x, pos_y;
	int bwi;

	if (cstc_pictures == NULL)
		return;

	if (border_width < 8)
		border_width = 8;

	bwi = (border_width == 16 ? 0 : border_width);

	if (fill_color >= 0 && 0)
		Draw_Fill(x, y, width, height, fill_color);

	// top left
	pos_x = x - border_width;
	pos_y = y - border_width;
	CSTC_DrawPicture(pos_x, pos_y, bwi, bwi, cstcp_border_top_left);

	// top
	pos_x += border_width;
	CSTC_DrawPicture(pos_x, pos_y, width, bwi, cstcp_border_top);

	// top right
	pos_x += width;
	CSTC_DrawPicture(pos_x, pos_y, bwi, bwi, cstcp_border_top_right);

	// right
	pos_y += border_width;
	CSTC_DrawPicture(pos_x, pos_y, bwi, height, cstcp_border_right);

	// bottom right
	pos_y += height;
	CSTC_DrawPicture(pos_x, pos_y, border_width, border_width, cstcp_border_bottom_right);

	// bottom
	pos_x -= width;
	CSTC_DrawPicture(pos_x, pos_y, width, border_width, cstcp_border_bottom);

	// bottom left
	pos_x -= border_width;
	CSTC_DrawPicture(pos_x, pos_y, border_width, border_width, cstcp_border_bottom_left);

	// left
	pos_y -= height;
	CSTC_DrawPicture(pos_x, pos_y, bwi, height, cstcp_border_left);
}

/*
 * segment_size is the width/height of one segment
 * segments is the amount of segments drawn between the 2 end segments
 */
static int CSTC_DrawSlider (int x, int y, int segment_size, int segments, double pos, qboolean vertical, char *text)
{
	int isz;
	int pos_x, pos_y;
	int i;
	int text_x, text_y;
	int size_x, size_y;
	enum CSTC_Pictures top, center, bottom;

	if (CSTC_PictureCheck())
	{
		if (segment_size < 8)
			segment_size = 8;

		isz = (segment_size == 16 ? 0 : segment_size);

		pos_x = x;
		pos_y = y;

		if (vertical)
		{
			top = cstcp_slider_vertical_top;
			center = cstcp_slider_vertical_center;
			bottom = cstcp_slider_vertical_bottom;
		}
		else
		{
			top = cstcp_textbox_left;
			center = cstcp_textbox_center;
			bottom = cstcp_textbox_right;
		}

		CSTC_DrawPicture(pos_x, pos_y, isz, isz, bottom);
		if (vertical)
			pos_y -= segment_size;
		else
			pos_x += segment_size;

		text_x = (vertical ? 0 : 1);
		text_y = (vertical ? 1 : 0);
		for (i=0; i<segments; i++, pos_y-= segment_size * text_y,  pos_x+=  segment_size * text_x)
			CSTC_DrawPicture(pos_x, pos_y, isz, isz, center);

		CSTC_DrawPicture(pos_x, pos_y, isz, isz, top);

		text_x = pos_x + segment_size * (vertical ? segments: 1);
		if (vertical)
			pos_y = y + (pos_y - y) * pos;
		else
			pos_x = x + (pos_x - x) * pos;
		CSTC_DrawPicture(pos_x, pos_y, isz, isz, cstcp_slider_knob);
		text_y = y * (vertical ? segments: 1);
	}
	else
	{
		pos_x = x;
		pos_y = y;
		if (vertical)
		{
			size_x = segment_size;
			size_y = segments * segment_size;
			pos_y -= size_y;
		}
		else
		{
			size_x = segments * segment_size;
			size_y = segment_size;
		}

		Draw_Fill(pos_x-1, pos_y-1, size_x + 2, size_y + 2, context_sensitive_tab_completion_slider_border_color.value);
		Draw_Fill(pos_x, pos_y, size_x, size_y, context_sensitive_tab_completion_slider_background_color.value);
		Draw_Fill(pos_x, pos_y, size_x * (vertical ? 1 : pos), size_y * (vertical ? pos : 1), context_sensitive_tab_completion_slider_color.value);
		text_x = pos_x + size_x;
		text_y = pos_y + size_y;
	}

	if (text)
		Draw_String(text_x, text_y, text);

	return pos_x;
}

static void CSTC_DrawTextbox(int x, int y, int segment_size, int segment_count, qboolean auto_size, char *text)
{
	int isz;
	int pos_x, pos_y;
	int i;

	if (auto_size && text == NULL)
		return;

	if (auto_size)
		segment_count = strlen(text);

	if (CSTC_PictureCheck())
	{

		if (segment_size < 8)
			segment_size = 8;

		isz = (segment_size == 16 ? 0 : segment_size);

		pos_x = x;
		pos_y = y;

		CSTC_DrawPicture(pos_x, pos_y, isz, isz, cstcp_textbox_left);
		pos_x += isz;

		for (i=0; i<segment_count - auto_size ? 1 : 0; i++, pos_x+=segment_size)
			CSTC_DrawPicture(pos_x, pos_y, isz, isz, cstcp_textbox_center);

		CSTC_DrawPicture(pos_x, pos_y, isz, isz, cstcp_textbox_right);

		if (text)
			Draw_String(x + segment_size/2, y - 1, text);
	}
	else
	{
		Draw_Fill(x, y, segment_size * (segment_count + 2 - (auto_size ? 1 : 0)), 8, 0);
		if (text)
			Draw_String(x + segment_size/2, y, text);
	}
}

static void CSTC_DrawBubble(int x, int y, int size, enum CSTC_Pictures pic, char *bubble_char, char *text)
{
	int isz;

	if (CSTC_PictureCheck())
	{
			if (size < 8)
			size = 8;

		isz = (size == 16 ? 0 : size);

		CSTC_DrawPicture(x, y, isz, isz, pic);

		if (bubble_char)
			Draw_String(x, y, bubble_char);
	}
	else
	{
		// there needs to be some better way to do this
		Draw_Fill(x, y, size, size, 5);

		if (bubble_char)
			Draw_String(x, y, bubble_char);
	}

	if (text)
		CSTC_DrawTextbox(x+size, y, size, 0, true, text);
}

static qboolean CSTC_ExecuteOnEnter(char *cmd)
{
	struct tokenized_string *ts;
	qboolean rval = false;
	int i;

	ts = Tokenize_String(context_sensitive_tab_completion_execute_on_enter.string);
	if (ts)
	{
		for (i=0; i<ts->count; i++)
		{
			if (strcmp(cmd, ts->tokens[i]) == 0)
			{
				rval = true;
				break;
			}
		}
		Tokenize_String_Delete(ts);
	}
	return rval;
}

void CSTC_Add(char *name, int (*conditions)(void), int (*result)(struct cst_info *self, int *results, int get_result, int result_type, char **result), int (*get_data)(struct cst_info *self, int remove), void (*draw)(struct cst_info *self), int flags, char *tooltip)
{
	struct cst_commands *command, *cc;
	char *in;

	if (name == NULL || result == NULL)
		return;

	in = strdup(name);
	if (in == NULL)
		return;

	command = (struct cst_commands *) calloc(1, sizeof(struct cst_commands));

	if (command == NULL)
	{
		free(in);
		return;
	}

	if (commands == NULL)
		commands = command;
	else
	{
		cc = commands;
		while (cc->next)
			cc = cc->next;
		cc->next = command;
	}

	command->name = in;
	command->conditions = conditions;
	command->result = result;
	command->get_data = get_data;
	command->draw = draw;
	command->flags = flags;
	if (flags & CSTC_MULTI_COMMAND)
		command->commands = Tokenize_String(command->name);
	command->tooltip = tooltip;
}

static void Tokenize_Input(struct cst_info *self)
{
	if (self == NULL)
		return;

	if (self->tokenized_input)
	{
		Tokenize_String_Delete(self->tokenized_input);
		self->tokenized_input= NULL;
	}

	self->tokenized_input = Tokenize_String(self->input);
}

static void insert_result(struct cst_info *self, char *ptr)
{
	char *result;
	char new_keyline[MAXCMDLINE];
	int i;

	if (ptr)
		result = ptr;
	else
		if (cst_info->result(cst_info, NULL, cst_info->selection, 0, &result))
			return;

	snprintf(new_keyline, sizeof(new_keyline),
			"%*.*s%s%s%s%s ",
			self->command_start, self->command_start, key_lines[edit_line],
			(context_sensitive_tab_completion_insert_slash.value == 1 && self->argument_start == 1 && key_lines[edit_line][1] != '/') ? "/" : "",
			self->flags & CSTC_COMMAND ? "" : self->real_name,
			self->flags & CSTC_COMMAND ? "" : " ",
			result
			);

	memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);
	key_linepos = self->argument_start + strlen(result);

	key_linepos++;
	if (self->flags & CSTC_COMMAND)
		key_linepos++;

	i = strlen(key_lines[edit_line]);
	if (key_linepos > i)
		key_linepos = i;

	if (key_linepos >= MAXCMDLINE)
		key_linepos = MAXCMDLINE - 1;
}

void CSTC_Insert_And_Close(void)
{
	insert_result(cst_info, NULL);
	context_sensitive_tab_completion_active = 0;
}

static void cstc_insert_only_find(struct cst_info *self)
{
	insert_result(self, NULL);
	if (CSTC_ExecuteOnEnter(self->real_name))
		Cbuf_AddText("\n");
	CSTC_Cleanup(self);
}

static int valid_color(int i)
{
	if (i < 8)
		return i * 16 + 15;
	else
		return i *16;
}

void Key_Console (int key);
void Context_Sensitive_Tab_Completion_Key(int key)
{
	int i;
	qboolean execute = false;

	if (context_sensitive_tab_completion_active == 0)
		return;

	// ignore alt tab
	if (keydown[K_ALT] && key == K_TAB)
		return;

	//toggle tooltip
	if (keydown[K_CTRL] && key == 'h')
	{
		cst_info->tooltip_show = !cst_info->tooltip_show;
		return;
	}

	// hide tooltip if any other key is pressed
	if (cst_info->tooltip_show)
		cst_info->tooltip_show = false;

	if (cst_info->flags & CSTC_COLOR_SELECTOR)
		i = 256;
	else
		cst_info->result(cst_info, &i, 0, 0, NULL);

	if (key == K_ESCAPE)
	{
		if (cst_info->flags & CSTC_SLIDER)
			if (cst_info->variable)
				Cvar_Set(cst_info->variable, va("%f", cst_info->double_var[SLIDER_ORIGINAL_VALUE]));
		CSTC_Cleanup(cst_info);
		return;
	}

	// wanted to use F1->F12 here
	if (keydown[K_CTRL] && key >= '0' && key <= '9')
	{
		i = key - '0';
		i--;
		if (i<0)
			i+=10;
		cst_info->toggleables[i] = !cst_info->toggleables[i];
		cst_info->toggleables_changed= true;

		if (!(cst_info->flags & CSTC_NO_INPUT) && !(cst_info->flags & CSTC_COLOR_SELECTOR))
			cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);

		return;
	}


	if (key == K_ENTER)
	{
		insert_result(cst_info, NULL);
		if ((CSTC_ExecuteOnEnter(cst_info->real_name) || cst_info->flags & CSTC_EXECUTE) && !keydown[K_CTRL])
			execute = true;
		CSTC_Cleanup(cst_info);
		if (execute)
			Key_Console(K_ENTER); //there might be a better way to do this :P
		return;
	}

	if (cst_info->flags & CSTC_SLIDER)
	{
		if (key == K_LEFTARROW)
			cst_info->double_var[SLIDER_VALUE] -= 0.01;

		if (key == K_RIGHTARROW)
			cst_info->double_var[SLIDER_VALUE] += 0.01;

		if (key == K_DOWNARROW)
			cst_info->double_var[SLIDER_VALUE] -= 0.1;

		if (key == K_UPARROW)
			cst_info->double_var[SLIDER_VALUE] += 0.1;

		cst_info->double_var[SLIDER_VALUE] = bound(cst_info->double_var[SLIDER_LOWER_LIMIT], cst_info->double_var[SLIDER_VALUE], cst_info->double_var[SLIDER_UPPER_LIMIT]);
		if (cst_info->variable)
			Cvar_Set(cst_info->variable, va("%f", cst_info->double_var[SLIDER_VALUE]));
		return;
	}

	if (cst_info->flags & CSTC_COLOR_SELECTOR)
	{
		if (key == K_LEFTARROW)
		{
			cst_info->selection--;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
		}
		else if (key == K_RIGHTARROW)
		{
			cst_info->selection++;
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;
		}
		else if (key == K_UPARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection -= 16;
			else
				cst_info->selection += 16;

			if (cst_info->selection < 0)
				cst_info->selection = i + cst_info->selection;
			if (cst_info->selection >= i)
				cst_info->selection = cst_info->selection - i;
		}
		else if (key == K_DOWNARROW)
		{
			if (cst_info->direction == 1)
				cst_info->selection += 16;
			else
				cst_info->selection -= 16;

			if (cst_info->selection < 0)
				cst_info->selection = i + cst_info->selection;
			if (cst_info->selection >= i)
				cst_info->selection = cst_info->selection - i;
		}
		cst_info->selection_changed = true;
		return;
	}
	else if (cst_info->flags & CSTC_PLAYER_COLOR_SELECTOR)
	{
		if (key == K_SPACE)
		{
			cst_info->int_var[cst_info->int_var[COLOR_ROW]] = cst_info->selection;
		}
		else if (key == K_UPARROW || key == K_DOWNARROW)
		{
			cst_info->int_var[COLOR_ROW] += 1 * (cst_info->direction == 1 ? 1 : -1) * (key == K_UPARROW ? -1 : 1);
			cst_info->int_var[COLOR_ROW] = bound(0, cst_info->int_var[COLOR_ROW], 1);
		}
		else if (key == K_LEFTARROW || key == K_RIGHTARROW)
		{
			cst_info->selection += 1 * key == K_LEFTARROW ? -1 : 1;
			cst_info->selection = bound(0, cst_info->selection, 13);
		}
		cst_info->selection_changed = true;
		return;
	}
	else
	{
		if (key == K_UPARROW || key == K_DOWNARROW)
		{
			cst_info->selection += (keydown[K_CTRL] ? 5 : 1) * (cst_info->direction == 1 ? 1 : -1) * (key == K_UPARROW ? -1 : 1);

			if (i == 0)
			{
				cst_info->selection = 0;
				return;
			}
			if (cst_info->selection < 0)
				cst_info->selection = i-1;
			if (cst_info->selection >= i)
				cst_info->selection = 0;

			cst_info->selection_changed = true;
			return;
		}
	}

	if (key == K_TAB)
	{
		if (context_sensitive_tab_completion_close_on_tab.value == 1)
		{
			CSTC_Cleanup(cst_info);
			return;
		}

		if (cst_info->direction == 1)
			cst_info->selection++;
		else
			cst_info->selection--;
		if (cst_info->selection < 0)
			cst_info->selection = i-1;
		if (cst_info->selection >= i)
			cst_info->selection = 0;

		return;
	}

	if (!(cst_info->flags & CSTC_NO_INPUT) && !(cst_info->flags & CSTC_COLOR_SELECTOR))
	{
		Text_Input_Handle_Key(cst_info->new_input, key);
		Tokenize_Input(cst_info);
		cst_info->input_changed = true;
		cst_info->result(cst_info, &cst_info->results, 0, 0, NULL);
	}
	return;
}

static void CSTC_Draw(struct cst_info *self, int y_offset)
{
	int i, j, result_offset, offset, rows, sup, sdown, x, y, pos_x, pos_y, sp_x, sp_y;
	char *ptr, *ptr_result, *s;
	char buf[128];

	if (self->direction == -1)
		offset = y_offset - 32;
	else
		offset = y_offset - 14;

	if (!(self->flags & CSTC_NO_INPUT) && !(self->flags & CSTC_COLOR_SELECTOR))
	{
		Draw_Fill(0, offset , vid.conwidth, 10, context_sensitive_tab_completion_inputbox_color.value);
		Draw_String(8, offset, self->input);
		Draw_String(8 + self->new_input->position * 8 , offset + 2, "_");
		offset += 8 * self->direction;
	}

	if (self->direction == -1)
		rows = offset / 8;
	else
		rows = (vid.conheight - offset) / 8;

	if (rows % 2 != 0)
		rows--;

	self->offset_y = offset;
	self->rows = rows;

	result_offset = sup = sdown = 0;

	if (rows < self->results)
	{
		sup = 1;

		if (self->selection > rows/2)
		{
			result_offset = self->selection - rows/2;
			sdown = 1;
		}

		if ((self->results - self->selection) < rows/2)
		{
			result_offset = self->results - rows;
			sup = 0;
		}
	}

	if (self->flags & CSTC_COLOR_SELECTOR)
	{
		sp_x = sp_y = 0;
		for (x=0; x<16; x++)
		{
			for (y=0; y<16; y++)
			{
				pos_x = self->argument_start * 8 + 16;
				if (pos_x > vid.conwidth/2)
					pos_x -= 16 *8;
				self->offset_x = pos_x;
				pos_x += x *8;
				pos_y = offset + y * 8 * self->direction;
				Draw_Fill(pos_x, pos_y, 8, 8, x + y * 16);
				if (self->selection == x + y * 16)
				{
					sp_x = pos_x;
					sp_y = pos_y;
				}
			}
		}
		Draw_String(sp_x, sp_y, "x");
	}
	else if (self->flags & CSTC_PLAYER_COLOR_SELECTOR)
	{
		pos_x = self->argument_start * 8 + 16;
		if (pos_x > vid.conwidth/2)
			pos_x -= 16 *8 + 4;
		self->offset_x = pos_x;
		pos_y = offset;
		Draw_Fill(pos_x - 2, pos_y + 8 * self->direction - 2, 6 * 8 + 4 + 4, 16 + 4, 0);
		Draw_String(pos_x, pos_y, self->direction > 0 ? "top" : "bottom");
		Draw_String(pos_x, pos_y + 8 * self->direction, self->direction > 0 ? "bottom" : "top");
		pos_x += 6 * 8 + 4;
		for (y=0; y<2; y++)
		{
			for (x=0; x<13; x++)
			{
				if (x<8)
					i = x * 16 + 15;
				else
					i = x * 16;
				Draw_Fill(pos_x + x *8, pos_y + y * 8 * self->direction, 8, 8, i);
				if (y == 0 && x == self->int_var[COLOR_COLOR1])
					Draw_String(pos_x + x *8, pos_y + y * 8 * self->direction, "o");
				if (y == 1 && x == self->int_var[COLOR_COLOR2])
					Draw_String(pos_x + x *8, pos_y + y * 8 * self->direction, "o");
			}
		}
		Draw_String(pos_x + self->selection * 8, pos_y + self->int_var[COLOR_ROW] * self->direction * 8, "x");
	}
	else if (self->flags & CSTC_SLIDER)
	{
		pos_x = self->argument_start * 8 + 16;
		self->offset_x = pos_x;
		if (context_sensitive_tab_completion_slider_no_offset.value)
			pos_y = offset - self->direction * 8;
		else 
			pos_y = offset;
		self->offset_y = pos_y;

		CSTC_DrawSlider(pos_x, pos_y, 16, 5, self->double_var[SLIDER_VALUE], false, va("%.2f", self->double_var[SLIDER_VALUE]));
	}
	else
	{

		for (i=0, ptr = NULL; i<rows; i++)
		{
			if (self->result(self, NULL, i + result_offset, cstc_rt_draw, &ptr))
				break;

			if (i + result_offset == self->selection)
			{
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_selected_color.value);
				self->offset_y = offset + i * 8 * self->direction;
			}
			else
				Draw_Fill(0, offset + i * 8 * self->direction, vid.conwidth, 8, context_sensitive_tab_completion_background_color.value);

			if (self->flags & CSTC_COMMAND || self->flags & CSTC_HIGLIGHT_INPUT)
			{
				self->result(self, NULL, i + result_offset, cstc_rt_highlight, &ptr_result);
				for (j=0; j<self->tokenized_input->count && ptr_result; j++)
				{
					s = Util_strcasestr(ptr_result, self->tokenized_input->tokens[j]);
					if (s)
					{
						x = s - ptr_result;
						Draw_Fill(32 + x * 8, offset + i * 8 * self->direction, strlen(self->tokenized_input->tokens[j]) * 8, 8, context_sensitive_tab_completion_highlight_color.value);
					}
				}
			}

			if (ptr)
				Draw_ColoredString(32, offset + i * 8 * self->direction, ptr, 0);
		}

		if (sup || sdown)
			CSTC_DrawSlider(8, offset + 8 * self->direction - 16, 16, rows/2 - 4, (float) self->selection/(self->results ? self->results : 1), true, NULL);

		if (sup)
		{
			if (cstc_pictures)
				CSTC_DrawPicture(8, offset + (rows - 1) * 8 * self->direction, 0, 0, cstcp_arrow_up);
			else
				Draw_String(8, offset + (rows - 1) * 8 * self->direction, "^");
		}

		if (sdown)
		{
			if (cstc_pictures)
				CSTC_DrawPicture(8, offset + 8 * self->direction, 0, 0, cstcp_arrow_down);
			else
				Draw_String(8, offset + 8 * self->direction, "v");
		}

		if (rows < self->results && context_sensitive_tab_completion_show_results.value == 1)
		{
			sprintf(buf, "showing %i of %i results", rows, self->results);
			Draw_String(vid.conwidth - strlen(buf) * 8, offset, buf);
		}
	}

	if (self->tooltip_show && self->tooltip)
	{
		Draw_Fill(0, offset , vid.conwidth, 8, context_sensitive_tab_completion_tooltip_color.value);
		Draw_String(0, offset , va("help: %s", self->tooltip));
	}

	if (self->draw)
		self->draw(self);

	//CSTC_DrawBorder(100, 100, 16, 16, 16, 2);
}

void Context_Sensitive_Tab_Completion_Draw(void)
{
	extern float scr_conlines;

	if (context_sensitive_tab_completion_active == 0)
		return;

	if (cst_info == NULL)
		return;

	if (scr_conlines > vid.conheight/2)
		cst_info->direction = -1;
	else
		cst_info->direction = 1;

	if (cst_info->results == 1)
	{
		cstc_insert_only_find(cst_info);
		return;
	}

	CSTC_Draw(cst_info, scr_conlines);

	// reset all changed flags
	cst_info->toggleables_changed = cst_info->selection_changed = cst_info->input_changed = false;
}

static void getarg(const char *s, char **start, char **end, char **next, qboolean cmd)
{
	char endchar;

	while(*s == ' ')
		s++;

	if (*s == '"')
	{
		endchar = '"';
		s++;
	}
	else
		if (cmd)
			endchar = ' ';
		else
			endchar = ';';

	*start = (char *)s;

	while(*s && *s != endchar && (endchar != ' ' || *s != ';'))
		s++;

	*end = (char *)s;

	if (*s == '"')
		s++;

	*next = (char *)s;
}

void read_info_new (char *string, int position, char **cmd_begin, int *cmd_len, char **arg_begin, int *arg_len, int *cursor_on_command, int *is_invalid)
{
	char *cmd_start, *cmd_stop, *arg_start, *arg_stop;
	char **start, **stop;
	char *s;
	char *next;
	int docontinue;
	int dobreak;
	int isinvalid;
	qboolean cmd = true;

	docontinue = 0;
	dobreak = 0;

	s = string;

	cmd_start = string;
	cmd_stop = string;
	arg_start = string;
	arg_stop = string;

	isinvalid = 0;

	if (cursor_on_command)
		*cursor_on_command = 0;


	while(*s)
	{
		if (*s == '/')
		{
			if (s != string)
				isinvalid = 1;

			s++;
		}

		if (*s == ' ')
			break;

		if (position < s - string)
			break;

		start = &cmd_start;
		stop = &cmd_stop;

		while(*s)
		{
			getarg(s, start, stop, &next, cmd);

#if 0
			printf("'%s' '%s'\n", cmd_start, cmd_stop);
#endif

			if (dobreak)
				break;

			s = next;
			while(*s == ' ')
				s++;

			if (position >= (*start - string) && position <= (next - string))
			{
#if 0
				printf("Cursor is in command\n");
#endif
				if (cursor_on_command)
					*cursor_on_command = 1;

				dobreak = 1;

#if 0
				if (*s == 0 || *s == ';')
#endif
					break;
			}

			if (*s == ';')
			{
				isinvalid = 0;

				s++;
				while(*s == ' ')
					s++;

				cmd_start = string;
				cmd_stop = string;
				arg_start = string;
				arg_stop = string;

				docontinue = 1;
				break;
			}

			start = &arg_start;
			stop = &arg_stop;
			cmd = false;
		}

		if (docontinue)
		{
			docontinue = 0;
			continue;
		}

		if (dobreak)
			break;
	}

	if (isinvalid)
	{
		cmd_start = string;
		cmd_stop = string;
		arg_start = string;
		arg_stop = string;

		if (cmd_begin)
			*cmd_begin = NULL;

		if (cmd_len)
			cmd_len = 0;

		if (arg_begin)
			*arg_begin = NULL;

		if (arg_len)
			*arg_len = 0;

		if (is_invalid)
			*is_invalid = 1;

	}
	else
	{
		if (cmd_begin)
			*cmd_begin = cmd_start;

		if (cmd_len)
			*cmd_len = cmd_stop - cmd_start;

		if (arg_begin)
			*arg_begin = arg_start;

		if (arg_len)
			*arg_len = arg_stop - arg_start;

		if (is_invalid)
			*is_invalid = 0;
	}

	/*
	printf(" %s\n", string);
	printf("%*s%s\n", position + 1, " ", "_");
	printf("%*s%s\n", cmd_start - string + 1, " ", "^");
	printf("%*s%s\n", cmd_stop - string + 1, " ", "^");
	printf("%*s%s\n", arg_start - string + 1, " ", "^");
	printf("%*s%s\n", arg_stop - string + 1, " ", "^");
	*/
}

static void setup_completion(struct cst_commands *cc, struct cst_info *c, int arg_start, int arg_len, int cmd_start, int cmd_len)
{
	if (c == NULL || cc == NULL)
		return;

	memset(c, 0, sizeof(struct cst_info));

	c->name = cc->name;
	c->commands = cc->commands;
	c->result = cc->result;
	c->get_data = cc->get_data;
	c->draw = cc->draw;
	snprintf(c->input, sizeof(c->input), "%.*s", arg_len, key_lines[edit_line] + arg_start);

	c->new_input = Text_Input_Create(c->input, sizeof(c->input), arg_len, 0);

	Tokenize_Input(c);
	c->argument_start = arg_start;
	c->argument_length = arg_len;
	c->command_start = cmd_start;
	c->command_length = cmd_len;
	if (c->get_data)
		c->get_data(c, 0);
	c->result(c, &c->results, 0, 0, NULL);
	c->flags = cc->flags;
	c->tooltip = cc->tooltip;
}

static void setup_slider(struct cst_info *c)
{
	c->double_var[SLIDER_LOWER_LIMIT] = 0;
	c->double_var[SLIDER_UPPER_LIMIT] = 1;
	if (c->variable)
	{
		c->double_var[SLIDER_VALUE] = bound(c->double_var[SLIDER_LOWER_LIMIT], c->variable->value, c->double_var[SLIDER_UPPER_LIMIT]);
		c->double_var[SLIDER_ORIGINAL_VALUE] = c->variable->value;
	}
	else
	{
		c->double_var[SLIDER_VALUE] = c->double_var[SLIDER_ORIGINAL_VALUE] = 0;
	}
}

static qboolean setup_current_command(qboolean check_only)
{
	int cmd_len, arg_len, cursor_on_command, isinvalid, i, dobreak;
	char *cmd_start, *arg_start, *name;
	int cmd_istart, arg_istart;
	struct cst_commands *c;
	cvar_t *var;
	char new_keyline[MAXCMDLINE], *cs;
	struct tokenized_string *ts, *var_ts;
	qboolean cvar_setup;

	read_info_new(key_lines[edit_line] + 1, key_linepos, &cmd_start, &cmd_len, &arg_start, &arg_len, &cursor_on_command, &isinvalid);

	if (isinvalid)
		return false;

	cs = key_lines[edit_line];

	cmd_istart = cmd_start - key_lines[edit_line];
	if (arg_start)
		arg_istart = arg_start - key_lines[edit_line];
	else
		arg_istart = cmd_istart + cmd_len;

	if (arg_istart < cmd_istart)
		arg_istart = cmd_istart + cmd_len;

	if (cursor_on_command || key_lines[edit_line] + key_linepos == cmd_start + cmd_len)
	{
		if ((context_sensitive_tab_completion_command_only_on_ctrl_tab.value == 1 && keydown[K_CTRL]) || context_sensitive_tab_completion_command_only_on_ctrl_tab.value == 0)
		{
			if (check_only == false)
				setup_completion(&Command_Completion, cst_info, cmd_istart , cmd_len, cmd_istart, cmd_len);
			return true;
		}
	}

	if (cmd_start && arg_start)
	{
		c = commands;
		dobreak = 0;

		while (c)
		{
			if (c->flags & CSTC_MULTI_COMMAND)
			{
				for (i=0; i<c->commands->count; i++)
				{
					if (cmd_len == strlen(c->commands->tokens[i]) && strncasecmp(c->commands->tokens[i], cmd_start, cmd_len) == 0)
					{
						dobreak = 1;
						name = c->commands->tokens[i];
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
						break;
					}
				}
			}
			else
			{
				if (cmd_len == strlen(c->name) && strncasecmp(c->name, cmd_start, cmd_len) == 0)
				{
					name = c->name;
					snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					break;
				}
			}

			if (dobreak)
				break;
			c = c->next;
		}

		// check user set variables
		if (!c)
		{
			// slider
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_slider_variables.string);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Slider;
						if (check_only == false)
							setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);

						cst_info->variable = Cvar_FindVar(name);
						setup_slider(cst_info);
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return true;
			}

			// player_color
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_player_color_variables);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Player_Color;
						var = Cvar_FindVar(name);
						if (check_only == false)
							setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);

						if (var)
						{
							var_ts = Tokenize_String(var->string);
							if (var_ts)
							{
								if (var_ts->count == 1)
								{
									cst_info->int_var[COLOR_COLOR1] = cst_info->int_var[COLOR_COLOR2] = atof(var_ts->tokens[0]);
								}
								else if (var_ts->count == 2)
								{
									cst_info->int_var[COLOR_COLOR1] = atoi(var_ts->tokens[0]);
									cst_info->int_var[COLOR_COLOR2] = atoi(var_ts->tokens[1]);
								}
								Tokenize_String_Delete(var_ts);
							}
						}
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return true;
			}

			// color selector
			cvar_setup = false;
			ts = Tokenize_String(context_sensitive_tab_completion_color_variables);
			if (ts)
			{
				if (ts->count)
				{
					name = NULL;
					for (i=0; i<ts->count;i++)
					{
						if (strlen(ts->tokens[i]) == cmd_len && strncasecmp(ts->tokens[i], cmd_start, cmd_len) == 0)
						{
							name = ts->tokens[i];
							break;
						}
					}

					if (name)
					{
						cvar_setup = true;
						c = &CC_Color;
						var = Cvar_FindVar(name);
						if (check_only == false)
							setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);
						if (var)
							cst_info->selection = var->value;
						snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
					}
				}
				Tokenize_String_Delete(ts);
				if (cvar_setup)
					return true;
			}
		}

		if (c)
		{
			if (c->conditions)
				if (c->conditions() == 0)
					return false;
			if (check_only == false)
				setup_completion(c, cst_info, arg_istart ,arg_len, cmd_istart, cmd_len);
			cst_info->function = Cmd_FindCommand(name);
			cst_info->variable = Cvar_FindVar(name);
			snprintf(cst_info->real_name, sizeof(cst_info->real_name), "%*.*s", cmd_len, cmd_len, cmd_start);
			if (cst_info->flags & CSTC_SLIDER)
				setup_slider(cst_info);
			return true;
		}
		else if (check_only == false)
		{
			snprintf(new_keyline, sizeof(new_keyline), "%*.*s", cmd_len, cmd_len, cmd_start);
			var = Cvar_FindVar(new_keyline);
			if (var && arg_len == 0)
			{
				snprintf(new_keyline, sizeof(new_keyline), "%s%s\"%s\"", key_lines[edit_line], key_lines[edit_line][strlen(key_lines[edit_line])-1] == ' ' ? "" : " ", var->string);
				key_linepos = strlen(new_keyline);
				memcpy(key_lines[edit_line], new_keyline, MAXCMDLINE);
			}
		}
	}
	return false;
}

int Context_Sensitive_Tab_Completion(void)
{
	if (context_sensitive_tab_completion_ignore_alt_tab.value == 1)
		if (keydown[K_ALT])
			return 0;

	if (setup_current_command(false))
	{
		context_sensitive_tab_completion_active = 1;
		return 1;
	}

	return 0;
}

void Context_Sensitive_Tab_Completion_Notification(qboolean input)
{
	static double last_input;
	static qboolean checked;
	static qboolean show_icon;
	extern float scr_conlines;
	int x, y;

	if (context_sensitive_tab_completion_active || context_sensitive_tab_completion_show_notification.value == 0)
		return;

	if (input)
	{
		last_input = Sys_DoubleTime();
		checked = false;
		return;
	}

	if (last_input + 1 > Sys_DoubleTime())
		return;

	if (checked == false)
	{
		show_icon = setup_current_command(true);
		CSTC_Console_Close();
		checked = true;
	}

	if (show_icon == false)
		return;

	x = strlen(key_lines[edit_line]) * 8 + 16;
	y = scr_conlines - 16;

	CSTC_DrawBubble(x, y, 8, cstcp_cstc_icon, NULL, last_input + 3 < Sys_DoubleTime() ? "press tab to start tabcompletion" : NULL);
}

int Cmd_CompleteCountPossible (char *partial);
int Cmd_AliasCompleteCountPossible (char *partial);
int Cvar_CompleteCountPossible (char *partial);

struct cva_s
{
	union
	{
		cmd_function_t *c;
		cmd_alias_t *a;
		cvar_t *v;
	} info;
	int type;
	unsigned int match;
};

static int name_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	char *na, *nb;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			na = x->info.c->name;
			break;
		case 1:
			na = x->info.a->name;
			break;
		case 2:
			na = x->info.v->name;
			break;
		default:
			return -1;
	}

	switch (y->type)
	{
		case 0:
			nb = y->info.c->name;
			break;
		case 1:
			nb = y->info.a->name;
			break;
		case 2:
			nb = y->info.v->name;
			break;
		default:
			return -1;
	}

	return(strcasecmp(na, nb));
}

static int new_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	char *na, *nb;
	int w1, w2;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			na = x->info.c->name;
			w1 = x->info.c->weight;
			break;
		case 1:
			na = x->info.a->name;
			w1 = x->info.a->weight;
			break;
		case 2:
			na = x->info.v->name;
			w1 = x->info.v->weight;
			break;
		default:
			return -1;
	}

	switch (y->type)
	{
		case 0:
			nb = y->info.c->name;
			w2 = y->info.c->weight;
			break;
		case 1:
			nb = y->info.a->name;
			w2 = y->info.a->weight;
			break;
		case 2:
			nb = y->info.v->name;
			w2 = y->info.v->weight;
			break;
		default:
			return -1;
	}

	return(strcasecmp(na, nb) + (x->match - y->match) +  (w2 - w1));
}


static int match_compare(const void *a, const void *b)
{
	struct cva_s *x, *y;
	int w1, w2;

	if (a == NULL)
		return -1;

	if (b == NULL)
		return -1;

	x = (struct cva_s *)a;
	y = (struct cva_s *)b;

	switch (x->type)
	{
		case 0:
			w1 = x->info.c->weight;
			break;
		case 1:
			w1 = x->info.a->weight;
			break;
		case 2:
			w1 = x->info.v->weight;
			break;
		default:
			w1 = 0;
	}

	switch (y->type)
	{
		case 0:
			w2 = y->info.c->weight;
			break;
		case 1:
			w2 = y->info.a->weight;
			break;
		case 2:
			w2 = y->info.v->weight;
			break;
		default:
			w2 = 0;
	}

	return x->match - y->match + w2 *2 - w1 *2;
}

static int setup_command_completion_data(struct cst_info *self)
{
	extern cvar_t *cvar_vars;
	extern cmd_function_t *cmd_functions;
	extern cmd_alias_t *cmd_alias;
	cmd_function_t *cmd;
	cmd_alias_t *alias;
	cvar_t *var;
	int count, i, add, match;

	char *s;

	struct cva_s *cd;

	if (self->data)
	{
		free(self->data);
		self->data = NULL;
	}

	count = 0;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (Util_strcasestr(cmd->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if (Util_strcasestr(alias->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	for (var=cvar_vars; var; var=var->next)
	{
		add = 1;

		for (i=0; i<self->tokenized_input->count; i++)
		{

			if (Util_strcasestr(var->name, self->tokenized_input->tokens[i]) == NULL)
			{
				add = 0;
				break;
			}
		}
		if (add)
			count++;
	}

	if (count == 0)
		return -1;

	self->data = calloc(count, sizeof(struct cva_s));
	if (self->data == NULL)
		return -1;

	cd = self->data;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
	{
		add = 1;
		match = 0;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(cmd->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
			match += s - cmd->name;
		}

		if (add)
		{
			cd->info.c = cmd;
			cd->type = 0;
			cd->match = match + cmd->weight;
			cd++;
		}
	}

	for (alias=cmd_alias; alias; alias=alias->next)
	{
		match = 0;
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(alias->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
				match += s - alias->name;
		}

		if (add)
		{
			cd->info.a = alias;
			cd->type = 1;
			cd->match = match + alias->weight;
			cd++;
		}
	}

	for (var=cvar_vars; var; var=var->next)
	{
		match = 0;
		add = 1;
		for (i=0; i<self->tokenized_input->count; i++)
		{
			if ((s = Util_strcasestr(var->name, self->tokenized_input->tokens[i])) == NULL)
			{
				add = 0;
				break;
			}
			match += s - var->name;
		}
		if (add)
		{
			cd->info.v = var;
			cd->type = 2;
			cd->match = match + var->weight;
			cd++;
		}
	}

	cd = self->data;

	if (context_sensitive_tab_completion_sorting_method.value == 1)
		qsort(self->data, count, sizeof(struct cva_s), &match_compare);
	else if (context_sensitive_tab_completion_sorting_method.value == 2)
		qsort(self->data, count, sizeof(struct cva_s), &new_compare);
	else
		qsort(self->data, count, sizeof(struct cva_s), &name_compare);

	return count;
}

static int Command_Completion_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	int count;
	struct cva_s *cc;
	char *res;
	char *t, *s;

	if (self == NULL)
		return 1;

	
	if (results || self->bool_var[CC_INITALIZED] == false)
	{
		count = setup_command_completion_data(self);

		if (count == -1)
			return 1;

		if (results)
			*results = count;

		self->results = count;

		self->bool_var[CC_INITALIZED] = true;

		return 0;
	}

	if (result == NULL)
		return 0;

	if (get_result >= self->results)
		return 1;


	if (self->data == NULL)
		return 1;

	cc = self->data;

	s = NULL;
	switch (cc[get_result].type)
	{
		case 0:
			res = cc[get_result].info.c->name;
			t = "&cf55";
			break;
		case 1:
			res = cc[get_result].info.a->name;
			t = "&c5f5";
			s = cc[get_result].info.a->value;
			break;
		case 2:
			res = cc[get_result].info.v->name;
			t = "&cff5";
			s = cc[get_result].info.v->string;
			break;
		default:
			*result = NULL;
			return 1;
	}

	if (result_type == cstc_rt_real)
	{
		*result = va("%s", res);
		snprintf(self->real_name, sizeof(self->real_name), "%s", res);
	}
	else if (result_type == cstc_rt_draw)
		*result = va("%s%s%s", context_sensitive_tab_completion_color_coded_types.value ? t : "",res, s ? va("%s -> %s", context_sensitive_tab_completion_color_coded_types.value ? "&cfff" : "",  s): "");
	else
		*result = va("%s%s", res, s ? s: "");

	return 0;
}

static int Command_Completion_Get_Data(struct cst_info *self, int remove)
{
	if (remove)
	{
		free(self->data);
		self->data = NULL;
	}
	return 0;
}

int weight_disable = 1;

void Weight_Disable_f(void)
{
	weight_disable = 1;
}

void Weight_Enable_f(void)
{
	weight_disable = 0;
}

void Weight_Set_f(void)
{
	cvar_t *cvar;
	cmd_function_t *cmd_function;
	cmd_alias_t *cmd_alias;

	if (Cmd_Argc() != 3)
	{
		Com_Printf("Usage: %s [variable/command/alias] [weight].\n", Cmd_Argv(0));
		return;
	}

	if ((cvar=Cvar_FindVar(Cmd_Argv(2))))
	{
		cvar->weight = atoi(Cmd_Argv(1));
		return;
	}

	if ((cmd_function=Cmd_FindCommand(Cmd_Argv(2))))
	{
		cmd_function->weight = atoi(Cmd_Argv(1));
		return;
	}

	if ((cmd_alias=Cmd_FindAlias(Cmd_Argv(2))))
	{
		cmd_alias->weight = atoi(Cmd_Argv(1));
		return;
	}
}

static int Player_Color_Selector_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%i %i", self->int_var[COLOR_COLOR1], self->int_var[COLOR_COLOR2]);
		return 0;
	}

	return 1;
}

static int Color_Selector_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%i", get_result);
		return 0;
	}
	return 1;
}

static int Slider_Result(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	if (result)
	{
		*result = va("%f", self->double_var[SLIDER_VALUE]);
		return 0;
	}
	return 1;
}

void Context_Sensitive_Tab_Completion_CvarInit(void)
{
	Command_Completion.name = "Command_Completion";
	Command_Completion.result = &Command_Completion_Result;
	Command_Completion.get_data = &Command_Completion_Get_Data;
	Command_Completion.conditions = NULL;
	Command_Completion.flags = CSTC_COMMAND;
	Cvar_Register(&context_sensitive_tab_completion);
	Cvar_Register(&context_sensitive_tab_completion_show_notification);
	Cvar_Register(&context_sensitive_tab_completion_use_pictures);
	Cvar_Register(&context_sensitive_tab_completion_command_only_on_ctrl_tab);
	Cvar_Register(&context_sensitive_tab_completion_color_coded_types);
	Cvar_Register(&context_sensitive_tab_completion_close_on_tab);
	Cvar_Register(&context_sensitive_tab_completion_sorting_method);
	Cvar_Register(&context_sensitive_tab_completion_show_results);
	Cvar_Register(&context_sensitive_tab_completion_ignore_alt_tab);
	Cvar_Register(&context_sensitive_tab_completion_background_color);
	Cvar_Register(&context_sensitive_tab_completion_selected_color);
	Cvar_Register(&context_sensitive_tab_completion_inputbox_color);
	Cvar_Register(&context_sensitive_tab_completion_tooltip_color);
	Cvar_Register(&context_sensitive_tab_completion_highlight_color);
	Cvar_Register(&context_sensitive_tab_completion_insert_slash);
	Cvar_Register(&context_sensitive_tab_completion_slider_no_offset);
	Cvar_Register(&context_sensitive_tab_completion_slider_border_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_background_color);
	Cvar_Register(&context_sensitive_tab_completion_slider_variables);
	Cvar_Register(&context_sensitive_tab_completion_execute_on_enter);
	Cmd_AddCommand("weight_enable", Weight_Enable_f);
	Cmd_AddCommand("weight_disable", Weight_Disable_f);
	Cmd_AddCommand("weight_set", Weight_Set_f);
	CC_Slider.result = &Slider_Result;
	CC_Slider.flags = CSTC_SLIDER | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Slider.tooltip = cstc_slider_tooltip;
	CC_Player_Color.result = &Player_Color_Selector_Result;
	CC_Player_Color.flags = CSTC_PLAYER_COLOR_SELECTOR | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Player_Color.tooltip = cstc_player_color_tooltip;

	CC_Color.result = &Color_Selector_Result;
	CC_Color.flags = CSTC_COLOR_SELECTOR | CSTC_NO_INPUT | CSTC_EXECUTE;
	CC_Color.tooltip = cstc_color_tooltip;

}

void Context_Weighting_Init(void)
{
	Cbuf_AddText("weight_disable\n");
	Cbuf_AddText("exec weight_file\n");
	Cbuf_AddText("weight_enable\n");
}

void CSTC_PictureInit(void)
{
	if (cstc_pictures)
		Draw_FreePicture(cstc_pictures);
	cstc_pictures = Draw_LoadPicture("gfx/cstc_pics.png", DRAW_LOADPICTURE_NOFALLBACK);
}

void CSTC_PictureShutdown(void)
{
	if (cstc_pictures)
	{
		Draw_FreePicture(cstc_pictures);
		cstc_pictures = NULL;
	}
}

void Context_Weighting_Shutdown(void)
{
	extern cvar_t *cvar_vars;
	extern cmd_function_t *cmd_functions;
	extern cmd_alias_t *cmd_alias;
	cmd_function_t *cmd;
	cmd_alias_t *alias;
	cvar_t *var;
	FILE *f;

	f = fopen("fodquake/weight_file", "w");

	if (f == NULL)
		return;

	for (cmd=cmd_functions; cmd; cmd=cmd->next)
		if (cmd->weight > 0)
			fprintf(f, "weight_set %i %s\n", cmd->weight, cmd->name);

	for (var=cvar_vars; var; var=var->next)
		if (var->weight > 0)
			fprintf(f, "weight_set %i %s\n", var->weight, var->name);

	for (alias=cmd_alias; alias; alias=alias->next)
		if (alias->weight > 0)
			fprintf(f, "weight_set %i %s\n", alias->weight, alias->name);

	fclose (f);
}

void CSTC_Console_Close(void)
{
	CSTC_Cleanup(cst_info);
}

void CSTC_Shutdown(void)
{
	struct cst_commands *c, *cc;

	c = commands;
	while (c)
	{
		cc = c->next;
		if (c->name)
			free(c->name);
		if (c->commands)
			Tokenize_String_Delete(c->commands);
		free(c);
		c = cc;
	}
}
