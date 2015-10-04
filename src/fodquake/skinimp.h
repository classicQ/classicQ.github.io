/*
Copyright (C) 2012 Mark Olsen

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

struct SkinImp *SkinImp_CreateSolidColour(float *colours);
struct SkinImp *SkinImp_CreateTexturePaletted(void *data, unsigned int width, unsigned int height, unsigned int modulo);
struct SkinImp *SkinImp_CreateTextureTruecolour(void *data, unsigned int width, unsigned int height);
void SkinImp_Destroy(struct SkinImp *skinimp);

