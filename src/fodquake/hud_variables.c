#include <stdlib.h>
#include <string.h>
#include "quakedef.h"
#include "linked_list.h"
#include "hud_new.h"
#include "hud_functions.h"
#include "utils.h"
#include "time.h"

static cvar_t hud_speed_update = {"hud_speed_update", "0.2"};

static double *hud_item_get_dvalue(struct hud_item *self, int color_table_setup)
{
	double *var;

	if (color_table_setup == 0)
		var = &self->dvalue;
	else if (color_table_setup == 1)
		var = &self->dvalue_color_table;
	else
		var = &self->dvalue;

	return var;
}

static int *hud_item_get_ivalue(struct hud_item *self, int color_table_setup)
{
	int *var;

	if (color_table_setup == 0)
		var = &self->ivalue;
	else if (color_table_setup == 1)
		var = &self->ivalue_color_table;
	else
		var = &self->ivalue;

	return var;
}

static void setup_speed_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	static float speed;
	double t;
	double *var;
	double oldval;
	int mynum;
	vec3_t vel;
	static float maxspeed = 0;
	static double lastframetime = 0;
	static double checkframetime = 0;
	static int lastmynum = -1;

	t = cls.realtime; 

	var = hud_item_get_dvalue(self, color_table_setup);

	oldval = *var;

	if (setup_only)
	{
		if (option == 0)
			*var = speed;
		else if (option == 1)
			*var = maxspeed;

		if (oldval == *var)
			self->set_flags &= ~SF_CHANGED;
		else
			self->set_flags |= SF_CHANGED;
		return;
	}

	if (checkframetime + hud_speed_update.value > t)
	{
		if (option == 0)
			*var = speed;
		else if (option == 1)
			*var = maxspeed;

		if (oldval == *var)
			self->set_flags &= ~SF_CHANGED;
		else
			self->set_flags |= SF_CHANGED;
		return;
	}

	checkframetime = t;

	if (!cl.spectator || (mynum = Cam_TrackNum()) == -1)
		mynum = cl.playernum;

	if (mynum != lastmynum) {
		lastmynum = mynum;
		maxspeed = 0;
		lastframetime = t;
	}

	if (!cl.spectator || cls.demoplayback || mynum == cl.playernum)
		VectorCopy (cl.simvel, vel);
	else
		VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[mynum].velocity, vel);

	vel[2] = 0;
	speed = VectorLength(vel);

	maxspeed = max(maxspeed, speed);

	if (option == 0)
		*var = speed;
	else if (option == 1)
		*var = maxspeed;

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_cl_stats_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	*var = cl.stats[option];

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_ping_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	double *var;
	double oldval;

	var = hud_item_get_dvalue(self, color_table_setup);

	oldval = *var;
	*var = cl.players[cl.playernum].ping;

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_fps_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	double t;
	static float lastfps;
	static double lastframetime;
	static int last_fps_count;
	extern int fps_count;
	double *var;
	double oldval;

	var = hud_item_get_dvalue(self, color_table_setup);

	oldval = *var;

	if (setup_only)
	{
		*var = lastfps;
		if (oldval == *var)
			self->set_flags &= ~SF_CHANGED;
		else
			self->set_flags |= SF_CHANGED;

		return; 
	}

	t = cls.realtime; 
	if ((t - lastframetime) >= 1.0) {
		lastfps = (fps_count - last_fps_count) / (t - lastframetime);
		last_fps_count = fps_count;
		lastframetime = t;
	}

	*var = lastfps;

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_armor_type_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;

	
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
		*var = 1;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
		*var = 2;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
		*var = 3;
	else
		*var = 0;

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;

}

static void setup_active_weapon_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case IT_AXE: *var = 0; break;
		case IT_SHOTGUN: *var = 1; break;
		case IT_SUPER_SHOTGUN: *var = 2; break;
		case IT_NAILGUN: *var = 3; break;
		case IT_SUPER_NAILGUN: *var = 4; break;
		case IT_GRENADE_LAUNCHER: *var = 5; break;
		case IT_ROCKET_LAUNCHER: *var = 6; break;
		case IT_LIGHTNING: *var = 7; break;
		default: *var = -1; break;
	}

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;

}

static void setup_mapname_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	extern cvar_t mapname;		

	

	if (self->cvalue)
	{

		if (strcmp(self->cvalue, mapname.string) == 0)
		{
			self->set_flags &= ~SF_CHANGED;
			return;
		}
		free(self->cvalue);

	}
	self->cvalue = strdup(mapname.string);
	if (self->cvalue == NULL)
	{
		Com_Printf("warning: strdup failed in \"setup_mapname_value\".\n");
	}
	self->set_flags |= SF_CHANGED;
}

