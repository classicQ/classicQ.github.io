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
#include <math.h>

#include "quakedef.h"

#ifndef CLIENTONLY
#include "filesystem.h"
#endif

#ifdef GLQUAKE

#include "gl_local.h"

#include "particles.h"

#else			//software

#include "r_local.h"

#endif

#define DEFAULT_NUM_PARTICLES	2048
#define ABSOLUTE_MIN_PARTICLES	512
#define ABSOLUTE_MAX_PARTICLES	8192

static int	ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
static int	ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
static int	ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

static particle_t	*particles, *active_particles, *free_particles;

static int			r_numparticles;

#ifdef GLQUAKE
void Classic_LoadParticleTextures (void)
{
	int	i, x, y;
	unsigned int data[32][32];

	particletexture = texture_extension_number++;
	GL_Bind(particletexture);

	// clear to transparent white
	for (i = 0; i < 32 * 32; i++)
		((unsigned int*) data)[i] = COLOURMASK_RGBA;

	// draw a circle in the top left corner
	for (x = 0; x < 16; x++)
	{
		for (y = 0; y < 16; y++)
		{
			if ((x - 7.5) * (x - 7.5) + (y - 7.5) * (y - 7.5) <= 8 * 8)
				data[y][x] = 0xFFFFFFFF;	// solid white
		}
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GL_Upload32 ((unsigned int*) data, 32, 32, TEX_MIPMAP | TEX_ALPHA | TEX_NOSCALE);
}
#endif

static int Classic_InitParticles(void)
{
	int i;

	if ((i = COM_CheckParm ("-particles")) && i + 1 < com_argc)	{
		r_numparticles = (int) (Q_atoi(com_argv[i + 1]));
		r_numparticles = bound(ABSOLUTE_MIN_PARTICLES, r_numparticles, ABSOLUTE_MAX_PARTICLES);
	}
	else
	{
		r_numparticles = DEFAULT_NUM_PARTICLES;
	}

	particles = (particle_t *)malloc(r_numparticles * sizeof(particle_t));
	if (particles)
		return 1;

	return 0;
}

static void Classic_ShutdownParticles()
{
	free(particles);
}

static void Classic_ClearParticles (void)
{
	int		i;

	free_particles = &particles[0];
	active_particles = NULL;

	for (i = 0;i < r_numparticles; i++)
		particles[i].next = &particles[i+1];
	particles[r_numparticles - 1].next = NULL;
}

#ifndef CLIENTONLY
void R_ReadPointFile_f (void)
{
	FILE *f;
	vec3_t org;
	int r, c;
	particle_t *p;
	char name[MAX_OSPATH];

	if (!com_serveractive)
		return;

	Q_snprintfz (name, sizeof(name), "maps/%s.pts", mapname.string);

	if (FS_FOpenFile (name, &f) == -1)
	{
		Com_Printf ("couldn't open %s\n", name);
		return;
	}

	Com_Printf ("Reading %s...\n", name);
	c = 0;
	while (1)
	{
		r = fscanf (f,"%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;

		c++;
		if (!free_particles)
		{
			Com_Printf ("Not enough free particles\n");
			break;
		}
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = 99999;
		p->color = (-c)&15;
		p->type = pt_static;
		VectorClear (p->vel);
		VectorCopy (org, p->org);
	}

	fclose (f);
	Com_Printf ("%i points read\n", c);
}
#endif

static void Classic_ParticleExplosion(vec3_t org)
{
	int	i, j;
	particle_t	*p;

	CL_ExplosionSprite(org);

	if (r_explosiontype.value == 1)
		return;

	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 5;
		p->color = ramp1[0];
		p->ramp = rand() & 3;
		if (i & 1)
		{
			p->type = pt_explode;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_explode2;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand()%512) - 256;
			}
		}
	}
}

