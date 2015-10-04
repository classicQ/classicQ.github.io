/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2005-2012 Mark Olsen

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
// snd_dma.c -- main control for any streaming sound output device

#include <stdlib.h>
#include <string.h>

#include "quakedef.h"
#include "sound.h"

struct SoundCard *soundcard;

static void S_Play_f(void);
static void S_PlayVol_f(void);
static void S_SoundList_f(void);
static void S_Update_();
static void S_StopAllSounds_f(void);

// =======================================================================
// Internal sound data & structures
// =======================================================================

channel_t	channels[MAX_CHANNELS];
int			total_channels;

int			snd_blocked = 0;
qboolean	snd_initialized = false;

static qboolean		snd_ambient = 1;

static vec3_t	listener_origin;
static vec3_t	listener_forward;
static vec3_t	listener_right;
static vec3_t	listener_up;
static vec_t	sound_nominal_clip_dist = 1000.0;

static int	soundtime;		// sample PAIRS
int   		paintedtime; 	// sample PAIRS

#define	MAX_SFX	512
static sfx_t	*known_sfx;		// malloc allocated [MAX_SFX]
static int		num_sfx;

static sfx_t	*ambient_sfx[NUM_AMBIENTS];

static int soundtime_bufferwraps, soundtime_oldsamplepos;

static int sound_started = 0;

cvar_t bgmvolume = {"bgmvolume", "1", CVAR_ARCHIVE};
cvar_t s_volume = {"volume", "0.5", CVAR_ARCHIVE};
cvar_t s_nosound = {"s_nosound", "0"};
cvar_t s_precache = {"s_precache", "1"};
cvar_t s_loadas8bit = {"s_loadas8bit", "0"};
cvar_t s_khz = {"s_khz", "11"};
cvar_t s_ambientlevel = {"s_ambientlevel", "0.3"};
cvar_t s_ambientfade = {"s_ambientfade", "100"};
cvar_t s_noextraupdate = {"s_noextraupdate", "0"};
cvar_t s_show = {"s_show", "0"};
cvar_t s_mixahead = {"s_mixahead", "0.1", CVAR_ARCHIVE};
cvar_t s_swapstereo = {"s_swapstereo", "0"};
cvar_t s_driver = {"s_driver", "auto"};

struct SoundDriver
{
	char *name;
	SoundInitFunc *init;
	SoundCvarInitFunc *cvarinit;
};

SoundCvarInitFunc AHI_CvarInit;
SoundCvarInitFunc ALSA_CvarInit;
SoundCvarInitFunc CoreAudio_CvarInit;
SoundCvarInitFunc DS7_CvarInit;
SoundCvarInitFunc OSS_CvarInit;
SoundCvarInitFunc PulseAudio_CvarInit;
SoundCvarInitFunc SNDIO_CvarInit;
SoundCvarInitFunc WaveOut_CvarInit;

SoundInitFunc AHI_Init;
SoundInitFunc ALSA_Init;
SoundInitFunc CoreAudio_Init;
SoundInitFunc DS7_Init;
SoundInitFunc OSS_Init;
SoundInitFunc PulseAudio_Init;
SoundInitFunc SNDIO_Init;
SoundInitFunc WaveOut_Init;

const static struct SoundDriver sounddrivers[] =
{
	{ "AHI", &AHI_Init, &AHI_CvarInit },
	{ "SNDIO", &SNDIO_Init, &SNDIO_CvarInit },
	{ "OSS", &OSS_Init, &OSS_CvarInit },
	{ "ALSA", &ALSA_Init, &ALSA_CvarInit },
	{ "PulseAudio", &PulseAudio_Init, &PulseAudio_CvarInit },
	{ "DS7", &DS7_Init, &DS7_CvarInit },
	{ "WaveOut", &WaveOut_Init, &WaveOut_CvarInit },
	{ "CoreAudio", &CoreAudio_Init, &CoreAudio_CvarInit },
};

#define NUMSOUNDDRIVERS (sizeof(sounddrivers)/sizeof(*sounddrivers))

// ====================================================================
// User-setable variables
// ====================================================================

