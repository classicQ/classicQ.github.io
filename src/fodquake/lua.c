#include "config.h"
#if USE_LUA
#include "quakedef.h"
#include "utils.h"
#include "keys.h"
#include "teamplay.h"
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <stdlib.h>
#include <string.h>

#include "context_sensitive_tab.h"
#include "tokenize_string.h"

#include "sound.h"

#define LUAF_PLAYER_STATS (1 << 0)

struct L_lua_states
{
	lua_State *L;
	char *name;
	char *script;
	int buggy;
	int flags;
	int player_update_frame;
	struct L_lua_states *next, *prev;
};

struct lua_rmf
{
	lua_State *L;
	char *function;
	struct L_lua_states *l;
};

static const char * const lua_helpers[] = {"keys", "macro", "split", "ui", "variables", NULL};

static keydest_t old_keydest;

static struct lua_rmf **lua_rmf_list;
static int lua_rmf_list_count;

static struct lua_rmf **lua_d2d_list;
static int lua_d2d_list_count;

static struct lua_rmf **lua_callable_function_list;
static int lua_callable_function_list_count;

static struct lua_rmf **lua_frame_function_list;
static int lua_frame_function_list_count;

static struct lua_rmf lua_key_function;

static struct L_lua_states *L_lua_states = NULL;

#define lua_getglobal(L,s) lua_getfield(L, LUA_GLOBALSINDEX, (s))
#define lua_tostring(L,i) lua_tolstring(L, (i), NULL)
#define lua_open() luaL_newstate()
#define lua_dofile(L, fn) \
        (lua_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))
#define lua_pushliteral(L, s)   \
        lua_pushlstring(L, "" s, (sizeof(s)/sizeof(char))-1)
#define lua_pop(L,n)            lua_settop(L, -(n)-1)
#define lua_checkstring(L,n)   (lua_checklstring(L, (n), NULL))
#define lua_getglobal(L,s)      lua_getfield(L, LUA_GLOBALSINDEX, (s))
#define lua_setglobal(L,s)      lua_setfield(L, LUA_GLOBALSINDEX, (s))

#define luaL_newtable (L)    (lua_newtable(L), lua_gettop(L))

static struct L_lua_states *Lua_FindStateLS(lua_State *L)
{
	struct L_lua_states *l;

	if (L_lua_states == NULL)
		return NULL;

	l = L_lua_states;

	while (l)
	{
		if (l->L == L)
			return l;
		l = l->next;
	}

	return NULL;
}

static int add_rmf_to_list(lua_State *L, char *function, struct lua_rmf ***rmflist, int *lua_rmf_count)
{
	struct lua_rmf **tmprmflist;
	struct lua_rmf *rmf;
	unsigned int i;

	if (L && function && rmflist && lua_rmf_count)
	{
		/* First see if this thing already exists in the list */
		tmprmflist = *rmflist;
		if (tmprmflist)
		{
			while(*tmprmflist)
			{
				if ((*tmprmflist)->L == L && strcmp((*tmprmflist)->function, function) == 0)
					return 1;
			}

			tmprmflist++;
		}

		rmf = malloc(sizeof(*rmf));
		if (rmf)
		{
			rmf->function = strdup(function);
			if (rmf->function)
			{
				rmf->L = L;
				rmf->l = Lua_FindStateLS(L);

				i = *lua_rmf_count;
				tmprmflist = realloc(*rmflist, sizeof(*tmprmflist) * (i + 2));
				if (tmprmflist)
				{
					tmprmflist[i] = rmf;
					tmprmflist[i + 1] = 0;

					*lua_rmf_count = i + 1;
					*rmflist = tmprmflist;

					return 0;
				}

				free(rmf->function);
			}

			free(rmf);
		}
	}

	return 1;
}

static int add_message_call(lua_State *L, char *function)
{
	add_rmf_to_list(L, function, &lua_rmf_list, &lua_rmf_list_count);
	return 0;
}

static int add_draw_2D_call(lua_State *L, char *function)
{
	add_rmf_to_list(L, function, &lua_d2d_list, &lua_d2d_list_count);
	return 0;
}

