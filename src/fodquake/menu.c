/*
Copyright (C) 1996-1997 Id Software, Inc.

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

#include "quakedef.h"
#include "filesystem.h"
#include "input.h"
#include "keys.h"
#include "server_browser.h"
#include "sound.h"
#include "version.h"
#ifndef CLIENTONLY
#include "server.h"
#endif

#include "utils.h"
#include "mp3_player.h"

#include "strl.h"

#ifdef GLQUAKE
#include "gl_cvars.h"
#endif

#include "menu.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#define	NUM_HELP_PAGES	6

struct Picture *qplaquepic;
struct Picture *ttl_mainpic;
struct Picture *mainmenupic;
struct Picture *menudotpic[6];
struct Picture *p_optionpic;
struct Picture *ttl_cstmpic;
struct Picture *vidmodespic;
struct Picture *helppic[NUM_HELP_PAGES];
struct Picture *ttl_sglpic;
struct Picture *sp_menupic;
struct Picture *p_loadpic;
struct Picture *p_savepic;
struct Picture *p_multipic;
struct Picture *bigboxpic;
struct Picture *menuplyrpic;

enum menustates
{
	m_none,
	m_main,
	m_singleplayer,
	m_load,
	m_save,
	m_multiplayer,
	m_setup,
	m_options,
	m_video,
	m_video_verify,
	m_keys,
	m_help,
	m_quit,
	m_gameoptions,
	m_fps,
	m_demos,
	m_mp3_control,
	m_mp3_playlist
};

static enum menustates m_state;

static void M_Menu_Main_f(void);
	static void M_Menu_SinglePlayer_f(void);
		static void M_Menu_Load_f(void);
		static void M_Menu_Save_f(void);
	static void M_Menu_MultiPlayer_f(void);
		static void M_Menu_Setup_f(void);
		static void M_Menu_Demos_f(void);
		static void M_Menu_GameOptions_f(void);
	static void M_Menu_Options_f(void);
		static void M_Menu_Keys_f(void);
		static void M_Menu_Fps_f(void);
		static void M_Menu_Video_f(void);
	static void M_Menu_MP3_Control_f(void);

static void M_Main_Draw(void);
	static void M_SinglePlayer_Draw(void);
		static void M_Load_Draw(void);
		static void M_Save_Draw(void);
	static void M_MultiPlayer_Draw(void);
		static void M_Setup_Draw(void);
		static void M_Demos_Draw(void);
		static void M_GameOptions_Draw(void);
	static void M_Options_Draw(void);
		static void M_Keys_Draw(void);
		static void M_Fps_Draw(void);
		static void M_Video_Draw(void);
	static void M_Help_Draw(void);
	static void M_Quit_Draw(void);

static void M_Main_Key(int key);
	static void M_SinglePlayer_Key(int key);
		static void M_Load_Key(int key);
		static void M_Save_Key(int key);

		static void M_Setup_Key(int key);
		static void M_Demos_Key(int key);
		static void M_GameOptions_Key(int key);

		static void M_Keys_Key(int key);
		static void M_Video_Key(int key);
	static void M_Help_Key(int key);
	static void M_Quit_Key(int key);

static struct Menu *multiplayermenu;
static struct Menu *optionsmenu;
static struct Menu *fpsmenu;

static struct MenuItem *optionsmenu_usemouse;

static qboolean m_entersound;     /* play after drawing a frame, so caching won't disrupt the sound */
static qboolean m_recursiveDraw;
static int m_topmenu;             /* set if a submenu was entered via a menu_* command */

//=============================================================================
/* Support Routines */

cvar_t	scr_scaleMenu = {"scr_scaleMenu","1"};
static int		menuwidth = 320;
static int		menuheight = 240;

cvar_t	scr_centerMenu = {"scr_centerMenu","1"};
int		m_yofs = 0;

static void M_DrawCharacter(int cx, int line, int num)
{
	Draw_Character (cx + ((menuwidth - 320)>>1), line + m_yofs, num);
}

static void M_Print(int cx, int cy, const char *str)
{
	Draw_Alt_String (cx + ((menuwidth - 320)>>1), cy + m_yofs, str);
}

static void M_PrintWhite(int cx, int cy, const char *str)
{
	Draw_String (cx + ((menuwidth - 320)>>1), cy + m_yofs, str);
}

static void M_DrawPic(struct Picture *pic, int x, int y, int width, int height)
{
	Draw_DrawPicture(pic, x + ((menuwidth - 320)>>1), y + m_yofs, width, height);
}

#if 0
static byte translationTable[256];

static void M_BuildTranslationTable(int top, int bottom)
{
	byte identityTable[256];
	int		j;
	byte	*dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j = 0; j < 16; j++)
			dest[TOP_RANGE + j] = source[top + 15 - j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j = 0; j < 16; j++)
			dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
}


static void M_DrawTransPicTranslate(int x, int y, mpic_t *pic)
{
	Draw_TransPicTranslate (x + ((menuwidth - 320) >> 1), y + m_yofs, pic, translationTable);
}
#endif


static void M_DrawTextBox(int x, int y, int width, int lines)
{
	Draw_TextBox (x + ((menuwidth - 320) >> 1), y + m_yofs, width, lines);
}

//=============================================================================

void M_ToggleMenu_f()
{
	m_entersound = true;

	if (key_dest == key_menu)
	{
		if (m_state != m_main)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		VID_SetMouseGrab(1);
		return;
	}
	else
	{
		VID_SetMouseGrab(0);
		M_Menu_Main_f ();
	}
}

static void M_EnterMenu(int state)
{
	if (key_dest != key_menu)
	{
		m_topmenu = state;
		Con_ClearNotify ();
		// hide the console
		scr_conlines = 0;
		scr_con_current = 0;
	}
	else
	{
		m_topmenu = m_none;
	}

	key_dest = key_menu;
	m_state = state;
	m_entersound = true;
}

static void M_LeaveMenu(int parent)
{
	if (m_topmenu == m_state)
	{
		m_state = m_none;
		key_dest = key_game;
		VID_SetMouseGrab(1);
	}
	else
	{
		m_state = parent;
		m_entersound = true;
	}
}

/* Some less ugly menu crapola */

struct MenuItem
{
	struct MenuItem *next;
	char *label;
	unsigned int labellen;
	int whitelabel;
	int isselectable;
	int hidden;

	int selectindex;

	unsigned int x;
	unsigned int y;

	void (*delete)(struct MenuItem *);
	void (*draw)(struct MenuItem *, unsigned int extra_x, unsigned int extra_width);
	void (*handlekey)(struct MenuItem *, int key);
};

struct MenuItemButton
{
	struct MenuItem menuitem;

	void (*callback)(void);
};

struct MenuItemCvarRange
{
	struct MenuItem menuitem;

	float minvalue;
	float maxvalue;
	float step;
	cvar_t *cvar;
};

struct MenuItemCvarBoolean
{
	struct MenuItem menuitem;

	cvar_t *cvar;
	int invert;
};

struct MenuItemCvarPosNegBoolean
{
	struct MenuItem menuitem;

	cvar_t *cvar;
};

struct MenuItemCvarMultiSelect
{
	struct MenuItem menuitem;

	cvar_t *cvar;
	const char * const *values;
	int numvalues;
};

struct Menu
{
	int error;

	struct MenuItem *items;
	struct MenuItem **selectableitemarray;
	unsigned int numselectableitems;

	int cursor_right;
	enum menustates prevmenu;

	unsigned int cursor;
	unsigned int cursor_x;
	unsigned int cursor_y;

	unsigned int extra_x;
	unsigned int extra_width;
};

static struct Menu *Menu_Create(int cursor_right, enum menustates prevmenu)
{
	struct Menu *menu;

	menu = malloc(sizeof(*menu));
	if (menu)
	{
		menu->items = 0;
		menu->selectableitemarray = 0;
		menu->numselectableitems = 0;
		menu->error = 0;
		menu->cursor = 0;

		menu->cursor_right = cursor_right;
		menu->prevmenu = prevmenu;

		return menu;
	}

	return 0;
}

static void Menu_Delete(struct Menu *menu)
{
	struct MenuItem *menuitem, *nextmenuitem;

	if (menu == 0)
		return;

	nextmenuitem = menu->items;
	while((menuitem = nextmenuitem))
	{
		nextmenuitem = menuitem->next;

		menuitem->delete(menuitem);
	}

	free(menu->selectableitemarray);
	free(menu);
}

static void Menu_Layout(struct Menu *menu)
{
	struct MenuItem *menuitem;
	unsigned int numselectableitems;
	unsigned int labelwidth;
	unsigned int labelstart;
	unsigned int y;
	unsigned int i;

	if (menu == 0)
		return;

	labelwidth = 0;
	numselectableitems = 0;
	menuitem = menu->items;
	while(menuitem)
	{
		if (!menuitem->hidden)
		{
			if (menuitem->labellen > labelwidth)
				labelwidth = menuitem->labellen;

			if (menuitem->isselectable)
				numselectableitems++;
		}

		menuitem = menuitem->next;
	}

	menu->numselectableitems = numselectableitems;

	free(menu->selectableitemarray);
	menu->selectableitemarray = malloc(numselectableitems*(sizeof(*menu->selectableitemarray)));
	if (menu->selectableitemarray == 0)
	{
		menu->error = 1;
		return;
	}

	labelstart = 48;
	y = 32;
	i = 0;

	if (menu->cursor_right)
	{
		menu->cursor_x = labelstart + labelwidth * 8 + 8;
	}
	else
	{
		menu->cursor_x = labelstart + 8;
		labelstart += 8 * 3;
	}

	menuitem = menu->items;
	while(menuitem)
	{
		if (!menuitem->hidden)
		{
			if (menuitem->isselectable)
			{
				menuitem->selectindex = i;
				menu->selectableitemarray[i++] = menuitem;
			}

			if (menu->cursor_right)
				menuitem->x = labelstart + (labelwidth - menuitem->labellen) * 8;
			else
				menuitem->x = labelstart;
			menuitem->y = y;

			y += 8;
		}

		menuitem = menuitem->next;
	}

	menu->cursor_y = 32;

	menu->extra_x = menu->cursor_x + 16;
	menu->extra_width = 320 - menu->extra_x - 8;
}

static void Menu_Draw(struct Menu *menu)
{
	struct MenuItem *menuitem;

	if (menu == 0 || menu->error)
		return;

	menuitem = menu->items;
	while(menuitem)
	{
		if (!menuitem->hidden)
		{
			if (menuitem->whitelabel)
				M_PrintWhite(menuitem->x, menuitem->y, menuitem->label);
			else
				M_Print(menuitem->x, menuitem->y, menuitem->label);

			menuitem->draw(menuitem, menu->extra_x, menu->extra_width);
		}

		menuitem = menuitem->next;
	}

	M_DrawCharacter(menu->cursor_x, menu->selectableitemarray[menu->cursor]->y, 12 + ((int)(curtime * 4) & 1));
}

static void Menu_ShowItem(struct Menu *menu, struct MenuItem *menuitem)
{
	if (menu == 0 || menuitem == 0)
		return;

	menuitem->hidden = 0;

	Menu_Layout(menu);

	if (menuitem->isselectable && menuitem->selectindex <= menu->cursor)
		menu->cursor++;
}

static void Menu_HideItem(struct Menu *menu, struct MenuItem *menuitem)
{
	if (menu == 0 || menuitem == 0)
		return;

	menuitem->hidden = 1;

	if (menuitem->isselectable && menu->cursor != 0 && menuitem->selectindex <= menu->cursor)
		menu->cursor--;

	Menu_Layout(menu);
}

static void Menu_HandleKey(struct Menu *menu, int key)
{
	switch(key)
	{
		case K_BACKSPACE:
			m_topmenu = m_none;	// intentional fallthrough
		case K_ESCAPE:
			M_LeaveMenu(menu?menu->prevmenu:m_main);
			break;
	}

	if (menu == 0 || menu->error)
		return;

	switch(key)
	{
		case K_HOME:
		case K_PGUP:
			menu->cursor = 0;

			break;

		case K_END:
		case K_PGDN:
			menu->cursor = menu->numselectableitems - 1;

			break;

		case K_UPARROW:
			if (menu->cursor == 0)
				menu->cursor = menu->numselectableitems - 1;
			else
				menu->cursor--;

			break;

		case K_DOWNARROW:
			if (menu->cursor == menu->numselectableitems - 1)
				menu->cursor = 0;
			else
				menu->cursor++;

			break;

		default:
			menu->selectableitemarray[menu->cursor]->handlekey(menu->selectableitemarray[menu->cursor], key);

			return;
	}

	S_LocalSound("misc/menu1.wav");
}

static int MenuItem_SetLabel(struct MenuItem *menuitem, const char *label, int white)
{
	unsigned int labellen;

	labellen = strlen(label);
	menuitem->label = malloc(labellen + 1);
	if (menuitem->label)
	{
		menuitem->labellen = labellen;
		menuitem->whitelabel = white;
		memcpy(menuitem->label, label, labellen + 1);

		return 1;
	}

	return 0;
}

static void Menu_AddItem(struct Menu *menu, struct MenuItem *menuitem)
{
	struct MenuItem *mi;

	if (menuitem == 0)
	{
		menu->error = 1;
		return;
	}

	menuitem->next = 0;

	if (menu->items)
	{
		mi = menu->items;
		while(mi->next)
			mi = mi->next;

		mi->next = menuitem;
	}
	else
	{
		menu->items = menuitem;
	}
}