void S_SoundInfo_f (void)
{
	if (!sound_started || !soundcard)
	{
		Com_Printf ("sound system not started\n");
		return;
	}
	Com_Printf ("%s driver\n", soundcard->drivername);
	Com_Printf ("%5d stereo\n", soundcard->channels - 1);
	Com_Printf ("%5d samples\n", soundcard->samples);
	Com_Printf ("%5d samplepos\n", soundcard->samplepos);
	Com_Printf ("%5d samplebits\n", soundcard->samplebits);
	Com_Printf ("%5d speed\n", soundcard->speed);
	Com_Printf ("0x%p dma buffer\n", soundcard->buffer);
	Com_Printf ("%5d total_channels\n", total_channels);
}

static void S_InitDriver()
{
	int rc = false;
	int i;
	unsigned int rate;

	if (!snd_initialized)
		return;

	if (s_khz.value == 44)
		rate = 44100;
	else if (s_khz.value == 22)
		rate = 22050;
	else
		rate = 11025;

	soundcard = malloc(sizeof(*soundcard));
	if (soundcard)
	{
		for(i=0;i<NUMSOUNDDRIVERS;i++)
		{
			if (*sounddrivers[i].init)
			{
				if(Q_strcasecmp(s_driver.string, "auto") != 0)
					if(Q_strcasecmp(sounddrivers[i].name, s_driver.string) != 0)
						continue;

				memset(soundcard, 0, sizeof(*soundcard));
				rc = (*sounddrivers[i].init)(soundcard, rate, 2, 16);
				if (rc)
				{
					soundcard->drivername = sounddrivers[i].name;
					break;
				}
			}
		}

		if (i == NUMSOUNDDRIVERS)
		{
			free(soundcard);
			soundcard = 0;
		}
	}

	if (!rc)
	{
		Com_ErrorPrintf("Unable to initialise sound output.\n");
		if(Q_strcasecmp(s_driver.string, "auto") != 0)
			Com_Printf("WARNING: You have specified a custom s_driver which may be the cause of failure. Try setting it to \"auto\"\n");
		free(soundcard);
		soundcard = 0;
		sound_started = 0;
		return;
	}

	soundtime_bufferwraps = 0;
	soundtime_oldsamplepos = 0;
	paintedtime = 0;

	sound_started = 1;
}

static void S_ShutdownDriver()
{
	if (!sound_started)
		return;

	sound_started = 0;

	soundcard->Shutdown(soundcard);

	free(soundcard);
	soundcard = 0;
}

void SND_Restart_f (void)
{
	int i;

	S_ShutdownDriver();
	sound_started = 0;

	for(i=0;i<num_sfx;i++)
	{
		free(known_sfx[i].sfxcache);
		known_sfx[i].sfxcache = 0;
	}

	if (!s_nosound.value)
	{
		S_InitDriver();

		for(i=0;i<num_sfx;i++)
		{
			S_LoadSound(&known_sfx[i]);
		}

		S_StopAllSounds(true);
	}
}

void S_CvarInit(void)
{
	unsigned int i;

	Cvar_SetCurrentGroup(CVAR_GROUP_SOUND);
	Cvar_Register(&bgmvolume);
	Cvar_Register(&s_volume);
	Cvar_Register(&s_nosound);
	Cvar_Register(&s_precache);
	Cvar_Register(&s_loadas8bit);
	Cvar_Register(&s_khz);
	Cvar_Register(&s_ambientlevel);
	Cvar_Register(&s_ambientfade);
	Cvar_Register(&s_noextraupdate);
	Cvar_Register(&s_show);
	Cvar_Register(&s_mixahead);
	Cvar_Register(&s_swapstereo);
	Cvar_Register(&s_driver);

	Cvar_ResetCurrentGroup();

	// compatibility with old configs
	Cmd_AddLegacyCommand ("volume", "s_volume");
	Cmd_AddLegacyCommand ("nosound", "s_nosound");
	Cmd_AddLegacyCommand ("precache", "s_precache");
	Cmd_AddLegacyCommand ("loadas8bit", "s_loadas8bit");
	Cmd_AddLegacyCommand ("ambient_level", "s_ambientlevel");
	Cmd_AddLegacyCommand ("ambient_fade", "s_ambientfade");
	Cmd_AddLegacyCommand ("snd_noextraupdate", "s_noextraupdate");
	Cmd_AddLegacyCommand ("snd_show", "s_show");
	Cmd_AddLegacyCommand ("_snd_mixahead", "s_mixahead");

	Cmd_AddCommand("snd_restart", SND_Restart_f);
	Cmd_AddCommand("play", S_Play_f);
	Cmd_AddCommand("playvol", S_PlayVol_f);
	Cmd_AddCommand("stopsound", S_StopAllSounds_f);
	Cmd_AddCommand("soundlist", S_SoundList_f);
	Cmd_AddCommand("soundinfo", S_SoundInfo_f);

	for(i=0;i<NUMSOUNDDRIVERS;i++)
	{
		if (*sounddrivers[i].cvarinit)
		{
			(*sounddrivers[i].cvarinit)();
		}
	}

	snd_initialized = true;
}