static void Classic_BlobExplosion(vec3_t org)
{
	int i, j;
	particle_t *p;

	for (i = 0; i < 1024; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 1 + (rand() & 8) * 0.05;

		if (i & 1)
		{
			p->type = pt_blob;
			p->color = 66 + rand() % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
		else
		{
			p->type = pt_blob2;
			p->color = 150 + rand() % 6;
			for (j = 0; j < 3; j++)
			{
				p->org[j] = org[j] + ((rand() % 32) - 16);
				p->vel[j] = (rand() % 512) - 256;
			}
		}
	}
}

static void Classic_RunParticleEffect(const vec3_t org, const vec3_t dir, int color, int count)
{
	int i, j, scale;
	particle_t *p;

	scale = (count > 130) ? 3 : (count > 20) ? 2  : 1;

	for (i = 0; i < count; i++)
	{
		if (!free_particles)
			return;
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		p->die = cl.time + 0.1 * (rand() % 5);
		p->color = (color & ~7) + (rand() & 7);
		p->type = pt_grav;
		for (j = 0; j < 3; j++)
		{
			p->org[j] = org[j] + scale * ((rand() & 15) - 8);
			p->vel[j] = dir[j] * 15;
		}
	}
}

static void Classic_LavaSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i++)
	{
		for (j = -16; j < 16; j++)
		{
			for (k = 0; k < 1; k++)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 2 + (rand() & 31) * 0.02;
				p->color = 224 + (rand() & 7);
				p->type = pt_grav;

				dir[0] = j * 8 + (rand() & 7);
				dir[1] = i * 8 + (rand() & 7);
				dir[2] = 256;

				p->org[0] = org[0] + dir[0];
				p->org[1] = org[1] + dir[1];
				p->org[2] = org[2] + (rand() & 63);

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

static void Classic_TeleportSplash(vec3_t org)
{
	int i, j, k;
	particle_t *p;
	float vel;
	vec3_t dir;

	for (i = -16; i < 16; i += 4)
	{
		for (j = -16; j < 16; j += 4)
		{
			for (k = -24; k < 32; k += 4)
			{
				if (!free_particles)
					return;
				p = free_particles;
				free_particles = p->next;
				p->next = active_particles;
				active_particles = p;

				p->die = cl.time + 0.2 + (rand() & 7) * 0.02;
				p->color = 7 + (rand() & 7);
				p->type = pt_grav;

				dir[0] = j * 8;
				dir[1] = i * 8;
				dir[2] = k * 8;

				p->org[0] = org[0] + i + (rand() & 3);
				p->org[1] = org[1] + j + (rand() & 3);
				p->org[2] = org[2] + k + (rand() & 3);

				VectorNormalizeFast (dir);
				vel = 50 + (rand() & 63);
				VectorScale (dir, vel, p->vel);
			}
		}
	}
}

static void Classic_ParticleTrail(vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type)
{
	vec3_t point, delta, dir;
	float len;
	int i, j, num_particles;
	particle_t *p;
	static int tracercount;

	VectorCopy (start, point);
	VectorSubtract (end, start, delta);
	if (!(len = VectorLength (delta)))
		goto done;
	VectorScale(delta, 1 / len, dir);	//unit vector in direction of trail

	switch (type)
	{
	case ALT_ROCKET_TRAIL:
		len /= 1.5; break;
	case BLOOD_TRAIL:
		len /= 6; break;
	default:
		len /= 3; break;
	}

	if (!(num_particles = (int) len))
		goto done;

	VectorScale (delta, 1.0 / num_particles, delta);

	for (i = 0; i < num_particles && free_particles; i++)
	{
		p = free_particles;
		free_particles = p->next;
		p->next = active_particles;
		active_particles = p;

		VectorClear (p->vel);
		p->die = cl.time + 2;

		switch(type)
		{
		case GRENADE_TRAIL:
			p->ramp = (rand() & 3) + 2;
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case BIG_BLOOD_TRAIL:
			p->type = pt_slowgrav;
			p->color = 67 + (rand() & 3);
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case TRACER1_TRAIL:
		case TRACER2_TRAIL:
			p->die = cl.time + 0.5;
			p->type = pt_static;
			if (type == TRACER1_TRAIL)
				p->color = 52 + ((tracercount & 4) << 1);
			else
				p->color = 230 + ((tracercount & 4) << 1);

			tracercount++;

			VectorCopy (point, p->org);
			if (tracercount & 1)
			{
				p->vel[0] = 90 * dir[1];
				p->vel[1] = 90 * -dir[0];
			}
			else
			{
				p->vel[0] = 90 * -dir[1];
				p->vel[1] = 90 * dir[0];
			}
			break;
		case VOOR_TRAIL:
			p->color = 9 * 16 + 8 + (rand() & 3);
			p->type = pt_static;
			p->die = cl.time + 0.3;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() & 15) - 8);
			break;
		case ALT_ROCKET_TRAIL:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		case ROCKET_TRAIL:
		default:
			p->ramp = (rand() & 3);
			p->color = ramp3[(int) p->ramp];
			p->type = pt_fire;
			for (j = 0; j < 3; j++)
				p->org[j] = point[j] + ((rand() % 6) - 3);
			break;
		}
		VectorAdd (point, delta, point);
	}
done:
	VectorCopy(point, *trail_origin);
}