static void MenuItem_Delete(struct MenuItem *menuitem)
{
	free(menuitem->label);
	free(menuitem);
}

static void MenuItemButton_HandleKey(struct MenuItem *menuitem, int key)
{
	struct MenuItemButton *menuitembutton;

	menuitembutton = (struct MenuItemButton *)menuitem;

	if (key == K_ENTER)
	{
		S_LocalSound("misc/menu2.wav");
		menuitembutton->callback();
	}
}

static void MenuItemButton_Draw(struct MenuItem *menuitem, unsigned int extra_x, unsigned int extra_width)
{
}

static struct MenuItem *MenuItemButton_Create(const char *label, int white, void (*callback)(void))
{
	struct MenuItemButton *menuitembutton;

	menuitembutton = malloc(sizeof(*menuitembutton));
	if (menuitembutton)
	{
		if (MenuItem_SetLabel((struct MenuItem *)menuitembutton, label, white))
		{
			menuitembutton->menuitem.delete = MenuItem_Delete;
			menuitembutton->menuitem.draw = MenuItemButton_Draw;
			menuitembutton->menuitem.handlekey = MenuItemButton_HandleKey;
			menuitembutton->menuitem.isselectable = 1;
			menuitembutton->menuitem.hidden = 0;

			menuitembutton->callback = callback;

			return (struct MenuItem *)menuitembutton;
		}

		free(menuitembutton);
	}

	return 0;
}

static void MenuItemCvarRange_Draw(struct MenuItem *menuitem, unsigned int extra_x, unsigned int extra_width)
{
	struct MenuItemCvarRange *menuitemcvarrange;
	int i;
	float position;

	menuitemcvarrange = (struct MenuItemCvarRange *)menuitem;

	position = (bound(menuitemcvarrange->minvalue, menuitemcvarrange->cvar->value, menuitemcvarrange->maxvalue) - menuitemcvarrange->minvalue) / (menuitemcvarrange->maxvalue - menuitemcvarrange->minvalue);
	if (menuitemcvarrange->step < 0)
		position = 1-position;

	M_DrawCharacter(extra_x, menuitemcvarrange->menuitem.y, 128);
	for(i=0;i<(extra_width/8)-2;i++)
		M_DrawCharacter(extra_x + 8 + i*8, menuitemcvarrange->menuitem.y, 129);
	M_DrawCharacter(extra_x + 8 + i*8, menuitemcvarrange->menuitem.y, 130);
	M_DrawCharacter(extra_x + 8 + (position * ((i-1)*8)), menuitemcvarrange->menuitem.y, 131);
}

static void MenuItemCvarRange_HandleKey(struct MenuItem *menuitem, int key)
{
	struct MenuItemCvarRange *menuitemcvarrange;

	menuitemcvarrange = (struct MenuItemCvarRange *)menuitem;

	switch(key)
	{
		case K_ENTER:
		case K_RIGHTARROW:
			Cvar_SetValue(menuitemcvarrange->cvar, bound(menuitemcvarrange->minvalue, menuitemcvarrange->cvar->value + menuitemcvarrange->step, menuitemcvarrange->maxvalue));
			break;

		case K_LEFTARROW:
			Cvar_SetValue(menuitemcvarrange->cvar, bound(menuitemcvarrange->minvalue, menuitemcvarrange->cvar->value - menuitemcvarrange->step, menuitemcvarrange->maxvalue));
			break;

		default:
			return;
	}

	S_LocalSound("misc/menu3.wav");
}

static struct MenuItem *MenuItemCvarRange_Create(const char *label, float minvalue, float maxvalue, float step, cvar_t *cvar)
{
	struct MenuItemCvarRange *menuitemcvarrange;

	menuitemcvarrange = malloc(sizeof(*menuitemcvarrange));
	if (menuitemcvarrange)
	{
		if (MenuItem_SetLabel((struct MenuItem *)menuitemcvarrange, label, 0))
		{
			menuitemcvarrange->menuitem.delete = MenuItem_Delete;
			menuitemcvarrange->menuitem.draw = MenuItemCvarRange_Draw;
			menuitemcvarrange->menuitem.handlekey = MenuItemCvarRange_HandleKey;
			menuitemcvarrange->menuitem.isselectable = 1;
			menuitemcvarrange->menuitem.hidden = 0;

			menuitemcvarrange->minvalue = minvalue;
			menuitemcvarrange->maxvalue = maxvalue;
			menuitemcvarrange->step = step;
			menuitemcvarrange->cvar = cvar;

			return (struct MenuItem *)menuitemcvarrange;
		}

		free(menuitemcvarrange);
	}

	return 0;
}

static void MenuItemCvarBoolean_Draw(struct MenuItem *menuitem, unsigned int extra_x, unsigned int extra_width)
{
	struct MenuItemCvarBoolean *menuitemcvarboolean;

	menuitemcvarboolean = (struct MenuItemCvarBoolean *)menuitem;

	M_Print(extra_x + 8, menuitem->y, ((!!menuitemcvarboolean->cvar->value)^menuitemcvarboolean->invert)?"on":"off");
}

static void MenuItemCvarBoolean_HandleKey(struct MenuItem *menuitem, int key)
{
	struct MenuItemCvarBoolean *menuitemcvarboolean;

	menuitemcvarboolean = (struct MenuItemCvarBoolean *)menuitem;

	switch(key)
	{
		case K_ENTER:
		case K_RIGHTARROW:
		case K_LEFTARROW:
			S_LocalSound("misc/menu3.wav");
			Cvar_SetValue(menuitemcvarboolean->cvar, !menuitemcvarboolean->cvar->value);
			break;
	}
}

static struct MenuItem *MenuItemCvarBoolean_Create(const char *label, cvar_t *cvar, int invert)
{
	struct MenuItemCvarBoolean *menuitemcvarboolean;

	menuitemcvarboolean = malloc(sizeof(*menuitemcvarboolean));
	if (menuitemcvarboolean)
	{
		if (MenuItem_SetLabel((struct MenuItem *)menuitemcvarboolean, label, 0))
		{
			menuitemcvarboolean->menuitem.delete = MenuItem_Delete;
			menuitemcvarboolean->menuitem.draw = MenuItemCvarBoolean_Draw;
			menuitemcvarboolean->menuitem.handlekey = MenuItemCvarBoolean_HandleKey;
			menuitemcvarboolean->menuitem.isselectable = 1;
			menuitemcvarboolean->menuitem.hidden = 0;

			menuitemcvarboolean->cvar = cvar;
			menuitemcvarboolean->invert = invert;

			return (struct MenuItem *)menuitemcvarboolean;
		}

		free(menuitemcvarboolean);
	}

	return 0;
}

static void MenuItemCvarPosNegBoolean_Draw(struct MenuItem *menuitem, unsigned int extra_x, unsigned int extra_width)
{
	struct MenuItemCvarPosNegBoolean *menuitemcvarposnegboolean;

	menuitemcvarposnegboolean = (struct MenuItemCvarPosNegBoolean *)menuitem;

	M_Print(extra_x + 8, menuitem->y, menuitemcvarposnegboolean->cvar->value<0?"on":"off");
}

static void MenuItemCvarPosNegBoolean_HandleKey(struct MenuItem *menuitem, int key)
{
	struct MenuItemCvarPosNegBoolean *menuitemcvarposnegboolean;

	menuitemcvarposnegboolean = (struct MenuItemCvarPosNegBoolean *)menuitem;

	switch(key)
	{
		case K_ENTER:
		case K_RIGHTARROW:
		case K_LEFTARROW:
			S_LocalSound("misc/menu3.wav");
			Cvar_SetValue(menuitemcvarposnegboolean->cvar, -menuitemcvarposnegboolean->cvar->value);
			break;
	}
}

static struct MenuItem *MenuItemCvarPosNegBoolean_Create(const char *label, cvar_t *cvar)
{
	struct MenuItemCvarPosNegBoolean *menuitemcvarposnegboolean;

	menuitemcvarposnegboolean = malloc(sizeof(*menuitemcvarposnegboolean));
	if (menuitemcvarposnegboolean)
	{
		if (MenuItem_SetLabel((struct MenuItem *)menuitemcvarposnegboolean, label, 0))
		{
			menuitemcvarposnegboolean->menuitem.delete = MenuItem_Delete;
			menuitemcvarposnegboolean->menuitem.draw = MenuItemCvarPosNegBoolean_Draw;
			menuitemcvarposnegboolean->menuitem.handlekey = MenuItemCvarPosNegBoolean_HandleKey;
			menuitemcvarposnegboolean->menuitem.isselectable = 1;
			menuitemcvarposnegboolean->menuitem.hidden = 0;

			menuitemcvarposnegboolean->cvar = cvar;

			return (struct MenuItem *)menuitemcvarposnegboolean;
		}

		free(menuitemcvarposnegboolean);
	}

	return 0;
}

static void MenuItemCvarMultiSelect_Draw(struct MenuItem *menuitem, unsigned int extra_x, unsigned int extra_width)
{
	struct MenuItemCvarMultiSelect *menuitemcvarmultiselect;
	int value;

	menuitemcvarmultiselect = (struct MenuItemCvarMultiSelect *)menuitem;

	value = menuitemcvarmultiselect->cvar->value;
	if (value < 0 || value >= menuitemcvarmultiselect->numvalues)
		value = 0;

	M_Print(extra_x + 8, menuitem->y, menuitemcvarmultiselect->values[value]);
}

static void MenuItemCvarMultiSelect_HandleKey(struct MenuItem *menuitem, int key)
{
	struct MenuItemCvarMultiSelect *menuitemcvarmultiselect;
	int value;

	menuitemcvarmultiselect = (struct MenuItemCvarMultiSelect *)menuitem;

	value = menuitemcvarmultiselect->cvar->value;

	switch(key)
	{
		case K_ENTER:
		case K_RIGHTARROW:
			S_LocalSound("misc/menu2.wav");
			value++;
			if (value >= menuitemcvarmultiselect->numvalues)
				value = 0;

			Cvar_SetValue(menuitemcvarmultiselect->cvar, value);
			break;

		case K_LEFTARROW:
			S_LocalSound("misc/menu2.wav");
			value--;
			if (value < 0)
				value = menuitemcvarmultiselect->numvalues - 1;

			Cvar_SetValue(menuitemcvarmultiselect->cvar, value);
			break;
	}
}

static struct MenuItem *MenuItemCvarMultiSelect_Create(const char *label, cvar_t *cvar, const char * const *values)
{
	struct MenuItemCvarMultiSelect *menuitemcvarmultiselect;
	unsigned int i;

	menuitemcvarmultiselect = malloc(sizeof(*menuitemcvarmultiselect));
	if (menuitemcvarmultiselect)
	{
		if (MenuItem_SetLabel((struct MenuItem *)menuitemcvarmultiselect, label, 0))
		{
			menuitemcvarmultiselect->menuitem.delete = MenuItem_Delete;
			menuitemcvarmultiselect->menuitem.draw = MenuItemCvarMultiSelect_Draw;
			menuitemcvarmultiselect->menuitem.handlekey = MenuItemCvarMultiSelect_HandleKey;
			menuitemcvarmultiselect->menuitem.isselectable = 1;
			menuitemcvarmultiselect->menuitem.hidden = 0;

			menuitemcvarmultiselect->cvar = cvar;
			menuitemcvarmultiselect->values = values;

			for(i=0;values[i];i++);

			menuitemcvarmultiselect->numvalues = i;

			return (struct MenuItem *)menuitemcvarmultiselect;
		}

		free(menuitemcvarmultiselect);
	}

	return 0;
}

static struct MenuItem *MenuItemSpacer_Create()
{
	struct MenuItem *menuitem;

	menuitem = malloc(sizeof(*menuitem));
	if (menuitem)
	{
		if (MenuItem_SetLabel(menuitem, "", 0))
		{
			menuitem->delete = MenuItem_Delete;
			menuitem->draw = MenuItemButton_Draw;
			menuitem->isselectable = 0;
			menuitem->hidden = 0;

			return menuitem;
		}

		free(menuitem);
	}

	return 0;
}

//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	5


static void M_Menu_Main_f()
{
	M_EnterMenu (m_main);
}

static void M_Main_Draw()
{
	int f;

	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(ttl_mainpic, (320 - 96) / 2, 4, 96, 24);
	M_DrawPic(mainmenupic, 72, 32, 240, 112);

	f = (int)(curtime * 10)%6;
	M_DrawPic(menudotpic[f], 54, 32 + m_main_cursor * 20, 16, 24);
}

static void M_Main_Key(int key)
{
	switch (key)
	{
	case K_ESCAPE:
		key_dest = key_game;
		m_state = m_none;
		VID_SetMouseGrab(1);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_MultiPlayer_f ();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

	#if defined(_WIN32) || defined(__XMMS__)
		case 3:
			M_Menu_MP3_Control_f ();
			break;
	#endif

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}
}


//=============================================================================
/* OPTIONS MENU */

static void M_Menu_Options_f()
{
	M_EnterMenu (m_options);
}

static void M_Options_GoToConsole()
{
	m_state = m_none;
	key_dest = key_console;
}

static void M_Options_ResetToDefaults()
{
	Cbuf_AddText("exec default.cfg\n");
}

static void M_Options_SaveConfiguration()
{
	Cbuf_AddText("cfg_save default.cfg\n");
}