void S_Init(void)
{
	if (!s_nosound.value)
		S_InitDriver();

	SND_InitScaletable();

	known_sfx = malloc(MAX_SFX*sizeof(sfx_t));
	if (known_sfx == 0)
		Sys_Error("S_Init: Out of memory for sound table\n");

	memset(known_sfx, 0, MAX_SFX*sizeof(sfx_t));

	num_sfx = 0;

	ambient_sfx[AMBIENT_WATER] = S_PrecacheSound("ambience/water1.wav");
	ambient_sfx[AMBIENT_SKY] = S_PrecacheSound("ambience/wind2.wav");

	S_StopAllSounds(true);
}

// =======================================================================
// Shutdown sound engine
// =======================================================================

void S_Shutdown(void)
{
	S_ShutdownDriver();
	free(known_sfx);
}

// =======================================================================
// Load a sound
// =======================================================================

static sfx_t *S_FindName (char *name)
{
	int i;
	sfx_t *sfx;

	if (!name)
		Sys_Error ("S_FindName: NULL");

	if (strlen(name) >= MAX_QPATH)
		Sys_Error ("Sound name too long: %s", name);

	// see if already loaded
	for (i = 0; i < num_sfx; i++)
	{
		if (!strcmp(known_sfx[i].name, name))
			return &known_sfx[i];
	}

	if (num_sfx == MAX_SFX)
		Sys_Error ("S_FindName: out of sfx_t");

	sfx = &known_sfx[i];
	strcpy (sfx->name, name);

	num_sfx++;

	return sfx;
}

sfx_t *S_PrecacheSound (char *name)
{
	sfx_t *sfx;

	sfx = S_FindName (name);

	// cache it in
	if (s_precache.value)
		S_LoadSound (sfx);

	return sfx;
}

//=============================================================================

channel_t *SND_PickChannel (int entnum, int entchannel)
{
	int ch_idx, first_to_die, life_left;

	// Check for replacement sound, or find the best one to replace
	first_to_die = -1;
	life_left = 0x7fffffff;
	for (ch_idx = NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++)
	{
		if (entchannel != 0		// channel 0 never overrides
			&& channels[ch_idx].entnum == entnum
			&& (channels[ch_idx].entchannel == entchannel || entchannel == -1) )
		{	// always override sound from same entity
			first_to_die = ch_idx;
			break;
		}

		// don't let monster sounds override player sounds
		if (channels[ch_idx].entnum == cl.playernum+1 && entnum != cl.playernum+1 && channels[ch_idx].sfx)
			continue;

		if (channels[ch_idx].end - paintedtime < life_left)
		{
			life_left = channels[ch_idx].end - paintedtime;
			first_to_die = ch_idx;
		}
	}

	if (first_to_die == -1)
		return NULL;

	if (channels[first_to_die].sfx)
		channels[first_to_die].sfx = NULL;

	return &channels[first_to_die];
}