static void Classic_DrawParticles(void)
{
	particle_t *p, *kill;
	int i;
	float time2, time3, time1, dvel, frametime, grav;

	if (!active_particles)
		return;

#ifdef GLQUAKE
	GL_DrawParticleBegin();
#else
	D_DrawParticleBegin();
#endif

	frametime = cls.frametime;
	if (cl.paused)
		frametime = 0;
	time3 = frametime * 15;
	time2 = frametime * 10; // 15;
	time1 = frametime * 5;
	grav = frametime * 800 * 0.05;
	dvel = 4 * frametime;

	while(1)
	{
		kill = active_particles;
		if (kill && kill->die < cl.time)
		{
			active_particles = kill->next;
			kill->next = free_particles;
			free_particles = kill;
			continue;
		}
		break;
	}

	for (p = active_particles; p ; p = p->next)
	{
		while (1)
		{
			kill = p->next;
			if (kill && kill->die < cl.time)
			{
				p->next = kill->next;
				kill->next = free_particles;
				free_particles = kill;
				continue;
			}
			break;
		}

#ifdef GLQUAKE
		GL_DrawParticle(p);
#else
		D_DrawParticle(p);
#endif

		p->org[0] += p->vel[0] * frametime;
		p->org[1] += p->vel[1] * frametime;
		p->org[2] += p->vel[2] * frametime;

		switch(p->type)
		{
			case pt_static:
				break;

			case pt_fire:
				p->ramp += time1;
				if (p->ramp >= 6)
					p->die = -1;
				else
					p->color = ramp3[(int) p->ramp];
				p->vel[2] += grav;
				break;

			case pt_explode:
				p->ramp += time2;
				if (p->ramp >=8)
					p->die = -1;
				else
					p->color = ramp1[(int) p->ramp];
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav * 30;
				break;

			case pt_explode2:
				p->ramp += time3;
				if (p->ramp >=8)
					p->die = -1;
				else
					p->color = ramp2[(int) p->ramp];
				for (i = 0; i < 3; i++)
					p->vel[i] -= p->vel[i] * frametime;
				p->vel[2] -= grav * 30;
				break;

			case pt_blob:
				for (i = 0; i < 3; i++)
					p->vel[i] += p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_blob2:
				for (i = 0; i < 2; i++)
					p->vel[i] -= p->vel[i] * dvel;
				p->vel[2] -= grav;
				break;

			case pt_slowgrav:
			case pt_grav:
				p->vel[2] -= grav;
				break;
		}
	}

#ifdef GLQUAKE
	GL_DrawParticleEnd();
#endif
}



int R_InitParticles(void)
{
	if (Classic_InitParticles())
	{
#ifdef GLQUAKE
		QMB_InitParticles();

		GL_DrawParticleInit();
#endif

		return 1;
	}

	return 0;
}

void R_ShutdownParticles()
{
#ifdef GLQUAKE
	QMB_ShutdownParticles();
#endif
	Classic_ShutdownParticles();
}

void R_ClearParticles(void)
{
	Classic_ClearParticles();
#ifdef GLQUAKE
	QMB_ClearParticles();
#endif
}

void R_DrawParticles(void)
{
	Classic_DrawParticles();
#ifdef GLQUAKE
	QMB_DrawParticles();
#endif
}

#define RunParticleEffect(var, org, dir, color, count)		\
	if (qmb_initialized && gl_part_##var.value)				\
		QMB_RunParticleEffect(org, dir, color, count);		\
	else													\
		Classic_RunParticleEffect(org, dir, color, count);

void R_RunParticleEffect(const vec3_t org, const vec3_t dir, int color, int count)
{
	#ifndef GLQUAKE

	Classic_RunParticleEffect(org, dir, color, count);

	#else

	if (color == 73 || color == 225)
	{
		RunParticleEffect(blood, org, dir, color, count);
		return;
	}

	switch (count)
	{
	case 10:
	case 20:
	case 30:
		RunParticleEffect(spikes, org, dir, color, count);
		break;
	default:
		RunParticleEffect(gunshots, org, dir, color, count);
	}

	#endif
}

void R_ParticleTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, trail_type_t type)
{
#ifdef GLQUAKE
	if (qmb_initialized && gl_part_trails.value)
		QMB_ParticleTrail(start, end, trail_origin, type);
	else
#endif
		Classic_ParticleTrail(start, end, trail_origin, type);
}

#ifdef GLQUAKE
#define ParticleFunction(var, name)				\
void R_##name (vec3_t org) {					\
	if (qmb_initialized && gl_part_##var.value)	\
		QMB_##name(org);						\
	else										\
		Classic_##name(org);					\
}
#else
#define ParticleFunction(var, name)	\
void R_##name (vec3_t org) {		\
	Classic_##name(org);			\
}
#endif

ParticleFunction(explosions, ParticleExplosion);
ParticleFunction(blobs, BlobExplosion);
ParticleFunction(lavasplash, LavaSplash);
ParticleFunction(telesplash, TeleportSplash);