static void M_Options_Draw()
{
	static int fs = 2;
	int newfs;

	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(p_optionpic, (320 - 144) / 2, 4, 144, 24);

	newfs = VID_GetFullscreen();
	if (newfs != fs)
	{
		if (newfs)
			Menu_HideItem(optionsmenu, optionsmenu_usemouse);
		else
			Menu_ShowItem(optionsmenu, optionsmenu_usemouse);

		fs = newfs;
	}

	Menu_Draw(optionsmenu);
}

//=============================================================================
/* KEYS MENU */

static char *bindnames[][2] =
{
	{ "+attack",    "attack" },
	{ "+use",       "use" },
	{ "+jump",      "jump" },
	{ "+forward",   "move forward" },
	{ "+back",      "move back" },
	{ "+moveleft",  "move left" },
	{ "+moveright", "move right"},
	{ "+moveup",    "swim up" },
	{ "+movedown",  "swim down" },
	{ "impulse 12", "previous weapon" },
	{ "impulse 10", "next weapon" },
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

static void M_Menu_Keys_f()
{
	M_EnterMenu (m_keys);
}

qboolean Key_IsLeftRightSameBind(int b);

static void M_FindKeysForCommand(char *command, int *twokeys)
{
	int count, j, l;
	char *b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j = 0 ; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			if (count)
			{
				if (j == twokeys[0] + 1 && (twokeys[0] == K_LCTRL || twokeys[0] == K_LSHIFT || twokeys[0] == K_LALT))
				{

					twokeys[0]--;
					continue;
				}
			}
			twokeys[count] = j;
			count++;
			if (count == 2)
			{

				if (Key_IsLeftRightSameBind(twokeys[1]))
					twokeys[1]++;
				break;
			}
		}
	}
}

static void M_UnbindCommand(char *command)
{
	int j, l;
	char *b;

	l = strlen(command);

	for (j = 0; j < 256; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_Unbind (j);
	}
}