void SND_Spatialize (channel_t *ch)
{
	vec_t dot, dist, lscale, rscale, scale;
	vec3_t source_vec;

	// anything coming from the view entity will always be full volume
	if (ch->entnum == cl.playernum + 1)
	{
		ch->leftvol = ch->master_vol;
		ch->rightvol = ch->master_vol;
		return;
	}

	// calculate stereo seperation and distance attenuation

	VectorSubtract(ch->origin, listener_origin, source_vec);

	dist = VectorNormalize(source_vec) * ch->dist_mult;
	dot = DotProduct(listener_right, source_vec);

	if (soundcard->channels == 1)
	{
		rscale = 1.0;
		lscale = 1.0;
	}
	else
	{
		rscale = 1.0 + dot;
		lscale = 1.0 - dot;
	}

	// add in distance effect
	scale = (1.0 - dist) * rscale;
	ch->rightvol = (int) (ch->master_vol * scale);
	if (ch->rightvol < 0)
		ch->rightvol = 0;

	scale = (1.0 - dist) * lscale;
	ch->leftvol = (int) (ch->master_vol * scale);
	if (ch->leftvol < 0)
		ch->leftvol = 0;
}

// =======================================================================
// Start a sound effect
// =======================================================================

void S_StartSound(int entnum, int entchannel, sfx_t *sfx, const vec3_t origin, float fvol, float attenuation)
{
	channel_t *target_chan, *check;
	sfxcache_t *sc;
	int vol, ch_idx, skip;

	if (!sound_started)
		return;

	if (!sfx)
		return;

	if (s_nosound.value)
		return;

	vol = fvol * 255;

	// pick a channel to play on
	target_chan = SND_PickChannel(entnum, entchannel);
	if (!target_chan)
		return;

	// spatialize
	memset (target_chan, 0, sizeof(*target_chan));
	VectorCopy(origin, target_chan->origin);
	target_chan->dist_mult = attenuation / sound_nominal_clip_dist;
	target_chan->master_vol = vol;
	target_chan->entnum = entnum;
	target_chan->entchannel = entchannel;
	SND_Spatialize(target_chan);

	if (!target_chan->leftvol && !target_chan->rightvol)
		return;		// not audible at all

	// new channel
	sc = S_LoadSound (sfx);
	if (!sc)
	{
		target_chan->sfx = NULL;
		return;		// couldn't load the sound's data
	}

	target_chan->sfx = sfx;
	target_chan->pos = 0.0;
	target_chan->end = paintedtime + sc->length;

	// if an identical sound has also been started this frame, offset the pos
	// a bit to keep it from just making the first one louder
	check = &channels[NUM_AMBIENTS];
	for (ch_idx=NUM_AMBIENTS; ch_idx < NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS; ch_idx++, check++)
	{
		if (check == target_chan)
			continue;
		if (check->sfx == sfx && !check->pos)
		{
			skip = rand () % (int)(0.1 * soundcard->speed);
			if (skip >= target_chan->end)
				skip = target_chan->end - 1;
			target_chan->pos += skip;
			target_chan->end -= skip;
			break;
		}
	}
}

void S_StopSound (int entnum, int entchannel)
{
	int i;

	for (i = 0; i < MAX_DYNAMIC_CHANNELS; i++)
	{
		if (channels[i].entnum == entnum && channels[i].entchannel == entchannel)
		{
			channels[i].end = 0;
			channels[i].sfx = NULL;
			return;
		}
	}
}

void S_StopAllSounds (qboolean clear)
{
	int i;

	total_channels = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;	// no statics

	for (i = 0; i < MAX_CHANNELS; i++)
	{
		if (channels[i].sfx)
			channels[i].sfx = NULL;
	}

	memset(channels, 0, MAX_CHANNELS * sizeof(channel_t));

	if (!sound_started)
		return;

	if (clear)
		S_ClearBuffer ();
}

static void S_StopAllSounds_f(void)
{
	S_StopAllSounds (true);
}

void S_ClearBuffer (void)
{
	unsigned char *buffer;
	int clear;

	if (!sound_started || !soundcard || !soundcard->buffer)
		return;

	clear = (soundcard->samplebits == 8) ? 0x80 : 0;

	if (soundcard->Lock)
		buffer = soundcard->Lock(soundcard);
	else
		buffer = soundcard->buffer;

	memset(buffer, clear, soundcard->samples * soundcard->samplebits/8);

	if (soundcard->Unlock)
		soundcard->Unlock(soundcard);
}