static void setup_active_ammo_type_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;

	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case IT_AXE: *var = 0; break;
		case IT_SHOTGUN: *var = 1; break;
		case IT_SUPER_SHOTGUN: *var = 1; break;
		case IT_NAILGUN: *var = 2; break;
		case IT_SUPER_NAILGUN: *var = 2; break;
		case IT_GRENADE_LAUNCHER: *var = 3; break;
		case IT_ROCKET_LAUNCHER: *var = 3; break;
		case IT_LIGHTNING: *var = 4; break;
		default: *var = -1; break;
	}

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_packet_loss_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	*var = cl.players[cl.playernum].pl;

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_variable_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldival;

	double *dvar;
	double olddval;

	if (!self->displayed_variable)
		return;

	if (option == 0)
	{
		var = hud_item_get_ivalue(self, color_table_setup);

		oldival = *var;
		*var = (int)self->displayed_variable->value;

		if (oldival == *var)
			self->set_flags &= ~SF_CHANGED;
		else
			self->set_flags |= SF_CHANGED;

		return;
	}
	else if (option == 1)
	{
		dvar = hud_item_get_dvalue(self, color_table_setup);

		olddval = *dvar;
		*dvar = self->displayed_variable->value;
	
		if (olddval == *dvar)
			self->set_flags &= ~SF_CHANGED;
		else
			self->set_flags |= SF_CHANGED;

		return;
	}
	else if (option == 2)
	{
		if (self->cvalue)
		{
			if (strcmp(self->cvalue, self->displayed_variable->string) == 0)
			{
				self->set_flags &= ~SF_CHANGED;
				return;
			}
		}
		free(self->cvalue);
		self->cvalue = strdup(self->displayed_variable->string);
		if (self->cvalue == NULL)
		{
			Com_Printf("warning: strdup failed in \"setup_variable_value\".\n");
		}

		self->set_flags |= SF_CHANGED;
	}
}

static void setup_powerup_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	*var = 0;
	
	if (cl.stats[STAT_ITEMS] & IT_QUAD)
		*var |= (1<<0);
	
	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
		*var |= (1<<1);
	
	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
		*var |= (1<<2);

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_individual_powerup_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	*var = 0;
	
	if (option == 0)
	{
		if (cl.stats[STAT_ITEMS] & IT_QUAD)
			*var = 1;
	}
	else if (option == 1)
	{
		if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY)
			*var = 1;
	}
	else if (option == 2)
	{
		if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
			*var = 1;
	}

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_individual_weapon_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;
	*var = 0;

	if (option == 0)
	{
		if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
			*var = 1;
	}
	else if (option == 1)
	{
		if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
			*var = 1;
	}
	else if (option == 2)
	{
		if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
			*var = 1;
	}
	else if (option == 3)
	{
		if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
			*var = 1;
	}
	else if (option == 4)
	{
		if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
			*var = 1;
	}
	else if (option == 5)
	{
		if (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
			*var = 1;
	}

	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_weapons_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	int *var;
	int oldval;

	var = hud_item_get_ivalue(self, color_table_setup);

	oldval = *var;

	*var = 0;
	
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN)
		*var |= (1<<0);

	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
		*var |= (1<<1);
	
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN)
		*var |= (1<<2);
	
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
		*var |= (1<<3);

	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
		*var |= (1<<4);

	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
		*var |= (1<<5);

	if (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
		*var |= (1<<6);
	
	if (oldval == *var)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;
}

static void setup_localtime_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	static double update_time = -1;
	static char date[80];
	struct tm *ptm;
	time_t t;

	if (update_time + 1 < cls.realtime)
	{
		time(&t);
		if ((ptm = localtime(&t)))
		{
			strftime(date, sizeof(date) -1, "%H:%M:%S", ptm);
		}
		else
		{
			strcpy (date, "#data date#");
		}

		self->set_flags |= SF_CHANGED;
		update_time = cls.realtime;
	}

	self->cvalue = date;
}

static void setup_servertime_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	static double update_time = -1;
	static char date[80];
	float time;

	if (update_time + 1 < cls.realtime)
	{
		time = (cl.servertime_works) ? cl.servertime : cls.realtime;
                Q_strncpyz (date, SecondsToHourString((int) time), sizeof(date));

		self->set_flags |= SF_CHANGED;
		update_time = cls.realtime;

	}
	self->cvalue = date;
}

static void setup_gametime_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{

	static double update_time = -1;
	static char date[80];
	float timelimit;

#warning Does this mean that the time might update almost a second late?
	if (update_time + 1 < cls.realtime)
	{
		timelimit = 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit"));

		if (cl.countdown || cl.standby)
			Q_strncpyz(date, SecondsToHourString(timelimit), sizeof(date));
		else
			Q_strncpyz(date, SecondsToHourString((int) abs(timelimit - cl.gametime)), sizeof(date));

		self->set_flags |= SF_CHANGED;
		update_time = cls.realtime;
	}

	self->cvalue = date;
}

static void setup_gametype_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	if (self->cvalue)
	{
		if (strcmp(self->cvalue, MT_Name()) == 0)
		{
			self->set_flags &= ~SF_CHANGED;
			return;
		}
	}
	free(self->cvalue);
	self->cvalue = strdup(MT_Name());
	if (self->cvalue == NULL)
	{
		Com_Printf("warning: strdup failed in \"setup_gametype_value\".\n");
	}
	self->set_flags |= SF_CHANGED;
}