#warning This is O(n). It should have a callback argument as well to avoid the function lookup.
static void call_function_skeleton (void)
{
	struct lua_rmf **f;
	int i;

	if (lua_callable_function_list == NULL)
		return;

	f = lua_callable_function_list;

	while (*f)
	{
		if (strcmp((*f)->function, Cmd_Argv(0)) == 0)
			break;
		f++;
	}

	if (*f == NULL)
	{
		Com_Printf("lua command not found!\n");
		return;
	}

	lua_getglobal((*f)->L, (*f)->function);
	for (i=1; i<Cmd_Argc();i++)
		lua_pushstring((*f)->L, Cmd_Argv(i));

	if (lua_pcall((*f)->L, i-1, 0, 0) != 0)
	{
		Com_Printf("lua error: %s\n", lua_tostring((*f)->L, -1));
		(*f)->l->buggy = 1;
	}

}

static int add_callable_function(lua_State *L, char *function)
{
	add_rmf_to_list(L, function, &lua_callable_function_list, &lua_callable_function_list_count);
	Cmd_AddCommand(function, call_function_skeleton);
	return 0;
}

static int add_frame_function(lua_State *L, char *function)
{
	add_rmf_to_list(L, function, &lua_frame_function_list, &lua_frame_function_list_count);
	return 0;
}

static void Lua_Clear_State(struct L_lua_states *ls)
{
	if (!ls)
		return;

	ls->buggy = 1;

	lua_getglobal(ls->L, "__shutdown");
	lua_pcall(ls->L, 0, 0, 0);

	free(ls->name);
	free(ls->script);

	if (ls->L)
		lua_close(ls->L);
}

static void Lua_Remove_State(struct L_lua_states *ls)
{
	if (!ls)
		return;

	if (ls->next)
		ls->next->prev = ls->prev;

	if (ls->prev)
		ls->prev->next = ls->next;
	else
		L_lua_states = ls->next;

	Lua_Clear_State(ls);
	free(ls);
}

static struct L_lua_states *Lua_FindState(char *script, char *name)
{
	struct L_lua_states *ls;

	ls = L_lua_states;

	while (ls)
	{
		if (ls->name && ls->script)
		{
			if (strcmp(ls->name, name) == 0)
				if (strcmp(ls->script, script) == 0)
					return ls;
		}
		ls = ls->next;
	}
	return NULL;
}

static struct L_lua_states *Lua_Add_State(char *script, char *name)
{
	struct L_lua_states *ls;
	struct L_lua_states *templs;

	if (script == NULL || name == NULL)
		return NULL;

	ls = Lua_FindState(script, name);
	if (ls)
	{
		Com_Printf("Lua State \"%s\" \"%s\" already exists.\n", name, script);
		return NULL;
	}

	ls = malloc(sizeof(*ls));
	if (ls)
	{
		memset(ls, 0, sizeof(*ls));
		ls->script = strdup(script);
		ls->name = strdup(name);
		if (ls->script && ls->name)
		{
			if (L_lua_states)
			{
				templs = L_lua_states;
				while(templs->next)
					templs = templs->next;

				templs->next = ls;
				ls->prev = templs;
			}
			else
			{
				L_lua_states = ls;
			}

			return ls;
		}

		free(ls->script);
		free(ls->name);
		free(ls);
	}

	return 0;
}

// stolen from awesome tiling wm
static void Lua_RegisterFunctions(const char *name,lua_State *L,
		const struct luaL_reg methods[],
		const struct luaL_reg meta[])
{
	luaL_newmetatable(L, name);                                        /* 1 */
	lua_pushvalue(L, -1);           /* dup metatable                      2 */
	lua_setfield(L, -2, "__index"); /* metatable.__index = metatable      1 */

	luaL_register(L, NULL, meta);                                      /* 1 */
	luaL_register(L, name, methods);                                   /* 2 */
	lua_pushvalue(L, -1);           /* dup self as metatable              3 */
	lua_setmetatable(L, -2);        /* set self as metatable              2 */
	lua_pop(L, 2);
}

/*
 * Lib functions
 */