static void M_Keys_Draw()
{
	int x, y, i, l, keys[2];
	char *name;

	M_DrawPic(ttl_cstmpic, (320 - 184) / 2, 4, 184, 24);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, del to clear");

// search for known bindings
	for (i = 0; i < NUMCOMMANDS; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		l = strlen (bindnames[i][0]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (156, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (156, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (156 + x + 8, y, "or");
				M_Print (156 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (142, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (142, 48 + keys_cursor*8, 12+((int)(curtime*4)&1));
}


static void M_Keys_Key(int k)
{
	int keys[2];

	if (bind_grab)
	{
		// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
			bind_grab = false;
		else if (k != '`')
			Key_SetBinding (k, bindnames[keys_cursor][0]);

		bind_grab = false;
		return;
	}

	switch (k)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_options);
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor = NUMCOMMANDS - 1;
		break;

	case K_ENTER:		// go into bind mode
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}


//=============================================================================
/* FPS SETTINGS MENU */

static void M_Menu_Fps_f()
{
	M_EnterMenu (m_fps);
}

extern cvar_t cl_rocket2grenade;
extern cvar_t v_bonusflash;
extern cvar_t v_damagecshift;
extern cvar_t r_fastsky;
extern cvar_t r_drawflame;

static const char * const explosiontypevalues[] =
{
	"fire + sparks",
	"fire only",
	"teleport",
	"blood",
	"big blood",
	"dbl gunshot",
	"blob effect",
	"big explosion",
	0
};

static const char * const muzzleflashvalues[] =
{
	"off",
	"on",
	"own off",
	0
};

static const char * const deadbodyfiltervalues[] =
{
	"off",
	"on (normal)",
	"on (instant)",
	0
};

static const char * const rocket2grenadevalues[] =
{
	"normal",
	"grenade",
	0
};

static const char * const rockettrailvalues[] =
{
	"off",
	"normal",
	"grenade",
	"alt normal",
	"slight blood",
	"big blood",
	"tracer 1",
	"tracer 2",
	0
};

static const char * const powerupglowvalues[] =
{
	"off",
	"on",
	"own off",
	0
};

void M_Fps_FastMode()
{
	Cvar_SetValue(&r_explosiontype, 1);
	Cvar_SetValue(&r_explosionlight, 0);
	Cvar_SetValue(&cl_muzzleflash, 0);
	Cvar_SetValue(&cl_gibfilter, 1);
	Cvar_SetValue(&cl_deadbodyfilter, 1);
	Cvar_SetValue(&r_rocketlight, 0);
	Cvar_SetValue(&r_powerupglow, 0);
	Cvar_SetValue(&r_drawflame, 0);
	Cvar_SetValue(&r_fastsky, 1);
	Cvar_SetValue(&r_rockettrail, 1);
	Cvar_SetValue(&v_damagecshift, 0);
	Cvar_SetValue(&v_bonusflash, 0);
#ifdef GLQUAKE
	Cvar_SetValue(&gl_flashblend, 1);
	Cvar_SetValue(&r_dynamic, 0);
	Cvar_SetValue(&gl_part_explosions, 0);
	Cvar_SetValue(&gl_part_trails, 0);
	Cvar_SetValue(&gl_part_spikes, 0);
	Cvar_SetValue(&gl_part_gunshots, 0);
	Cvar_SetValue(&gl_part_blood, 0);
	Cvar_SetValue(&gl_part_telesplash, 0);
	Cvar_SetValue(&gl_part_blobs, 0);
	Cvar_SetValue(&gl_part_lavasplash, 0);
	Cvar_SetValue(&gl_part_inferno, 0);
#endif
}

void M_Fps_HighQuality()
{
	Cvar_SetValue(&r_explosiontype, 0);
	Cvar_SetValue(&r_explosionlight, 1);
	Cvar_SetValue(&cl_muzzleflash, 1);
	Cvar_SetValue(&cl_gibfilter, 0);
	Cvar_SetValue(&cl_deadbodyfilter, 0);
	Cvar_SetValue(&r_rocketlight, 1);
	Cvar_SetValue(&r_powerupglow, 2);
	Cvar_SetValue(&r_drawflame, 1);
	Cvar_SetValue(&r_fastsky, 0);
	Cvar_SetValue(&r_rockettrail, 1);
	Cvar_SetValue(&v_damagecshift, 1);
	Cvar_SetValue(&v_bonusflash, 1);
#ifdef GLQUAKE
	Cvar_SetValue(&gl_flashblend, 0);
	Cvar_SetValue(&r_dynamic, 1);
	Cvar_SetValue(&gl_part_explosions, 1);
	Cvar_SetValue(&gl_part_trails, 1);
	Cvar_SetValue(&gl_part_spikes, 1);
	Cvar_SetValue(&gl_part_gunshots, 1);
	Cvar_SetValue(&gl_part_blood, 1);
	Cvar_SetValue(&gl_part_telesplash, 1);
	Cvar_SetValue(&gl_part_blobs, 1);
	Cvar_SetValue(&gl_part_lavasplash, 1);
	Cvar_SetValue(&gl_part_inferno, 1);
#endif
}

static void M_Fps_Draw()
{
	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(ttl_cstmpic, (320 - 184) / 2, 4, 184, 24);

	Menu_Draw(fpsmenu);
}

//=============================================================================
/* VIDEO MENU */

extern cvar_t vid_width;
extern cvar_t vid_height;
extern cvar_t vid_mode;
extern cvar_t vid_fullscreen;

static char *video_verify_oldcvar_width;
static char *video_verify_oldcvar_height;
static char *video_verify_oldcvar_fullscreen;
static char *video_verify_oldcvar_mode;

static unsigned int video_verify_oldactive_width;
static unsigned int video_verify_oldactive_height;
static qboolean video_verify_oldactive_fullscreen;
static char *video_verify_oldactive_mode;

static char *video_selectedmodeline;

static double video_verify_fail_time;

static void M_Menu_Video_Verify_Revert()
{
	Cvar_SetValue(&vid_width, video_verify_oldactive_width);
	Cvar_SetValue(&vid_height, video_verify_oldactive_height);
	Cvar_SetValue(&vid_fullscreen, video_verify_oldactive_fullscreen);
	Cvar_Set(&vid_mode, video_verify_oldactive_mode);
	VID_Restart();
	Cvar_Set(&vid_width, video_verify_oldcvar_width);
	Cvar_Set(&vid_height, video_verify_oldcvar_height);
	Cvar_Set(&vid_fullscreen, video_verify_oldcvar_fullscreen);
	Cvar_Set(&vid_mode, video_verify_oldcvar_mode);
}

static void M_Menu_Video_Verify_Cleanup()
{
	free(video_verify_oldcvar_width);
	free(video_verify_oldcvar_height);
	free(video_verify_oldcvar_fullscreen);
	free(video_verify_oldcvar_mode);
	free(video_verify_oldactive_mode);
}

void M_Menu_Video_Verify_f()
{
	video_verify_oldcvar_width = strdup(vid_width.string);
	video_verify_oldcvar_height = strdup(vid_height.string);
	video_verify_oldcvar_fullscreen = strdup(vid_fullscreen.string);
	video_verify_oldcvar_mode = strdup(vid_mode.string);

	video_verify_oldactive_width = VID_GetWidth();
	video_verify_oldactive_height = VID_GetHeight();
	video_verify_oldactive_fullscreen = VID_GetFullscreen();
	video_verify_oldactive_mode = strdup(VID_GetMode());

	if (video_verify_oldcvar_width == 0
	 || video_verify_oldcvar_height == 0
	 || video_verify_oldcvar_fullscreen == 0
	 || video_verify_oldcvar_mode == 0
	 || video_verify_oldactive_mode == 0)
	{
		M_Menu_Video_Verify_Cleanup();
		return;
	}

	Cvar_Set(&vid_mode, video_selectedmodeline);
	Cvar_SetValue(&vid_fullscreen, 1);
	VID_Restart();

	video_verify_fail_time = curtime + 15;

	M_EnterMenu(m_video_verify);
}

void M_Video_Verify_Draw()
{
	int timeremaining;
	char buf[40];
	unsigned int middle;

	if (scr_centerMenu.value)
		middle = 100;
	else
		middle = 36+8;

	M_Print(160-(32*4), middle-36, "Testing your selected video mode");
	M_Print(160-(38*4), middle-28, "If you can see this text, then press y");
	M_Print(160-(36*4), middle-20, "to verify that this video mode works");

	timeremaining = video_verify_fail_time - curtime;
	if (timeremaining < 0)
		timeremaining = 0;

	snprintf(buf, sizeof(buf), "Reverting in %d seconds", timeremaining);

	M_PrintWhite(160-(strlen(buf)*4), middle-4, buf);

	M_Print(160-(35*4), middle+12, "Press escape or n to cancel setting");
	M_Print(160-(15*4), middle+20, "this video mode");

}

void M_Video_Verify_Key(int key)
{
	switch(key)
	{
		case K_BACKSPACE:
		case K_ESCAPE:
		case 'n':
			M_Menu_Video_Verify_Revert();
			M_Menu_Video_Verify_Cleanup();
			M_LeaveMenu(m_video);
			break;

		case 'y':
			M_Menu_Video_Verify_Cleanup();
			M_LeaveMenu(m_video);
			break;
	}
}

struct
{
	unsigned int width;
	unsigned int height;
} static const windowedresolutions[] =
{
	{ 320, 200 },
	{ 320, 240 },
	{ 512, 384 },
	{ 640, 400 },
	{ 640, 480 },
	{ 800, 600 },
	{ 1024, 768 },
	{ 1280, 800 },
	{ 1280, 1024 },
	{ 1440, 900 },
	{ 1600, 1200 },
	{ 1680, 1050 },
	{ 1920, 1080 },
	{ 2560, 1440 },
};

static const char *windowedresolutionnames[] =
{
	"320x200",
	"320x240",
	"512x384",
	"640x400",
	"640x480",
	"800x600",
	"1024x768",
	"1280x800",
	"1280x1024",
	"1440x900",
	"1600x1200",
	"1680x1050",
	"1920x1080",
	"2560x1440",
};

#define NUMWINDOWEDRESOLUTIONS (sizeof(windowedresolutions)/sizeof(*windowedresolutions))

static unsigned int video_fullscreenmodecursor;
static unsigned int video_fullscreenmodelistbegin;
static unsigned int video_windowmodecursor;
static unsigned int video_windowmodelistbegin;
static unsigned int video_typenum;

void M_Menu_Video_f()
{
	M_EnterMenu (m_video);

	if (vid_fullscreen.value)
	{
		if (*vid_mode.string)
			video_typenum = 2;
		else
			video_typenum = 1;
	}
	else
		video_typenum = 0;

	video_fullscreenmodelistbegin = 0;
	video_windowmodelistbegin = 0;
}

static void M_Video_Draw()
{
	const char * const *vidmodes;
	const char *t;
	char modestring[40];
	unsigned int i;
	unsigned int j;
	unsigned int maxwidth;
	unsigned int bottom;
	unsigned int modelines;
	const char *curmode;
	static const char *displaytypes[] =
	{
		"Windowed",
		"Fullscreen, clone desktop",
		"Fullscreen, custom mode"
	};

	if (scr_centerMenu.value)
		bottom = 200;
	else
		bottom = menuheight;

	M_DrawPic(vidmodespic, (320 - 216) / 2, 4, 216, 24);

	curmode = VID_GetMode();

	M_Print(160-(6*8)-4, 36, "Current mode:");
	if (VID_GetFullscreen())
	{
		strlcpy(modestring, "Fullscreen, unknown mode", sizeof(modestring));

		if (curmode && *curmode)
		{
			t = Sys_Video_GetModeDescription(curmode);
			if (t)
			{
				strlcpy(modestring, t, sizeof(modestring));

				Sys_Video_FreeModeDescription(t);
			}
		}
	}
	else
	{
		snprintf(modestring, sizeof(modestring), "Windowed, %dx%d", VID_GetWidth(), VID_GetHeight());
	}

	M_Print(160-(strlen(modestring)*4), 44, modestring);

	M_Print(160-(6*8)-4, 60, "Display type:");
	M_Print(160-((strlen(displaytypes[video_typenum])*8)/2), 68, displaytypes[video_typenum]);

	modelines = (bottom-32-92)/8;

	if (video_typenum == 0)
	{
		if (video_windowmodecursor >= NUMWINDOWEDRESOLUTIONS)
		{
			video_windowmodecursor = NUMWINDOWEDRESOLUTIONS - 1;
		}

		if (video_windowmodecursor < video_windowmodelistbegin)
			video_windowmodelistbegin = video_windowmodecursor;
		else if (video_windowmodecursor >= modelines + video_windowmodelistbegin)
			video_windowmodelistbegin = video_windowmodecursor - modelines + 1;

		maxwidth = 0 /* 16 */;
		for(i=0;i<NUMWINDOWEDRESOLUTIONS;i++)
		{
			if (strlen(windowedresolutionnames[i]) > maxwidth)
				maxwidth = strlen(windowedresolutionnames[i]);
		}

		M_Print(160-((16*8)/2), 84, "Available modes:");
		for(i=video_windowmodelistbegin,j=0;i<NUMWINDOWEDRESOLUTIONS && j<modelines;i++,j++)
		{
			if (VID_GetFullscreen() == 0 && VID_GetWidth() == windowedresolutions[i].width && VID_GetHeight() == windowedresolutions[i].height)
				M_PrintWhite(160-((maxwidth*8)/2), 92+j*8, windowedresolutionnames[i]);
			else
				M_Print(160-((maxwidth*8)/2), 92+j*8, windowedresolutionnames[i]);
		}

		M_DrawCharacter(160-((maxwidth*8)/2)-16, 92 + (video_windowmodecursor - video_windowmodelistbegin) * 8, 12 + ((int)(curtime * 4) & 1));
	}
	else if (video_typenum == 2)
	{
		vidmodes = Sys_Video_GetModeList();
		if (vidmodes)
		{
			maxwidth = 16;
			for(i=0;vidmodes[i];i++)
			{
				t = Sys_Video_GetModeDescription(vidmodes[i]);
				if (t)
				{
					if (strlen(t) > maxwidth)
						maxwidth = strlen(t);

					Sys_Video_FreeModeDescription(t);
				}
			}

			if (video_fullscreenmodecursor >= i)
			{
				if (i > 0)
					video_fullscreenmodecursor = i - 1;
				else
					video_fullscreenmodecursor = 0;
			}

			if (video_fullscreenmodecursor < video_fullscreenmodelistbegin)
				video_fullscreenmodelistbegin = video_fullscreenmodecursor;
			else if (video_fullscreenmodecursor >= modelines + video_fullscreenmodelistbegin)
				video_fullscreenmodelistbegin = video_fullscreenmodecursor - modelines + 1;

			if (maxwidth > (menuwidth/8)-4)
				maxwidth = (menuwidth/8)-4;

			M_Print(160-((maxwidth*8)/2), 84, "Available modes:");
			for(i=video_fullscreenmodelistbegin,j=0;vidmodes[i] && j<modelines;i++,j++)
			{
				t = Sys_Video_GetModeDescription(vidmodes[i]);
				if (t)
				{
					if (strcmp(curmode, vidmodes[i]) == 0)
						M_PrintWhite(160-((maxwidth*8)/2), 92+j*8, t);
					else
						M_Print(160-((maxwidth*8)/2), 92+j*8, t);

					Sys_Video_FreeModeDescription(t);
				}
				else
					M_Print(160-((maxwidth*8)/2), 92+j*8, "Unknown");
			}

			free(video_selectedmodeline);
			video_selectedmodeline = strdup(vidmodes[video_fullscreenmodecursor]);

			Sys_Video_FreeModeList(vidmodes);

			if (video_fullscreenmodecursor < video_fullscreenmodelistbegin)
				video_fullscreenmodelistbegin = video_fullscreenmodecursor;
			else if (video_fullscreenmodecursor >= modelines + video_fullscreenmodelistbegin)
				video_fullscreenmodelistbegin = video_fullscreenmodecursor - modelines + 1;

			M_DrawCharacter(160-((maxwidth*8)/2)-16, 92 + (video_fullscreenmodecursor - video_fullscreenmodelistbegin) * 8, 12 + ((int)(curtime * 4) & 1));
		}
	}

	M_Print(160-(37*4), bottom-24, "Left/right arrows change display type");
	M_Print(160-(35*4), bottom-16, "Up/down arrows scroll the mode list");
	M_Print(160-(20*4), bottom-8, "Enter selects a mode");
}

static void M_Video_Key(int key)
{
	switch(key)
	{
		case K_BACKSPACE:
			m_topmenu = m_none;	// intentional fallthrough
		case K_ESCAPE:
			free(video_selectedmodeline);
			video_selectedmodeline = 0;
			M_LeaveMenu(m_options);
			break;

		case K_UPARROW:
			S_LocalSound ("misc/menu1.wav");

			if (video_typenum == 0)
			{
				if (video_windowmodecursor)
					video_windowmodecursor--;
			}
			else if (video_typenum == 2)
			{
				if (video_fullscreenmodecursor)
					video_fullscreenmodecursor--;
			}
			break;
		case K_DOWNARROW:
			S_LocalSound ("misc/menu1.wav");

			if (video_typenum == 0 && video_windowmodecursor < NUMWINDOWEDRESOLUTIONS - 1)
				video_windowmodecursor++;
			else if (video_typenum == 2)
				video_fullscreenmodecursor++;
			break;

		case K_LEFTARROW:
			S_LocalSound ("misc/menu1.wav");

			if (video_typenum)
				video_typenum--;
			else
				video_typenum = 2;
			break;

		case K_RIGHTARROW:
			S_LocalSound ("misc/menu1.wav");

			video_typenum++;
			if (video_typenum > 2)
				video_typenum = 0;

			break;

		case K_ENTER:
			m_entersound = true;

			if (video_typenum == 0)
			{
				if (!(VID_GetFullscreen() == 0 && VID_GetWidth() == windowedresolutions[video_windowmodecursor].width && VID_GetHeight() == windowedresolutions[video_windowmodecursor].height))
				{
					Cvar_SetValue(&vid_width, windowedresolutions[video_windowmodecursor].width);
					Cvar_SetValue(&vid_height, windowedresolutions[video_windowmodecursor].height);
					Cvar_SetValue(&vid_fullscreen, 0);
					Cbuf_AddText("vid_restart\n");
				}
			}
			else if (video_typenum == 1)
			{
				Cvar_SetValue(&vid_fullscreen, 1);
				Cvar_Set(&vid_mode, "");
				Cbuf_AddText("vid_restart\n");
			}
			else if (video_typenum == 2 && video_selectedmodeline)
			{
				M_Menu_Video_Verify_f();
			}
			break;
	}
}

//=============================================================================
/* HELP MENU */

int		help_page;

static void M_Menu_Help_f()
{
	M_EnterMenu (m_help);
	help_page = 0;
}

static void M_Help_Draw()
{
	M_DrawPic(helppic[help_page], 0, 0, 320, 200);
}

static void M_Help_Key(int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

void M_Menu_Quit_f()
{
	extern cvar_t cl_confirmquit;

	if (cl_confirmquit.value)
	{
		if (m_state == m_quit)
			return;
		wasInMenus = (key_dest == key_menu);
		m_quit_prevstate = m_state;
		msgNumber = rand()&7;
		M_EnterMenu (m_quit);
	}
	else
	{
		Host_Quit();
	}
}

static void M_Quit_Key(int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
			VID_SetMouseGrab(1);
		}
		break;

	case K_ENTER:
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit ();
		break;

	default:
		break;
	}
}

//=============================================================================
/* SINGLE PLAYER MENU */

#ifndef CLIENTONLY

#define	SINGLEPLAYER_ITEMS	3
int	m_singleplayer_cursor;
qboolean m_singleplayer_confirm;
qboolean m_singleplayer_notavail;

extern	cvar_t	maxclients;

static void M_Menu_SinglePlayer_f()
{
	M_EnterMenu (m_singleplayer);
	m_singleplayer_confirm = false;
	m_singleplayer_notavail = false;
}

static void M_SinglePlayer_Draw()
{
	int f;

	M_DrawPic(ttl_sglpic, (320 - 128) / 2, 4, 128, 24);
	M_DrawTextBox(48, 10*8, 25, 5);
	M_PrintWhite(68+16, 12*8, "Single player games");
	M_PrintWhite(68+8, 13*8, "currently do not work");
	M_PrintWhite(68, 14*8, "Please check back later");
	return;

	if (m_singleplayer_notavail)
	{
		M_DrawPic(ttl_sglpic, (320 - 128) / 2, 4, 128, 24);
		M_DrawTextBox (60, 10*8, 24, 4);
		M_PrintWhite (80, 12*8, " Cannot start a game");
		M_PrintWhite (80, 13*8, "spprogs.dat not found");
		return;
	}

	if (m_singleplayer_confirm)
	{
		M_PrintWhite (64, 11*8, "Are you sure you want to");
		M_PrintWhite (64, 12*8, "    start a new game?");
		return;
	}

	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(ttl_sglpic, (320 - 128) / 2, 4, 128, 24);
	M_DrawPic(sp_menupic, 72, 32, 232, 64);

	f = (int)(curtime * 10)%6;
	M_DrawPic(menudotpic[f], 54, 32 + m_main_cursor * 20, 16, 24);
}

static void CheckSPGame ()
{
	m_singleplay_notavail = !FS_FileExists("spprogs.dat");
}

extern int file_from_gamedir;

static void StartNewGame ()
{
#if 0
	key_dest = key_game;
	Cvar_Set (&maxclients, "1");
	Cvar_Set (&teamplay, "0");
	Cvar_Set (&deathmatch, "0");
	Cvar_Set (&coop, "0");

	if (com_serveractive)
		Cbuf_AddText ("disconnect\n");


	progs = (dprograms_t *) FS_LoadHunkFile ("spprogs.dat");
	if (progs && !file_from_gamedir)
		Cbuf_AddText ("gamedir qw\n");
	Cbuf_AddText ("map start\n");
#endif
}

static void M_SinglePlayer_Key(int key)
{
	if (m_singleplayer_notavail)
	{
		switch (key)
		{
		case K_BACKSPACE:
		case K_ESCAPE:
		case K_ENTER:
			m_singleplayer_notavail = false;
			break;
		}
		return;
	}

	if (m_singleplayer_confirm)
	{
		if (key == K_ESCAPE || key == 'n')
		{
			m_singleplayer_confirm = false;
			m_entersound = true;
		}
		else if (key == 'y' || key == K_ENTER)
		{
			StartNewGame ();
		}
		return;
	}

	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_main);
		break;

#if 0
	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_HOME:
	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = 0;
		break;

	case K_END:
	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
		switch (m_singleplayer_cursor)
		{
		case 0:
			CheckSPGame ();
			if (m_singleplayer_notavail)
			{
				m_entersound = true;
				return;
			}
			if (com_serveractive)
			{
				// bring up confirmation dialog
				m_singleplayer_confirm = true;
				m_entersound = true;
			}
			else
			{
				StartNewGame ();
			}
			break;

		case 1:
			M_Menu_Load_f ();
			break;

		case 2:
			M_Menu_Save_f ();
			break;
		}
#endif
	}
}

#else	// !CLIENTONLY

static void M_Menu_SinglePlayer_f()
{
	M_EnterMenu (m_singleplayer);
}

static void M_SinglePlayer_Draw()
{
	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(ttl_sglpic, (320 - 128) / 2, 4, 128, 24);
//	M_DrawPic(sp_menupic, 72, 32, 232, 64);

	M_DrawTextBox (60, 10*8, 23, 4);
	M_PrintWhite (88, 12*8, "This client is for");
	M_PrintWhite (88, 13*8, "Internet play only");
}

static void M_SinglePlayer_Key(key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
	case K_ENTER:
		M_LeaveMenu (m_main);
		break;
	}
}
#endif	// CLIENTONLY


//=============================================================================
/* LOAD/SAVE MENU */

#ifndef CLIENTONLY

#define	MAX_SAVEGAMES		12

int		load_cursor;		// 0 < load_cursor < MAX_SAVEGAMES
char	m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
int		loadable[MAX_SAVEGAMES];

static void M_ScanSaves(char *sp_gamedir)
{
	int i, j, version;
	char name[MAX_OSPATH];
	FILE *f;

	for (i = 0; i < MAX_SAVEGAMES; i++)
	{
		strcpy (m_filenames[i], "--- UNUSED SLOT ---");
		loadable[i] = false;
		snprintf(name, sizeof(name), "%s/save/s%i.sav", sp_gamedir, i);
		if (!(f = fopen (name, "r")))
			continue;
		fscanf (f, "%i\n", &version);
		fscanf (f, "%79s\n", name);
		Q_strncpyz (m_filenames[i], name, sizeof(m_filenames[i]));

		// change _ back to space
		for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
			if (m_filenames[i][j] == '_')
				m_filenames[i][j] = ' ';
		loadable[i] = true;
		fclose (f);
	}
}

static void M_Menu_Load_f()
{
	FILE *f;

	if (!FS_FileExists("spprogs.dat"))
		return;

	M_EnterMenu(m_load);
	M_ScanSaves(com_gamedir);
}

static void M_Menu_Save_f()
{
	if (sv.state != ss_active)
		return;
	if (cl.intermission)
		return;

	M_EnterMenu (m_save);
	M_ScanSaves (com_gamedir);
}

static void M_Load_Draw()
{
	int i;

	M_DrawPic(p_loadpic, (320 - 104) >> 1, 4, 104, 24);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8*i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, 12 + ((int)(curtime * 4) & 1));
}