void S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	channel_t *ss;
	sfxcache_t *sc;

	if (!sfx)
		return;

	if (total_channels == MAX_CHANNELS)
	{
		Com_Printf ("total_channels == MAX_CHANNELS\n");
		return;
	}

	ss = &channels[total_channels];
	total_channels++;

	sc = S_LoadSound (sfx);
	if (!sc)
		return;

	if (sc->loopstart == -1)
	{
		Com_Printf ("Sound %s not looped\n", sfx->name);
		return;
	}

	ss->sfx = sfx;
	VectorCopy (origin, ss->origin);
	ss->master_vol = vol;
	ss->dist_mult = (attenuation/64) / sound_nominal_clip_dist;
	ss->end = paintedtime + sc->length;

	SND_Spatialize (ss);
}

//=============================================================================

static void S_UpdateAmbientSounds (void)
{
	mleaf_t *l;
	float vol;
	int ambient_channel;
	channel_t *chan;

	if (!snd_ambient)
		return;

	// calc ambient sound levels
	if (!cl.worldmodel)
		return;

	l = Mod_PointInLeaf (listener_origin, cl.worldmodel);
	if (!l || !s_ambientlevel.value)
	{
		for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
			channels[ambient_channel].sfx = NULL;
		return;
	}

	for (ambient_channel = 0 ; ambient_channel< NUM_AMBIENTS ; ambient_channel++)
	{
		chan = &channels[ambient_channel];
		chan->sfx = ambient_sfx[ambient_channel];

		vol = s_ambientlevel.value * l->ambient_sound_level[ambient_channel];
		if (vol < 8)
			vol = 0;

		// don't adjust volume too fast
		if (chan->master_vol < vol)
		{
			chan->master_vol += cls.frametime * s_ambientfade.value;
			if (chan->master_vol > vol)
				chan->master_vol = vol;
		}
		else if (chan->master_vol > vol)
		{
			chan->master_vol -= cls.frametime * s_ambientfade.value;
			if (chan->master_vol < vol)
				chan->master_vol = vol;
		}

		chan->leftvol = chan->rightvol = chan->master_vol;
	}
}

//Called once each time through the main loop
void S_Update(const vec3_t origin, const vec3_t forward, const vec3_t right, const vec3_t up)
{
	int i, j, total;
	channel_t *ch, *combine;

	if (!sound_started || (snd_blocked > 0))
		return;

	VectorCopy(origin, listener_origin);
	VectorCopy(forward, listener_forward);
	VectorCopy(right, listener_right);
	VectorCopy(up, listener_up);

	// update general area ambient sound sources
	S_UpdateAmbientSounds ();

	combine = NULL;

	// update spatialization for static and dynamic sounds
	ch = channels+NUM_AMBIENTS;
	for (i = NUM_AMBIENTS; i < total_channels; i++, ch++)
	{
		if (!ch->sfx)
			continue;
		SND_Spatialize(ch);         // respatialize channel
		if (!ch->leftvol && !ch->rightvol)
			continue;

		// try to combine static sounds with a previous channel of the same
		// sound effect so we don't mix five torches every frame

		if (i >= MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS)
		{
			// see if it can just use the last one
			if (combine && combine->sfx == ch->sfx)
			{
				combine->leftvol += ch->leftvol;
				combine->rightvol += ch->rightvol;
				ch->leftvol = ch->rightvol = 0;
				continue;
			}
			// search for one
			combine = channels+MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS;
			for (j = MAX_DYNAMIC_CHANNELS + NUM_AMBIENTS; j < i; j++, combine++)
				if (combine->sfx == ch->sfx)
					break;

			if (j == total_channels)
			{
				combine = NULL;
			}
			else
			{
				if (combine != ch)
				{
					combine->leftvol += ch->leftvol;
					combine->rightvol += ch->rightvol;
					ch->leftvol = ch->rightvol = 0;
				}
				continue;
			}
		}
	}

	// debugging output
	if (s_show.value)
	{
		total = 0;
		ch = channels;
		for (i = 0; i < total_channels; i++, ch++)
			if (ch->sfx && (ch->leftvol || ch->rightvol))
			{
				//Com_Printf ("%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->sfx->name);
				total++;
			}

		Com_Printf ("----(%i)----\n", total);
	}

	// mix some sound
	S_Update_();
}