static int LF_Com_Printf(lua_State *L)
{
	const char *string;

	string = luaL_checkstring(L, 1);
	if (string)
	{
		if (lua_isnumber(L, 2))
		{
			Com_Printf("%s", TP_ParseWhiteText((char *)string, true, 0));
		}
		else
			Com_Printf("%s", string);
	}
	return 0;
}

static int LF_Printf(lua_State *L)
{
	const char *string;

	string = luaL_checkstring(L, 1);
	if (string)
		printf("%s\n", string);

	return 0;
}

static int LF_Add_Text(lua_State *L)
{
	const char *string;

	string = luaL_checkstring(L, 1);
	if (string)
		Cbuf_AddText((char *)string);

	return 0;
}

static int LF_Register_Message_Function(lua_State *L)
{
	const char *string;

	string = luaL_checkstring(L, 1);
	if (string)
		add_message_call(L, (char *)string);

	return 0;
}

static int LF_Register_Draw_2D_Function(lua_State *L)
{
	const char *string;
	string = luaL_checkstring(L, 1);
	if (string)
		add_draw_2D_call(L, (char *)string);

	return 0;
}

static int LF_Register_Callable_Function(lua_State *L)
{
	const char *string;
	string = luaL_checkstring(L, 1);
	if (string)
		add_callable_function(L, (char *)string);

	return 0;
}

static int LF_Register_Frame_Function(lua_State *L)
{
	const char *string;
	string = luaL_checkstring(L, 1);
	if (string)
		add_frame_function(L, (char *)string);

	return 0;
}

static int LF_Set_Key_Function(lua_State *L)
{
	const char *string;

	string = luaL_checkstring(L, 1);

	if (string)
	{
		if (lua_key_function.function)
			free(lua_key_function.function);
		lua_key_function.function = strdup(string);
		if (lua_key_function.function == NULL)
		{
			lua_pushboolean(L, 0);
			return 1;
		}
		lua_key_function.L = L;
		old_keydest = key_dest;
		key_dest = key_lua;
		lua_pushboolean(L, 1);
	}
	else
		lua_pushboolean(L, 0);

	return 1;
}

static int LF_Unset_Key_Function(lua_State *L)
{
	free(lua_key_function.function);
	lua_key_function.function = NULL;
	lua_key_function.L = NULL;
	key_dest = old_keydest;

	return 0;
}

static int LF_Keydown(lua_State *L)
{
	int key;
	key = luaL_checknumber(L, 1);

	if (keydown[key])
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);

	return 1;
}