static void M_Save_Draw()
{
	int i;

	M_DrawPic(p_savepic, (320 - 88) >> 1, 4, 88, 24);

	for (i = 0; i < MAX_SAVEGAMES; i++)
		M_Print (16, 32 + 8 * i, m_filenames[i]);

// line cursor
	M_DrawCharacter (8, 32 + load_cursor * 8, 12 + ((int)(curtime * 4) & 1));
}

static void M_Load_Key(int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_singleplayer);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
		if (!loadable[load_cursor])
			return;
		m_state = m_none;
		key_dest = key_game;
		VID_SetMouseGrab(1);

#if 0
		// issue the load command
		if (FS_LoadHunkFile ("spprogs.dat") && !file_from_gamedir)
			Cbuf_AddText("disconnect; gamedir qw\n");
		Cbuf_AddText (va ("load s%i\n", load_cursor) );
#endif
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

static void M_Save_Key(int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_singleplayer);
		break;

	case K_ENTER:
		m_state = m_none;
		key_dest = key_game;
		VID_SetMouseGrab(1);
		Cbuf_AddText (va("save s%i\n", load_cursor));
		return;

	case K_UPARROW:
	case K_LEFTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor--;
		if (load_cursor < 0)
			load_cursor = MAX_SAVEGAMES-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		load_cursor++;
		if (load_cursor >= MAX_SAVEGAMES)
			load_cursor = 0;
		break;
	}
}

#endif

//=============================================================================
/* MULTIPLAYER MENU */

static void M_Menu_MultiPlayer_f()
{
	M_EnterMenu (m_multiplayer);
}

static void M_MultiPlayer_ServerBrowser()
{
	key_dest = key_game;
	m_state = m_none;
	VID_SetMouseGrab(1);
	SB_Activate_f();
}

void M_MultiPlayer_Draw()
{
	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(p_multipic, (320 - 216) / 2, 4, 216, 24);

	Menu_Draw(multiplayermenu);
}

//=============================================================================

#if _WIN32 || defined(__XMMS__)

#define M_MP3_CONTROL_HEADINGROW	8
#define M_MP3_CONTROL_MENUROW		(M_MP3_CONTROL_HEADINGROW + 56)
#define M_MP3_CONTROL_COL			104
#define M_MP3_CONTROL_NUMENTRIES	12
#define M_MP3_CONTROL_BARHEIGHT		(200 - 24)

static int mp3_cursor = 0;
static int last_status;

void MP3_Menu_DrawInfo(void);
static void M_Menu_MP3_Playlist_f(void);

static void M_MP3_Control_Draw()
{
	char songinfo_scroll[38 + 1], *s = NULL;
	int i, scroll_index, print_time;
	float frac, elapsed, realtime;

	static char lastsonginfo[128], last_title[128];
	static float initial_time;
	static int last_length, last_elapsed, last_total, last_shuffle, last_repeat;


	M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " CONTROL")) >> 1, M_MP3_CONTROL_HEADINGROW, MP3_PLAYERNAME_ALLCAPS " CONTROL");

	M_Print (8, M_MP3_CONTROL_HEADINGROW + 16, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");


	if (!MP3_IsActive())
	{
		M_PrintWhite((320 - 24 * 8) >> 1, M_MP3_CONTROL_HEADINGROW + 40, "XMMS LIBRARIES NOT FOUND");
		return;
	}

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Stopped");
	else
		M_PrintWhite(312 - 11 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Not Running");

	if (last_status == MP3_NOTRUNNING)
	{
		M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " is not running")) >> 1, 40, MP3_PLAYERNAME_LEADINGCAP " is not running");
		M_PrintWhite (56, 72, "Press");
		M_Print (56 + 48, 72, "ENTER");
		M_PrintWhite (56 + 48 + 48, 72, "to start " MP3_PLAYERNAME_NOCAPS);
		M_PrintWhite (56, 84, "Press");
		M_Print (56 + 48, 84, "ESC");
		M_PrintWhite (56 + 48 + 32, 84, "to exit this menu");
		M_Print (16, 116, "The variable");
		M_PrintWhite (16 + 104, 116, mp3_dir.name);
		M_Print (16 + 104 + 8 * (strlen(mp3_dir.name) + 1), 116, "needs to");
		M_Print (20, 124, "be set to the path for " MP3_PLAYERNAME_NOCAPS " first");
		return;
	}

	s = MP3_Menu_SongtTitle();
	if (!strcmp(last_title, s = MP3_Menu_SongtTitle()))
	{
		elapsed = 3.5 * max(realtime - initial_time - 0.75, 0);
		scroll_index = (int) elapsed;
		frac = bound(0, elapsed - scroll_index, 1);
		scroll_index = scroll_index % last_length;
	}
	else
	{
		snprintf(lastsonginfo, sizeof(lastsonginfo), "%s  ***  ", s);
		Q_strncpyz(last_title, s, sizeof(last_title));
		last_length = strlen(lastsonginfo);
		initial_time = realtime;
		frac = scroll_index = 0;
	}

	if ((!mp3_scrolltitle.value || last_length <= 38 + 7) && mp3_scrolltitle.value != 2)
	{
		char name[38 + 1];
		Q_strncpyz(name, last_title, sizeof(name));
		M_PrintWhite(max(8, (320 - (last_length - 7) * 8) >> 1), M_MP3_CONTROL_HEADINGROW + 32, name);
		initial_time = realtime;
	}
	else
	{
		for (i = 0; i < sizeof(songinfo_scroll) - 1; i++)
			songinfo_scroll[i] = lastsonginfo[(scroll_index + i) % last_length];
		songinfo_scroll[sizeof(songinfo_scroll) - 1] = 0;
		M_PrintWhite(12 -  (int) (8 * frac), M_MP3_CONTROL_HEADINGROW + 32, songinfo_scroll);
	}

	if (mp3_showtime.value)
	{
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, M_MP3_CONTROL_HEADINGROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW, "Play");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 8, last_status == MP3_PAUSED ? "Unpause" : "Pause");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 16, "Stop");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 24, "Next Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 32, "Prev Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 40, "Fast Forwd");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 48, "Rewind");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 56, "Volume");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 64, "Shuffle");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 72, "Repeat");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 88, "View Playlist");

	M_DrawCharacter (M_MP3_CONTROL_COL - 8, M_MP3_CONTROL_MENUROW + mp3_cursor * 8, 12 + ((int)(curtime * 4) & 1));

#warning I broke this
#if 0
	if (mp3_volumectrl_active)
		M_DrawSlider(M_MP3_CONTROL_COL + 96, M_MP3_CONTROL_MENUROW + 56, bound(0, mp3_volume.value, 1));
	else
