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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quakedef.h"
#include "linked_list.h"
#include "hud_new.h"
#include "hud_functions.h"
#include "strl.h"

#define HIS_PLAIN_TEXT (1<<0)
#define HIS_FUNCTION (1<<1)

#define HIS_IDENTIFIER '$'
#define HIS_IDENTIFIER_OPENING_BRACKET '{'
#define HIS_IDENTIFIER_CLOSING_BRACKET '}'

struct hud_info_string_type
{
	char *name;
	char *(*value_function)(void *info);
	int type; // 0 = team, 1 = player
};

static char *team_rl_count_vf(void *info)
{
	static char buf[512];
	struct team *team;
	team = (struct team *)info;
	snprintf(buf, sizeof(buf), "%i", team->rl_count);
	return buf;
}

#if 1
#warning Please check my suggestion for a replacement below this function :P

static char *team_armor_count_vf(void *info)
{
	static char buf[512];
	int space = 0;
	struct team *team;
	team = (struct team *)info;

	buf[0] = '\0';
	if (team->ra_count > 0)
	{
		strlcat(buf, va("&cf00%i", team->ra_count), sizeof(buf));
		space = 1;
	}

	if (team->ya_count > 0)
	{
		if (space)
			strlcat(buf, " ", sizeof(buf));
		strlcat(buf, va("&cff0%i", team->ya_count), sizeof(buf));
		space = 1;
	}

	if (team->ga_count > 0)
	{
		if (space)
			strlcat(buf, " ", sizeof(buf));
		strlcat(buf, va("&c0f0%i", team->ga_count), sizeof(buf));
		space = 1;
	}

	if (space)
		strlcat(buf, "&cfff", sizeof(buf));

	return buf;	
}
#else
static char *team_armor_count_vf(void *info)
{
	static char buf[512];
	int space = 0;
	struct team *team;
	team = (struct team *)info;

	buf[0] = '\0';

	if (team->ra_count > 0)
		strlcat(buf, va("&cf00%i ", team->ra_count), sizeof(buf));

	if (team->ya_count > 0)
		strlcat(buf, va("&cff0%i ", team->ya_count), sizeof(buf));

	if (team->ga_count > 0)
		strlcat(buf, va("&c0f0%i ", team->ga_count), sizeof(buf));

	if (buf[0])
	{
		buf[strlen(buf)-1] = 0;
		strlcat(buf, "&cfff", sizeof(buf));
	}

	return buf;	
}
#endif

static char *team_powerups_vf(void *info)
{
	static char buf[4];
	int i;
	struct team *team;
	team = (struct team *)info;
	i = 0;
	if (team->items & IT_QUAD)
	{
		buf[i] = 'q';
		buf[i++] |= 128;
	}
	if (team->items & IT_INVULNERABILITY)
	{
		buf[i] = 'p';
		buf[i++] |= 128;
	}
	if (team->items & IT_INVISIBILITY)
	{
		buf[i] = 'r';
		buf[i++] |= 128;
	}
	buf[i] = '\0';
	return buf;
}

static char *player_health_vf(void *info)
{
	static char buf[512];
	player_info_t *player;
	player = (player_info_t *)info;
	snprintf(buf, sizeof(buf), "%i", player->stats[STAT_HEALTH]);
	return buf;
}

static char *player_armor_vf(void *info)
{
	static char buf[5];
	player_info_t *player;
	player = (player_info_t *)info;
	snprintf(buf, sizeof(buf), "%i", player->stats[STAT_ARMOR]);
	return buf;
}

static char *player_armor_type_vf(void *info)
{
	player_info_t *player;
	player = (player_info_t *)info;
	
	if (player->stats[STAT_ITEMS] & IT_ARMOR1)
		return "g";
	if (player->stats[STAT_ITEMS] & IT_ARMOR2)
		return "y";
	if (player->stats[STAT_ITEMS] & IT_ARMOR2)
		return "r";

	return "";
}

static char *player_weapons_vf(void *info)
{
	static char buf[7];
	player_info_t *player;
	int i;
	player = (player_info_t *)info;

	i = 0;

	if (player->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
	{
		buf[i++] = '3';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_SUPER_SHOTGUN)
			buf[i-1] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_NAILGUN)
	{
		buf[i++] = '4';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_NAILGUN)
			buf[i-1] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
	{
		buf[i++] = '5';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_SUPER_NAILGUN)
			buf[i-1] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
	{
		buf[i++] = '6';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_GRENADE_LAUNCHER)
			buf[i-1] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
	{
		buf[i++] = '7';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_ROCKET_LAUNCHER)
			buf[i-1] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_LIGHTNING)
	{
		buf[i++] = '8';
		if (player->stats[STAT_ACTIVEWEAPON] & IT_LIGHTNING)
			buf[i-1] |= 128;
	}
	buf[i] = '\0';
	return buf;
}