static int LF_Macro(lua_State *L)
{
	const char *s;
	char *r;
	int len;

	s = luaL_checkstring(L, 1);
	if (s)
	{
		r = Cmd_MacroString ((char *)s, &len);
		if (r)
			lua_pushstring(L, r);
		else
			lua_pushnil(L);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int LF_Play_Sound(lua_State *L)
{
	const char *s;

	s = luaL_checkstring(L, 1);
	if (s)
		S_LocalSound((char *)s);

	return 0;
}

static int LF_Request_Player_Stats(lua_State *L)
{
	struct L_lua_states *l;

	l = Lua_FindStateLS(L);

	if (l)
		l->flags |= LUAF_PLAYER_STATS;

	return 0;
}

static luaL_reg Basic_Functions_Methods[] =
{
	{"print", LF_Com_Printf},
	{"printf", LF_Printf},
	{"add_text", LF_Add_Text},
	{"register_message_function", LF_Register_Message_Function},
	{"register_draw_2d_function", LF_Register_Draw_2D_Function},
	{"register_callable_function", LF_Register_Callable_Function},
	{"register_frame_function", LF_Register_Frame_Function},
	{"set_key_function", LF_Set_Key_Function},
	{"unset_key_function", LF_Unset_Key_Function},
	{"keydown", LF_Keydown},
	{"macro", LF_Macro},
	{"play", LF_Play_Sound},
	{"request_player_stats", LF_Request_Player_Stats},
	{0, 0}
};

static int LDF_Draw_String(lua_State *L)
{
	const char *string;
	int x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	string = luaL_checkstring(L, 3);
	if (string)
		Draw_ColoredString(x, y, (char *)string, 1);
	return 0;
}

static int LDF_Draw_Plain_String(lua_State *L)
{
	const char *string;
	int x, y;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	string = luaL_checkstring(L, 3);
	if (string)
		Draw_String(x, y, string);
	return 0;
}

#warning This should probably use the ARGB version instead... Fuck palette indices.
static int LDF_Fill(lua_State *L)
{
	int x, y, w, h, c;
	x = luaL_checknumber(L, 1);
	y = luaL_checknumber(L, 2);
	w = luaL_checknumber(L, 3);
	h = luaL_checknumber(L, 4);
	c = luaL_checknumber(L, 5);
	Draw_Fill(x, y, w, h, c);
	return 0;
}

static int LDF_Dimensions(lua_State *L)
{
	lua_pushnumber(L, vid.conwidth);
	lua_pushnumber(L, vid.conheight);
	return 2;
}

static luaL_reg Draw_Functions_Methods[] =
{
	{"string", LDF_Draw_String},
	{"plain_string", LDF_Draw_Plain_String},
	{"fill", LDF_Fill},
	{"get_screen_dimensions", LDF_Dimensions},
	{0, 0}
};

static luaL_reg Functions_Meta[] =
{
	{0,0}
};

// Variable handling

static int LVF_Get_Variable_Pointer(lua_State *L)
{
	const char *name;
	cvar_t *c;

	name = luaL_checkstring(L, 1);
	if (name)
	{
		if ((c = Cvar_FindVar((char *)name)))
		{
			lua_pushlightuserdata(L, c);
		}
		else
			lua_pushnil(L);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int LVF_Set_Variable(lua_State *L)
{
	cvar_t *c;
	const char *cval;

	c = lua_touserdata(L, 1);
	if (c)
	{
		if (lua_isstring(L, 2))
		{
			cval = luaL_checkstring(L, 2);
		}
		else if (lua_isnumber(L, 2))
		{
			cval = va("%f", luaL_checknumber(L, 2));
		}
		else
		{
			lua_pushboolean(L, 0);
			return 1;
		}

		Cvar_Set(c, (char *)cval);
		lua_pushboolean(L, 1);
	}
	else
		lua_pushboolean(L, 0);

	return 1;
}

static int LVF_Get_Variable_Value(lua_State *L)
{
	cvar_t *c;

	c = lua_touserdata(L, 1);
	if (c)
	{
		lua_pushstring(L, c->string);
	}
	else
		lua_pushnil(L);

	return 1;
}

static int LVF_Create_Variable(lua_State *L)
{
	cvar_t *c;
	const char *name;
	const char *cval;

	name = luaL_checkstring(L, 1);
	if (name)
	{
		if (lua_isstring(L, 2))
		{
			cval = luaL_checkstring(L, 2);
		}
		else if (lua_isnumber(L, 2))
		{
			cval = va("%f", luaL_checknumber(L, 2));
		}
		else
		{
			lua_pushnil(L);
			return 1;
		}

		c = Cvar_Create((char *)name, (char *)cval, CVAR_CHANGED);
		lua_pushlightuserdata(L, c);
	}
	else
	{
		lua_pushnil(L);
	}

	return 1;
}

static luaL_reg Variables_Methods[] =
{
	{"__get_variable_pointer", LVF_Get_Variable_Pointer},
	{"__set_variable", LVF_Set_Variable},
	{"__get_variable_value", LVF_Get_Variable_Value},
	{"__create_variable", LVF_Create_Variable},
	{0, 0}
};

static luaL_reg Variables_Meta[] =
{
	{0, 0}
};

static struct L_lua_states *Lua_CreateState(char *script, char *name)
{
	struct L_lua_states *ls;
	const char * const *s;

	ls = Lua_Add_State(script, name);

	if (ls == NULL)
		return NULL;

	ls->L = lua_open();
	if (ls->L == NULL)
	{
		Com_Printf("lua_open failed for \"%s\".\n", script);
		Lua_Remove_State(ls);
		return NULL;
	}

	/* lets try and restrict this
	   luaL_openlibs(ls->L);
	   */
	// base package
	lua_pushcfunction(ls->L, luaopen_base);
	lua_pushstring(ls->L, "");
	lua_call(ls->L, 1, 0);

	// tables
	lua_pushcfunction(ls->L, luaopen_table);
	lua_pushstring(ls->L, "table");
	lua_call(ls->L, 1, 0);

	// math
	lua_pushcfunction(ls->L, luaopen_math);
	lua_pushstring(ls->L, "math");
	lua_call(ls->L, 1, 0);

	// string
	lua_pushcfunction(ls->L, luaopen_string);
	lua_pushstring(ls->L, "string");
	lua_call(ls->L, 1, 0);

	Lua_RegisterFunctions("fodquake", ls->L, Basic_Functions_Methods, Functions_Meta);
	Lua_RegisterFunctions("draw", ls->L, Draw_Functions_Methods, Functions_Meta);
	Lua_RegisterFunctions("variables", ls->L, Variables_Methods, Variables_Meta);

	// load all existing helpers
	s = lua_helpers;
	while (*s)
	{
		if (luaL_dofile(ls->L, va("fodquake/lua/helpers/%s.lua", *s)) != 0)
		{
			Com_Printf("error loading %s helper\n", *s);
			Lua_Remove_State(ls);
			return NULL;
		}
		s++;
	}

	if (luaL_loadfile(ls->L, script) != 0)
	{
		Com_Printf("lua error in script \"%s\": %s\n", script, lua_tostring(ls->L, -1));
		Lua_Remove_State(ls);
		return NULL;
	}

	if (lua_pcall(ls->L, 0, LUA_MULTRET, 0) != 0)
	{
		Com_Printf("lua error in script \"%s\": %s\n", script, lua_tostring(ls->L, -1));
		Lua_Remove_State(ls);
		return NULL;
	}

	lua_getglobal(ls->L, "__init");
	if (lua_pcall(ls->L, 0, 0, 0) != 0)
	{
		Com_Printf("lua script \"%s\" has no __init function: %s\n", script, lua_tostring(ls->L, -1));
		Lua_Remove_State(ls);
		return NULL;
	}

	Com_Printf("lua script \"%s\" loaded.\n", script);

	return ls;
}

static void Lua_Load_Scripts(void)
{
	struct directory_list *dlist;
	int i;
	static const char * const filters[] = {"$.lua", NULL};

	dlist = Util_Dir_Read("fodquake/lua/autoload", 1, 1, filters);

	if (dlist == NULL)
	{
		Com_Printf("no scripts in fodquake/lua/autoload\n");
		return;
	}

	for (i=0; i<dlist->entry_count; i++)
	{
		Lua_CreateState(va("fodquake/lua/autoload/%s", dlist->entries[i].name), dlist->entries[i].name);
	}

	Util_Dir_Delete(dlist);
}

static void Lua_CallFunction_f(void)
{
	struct lua_rmf **list;
	int i;

	if (Cmd_Argc() < 2)
	{
		Com_Printf("usage: %s function arguments.\n", Cmd_Argv(0));
		return;
	}

	if (lua_callable_function_list == NULL)
		return;

	list = lua_callable_function_list;

	while (*list)
	{
		if (strcmp((*list)->function, Cmd_Argv(1)) == 0)
		{
			break;
		}

		list++;
	}

	if (list == NULL)
	{
		Com_Printf("Lua function \"%s\" not found.\n", Cmd_Argv(1));
		return;
	}

	if ((*list)->l->buggy != 0)
		return;

	lua_getglobal((*list)->L, (*list)->function);
	for (i=2; i<Cmd_Argc();i++)
		lua_pushstring((*list)->L, Cmd_Argv(i));

	if (lua_pcall((*list)->L, i-2, 0, 0) != 0)
	{
		Com_Printf("lua error: %s\n", lua_tostring((*list)->L, -1));
		(*list)->l->buggy = 1;
	}
}

static void Lua_List_CFunctions(void)
{
	struct lua_rmf **l;

	Com_Printf("RMF: %i\n", lua_rmf_list_count);
	l = lua_rmf_list;
	while (l && *l)
	{
		Com_Printf("  %s: %p\n", (*l)->function, (*l)->L);
		l++;
	}

	Com_Printf("D2D: %i\n", lua_d2d_list_count);
	l = lua_d2d_list;
	while (l && *l)
	{
		Com_Printf("  %s: %p\n", (*l)->function, (*l)->L);
		l++;
	}

	Com_Printf("Callable: %i\n", lua_callable_function_list_count);
	l = lua_callable_function_list;
	while (l && *l)
	{
		Com_Printf("  %s: %p\n", (*l)->function, (*l)->L);
		l++;
	}

	Com_Printf("Frame: %i\n", lua_frame_function_list_count);
	l = lua_frame_function_list;
	while (l && *l)
	{
		Com_Printf("  %s: %p\n", (*l)->function, (*l)->L);
		l++;
	}
}

static void Lua_Load(void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf("Usage: %s script.lua.\n", Cmd_Argv(0));
		return;
	}
	Lua_CreateState(va("fodquake/lua/%s", Cmd_Argv(1)), Cmd_Argv(0));
}

#warning This could probably be done a lot better.
static void Lua_PushPlayerStats(struct L_lua_states *l)
{
	struct lua_State *L;
	player_info_t *p;
	int i;
	//static char *weapons[] = {"axe", "sg", "ssg", "ng", "sng", "gl", "rl", "lg"};
	int new_table;
	int stats[4];
	//int weapon_kills[8];
	//int send_weapon_kills;

	if (l == NULL)
		return;

	if (l->player_update_frame == cls.framecount)
		return;

	L = l->L;

	lua_getglobal(L, "players");
	if (LUA_TTABLE != lua_type(L, -1))
	{
		lua_newtable(L);
		lua_setglobal(L, "players");
		lua_getglobal(L, "players");
		if (LUA_TTABLE != lua_type(L, -1))
		{
			Com_Printf("something went really wrong!\n");
			l->buggy = 1;
			return;
		}
	}

	for (i=0; i<MAX_CLIENTS; i++)
	{
		p = &cl.players[i];
		lua_pushnumber(L, i);
		lua_gettable(L, -2);
		if (p->name[0] == '\0')
		{
			lua_pop(L, 1);
			lua_pushinteger(L, i);
			lua_pushnil(L);
			lua_settable(L, -3);
		}
		else
		{
			new_table = 0;
			if (LUA_TTABLE != lua_type(L, -1))
			{
				lua_pop(L, 1);
				lua_pushnumber(L, i);
				lua_newtable(L);
				new_table = 1;
			}

			lua_pushstring(L, p->name);
			lua_setfield(L, -2, "name");

			lua_pushstring(L, p->team);
			lua_setfield(L, -2, "team");

			lua_pushnumber(L, p->ping);
			lua_setfield(L, -2, "ping");

			lua_pushnumber(L, p->pl);
			lua_setfield(L, -2, "pl");

			lua_pushnumber(L, p->frags);
			lua_setfield(L, -2, "frags");

			lua_pushnumber(L, p->topcolor);
			lua_setfield(L, -2, "topcolor");

			lua_pushnumber(L, p->bottomcolor);
			lua_setfield(L, -2, "bottomcolor");

			if (p->spectator == 1)
				lua_pushboolean(L, 1);
			else
				lua_pushboolean(L, 0);
			lua_setfield(L, -2, "spectator");

			Stats_GetBasicStats(i, stats);

			lua_pushinteger(L, stats[0]);
			lua_setfield(L, -2, "totalfrags");

			lua_pushinteger(L, stats[1]);
			lua_setfield(L, -2, "totaldeaths");

			lua_pushinteger(L, stats[2]);
			lua_setfield(L, -2, "totalteamkills");

			lua_pushinteger(L, stats[3]);
			lua_setfield(L, -2, "totalsuicides");

			/*
			for (x=0; x<8; x++)
			{
				lua_pushinteger(L, p->weapon_stats[x][0]);
				lua_setfield(L, -2, va("%s_attacks", weapons[x]));
				lua_pushinteger(L, p->weapon_stats[x][1]);
				lua_setfield(L, -2, va("%s_hits", weapons[x]));
				lua_pushinteger(L, Stats_GetWeaponKill(i, weapons[x]));
				lua_setfield(L, -2, va("%s_kills", weapons[x]));
			}
			*/

			if (new_table)
				lua_settable(L, -3);
			else
				lua_pop(L, 1);
		}
	}
}

/**********************************
 * Interface to the rest of Quake *
 **********************************/

void Lua_Frame(void)
{
	struct lua_rmf **l;

	if (lua_frame_function_list == NULL)
		return;

	l = lua_frame_function_list;

	while (*l)
	{
		if ((*l)->l->buggy == 0)
		{
			if ((*l)->l)
				if ((*l)->l->flags & LUAF_PLAYER_STATS)
					Lua_PushPlayerStats((*l)->l);
			lua_getglobal((*l)->L, (*l)->function);
			lua_pushnumber((*l)->L, cls.realtime);
			if (lua_pcall((*l)->L, 1, 0, 0) != 0)
			{
				Com_Printf("lua error: %s\n", lua_tostring((*l)->L, -1));
				(*l)->l->buggy = 1;
			}
		}
		l++;
	}
}

void Lua_Frame_2D(void)
{
	struct lua_rmf **l;

	if (lua_d2d_list == NULL)
		return;

	l = lua_d2d_list;

	while (*l)
	{
		if ((*l)->l->buggy == 0)
		{
			if ((*l)->l)
				if ((*l)->l->flags & LUAF_PLAYER_STATS)
					Lua_PushPlayerStats((*l)->l);
			lua_getglobal((*l)->L, (*l)->function);
			lua_getglobal((*l)->L, (*l)->function);
			lua_pushnumber((*l)->L, cls.realtime);
			lua_pushnumber((*l)->L, vid.conwidth);
			lua_pushnumber((*l)->L, vid.conheight);

			if (lua_pcall((*l)->L, 3, 0, 0) != 0)
			{
				Com_Printf("lua error: %s\n", lua_tostring((*l)->L, -1));
				(*l)->l->buggy = 1;
			}
		}
		l++;
	}
}

void Lua_Key(int key)
{
	if (lua_key_function.L == NULL)
		return;

	lua_getglobal(lua_key_function.L, lua_key_function.function);
	lua_pushnumber(lua_key_function.L, key);

	if (lua_pcall(lua_key_function.L, 1, 0, 0) != 0)
		Com_Printf("lua error: %s\n", lua_tostring(lua_key_function.L, -1));

	if (key == K_ESCAPE)
	{
		key_dest = old_keydest;
	}
}

/********
 * CSTC *
 ********/

struct cstc_lua_load_info
{
	struct directory_list *dl;
	qboolean *checked;
	qboolean initialized;
};

static int cstc_lua_load_check(char *entry, struct tokenized_string *ts)
{
	int i;

	for (i=0; i<ts->count; i++)
	{
		if (Util_strcasestr(entry, ts->tokens[i]) == NULL)
			return 0;
	}
	return 1;
}

static int cstc_lua_load_get_data(struct cst_info *self, int remove)
{
	struct cstc_lua_load_info *data;
	const char * const script_endings[] = { "$.lua", NULL};

	if (!self)
		return 1;

	if (self->data)
	{
		data = (struct cstc_lua_load_info *)self->data;
		Util_Dir_Delete(data->dl);
		free(data->checked);
		free(data);
		self->data = NULL;
	}

	if (remove)
		return 0;

	if ((data = calloc(1, sizeof(*data))))
	{
		if ((data->dl = Util_Dir_Read("fodquake/lua/", 0, 1, script_endings)))
		{
			if (data->dl->entries == 0)
			{
				cstc_lua_load_get_data(self, 1);
				return 1;
			}
			self->data = data;
			return 0;
		}
		free(data);
	}
	return 1;
}

static int cstc_lua_load_get_results(struct cst_info *self, int *results, int get_result, int result_type, char **result)
{
	struct cstc_lua_load_info *data;
	int count, i;

	if (self->data == NULL)
		return 1;

	data = (struct cstc_lua_load_info *)self->data;

	if (results || data->initialized == false)
	{
		if (data->checked)
			free(data->checked);
		if (!(data->checked= calloc(data->dl->entry_count, sizeof(qboolean))))
			return 1;

		for (i=0, count=0; i<data->dl->entry_count; i++)
		{
			if (cstc_lua_load_check(data->dl->entries[i].name, self->tokenized_input))
			{
				data->checked[i] = true;
				count++;
			}
		}
		if (results)
			*results = count;

		data->initialized = true;
		return 0;
	}

	if (result == NULL)
		return 0;

	for (i=0, count=-1; i<data->dl->entry_count; i++)
	{
		if (data->checked[i] == true)
			count++;
		if (count == get_result)
		{
			*result = data->dl->entries[i].name;
			return 0;
		}
	}
	return 1;
}

/********
 * Init *
 ********/

void Lua_Init(void)
{
	Lua_Load_Scripts();
	lua_key_function.L = NULL;
	lua_key_function.function = NULL;
}

static void Lua_ClearRMF(struct lua_rmf **list)
{
	struct lua_rmf **l;

	l = list;
	if (l == NULL)
		return;
	while (*l)
	{
		free((*l)->function);
		free(*l);
		l++;
	}

	free(list);
}

void Lua_MessageFunctions(int level, const char *message)
{
	struct lua_rmf **l;

	l = lua_rmf_list;
	if (l == NULL)
		return;

	while (*l)
	{
		if ((*l)->l->buggy == 0)
		{
			lua_getglobal((*l)->L, (*l)->function);
			lua_pushnumber((*l)->L, level);
			lua_pushstring((*l)->L, message);
			if (lua_pcall((*l)->L, 2, 0, 0) != 0)
			{
				Com_Printf("lua error: %s\n", lua_tostring((*l)->L, -1));
				(*l)->l->buggy = 1;
			}
		}
		l++;
	}
}

void Lua_Shutdown(void)
{
	struct L_lua_states *ls, *lsc;
	struct lua_rmf **lo;

	ls = L_lua_states;

	while (ls)
	{
		lsc = ls;
		ls = ls->next;

		Lua_Clear_State(lsc);
		free(lsc);
	}

	L_lua_states = NULL;

	lo = lua_rmf_list;
	lua_rmf_list = NULL;
	lua_rmf_list_count = 0;
	Lua_ClearRMF(lo);

	lo = lua_d2d_list;
	lua_d2d_list = NULL;
	lua_d2d_list_count = 0;
	Lua_ClearRMF(lo);

	lo = lua_d2d_list;
	lua_d2d_list = NULL;
	lua_d2d_list_count = 0;
	Lua_ClearRMF(lo);

	lo = lua_callable_function_list;
	lua_callable_function_list = NULL;
	lua_callable_function_list_count = 0;
	Lua_ClearRMF(lo);

	lo = lua_frame_function_list;
	lua_frame_function_list = NULL;
	lua_frame_function_list_count = 0;
	Lua_ClearRMF(lo);
}

static void Lua_Restart(void)
{
	Lua_Shutdown();
	Lua_Init();
}

void Lua_CvarInit(void)
{
	Cmd_AddCommand("lua_list_cfunctions", Lua_List_CFunctions);
	Cmd_AddCommand("lua_restart", Lua_Restart);
	Cmd_AddCommand("lua_load", Lua_Load);
	CSTC_Add("lua_load", NULL, &cstc_lua_load_get_results, &cstc_lua_load_get_data, NULL, CSTC_EXECUTE, "arrow up/down to navigate");
}
#else
void Lua_Frame(void)
{
}

void Lua_MessageFunctions(int level, const char *message)
{
}

void Lua_Key(int key)
{
}

void Lua_Frame_2D(void)
{
}

void Lua_Init(void)
{
}

void Lua_Shutdown(void)
{
}

void Lua_CvarInit(void)
{
}
#endif