#endif
		M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 56, "Disabled");;

	MP3_GetToggleState(&last_shuffle, &last_repeat);
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 64, last_shuffle ? "On" : "Off");
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 72, last_repeat ? "On" : "Off");

	M_DrawCharacter (16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter (24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter (320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter (17 + 286 * ((float) last_elapsed / last_total), M_MP3_CONTROL_BARHEIGHT, 131);
}

void M_Menu_MP3_Control_Key(int key)
{
	float volume;

	if (!MP3_IsActive() || last_status == MP3_NOTRUNNING)
	{
		switch(key)
		{
			case K_BACKSPACE:
				m_topmenu = m_none;
			case K_ESCAPE:
				M_LeaveMenu (m_main);
				break;
			case K_ENTER:
				if (MP3_IsActive())
					MP3_Execute_f();
				break;
		}
		return;
	}

	Con_Suppress();

	switch (key)
	{
		case K_BACKSPACE:
			m_topmenu = m_none;
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;
		case K_HOME:
		case K_PGUP:
			mp3_cursor = 0;
			break;
		case K_END:
		case K_PGDN:
			mp3_cursor = M_MP3_CONTROL_NUMENTRIES - 1;
			break;
		case K_DOWNARROW:
			if (mp3_cursor < M_MP3_CONTROL_NUMENTRIES - 1)
				mp3_cursor++;
			if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
				mp3_cursor++;
			break;
		case K_UPARROW:
			if (mp3_cursor > 0)
				mp3_cursor--;
			if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
				mp3_cursor--;
			break;
		case K_ENTER:
			switch (mp3_cursor)
			{
				case 0:	MP3_Play_f(); break;
				case 1:	MP3_Pause_f(); break;
				case 2:	MP3_Stop_f(); break;
				case 3: MP3_Next_f(); break;
				case 4:	MP3_Prev_f(); break;
				case 5: MP3_FastForward_f(); break;
				case 6: MP3_Rewind_f(); break;
				case 7: break;
				case 8: MP3_ToggleShuffle_f(); break;
				case 9: MP3_ToggleRepeat_f(); break;
				case 10: break;
				case 11: M_Menu_MP3_Playlist_f(); break;
			}
			break;
		case K_RIGHTARROW:
			switch(mp3_cursor)
			{
				case 7:
					volume = bound(0, mp3_volume.value, 1);
					Cvar_SetValue(&mp3_volume, bound(0, volume + 0.02, 1));
					break;
				default:
					MP3_FastForward_f();
					break;
			}
			break;
		case K_LEFTARROW:
			switch(mp3_cursor)
			{
				case 7:
					volume = bound(0, mp3_volume.value, 1);
					Cvar_SetValue(&mp3_volume, bound(0, volume - 0.02, 1));
					break;
				default:
					MP3_Rewind_f();
					break;
			}
			break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f(); 	break;
		case 'V': MP3_FadeOut_f();	break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: MP3_Rewind_f(); break;
		case KP_PGUP: MP3_FastForward_f(); break;
	}

	Con_Unsuppress();
}

static void M_Menu_MP3_Control_f(void){
	M_EnterMenu (m_mp3_control);
}

#define PLAYLIST_MAXENTRIES		2048
#define PLAYLIST_MAXLINES		17
#define PLAYLIST_HEADING_ROW	8

#define PLAYLIST_MAXTITLE	(32 + 1)

static int playlist_size = 0;
static int playlist_cursor = 0, playlist_base = 0;

static void Center_Playlist()
{
	int current;

	MP3_GetPlaylistInfo(&current, NULL);
	if (current >= 0 && current < playlist_size)
	{
		if (playlist_size - current - 1 < (PLAYLIST_MAXLINES >> 1))
		{
			playlist_base = max(0, playlist_size - PLAYLIST_MAXLINES);
			playlist_cursor = current - playlist_base;
		}
		else
		{
			playlist_base = max(0, current - (PLAYLIST_MAXLINES >> 1));
			playlist_cursor = current - playlist_base;
		}
	}
}

static char *playlist_entries[PLAYLIST_MAXENTRIES];

#ifdef _WIN32

void M_Menu_MP3_Playlist_Read()
{
	int i, count = 0, skip = 0;
	long length;
	char *playlist_buf = NULL;

	for (i = 0; i < playlist_size; i++)
	{
		if (playlist_entries[i])
		{
			free(playlist_entries[i]);
			playlist_entries[i] = NULL;
		}
	}

	playlist_base = playlist_cursor = playlist_size = 0;

	if ((length = MP3_GetPlaylist(&playlist_buf)) == -1)
		return;

	playlist_size = MP3_ParsePlaylist_EXTM3U(playlist_buf, length, playlist_entries, PLAYLIST_MAXENTRIES, PLAYLIST_MAXTITLE);
	free(playlist_buf);
}

#else

void M_Menu_MP3_Playlist_Read()
{
	int i;
	char *title;

	for (i = 0; i < playlist_size; i++)
	{
		if (playlist_entries[i])
		{
			free(playlist_entries[i]);
			playlist_entries[i] = NULL;
		}
	}

	playlist_base = playlist_cursor = playlist_size = 0;

	if (MP3_GetStatus() == MP3_NOTRUNNING)
		return;

	playlist_size = qxmms_remote_get_playlist_length(XMMS_SESSION);

	for (i = 0; i < PLAYLIST_MAXENTRIES && i < playlist_size; i++)
	{
		title = qxmms_remote_get_playlist_title(XMMS_SESSION, i);
		if (strlen(title) > PLAYLIST_MAXTITLE)
			title[PLAYLIST_MAXTITLE] = 0;
		playlist_entries[i] = strdup(title);
		g_free(title);
	}
}

#endif

void M_Menu_MP3_Playlist_Draw()
{
	int	index, print_time, i;
	char name[PLAYLIST_MAXTITLE];
	float realtime;

	static int last_status,last_elapsed, last_total, last_current;

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_NOTRUNNING)
	{
		M_Menu_MP3_Control_f();
		return;
	}

	M_Print ((320 - 8 * strlen(MP3_PLAYERNAME_ALLCAPS " PLAYLIST")) >> 1, PLAYLIST_HEADING_ROW, MP3_PLAYERNAME_ALLCAPS " PLAYLIST");
	M_Print (8, PLAYLIST_HEADING_ROW + 16, "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, PLAYLIST_HEADING_ROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Stopped");

	if (mp3_showtime.value)
	{
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, PLAYLIST_HEADING_ROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	if (!playlist_size)
	{
		M_Print (92, 32, "Playlist is empty");
		return;
	}

	MP3_GetPlaylistInfo(&last_current, NULL);

	for (index = playlist_base; index < playlist_size && index < playlist_base + PLAYLIST_MAXLINES; index++)
	{
		char *spaces;

		if (index + 1 < 10)
			spaces = "  ";
		else if (index + 1 < 100)
			spaces = " ";
		else
			spaces = "";
		Q_strncpyz(name, playlist_entries[index], sizeof(name));
		if (last_current != index)
			M_Print (16, PLAYLIST_HEADING_ROW + 24 + (index - playlist_base) * 8, va("%s%d %s", spaces, index + 1, name));
		else
			M_PrintWhite (16, PLAYLIST_HEADING_ROW + 24 + (index - playlist_base) * 8, va("%s%d %s", spaces, index + 1, name));
	}
	M_DrawCharacter (8, PLAYLIST_HEADING_ROW + 24 + playlist_cursor * 8, 12 + ((int)(curtime * 4) & 1));

	M_DrawCharacter (16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter (24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter (320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter (17 + 286 * ((float) last_elapsed / last_total), M_MP3_CONTROL_BARHEIGHT, 131);
}

static void M_Menu_MP3_Playlist_Key(int k)
{
	Con_Suppress();

	switch (k)
	{
         case K_BACKSPACE:
			m_topmenu = m_none;
         case K_ESCAPE:
			M_LeaveMenu (m_mp3_control);
			break;

		case K_UPARROW:
			if (playlist_cursor > 0)
				playlist_cursor--;
			else if (playlist_base > 0)
				playlist_base--;
			break;

		case K_DOWNARROW:
			if (playlist_cursor + playlist_base < playlist_size - 1)
			{
				if (playlist_cursor < PLAYLIST_MAXLINES - 1)
					playlist_cursor++;
				else
					playlist_base++;
			}
			break;

		case K_HOME:
			playlist_cursor = 0;
			playlist_base = 0;
			break;

		case K_END:
			if (playlist_size > PLAYLIST_MAXLINES)
			{
				playlist_cursor = PLAYLIST_MAXLINES - 1;
				playlist_base = playlist_size - playlist_cursor - 1;
			}
			else
			{
				playlist_base = 0;
				playlist_cursor = playlist_size - 1;
			}
			break;

		case K_PGUP:
			playlist_cursor -= PLAYLIST_MAXLINES - 1;
			if (playlist_cursor < 0)
			{
				playlist_base += playlist_cursor;
				if (playlist_base < 0)
					playlist_base = 0;
				playlist_cursor = 0;
			}
			break;

		case K_PGDN:
			playlist_cursor += PLAYLIST_MAXLINES - 1;
			if (playlist_base + playlist_cursor >= playlist_size)
				playlist_cursor = playlist_size - playlist_base - 1;
			if (playlist_cursor >= PLAYLIST_MAXLINES)
			{
				playlist_base += playlist_cursor - (PLAYLIST_MAXLINES - 1);
				playlist_cursor = PLAYLIST_MAXLINES - 1;
				if (playlist_base + playlist_cursor >= playlist_size)
					playlist_base = playlist_size - playlist_cursor - 1;
			}
			break;

		case K_ENTER:
			if (!playlist_size)
				break;
			Cbuf_AddText(va("mp3_playtrack %d\n", playlist_cursor + playlist_base + 1));
			break;
		case K_SPACE: M_Menu_MP3_Playlist_Read(); Center_Playlist();break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f(); 	break;
		case 'V': MP3_FadeOut_f();	break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: case K_LEFTARROW:  MP3_Rewind_f(); break;
		case KP_PGUP: case K_RIGHTARROW: MP3_FastForward_f(); break;
	}

	Con_Unsuppress();
}

static void M_Menu_MP3_Playlist_f(void){
	if (!MP3_IsActive())
	{
		M_Menu_MP3_Control_f();
		return;
	}

	M_Menu_MP3_Playlist_Read();
	M_EnterMenu (m_mp3_playlist);
	Center_Playlist();
}

#endif

//=============================================================================
/* DEMO MENU */

extern cvar_t demo_dir;

#ifdef _WIN32
#define	DEMO_TIME	FILETIME
#else
#define	DEMO_TIME	time_t
#endif

#define MAX_DEMO_NAME	MAX_OSPATH
#define MAX_DEMO_FILES	2048
#define DEMO_MAXLINES	17

typedef enum direntry_type_s {dt_file = 0, dt_dir, dt_up, dt_msg} direntry_type_t;
typedef enum demosort_type_s {ds_name = 0, ds_size, ds_time} demo_sort_t;

typedef struct direntry_s
{
	direntry_type_t	type;
	char			*name;
	int				size;
	DEMO_TIME		time;
} direntry_t;

static direntry_t	demolist_data[MAX_DEMO_FILES];
static direntry_t	*demolist[MAX_DEMO_FILES];
static int			demolist_count;
static char			demo_currentdir[MAX_OSPATH] = {0};
static char			demo_prevdemo[MAX_DEMO_NAME] = {0};

static float		last_demo_time = 0;

static int			demo_cursor = 0;
static int			demo_base = 0;

static demo_sort_t	demo_sorttype = ds_name;
static qboolean		demo_reversesort = false;

int Demo_SortCompare(const void *p1, const void *p2)
{
	int retval;
	int sign;
	direntry_t *d1, *d2;

	d1 = *((direntry_t **) p1);
	d2 = *((direntry_t **) p2);

	if ((retval = d2->type - d1->type) || d1->type > dt_dir)
		return retval;


	if (d1->type == dt_dir)
		return Q_strcasecmp(d1->name, d2->name);


	sign = demo_reversesort ? -1 : 1;

	switch (demo_sorttype)
	{
	case ds_name:
		return sign * Q_strcasecmp(d1->name, d2->name);
	case ds_size:
		return sign * (d1->size - d2->size);
	case ds_time:
#ifdef _WIN32
		return -sign * CompareFileTime(&d1->time, &d2->time);
#else
		return -sign * (d1->time - d2->time);
#endif
	default:
		Sys_Error("Demo_SortCompare: unknown demo_sorttype (%d)", demo_sorttype);
		return 0;
	}
}

static void Demo_SortDemos()
{
	int i;

	last_demo_time = 0;

	for (i = 0; i < demolist_count; i++)
		demolist[i] = &demolist_data[i];

	qsort(demolist, demolist_count, sizeof(direntry_t *), Demo_SortCompare);
}

static void Demo_PositionCursor()
{
	int i;

	last_demo_time = 0;
	demo_base = demo_cursor = 0;

	if (demo_prevdemo[0])
	{
		for (i = 0; i < demolist_count; i++)
		{
			if (!strcmp (demolist[i]->name, demo_prevdemo))
			{
				demo_cursor = i;
				if (demo_cursor >= DEMO_MAXLINES)
				{
					demo_base += demo_cursor - (DEMO_MAXLINES - 1);
					demo_cursor = DEMO_MAXLINES - 1;
				}
				break;
			}
		}
	}
	demo_prevdemo[0] = 0;
}


static void Demo_ReadDirectory()
{
	int i, size;
	direntry_type_t type;
	DEMO_TIME time;
	char name[MAX_DEMO_NAME];
#ifdef _WIN32
	HANDLE h;
	WIN32_FIND_DATA fd;
#else
	DIR *d;
	struct dirent *dstruct;
	struct stat fileinfo;
#endif

	demolist_count = demo_base = demo_cursor = 0;

	for (i = 0; i < MAX_DEMO_FILES; i++)
	{
		if (demolist_data[i].name)
		{
			free(demolist_data[i].name);
			demolist_data[i].name = NULL;
		}
	}

	if (demo_currentdir[0])
	{
		demolist_data[0].name = strdup ("..");
		demolist_data[0].type = dt_up;
		demolist_count = 1;
	}

#ifdef _WIN32
	h = FindFirstFile (va("%s%s/*.*", com_basedir, demo_currentdir), &fd);
	if (h == INVALID_HANDLE_VALUE)
	{
		demolist_data[demolist_count].name = strdup ("Error reading directory");
		demolist_data[demolist_count].type = dt_msg;
		demolist_count++;
		Demo_SortDemos();
		return;
	}
#else
	if (!(d = opendir(va("%s%s", com_basedir, demo_currentdir))))
	{
		demolist_data[demolist_count].name = strdup ("Error reading directory");
		demolist_data[demolist_count].type = dt_msg;
		demolist_count++;
		Demo_SortDemos();
		return;
	}
	dstruct = readdir (d);
#endif

	do
	{
	#ifdef _WIN32
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;
			type = dt_dir;
			size = 0;
			memset(&time, 0, sizeof(time));
		}
		else
		{
			i = strlen(fd.cFileName);
			if (i < 5 ||
				(
					Q_strcasecmp(fd.cFileName + i - 4, ".qwd") &&
					Q_strcasecmp(fd.cFileName +i - 4, ".qwz") &&
					Q_strcasecmp(fd.cFileName + i - 4, ".mvd")
				)
			)
				continue;
			type = dt_file;
			size = fd.nFileSizeLow;
			time = fd.ftLastWriteTime;
		}

		Q_strncpyz (name, fd.cFileName, sizeof(name));
	#else
		stat (va("%s%s/%s", com_basedir, demo_currentdir, dstruct->d_name), &fileinfo);

		if (S_ISDIR(fileinfo.st_mode))
		{
			if (!strcmp(dstruct->d_name, ".") || !strcmp(dstruct->d_name, ".."))
				continue;
			type = dt_dir;
			time = size = 0;
		}
		else
		{
			i = strlen(dstruct->d_name);
			if (i < 5 ||
				(
					Q_strcasecmp(dstruct->d_name + i - 4, ".qwd")
					&& Q_strcasecmp(dstruct->d_name + i - 4, ".mvd")
				)
			)
				continue;
			type = dt_file;
			size = fileinfo.st_size;
			time = fileinfo.st_mtime;
		}

		Q_strncpyz (name, dstruct->d_name, sizeof(name));
	#endif

		if (demolist_count == MAX_DEMO_FILES)
			break;

		demolist_data[demolist_count].name = strdup(name);
		demolist_data[demolist_count].type = type;
		demolist_data[demolist_count].size = size;
		demolist_data[demolist_count].time = time;
		demolist_count++;

#ifdef _WIN32
	} while (FindNextFile(h, &fd));
	FindClose (h);
#else
	} while ((dstruct = readdir (d)));
	closedir (d);
#endif

	if (!demolist_count)
	{
		demolist_data[0].name = strdup("[ no files ]");
		demolist_data[0].type = dt_msg;
		demolist_count = 1;
	}

	Demo_SortDemos();
	Demo_PositionCursor();
}

static void M_Menu_Demos_f()
{
	static qboolean demo_currentdir_init = false;
	char *s;

	M_EnterMenu(m_demos);


	if (!demo_currentdir_init)
	{
		demo_currentdir_init = true;
		if (demo_dir.string[0])
		{
			for (s = demo_dir.string; *s == '/' || *s == '\\'; s++)
				;
			if (*s)
			{
				strcpy(demo_currentdir, "/");
				strlcat(demo_currentdir, s, sizeof(demo_currentdir));

				for (s = demo_currentdir + strlen(demo_currentdir) - 1; *s == '/' || *s == '\\'; s--)
					*s = 0;
			}
		}
		else
		{
			strcpy(demo_currentdir, "/qw");
		}
	}

	Demo_ReadDirectory();
}

static void Demo_FormatSize (char *t)
{
	char *s;

	for (s = t; *s; s++)
	{
		if (*s >= '0' && *s <= '9')
			*s = *s - '0' + 18;
		else
			*s |= 128;
	}
}

#define DEMOLIST_NAME_WIDTH	29

static void M_Demos_Draw()
{
	int i, y;
	direntry_t *d;
	char demoname[DEMOLIST_NAME_WIDTH], demosize[36 - DEMOLIST_NAME_WIDTH];

	static char last_demo_name[MAX_DEMO_NAME + 7];
	static int last_demo_index = 0, last_demo_length = 0;
	char demoname_scroll[38 + 1];
	int demoindex, scroll_index;
	float frac, time, elapsed;


	M_Print (140, 8, "DEMOS");
	Q_strncpyz(demoname_scroll, demo_currentdir[0] ? demo_currentdir : "/", sizeof(demoname_scroll));
	M_PrintWhite (16, 16, demoname_scroll);
	M_Print (8, 24, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1f");

	for (i = 0; i < demolist_count - demo_base && i < DEMO_MAXLINES; i++)
	{
		d = demolist[demo_base + i];
		y = 32 + 8 * i;
		Q_strncpyz (demoname, d->name, sizeof(demoname));

		switch (d->type)
		{
		case dt_file:
			M_Print (24, y, demoname);
			if (d->size > 99999 * 1024)
				snprintf(demosize, sizeof(demosize), "%5iM", d->size >> 20);
			else
				snprintf(demosize, sizeof(demosize), "%5iK", d->size >> 10);
			Demo_FormatSize(demosize);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, demosize);
			break;
		case dt_dir:
			M_PrintWhite (24, y, demoname);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, "folder");
			break;
		case dt_up:
			M_PrintWhite (24, y, demoname);
			M_PrintWhite (24 + 8 * DEMOLIST_NAME_WIDTH, y, "    up");
			break;
		case dt_msg:
			M_Print (24, y, demoname);
			break;
		default:
			Sys_Error("M_Demos_Draw: unknown d->type (%d)", d->type);
		}
	}

	M_DrawCharacter (8, 32 + demo_cursor * 8, 12 + ((int) (curtime * 4) & 1));


	demoindex = demo_base + demo_cursor;
	if (demolist[demoindex]->type == dt_file)
	{
		time = (float) Sys_DoubleTime();
		if (!last_demo_time || last_demo_index != demoindex)
		{
			last_demo_index = demoindex;
			last_demo_time = time;
			frac = scroll_index = 0;
			snprintf(last_demo_name, sizeof(last_demo_name), "%s  ***  ", demolist[demoindex]->name);
			last_demo_length = strlen(last_demo_name);
		}
		else
		{

			elapsed = 3.5 * max(time - last_demo_time - 0.75, 0);
			scroll_index = (int) elapsed;
			frac = bound(0, elapsed - scroll_index, 1);
			scroll_index = scroll_index % last_demo_length;
		}


		if (last_demo_length <= 38 + 7)
		{
			Q_strncpyz(demoname_scroll, demolist[demoindex]->name, sizeof(demoname_scroll));
			M_PrintWhite (160 - strlen(demoname_scroll) * 4, 40 + 8 * DEMO_MAXLINES, demoname_scroll);
		}
		else
		{
			for (i = 0; i < sizeof(demoname_scroll) - 1; i++)
				demoname_scroll[i] = last_demo_name[(scroll_index + i) % last_demo_length];
			demoname_scroll[sizeof(demoname_scroll) - 1] = 0;
			M_PrintWhite (12 -  (int) (8 * frac), 40 + 8 * DEMO_MAXLINES, demoname_scroll);
		}
	}
	else
	{
		last_demo_time = 0;
	}

}

static void M_Demos_Key(int key)
{
	char *p;
	demo_sort_t sort_target;

	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor > 0)
			demo_cursor--;
		else if (demo_base > 0)
			demo_base--;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		if (demo_cursor + demo_base < demolist_count - 1)
		{
			if (demo_cursor < DEMO_MAXLINES - 1)
				demo_cursor++;
			else
				demo_base++;
		}
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor = 0;
		demo_base = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		if (demolist_count > DEMO_MAXLINES)
		{
			demo_cursor = DEMO_MAXLINES - 1;
			demo_base = demolist_count - demo_cursor - 1;
		}
		else
		{
			demo_base = 0;
			demo_cursor = demolist_count - 1;
		}
		break;

	case K_PGUP:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor -= DEMO_MAXLINES - 1;
		if (demo_cursor < 0)
		{
			demo_base += demo_cursor;
			if (demo_base < 0)
				demo_base = 0;
			demo_cursor = 0;
		}
		break;

	case K_PGDN:
		S_LocalSound ("misc/menu1.wav");
		demo_cursor += DEMO_MAXLINES - 1;
		if (demo_base + demo_cursor >= demolist_count)
			demo_cursor = demolist_count - demo_base - 1;
		if (demo_cursor >= DEMO_MAXLINES)
		{
			demo_base += demo_cursor - (DEMO_MAXLINES - 1);
			demo_cursor = DEMO_MAXLINES - 1;
			if (demo_base + demo_cursor >= demolist_count)
				demo_base = demolist_count - demo_cursor - 1;
		}
		break;

	case K_ENTER:
		if (!demolist_count || demolist[demo_base + demo_cursor]->type == dt_msg)
			break;

		if (demolist[demo_base + demo_cursor]->type != dt_file)
		{
			if (demolist[demo_base + demo_cursor]->type == dt_up)
			{
				if ((p = strrchr(demo_currentdir, '/')) != NULL)
				{
					Q_strncpyz(demo_prevdemo, p + 1, sizeof(demo_prevdemo));
					*p = 0;
				}
			}
			else
			{
				strncat(demo_currentdir, "/", sizeof(demo_currentdir) - strlen(demo_currentdir) - 1);
				strncat(demo_currentdir, demolist[demo_base + demo_cursor]->name, sizeof(demo_currentdir) - strlen(demo_currentdir) - 1);
			}
			demo_cursor = 0;
			Demo_ReadDirectory();
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
			VID_SetMouseGrab(1);
			if (keydown[K_CTRL])
				Cbuf_AddText (va("timedemo \"..%s/%s\"\n", demo_currentdir, demolist[demo_cursor + demo_base]->name));
			else
				Cbuf_AddText (va("playdemo \"..%s/%s\"\n", demo_currentdir, demolist[demo_cursor + demo_base]->name));
			Q_strncpyz(demo_prevdemo, demolist[demo_base + demo_cursor]->name, sizeof(demo_prevdemo));
		}
		break;

	case 'n':
	case 's':
	case 't':
		if (!keydown[K_CTRL])
			break;

		sort_target = (key == 'n') ? ds_name : (key == 's') ? ds_size : ds_time;
		if (demo_sorttype == sort_target)
		{
			demo_reversesort = !demo_reversesort;
		}
		else
		{
			demo_sorttype = sort_target;
			demo_reversesort = false;
		}
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		Demo_SortDemos();
		Demo_PositionCursor();
		break;

	case K_SPACE:
		Q_strncpyz(demo_prevdemo, demolist[demo_cursor + demo_base]->name, sizeof(demo_prevdemo));
		Demo_ReadDirectory();
		break;
	}
}