static char *player_powerups_vf(void *info)
{
	static char buf[4];
	player_info_t *player;
	int i;
	player = (player_info_t *)info;

	i = 0;

	if (player->stats[STAT_ITEMS] & IT_QUAD)
	{
		buf[i] = 'q';
		buf[i++] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_INVULNERABILITY)
	{
		buf[i] = 'p';
		buf[i++] |= 128;
	}
	if (player->stats[STAT_ITEMS] & IT_INVISIBILITY)
	{
		buf[i] = 'r';
		buf[i++] |= 128;
	}
	buf[i] = '\0';
	return buf;
}

static struct hud_info_string_type types[] = {
	{ "team_rl_count", team_rl_count_vf, 0},
	{ "team_armor_count", team_armor_count_vf, 0},
	{ "team_powerups", team_powerups_vf, 0},
	{ "player_health", player_health_vf, 1},
	{ "player_armor", player_armor_vf, 1},
	{ "player_armor_type", player_armor_type_vf, 1},
	{ "player_powerups", player_powerups_vf, 1},
	{ "player_weapons", player_weapons_vf, 1}
	};

static void Clear_Info_String(struct hud_info_string *info_string)
{

	struct hud_info_string_entry *entry, *current;

	if (!info_string)
		return;

	entry = info_string->entry;

	while(entry)
	{
		current = entry;
		entry = entry->next;
		if (current->type == HIS_PLAIN_TEXT)
			if (current->string)
				free(current->string);
		free(current);
	}
	free(info_string);
}

static struct hud_info_string_type *get_type(char *name, int type)
{
	int i;
	for (i=0;i<(sizeof(types)/sizeof(*types));i++)
		if (types[i].type == type)
			if (strcmp(types[i].name, name) == 0)
				return &types[i];
	return NULL;

}

static int set_variable(char *string, struct hud_info_string_entry *entry, int type)
{
	char *ptr;
	int i, max;
	char buf[256];
	struct hud_info_string_type *string_type;

	ptr = string;
	ptr += 2;	
	i=0;
	max = strlen(string);

#warning You read 2 bytes past the end of the input here, don't you? ptr = string; ptr += 2; max = strlen(string) and then you use max to indicate the end of ptr, which is +2.
	while (*ptr != HIS_IDENTIFIER_CLOSING_BRACKET && i < sizeof(buf) && i < max)
	{
		buf[i] = *ptr;
		ptr++;
		i++;
	}

	if (*ptr != HIS_IDENTIFIER_CLOSING_BRACKET)
	{
		Com_Printf("malformed info string, could not find closing \"%c\"\n.", HIS_IDENTIFIER_CLOSING_BRACKET);
		return 1;
	}

#warning Off-by-one bug here. The loop above will exit when i >= sizeof(buf).
	buf[i] = '\0';
	string_type = get_type(buf, type);
	if (!string_type)
	{
#warning printf? Ugly, ugly. Yes, there are more than one. Please fix them all :)
		printf("type \"%s\" not found.\n", buf);
		return 1;
	}
	entry->value_function = string_type->value_function;
	entry->type = HIS_FUNCTION;
	return 0;
}

static int set_string(char *string, struct hud_info_string_entry *entry)
{
	char *ptr;
	int i, max;
	char buf[256];

	ptr = string;
	i=0;
	max = Colored_String_Length(string);
#warning This logic looks broken... Using the parsed string length on the raw string input? Same as from above basically.

	while (*ptr != HIS_IDENTIFIER && i < sizeof(buf) && i < max)
	{
		buf[i] = *ptr;
		ptr++;
		i++;
	}

#warning Off-by-one bug here. The loop above will exit when i >= sizeof(buf).
	buf[i] = '\0';
	entry->string = strdup(buf);
	if (entry->string == NULL)
		return 1;
		
	entry->type = HIS_PLAIN_TEXT;
	return 0;
}