static void setup_gametype_int_value(struct hud_item *self, int setup_only, int option, int color_table_setup)
{
	extern cvar_t mapname;		
	int oldval;

	oldval = self->ivalue;

	self->ivalue = MT_Value();

	if (oldval == self->ivalue)
		self->set_flags &= ~SF_CHANGED;
	else
		self->set_flags |= SF_CHANGED;

}

static struct vartype_string vartype_string[] = 
{
	{"health", 0, &setup_cl_stats_value, -1.0f, STAT_HEALTH},
	{"armor", 0, &setup_cl_stats_value, -1.0f, STAT_ARMOR},
	{"ping", 1, &setup_ping_value, -1.0f, 0},
	{"speed", 1, &setup_speed_value, -1.0f, 0},
	{"max_speed", 1, &setup_speed_value, -1.0f, 1},
	{"fps", 1, &setup_fps_value, -1.0f, 0},
	{"armor_type", 0, &setup_armor_type_value, -1.0f, 0},
	{"active_weapon", 0, &setup_active_weapon_value, -1.0f, 0},
	{"active_ammo", 0, &setup_cl_stats_value, -1.0f, STAT_AMMO},
	{"active_ammo_type", 0, &setup_active_ammo_type_value, -1.0f, 0},
	{"shell_count", 0, &setup_cl_stats_value, -1.0f, STAT_SHELLS},
	{"nails_count", 0, &setup_cl_stats_value, -1.0f, STAT_NAILS},
	{"rocket_count", 0, &setup_cl_stats_value, -1.0f, STAT_ROCKETS},
	{"cell_count", 0, &setup_cl_stats_value, -1.0f, STAT_CELLS},
	{"packet_loss", 0, &setup_packet_loss_value, -1.0f, 0},
	{"variable_int", 0, &setup_variable_value, -1.0f, 0},
	{"variable_float", 1, &setup_variable_value, -1.0f, 1},
	{"variable_char", 2, &setup_variable_value, -1.0f, 2},
	{"mapname", 2, &setup_mapname_value, -1.0f, 0},
	{"powerups", 0, &setup_powerup_value, -1.0f, 0},
	{"weapons", 0, &setup_weapons_value, -1.0f, 0},
	{"quad", 0, &setup_individual_powerup_value, -1.0f, 0},
	{"pent", 0, &setup_individual_powerup_value, -1.0f, 1},
	{"ring", 0, &setup_individual_powerup_value, -1.0f, 2},
	{"ssg", 0, &setup_individual_weapon_value, -1.0f, 0},
	{"ng", 0, &setup_individual_weapon_value, -1.0f, 1},
	{"sng", 0, &setup_individual_weapon_value, -1.0f, 2},
	{"gl", 0, &setup_individual_weapon_value, -1.0f, 3},
	{"rl", 0, &setup_individual_weapon_value, -1.0f, 4},
	{"lg", 0, &setup_individual_weapon_value, -1.0f, 5},
	{"gametype", 2, &setup_gametype_value, -1.0f, 0},
	{"gametype_int", 0, &setup_gametype_int_value, -1.0f, 0},
	{"localtime", 2, &setup_localtime_value, -1.0f, 0},
	{"gametime", 2, &setup_gametime_value, -1.0f, 0}
};

struct vartype_string *HUD_Get_vartype_string(char *name)
{
	int i;
	for (i = 0; i < (sizeof(vartype_string)/sizeof(*vartype_string)); i++)
		if (!strcmp(name, vartype_string[i].name))
			return &vartype_string[i];
	return NULL;
}

void HUD_List_Variables_f(void)
{
	int i;

	for (i = 0; i < (sizeof(vartype_string)/sizeof(*vartype_string)); i++)
	{
		Com_Printf("%s\n", vartype_string[i].name);
	}
}

void HUD_Variables_Init(void)
{
	Cmd_AddCommand("hud_list_variables", &HUD_List_Variables_f);
	Cvar_Register(&hud_speed_update);
}

void HUD_Setup_Item_Type(struct hud_item *self, double time)
{
	if (!self->vartype_string)
		return;

	if (self->vartype_string->update_time != time)
	{
		self->vartype_string->setup_function(self, 0, self->vartype_string->option, 0);
		self->vartype_string->update_time = time;
		
	}
	else
	{
		self->vartype_string->setup_function(self, 1, self->vartype_string->option, 0);
	}

	if (self->color_table_setup)
	{
		if (self->color_table_setup->update_time != time)
		{
			self->color_table_setup->setup_function(self, 0, self->vartype_string->option, 1);
			self->color_table_setup->update_time = time;
		
		}
		else
		{
			self->color_table_setup->setup_function(self, 1, self->color_table_setup->option, 1);
		}
	}
	else
	{
		self->ivalue_color_table = self->ivalue;
		self->dvalue_color_table = self->dvalue;
	}
}