//=============================================================================
/* GAME OPTIONS MENU */

#ifndef CLIENTONLY

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

extern cvar_t maxclients, maxspectators;

int	startepisode;
int	startlevel;
int _maxclients, _maxspectators;
int _deathmatch, _teamplay, _skill, _coop;
int _fraglimit, _timelimit;

static void M_Menu_GameOptions_f()
{
	M_EnterMenu (m_gameoptions);

	// 16 and 8 are not really limits --- just sane values
	// for these variables...
	_maxclients = min(16, (int)maxclients.value);
	if (_maxclients < 2) _maxclients = 8;
	_maxspectators = max(0, min((int)maxspectators.value, 8));

	_deathmatch = max (0, min((int)deathmatch.value, 5));
	_teamplay = max (0, min((int)teamplay.value, 2));
	_skill = max (0, min((int)skill.value, 3));
	_fraglimit = max (0, min((int)fraglimit.value, 100));
	_timelimit = max (0, min((int)timelimit.value, 60));
}

int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 96, 104, 120, 128};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

static void M_GameOptions_Draw()
{
	char *msg;

	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(p_multipic, (320 - 216) / 2, 4, 216, 24);

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "        game type");
	if (!_deathmatch)
		M_Print (160, 56, "cooperative");
	else
		M_Print (160, 56, va("deathmatch %i", _deathmatch));

	M_Print (0, 64, "         teamplay");

	switch(_teamplay)
	{
		default: msg = "Off"; break;
		case 1: msg = "No Friendly Fire"; break;
		case 2: msg = "Friendly Fire"; break;
	}
	M_Print (160, 64, msg);

	if (_deathmatch == 0)
	{
		M_Print (0, 72, "            skill");
		switch (_skill)
		{
		case 0:  M_Print (160, 72, "Easy"); break;
		case 1:  M_Print (160, 72, "Normal"); break;
		case 2:  M_Print (160, 72, "Hard"); break;
		default: M_Print (160, 72, "Nightmare");
		}
	}
	else
	{
		M_Print (0, 72, "        fraglimit");
		if (_fraglimit == 0)
			M_Print (160, 72, "none");
		else
			M_Print (160, 72, va("%i frags", _fraglimit));

		M_Print (0, 80, "        timelimit");
		if (_timelimit == 0)
			M_Print (160, 80, "none");
		else
			M_Print (160, 80, va("%i minutes", _timelimit));
	}
	M_Print (0, 96, "       maxclients");
	M_Print (160, 96, va("%i", _maxclients) );

	M_Print (0, 104, "       maxspect.");
	M_Print (160, 104, va("%i", _maxspectators) );

	M_Print (0, 120, "         Episode");
    M_Print (160, 120, episodes[startepisode].description);

	M_Print (0, 128, "           Level");
    M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].description);
	M_Print (160, 136, levels[episodes[startepisode].firstLevel + startlevel].name);

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(curtime*4)&1));
}

static void M_NetStart_Change(int dir)
{
	int count;
	extern cvar_t	registered;

	switch (gameoptions_cursor)
	{
	case 1:
		_deathmatch += dir;
		if (_deathmatch < 0) _deathmatch = 5;
		else if (_deathmatch > 5) _deathmatch = 0;
		break;

	case 2:
		_teamplay += dir;
		if (_teamplay < 0) _teamplay = 2;
		else if (_teamplay > 2) _teamplay = 0;
		break;

	case 3:
		if (_deathmatch == 0)
		{
			_skill += dir;
			if (_skill < 0) _skill = 3;
			else if (_skill > 3) _skill = 0;
		}
		else
		{
			_fraglimit += dir * 10;
			if (_fraglimit < 0) _fraglimit = 100;
			else if (_fraglimit > 100) _fraglimit = 0;
		}
		break;

	case 4:
		_timelimit += dir*5;
		if (_timelimit < 0) _timelimit = 60;
		else if (_timelimit > 60) _timelimit = 0;
		break;

	case 5:
		_maxclients += dir;
		if (_maxclients > 16)
			_maxclients = 2;
		else if (_maxclients < 2)
			_maxclients = 16;
		break;

	case 6:
		_maxspectators += dir;
		if (_maxspectators > 8)
			_maxspectators = 0;
		else if (_maxspectators < 0)
			_maxspectators = 8;
		break;

	case 7:
		startepisode += dir;
		if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

static void M_GameOptions_Key(int key)
{
	switch (key)
	{
	case K_BACKSPACE:
		m_topmenu = m_none;	// intentional fallthrough
	case K_ESCAPE:
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (!_deathmatch && gameoptions_cursor == 4)
			gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (!_deathmatch && gameoptions_cursor == 4)
			gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
		S_LocalSound ("misc/menu2.wav");
//		if (gameoptions_cursor == 0)
		{
			key_dest = key_game;
			VID_SetMouseGrab(1);

			// Kill the server, unless we continue playing
			// deathmatch on another level
			if (!_deathmatch || !deathmatch.value)
				Cbuf_AddText ("disconnect\n");

			if (_deathmatch == 0)
			{
				_coop = 1;
				_timelimit = 0;
				_fraglimit = 0;
			}
			else
			{
				_coop = 0;
			}

			Cvar_Set (&deathmatch, va("%i", _deathmatch));
			Cvar_Set (&skill, va("%i", _skill));
			Cvar_Set (&coop, va("%i", _coop));
			Cvar_Set (&fraglimit, va("%i", _fraglimit));
			Cvar_Set (&timelimit, va("%i", _timelimit));
			Cvar_Set (&teamplay, va("%i", _teamplay));
			Cvar_Set (&maxclients, va("%i", _maxclients));
			Cvar_Set (&maxspectators, va("%i", _maxspectators));

			// Cbuf_AddText ("gamedir qw\n");
			Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );
			return;
		}

//		M_NetStart_Change (1);
		break;
	}
}
#endif	// !CLIENTONLY

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 0;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_name[16];
char	setup_team[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

extern cvar_t	name, team;
extern cvar_t	topcolor, bottomcolor;

#define	NUM_SETUP_CMDS	5

static void M_Menu_Setup_f()
{
	M_EnterMenu (m_setup);
	Q_strncpyz (setup_name, name.string, sizeof(setup_name));
	Q_strncpyz (setup_team, team.string, sizeof(setup_team));
	setup_top = setup_oldtop = (int)topcolor.value;
	setup_bottom = setup_oldbottom = (int)bottomcolor.value;
}

static void M_Setup_Draw()
{
	M_DrawPic(qplaquepic, 16, 4, 32, 144);
	M_DrawPic(p_multipic, (320 - 216) / 2, 4, 216, 24);

	M_Print (64, 40, "Your name");
	M_DrawTextBox (160, 32, 16, 1);
	M_PrintWhite (168, 40, setup_name);

	M_Print (64, 56, "Your team");
	M_DrawTextBox (160, 48, 16, 1);
	M_PrintWhite (168, 56, setup_team);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_DrawTextBox (64, 140-8, 14, 1);
	M_Print (72, 140, "Accept Changes");

	M_DrawPic(bigboxpic, 160, 64, 72, 72);

#if 0
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);
#endif
	#warning Fix this
	/* M_DrawPic(menuplyr, 172, 72, 48, 56); */

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(curtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_name), setup_cursor_table [setup_cursor], 10+((int)(curtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_team), setup_cursor_table [setup_cursor], 10+((int)(curtime*4)&1));
}

static void M_Setup_Key(int k)
{
	int l;

	switch (k)
	{
	case K_ESCAPE:
		M_LeaveMenu (m_multiplayer);
		break;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_HOME:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = 0;
		break;

	case K_END:
		S_LocalSound ("misc/menu1.wav");
		setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
//forward:
		S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
//		if (setup_cursor == 0 || setup_cursor == 1)
//			return;

//		if (setup_cursor == 2 || setup_cursor == 3)
//			goto forward;

		// setup_cursor == 4 (OK)
		Cvar_Set (&name, setup_name);
		Cvar_Set (&team, setup_team);
		Cvar_Set (&topcolor, va("%i", setup_top));
		Cvar_Set (&bottomcolor, va("%i", setup_bottom));
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_name))
				setup_name[strlen(setup_name)-1] = 0;
		}
		else if (setup_cursor == 1)
		{
			if (strlen(setup_team))
				setup_team[strlen(setup_team)-1] = 0;
		}
		else
		{
			m_topmenu = m_none;
			M_LeaveMenu (m_multiplayer);
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_name);
			if (l < 15)
			{
				setup_name[l+1] = 0;
				setup_name[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_team);
			if (l < 15)
			{
				setup_team[l + 1] = 0;
				setup_team[l] = k;
			}
		}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