void GetSoundtime (void)
{
	int samplepos, fullsamples;

	fullsamples = soundcard->samples / soundcard->channels;

	// it is possible to miscount buffers if it has wrapped twice between calls to S_Update.  Oh well.
	samplepos = soundcard->GetDMAPos(soundcard);

	if (samplepos < soundtime_oldsamplepos)
	{
		soundtime_bufferwraps++;					// buffer wrapped

		if (paintedtime > 0x40000000)
		{
			// time to chop things off to avoid 32 bit limits
			soundtime_bufferwraps = 0;
			paintedtime = fullsamples;
			S_StopAllSounds (true);
		}
	}

	soundtime_oldsamplepos = samplepos;

	soundtime = soundtime_bufferwraps * fullsamples + samplepos/soundcard->channels;
}

void S_ExtraUpdate(void)
{
	if (s_noextraupdate.value)
		return;		// don't pollute timings
	S_Update_();
}

static void S_Update_(void)
{
	unsigned endtime;
	int samps;
	int avail;

	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	GetSoundtime();

	// check to make sure that we haven't overshot
	if (paintedtime < soundtime)
	{
		//Com_Printf ("S_Update_ : overflow\n");
		paintedtime = soundtime;
	}

	// mix ahead of current position
	if (soundcard->GetAvail)
	{
		avail = soundcard->GetAvail(soundcard);
		if (avail <= 0)
			return;

		endtime = soundtime + avail;
	}
	else
		endtime = soundtime + (int)(s_mixahead.value * soundcard->speed);
	samps = soundcard->samples >> (soundcard->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	if (soundcard->Restore)
		soundcard->Restore(soundcard);

	S_PaintChannels(endtime);

	soundcard->Submit(soundcard, paintedtime - soundtime);
}

/*
===============================================================================
console functions
===============================================================================
*/

static void S_Play_f(void)
{
	int i;
	char name[256];
	sfx_t *sfx;
	static int hash = 345;

	for (i = 1; i < Cmd_Argc(); i++)
	{
		Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
		COM_DefaultExtension(name, ".wav");
		sfx = S_PrecacheSound(name);
		S_StartSound(hash++, 0, sfx, listener_origin, 1.0, 0.0);
	}
}

static void S_PlayVol_f(void)
{
	int i;
	float vol;
	char name[256];
	sfx_t *sfx;
	static int hash = 543;

	for (i = 1; i+1 < Cmd_Argc(); i += 2)
	{
		Q_strncpyz(name, Cmd_Argv(i), sizeof(name));
		COM_DefaultExtension(name, ".wav");
		sfx = S_PrecacheSound(name);
		vol = Q_atof(Cmd_Argv(i + 1));
		S_StartSound(hash++, 0, sfx, listener_origin, vol, 0.0);
	}
}

static void S_SoundList_f(void)
{
	int i, size, total;
	sfx_t *sfx;
	sfxcache_t *sc;

	total = 0;
	for (sfx = known_sfx, i = 0; i < num_sfx; i++, sfx++)
	{
		sc = sfx->sfxcache;
		if (!sc)
			continue;
		size = sc->length * sc->width * (sc->stereo + 1);
		total += size;
		if (sc->loopstart >= 0)
			Com_Printf("L");
		else
			Com_Printf(" ");
		Com_Printf("(%2db) %6i : %s\n",sc->width*8,  size, sfx->name);
	}
	Com_Printf("Total resident: %i\n", total);
}

void S_LocalSound (char *sound)
{
	sfx_t *sfx;

	if (s_nosound.value)
		return;
	if (!sound_started)
		return;

	sfx = S_PrecacheSound (sound);
	if (!sfx)
	{
		Com_Printf ("S_LocalSound: can't cache %s\n", sound);
		return;
	}
	S_StartSound (cl.playernum+1, -1, sfx, vec3_origin, 1, 0);
}

