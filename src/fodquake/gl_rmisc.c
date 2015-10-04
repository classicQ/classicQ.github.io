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
// gl_rmisc.c

#include <string.h>
#include <stdlib.h>

#include "quakedef.h"
#include "gl_local.h"

#include "ruleset.h"


void R_InitOtherTextures (void)
{
	static const int flags = TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN;

	underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags );
	detailtexture = GL_LoadTextureImage("textures/detail", NULL, 256, 256, flags);
}

int R_InitTextures(void)
{
	int x,y, m;
	byte *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = malloc(sizeof(texture_t) + 16 * 16 + 8 * 8+4 * 4 + 2 * 2);
	if (r_notexture_mip)
	{
		strcpy(r_notexture_mip->name, "notexture");
		r_notexture_mip->width = r_notexture_mip->height = 16;
		r_notexture_mip->offsets[0] = sizeof(texture_t);
		r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
		r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
		r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

		for (m = 0; m < 4; m++)
		{
			dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
			for (y = 0; y < (16 >> m); y++)
			{
				for (x = 0; x < (16 >> m); x++)
				{
					if ((y < (8 >> m)) ^ (x < (8 >> m)))
						*dest++ = 0;
					else
						*dest++ = 0x0e;
				}
			}
		}

		return 1;
	}

	return 0;
}

void R_ShutdownTextures()
{
	free(r_notexture_mip);
}

void R_PreMapLoad()
{
	if (!dedicated)
		lightmode = gl_lightmode.value == 0 ? 0 : 2;
}

void R_NewMap (void)
{
	int	i, waterline;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;

	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	// identify sky texture
	for (i = 0; i < cl.worldmodel->numtextures; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		for (waterline = 0; waterline < 2; waterline++)
		{
 			cl.worldmodel->textures[i]->texturechain[waterline] = NULL;
			cl.worldmodel->textures[i]->texturechain_tail[waterline] = &cl.worldmodel->textures[i]->texturechain[waterline];
		}
	}
}

void R_TimeRefresh_f (void)
{
	int i;
	float start, stop, time;

	if (cls.state != ca_active)
		return;

	if (!(cl.spectator || cls.demoplayback || cl.standby) && !Ruleset_AllowTimeRefresh())
	{
		Com_Printf("Timerefresh's disabled during match\n");
		return;
	}

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++)
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer  (GL_BACK);
	VID_Update(0);
}