static void M_Quit_Draw()
{
	static char *quitmsg[] =
	{
		"0Fodquake "FODQUAKE_VERSION,
		"0",
		"1Programming:",
		"0David Walton",
		"0Florian Zwoch",
		"0Jacek Piszczek",
		"0Jonny Tornbom",
		"0Juergen Legler",
		"0Mark Olsen",
		"0Morten Bojsen-Hansen",
		"0",
		"1Artwork:",
		"0Marcin Kornas",
		"0",
		"1Based on:",
		"0FuhQuake by A Nourai",
		"0ZQuake by Anton Gavrilov",
		"0Quakeworld by Id Software",
		"0",
		"1Press 'y' to exit Fodquake",
		NULL
	};
	char **p;
	int x, y;

	M_DrawTextBox (0, 11, 38, 21);
	y = 24;
	for (p = quitmsg; *p; p++, y += 8)
	{
		x = 18 + (36 - (strlen(*p + 1))) * 4;
		if (**p == '0')
			M_PrintWhite (x, y, *p + 1);
		else
			M_Print (x, y,	*p + 1);
	}
}

//=============================================================================
/* Menu Subsystem */

void M_CvarInit()
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_centerMenu);
	Cvar_Register (&scr_scaleMenu);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_load", M_Menu_Load_f);
	Cmd_AddCommand ("menu_save", M_Menu_Save_f);
#endif
	Cmd_AddCommand ("menu_multiplayer", M_Menu_MultiPlayer_f);
	Cmd_AddCommand ("menu_setup", M_Menu_Setup_f);
#if defined(_WIN32) || defined(__XMMS__)
	Cmd_AddCommand ("menu_mp3_control", M_Menu_MP3_Control_f);
	Cmd_AddCommand ("menu_mp3_playlist", M_Menu_MP3_Playlist_f);
#endif
	Cmd_AddCommand ("menu_demos", M_Menu_Demos_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_fps", M_Menu_Fps_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_help", M_Menu_Help_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}

void M_Draw()
{
	int oldconwidth;
	int oldconheight;

	if (m_state == m_none || key_dest != key_menu)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (SCR_NEED_CONSOLE_BACKGROUND)
		{
			Draw_ConsoleBackground (scr_con_current);
#ifndef GLQUAKE
			VID_UnlockBuffer ();
#endif
			S_ExtraUpdate ();
#ifndef GLQUAKE
			VID_LockBuffer ();
#endif
		}
		else
		{
			Draw_FadeScreen ();
		}

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	if (scr_scaleMenu.value)
	{
		oldconwidth = vid.conwidth;
		oldconheight = vid.conheight;

		if (((double)vid.conwidth)/320 > ((double)vid.conheight)/200)
		{
			menuwidth = ((double)vid.conwidth)*(200.0/((double)vid.conheight));
			menuheight = 200;
		}
		else
		{
			menuwidth = 320;
			menuheight = ((double)vid.conheight)*(320.0/((double)vid.conwidth));
		}

		vid.conwidth = menuwidth;
		vid.conheight = menuheight;

		Draw_SetSize(menuwidth, menuheight);
	}
	else
	{
		menuwidth = vid.conwidth;
		menuheight = vid.conheight;
	}

	if (scr_centerMenu.value)
		m_yofs = (menuheight - 200) / 2;
	else
		m_yofs = 0;

	switch (m_state)
	{
	case m_none:
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

#ifndef CLIENTONLY
	case m_load:
		M_Load_Draw ();
		break;

	case m_save:
		M_Save_Draw ();
		break;
#endif

	case m_multiplayer:
		M_MultiPlayer_Draw ();
		break;

	case m_setup:
		M_Setup_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_fps:
		M_Fps_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_video_verify:
		M_Video_Verify_Draw ();
		break;

	case m_help:
		M_Help_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

#ifndef CLIENTONLY
	case m_gameoptions:
		M_GameOptions_Draw ();
		break;
#endif

	case m_demos:
		M_Demos_Draw ();
		break;

#if defined(_WIN32) || defined(__XMMS__)
	case m_mp3_control:
		M_MP3_Control_Draw ();
		break;

	case m_mp3_playlist:
		M_Menu_MP3_Playlist_Draw();
		break;
#endif
	}

	if (scr_scaleMenu.value)
	{
		vid.conwidth = oldconwidth;
		vid.conheight = oldconheight;

		Draw_SetSize(vid.conwidth, vid.conheight);
	}

	if (m_entersound)
	{
		S_LocalSound ("misc/menu2.wav");
		m_entersound = false;
	}

#ifndef GLQUAKE
	VID_UnlockBuffer ();
#endif
	S_ExtraUpdate ();
#ifndef GLQUAKE
	VID_LockBuffer ();
#endif
}

void M_Keydown(int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

#ifndef CLIENTONLY
	case m_load:
		M_Load_Key (key);
		return;

	case m_save:
		M_Save_Key (key);
		return;
#endif

	case m_multiplayer:
		Menu_HandleKey(multiplayermenu, key);
		return;

	case m_setup:
		M_Setup_Key (key);
		return;

	case m_options:
		Menu_HandleKey(optionsmenu, key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_fps:
		Menu_HandleKey(fpsmenu, key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_video_verify:
		M_Video_Verify_Key (key);
		return;

	case m_help:
		M_Help_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

#ifndef CLIENTONLY
	case m_gameoptions:
		M_GameOptions_Key (key);
		return;
#endif

	case m_demos:
		M_Demos_Key (key);
		break;

#if defined(_WIN32) || defined(__XMMS__)
	case m_mp3_control:
		M_Menu_MP3_Control_Key (key);
		break;

	case m_mp3_playlist:
		M_Menu_MP3_Playlist_Key (key);
		break;
#endif
	}
}

void M_PerFramePreRender()
{
	if (m_state == m_video_verify)
	{
		if (curtime >= video_verify_fail_time)
		{
			M_Menu_Video_Verify_Revert();
			M_Menu_Video_Verify_Cleanup();
			M_LeaveMenu(m_video);
		}
	}
}

void M_Init()
{
	optionsmenu = Menu_Create(1, m_main);
	if (optionsmenu)
	{
		Menu_AddItem(optionsmenu, MenuItemButton_Create("Customize controls", 0, M_Menu_Keys_f));
		Menu_AddItem(optionsmenu, MenuItemButton_Create("Go to console", 0, M_Options_GoToConsole));
		Menu_AddItem(optionsmenu, MenuItemButton_Create("Reset to defaults", 0, M_Options_ResetToDefaults));
		Menu_AddItem(optionsmenu, MenuItemButton_Create("Save configuration", 0, M_Options_SaveConfiguration));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("Screen size", 30, 120, 10, &scr_viewsize));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("Gamma", 0.5, 1, -0.05, &v_gamma));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("Contrast", 1, 2, 0.1, &v_contrast));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("Mouse speed", 1, 11, 0.5, &sensitivity));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("CD music volume", 0, 1, 0.1, &bgmvolume));
		Menu_AddItem(optionsmenu, MenuItemCvarRange_Create("Sound volume", 0, 1, 0.1, &s_volume));
		Menu_AddItem(optionsmenu, MenuItemCvarPosNegBoolean_Create("Invert mouse", &m_pitch));
		Menu_AddItem(optionsmenu, MenuItemCvarBoolean_Create("Use old status bar", &cl_sbar, 0));
		Menu_AddItem(optionsmenu, MenuItemCvarBoolean_Create("HUD on left side", &cl_hudswap, 0));
		Menu_AddItem(optionsmenu, MenuItemButton_Create("FPS settings", 0, M_Menu_Fps_f));
		Menu_AddItem(optionsmenu, MenuItemButton_Create("Video modes", 0, M_Menu_Video_f));

		Menu_AddItem(optionsmenu, optionsmenu_usemouse = MenuItemCvarBoolean_Create("Use mouse", &in_grab_windowed_mouse, 0));

		Menu_Layout(optionsmenu);
	}

	/* FPS menu */
	fpsmenu = Menu_Create(1, m_options);
	if (fpsmenu)
	{
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Explosions", &r_explosiontype, explosiontypevalues));
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Muzzleflashes", &cl_muzzleflash, muzzleflashvalues));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Gib filter", &cl_gibfilter, 0));
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Dead body filter", &cl_deadbodyfilter, deadbodyfiltervalues));
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Rocket model", &cl_rocket2grenade, rocket2grenadevalues));
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Rocket trail", &r_rockettrail, rockettrailvalues));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Rocket light", &r_rocketlight, 0));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Damage filter", &v_damagecshift, 1));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Pickup flashes", &v_bonusflash, 0));
		Menu_AddItem(fpsmenu, MenuItemCvarMultiSelect_Create("Powerup glow", &r_powerupglow, powerupglowvalues));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Draw torches", &r_drawflame, 0));
		Menu_AddItem(fpsmenu, MenuItemCvarBoolean_Create("Fast sky", &r_fastsky, 0));
#ifdef GLQUAKE
		Menu_AddItem(fpsmenu, MenuItemCvarPosNegBoolean_Create("Fast lights", &gl_flashblend));
#endif

		Menu_AddItem(fpsmenu, MenuItemSpacer_Create());

		Menu_AddItem(fpsmenu, MenuItemButton_Create("Fast mode", 1, M_Fps_FastMode));
		Menu_AddItem(fpsmenu, MenuItemButton_Create("High quality", 1, M_Fps_HighQuality));

		Menu_Layout(fpsmenu);
	}

	/* Multiplayer menu */
	multiplayermenu = Menu_Create(0, m_main);
	if (multiplayermenu)
	{
		Menu_AddItem(multiplayermenu, MenuItemButton_Create("Server Browser", 0, M_MultiPlayer_ServerBrowser));
		Menu_AddItem(multiplayermenu, MenuItemButton_Create("Player Setup", 0, M_Menu_Setup_f));
		Menu_AddItem(multiplayermenu, MenuItemButton_Create("Demos", 0, M_Menu_Demos_f));
#ifndef CLIENTONLY
		Menu_AddItem(multiplayermenu, MenuItemButton_Create("New Game", 0, M_Menu_GameOptions_f));
#endif

		Menu_Layout(multiplayermenu);
	}
}

void M_Shutdown()
{
	Menu_Delete(optionsmenu);
	optionsmenu = 0;

	Menu_Delete(fpsmenu);
	fpsmenu = 0;

	Menu_Delete(multiplayermenu);
	multiplayermenu = 0;
}

void M_VidInit()
{
	unsigned int i;
	char buf[64];

	qplaquepic = Draw_LoadPicture("gfx/qplaque.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
	ttl_mainpic = Draw_LoadPicture("gfx/ttl_main.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
	mainmenupic = Draw_LoadPicture("gfx/mainmenu.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	for(i=0;i<6;i++)
	{
		snprintf(buf, sizeof(buf), "gfx/menudot%d.lmp", i+1);
		menudotpic[i] = Draw_LoadPicture(buf, DRAW_LOADPICTURE_DUMMYFALLBACK);
	}

	p_optionpic = Draw_LoadPicture("gfx/p_option.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	ttl_cstmpic = Draw_LoadPicture("gfx/ttl_cstm.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	vidmodespic = Draw_LoadPicture("gfx/vidmodes.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	for(i=0;i<NUM_HELP_PAGES;i++)
	{
		snprintf(buf, sizeof(buf), "gfx/help%d.lmp", i);
		helppic[i] = Draw_LoadPicture(buf, DRAW_LOADPICTURE_DUMMYFALLBACK);
	}

	ttl_sglpic = Draw_LoadPicture("gfx/ttl_sgl.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	sp_menupic = Draw_LoadPicture("gfx/sp_menu.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	p_loadpic = Draw_LoadPicture("gfx/p_load.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	p_savepic = Draw_LoadPicture("gfx/p_save.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	p_multipic = Draw_LoadPicture("gfx/p_multi.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	bigboxpic = Draw_LoadPicture("gfx/bigbox.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);

	menuplyrpic = Draw_LoadPicture("gfx/menuplyr.lmp", DRAW_LOADPICTURE_DUMMYFALLBACK);
}

void M_VidShutdown()
{
	unsigned int i;

	Draw_FreePicture(menuplyrpic);

	Draw_FreePicture(bigboxpic);

	Draw_FreePicture(p_multipic);

	Draw_FreePicture(p_savepic);

	Draw_FreePicture(p_loadpic);

	Draw_FreePicture(sp_menupic);

	Draw_FreePicture(ttl_sglpic);

	for(i=0;i<NUM_HELP_PAGES;i++)
	{
		Draw_FreePicture(helppic[i]);
	}

	Draw_FreePicture(vidmodespic);

	Draw_FreePicture(ttl_cstmpic);

	Draw_FreePicture(p_optionpic);

	for(i=0;i<6;i++)
	{
		Draw_FreePicture(menudotpic[i]);
	}

	Draw_FreePicture(mainmenupic);
	Draw_FreePicture(ttl_mainpic);
	Draw_FreePicture(qplaquepic);
}

