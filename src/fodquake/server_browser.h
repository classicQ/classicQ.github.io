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

void SB_Init(void);
void SB_CvarInit(void);
void SB_Frame(void);
void SB_Key(int key);
void SB_Activate_f(void);


char *SB_Macro_Ip(void);
char *SB_Macro_Hostname(void);
char *SB_Macro_Map(void);
char *SB_Macro_Ping(void);
char *SB_Macro_Player(void);
char *SB_Macro_Max_Player(void);
char *SB_Macro_Player_Names(void);

void SB_Quit(void);