static int Parse_Info_String(char *string, struct hud_info_string *info_string, int type)
{
	struct hud_info_string_entry *entry;
	int length, once;
	char *ptr;

	if (!string || !info_string)
		return 1;

	length = Colored_String_Length(string);
#warning This logic looks broken... Using the parsed string length on the raw string input? Same as from above basically.
	entry = calloc(1, sizeof(*entry));
	if (entry == NULL)
		return 1;

	info_string->entry = entry;

	ptr = string;
	once = 1;
	while (ptr < (string + length) && *ptr != '\0')
	{
		if (!once)
		{
			entry->next = calloc(1, sizeof(*entry));
			if (entry->next == NULL)
				return 1;
			entry = entry->next;
		}

		once = 0;
		// check if its text 
		if (*ptr != HIS_IDENTIFIER)
		{
			if(set_string(ptr, entry))
				return 1;
			ptr = strchr(ptr, HIS_IDENTIFIER);	
			if (ptr == 0)
				break;
			continue;
		}

		if (*ptr == HIS_IDENTIFIER && *(ptr +1) == HIS_IDENTIFIER)
			ptr++;

		// check if next part is a variable
		if (*(ptr+1) == HIS_IDENTIFIER_OPENING_BRACKET)
		{
			if (*ptr == HIS_IDENTIFIER)
			{
				if(set_variable(ptr, entry, type))
					return 1;
				ptr = strchr(ptr, HIS_IDENTIFIER_CLOSING_BRACKET);	
				if (ptr == 0)
					break;
				ptr++;
			}
			continue;
		}
				
	}
	return 0;
}

int HUD_Info_String_Changed(struct hud_item *self, int type)
{
	
	struct hud_info_string *is;
	char *string;
	is = NULL;
	string = NULL;

	if (type != 0 && type != 1)
		return 1;

	if (type == 0)
		Clear_Info_String(self->team_info_string);
	else if (type == 1)
		Clear_Info_String(self->team_player_info_string);

	if (type == 0)
		is = self->team_info_string = calloc(1, sizeof(struct hud_info_string));
	else if (type == 1)
		is = self->team_player_info_string = calloc(1, sizeof(struct hud_info_string));

	if (is == NULL)
	{
		Com_Printf("error allocating hud_info_string\n");
		return 1;
	}

	if (type == 0)
		string = self->team_info_string_plain;
	else if (type == 1)
		string = self->team_player_info_string_plain; 

	if (!string)
		return 1;

	if (Parse_Info_String(string, is, type))
	{
		Com_Printf("error parsing the string\n");
		Clear_Info_String(is);
		return 1;
	}
	return 0;
}

void HUD_Setup_Player_Info_String(struct hud_item *self, struct team *team, int number)
{
	struct hud_info_string_entry *entry;	
	char buf[512];

	if (team->players_frag_sorted_info_string[number])
		free (team->players_frag_sorted_info_string[number]);

	team->players_frag_sorted_info_string[number] = NULL;;

	if (!self->team_player_info_string)
		return;
	
	buf[0] = '\0';
	entry = self->team_player_info_string->entry;
	while (entry)
	{
		if (entry->type == HIS_PLAIN_TEXT)
		{
			strlcat(buf, entry->string, sizeof(buf));
		}
		else if (entry->type == HIS_FUNCTION)
		{
			strlcat(buf, entry->value_function((void *)team->players_frag_sorted[number]), sizeof(buf));
		}
		entry = entry->next;
	}

	team->players_frag_sorted_info_string[number] = strdup(buf);
	if (team->players_frag_sorted_info_string[number] == NULL)
		Com_Printf("warning: strdup in \"HUD_Setup_Player_Info_String\" failed.\n");
}

void HUD_Setup_Team_Info_String(struct hud_item *self, struct team *team)
{
	struct hud_info_string_entry *entry;	
	char buf[512];

	if (team->info_string)
		free (team->info_string);

	team->info_string = NULL;

	if (!self->team_info_string)
		return;
	
	buf[0] = '\0';
	entry = self->team_info_string->entry;
	while (entry)
	{
		if (entry->type == HIS_PLAIN_TEXT)
		{
			strlcat(buf, entry->string, sizeof(buf));
		}
		else if (entry->type == HIS_FUNCTION)
		{
			strlcat(buf, entry->value_function((void *)team), sizeof(buf));
		}
		entry = entry->next;
	}

	team->info_string = strdup(buf);
	if (team->info_string == NULL)
		Com_Printf("warning: strdup in \"HUD_Setup_Team_Info_String\" failed.\n");
}

