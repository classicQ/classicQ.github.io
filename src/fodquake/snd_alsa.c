/*
	snd_alsa.c

	Support for the ALSA 1.0.1 sound driver

	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
//actually stolen from darkplaces.
//I guess noone can be arsed to write it themselves. :/
//
//This file is otherwise known as 'will the linux jokers please stop fucking over the open sound system please'
/* ^^^ and that's the bloody truth (and I stole this from FTE) */

#include <alsa/asoundlib.h>

#include "quakedef.h"
#include "sound.h"
#include <dlfcn.h>

static cvar_t snd_alsa_device = { "snd_alsa_device", "hw", CVAR_ARCHIVE };

struct alsa_private
{
	void *alsasharedobject;
	snd_pcm_t *handle;

	int (*psnd_pcm_open)				(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
	int (*psnd_pcm_close)				(snd_pcm_t *pcm);
	const char *(*psnd_strerror)			(int errnum);
	int (*psnd_pcm_hw_params_any)			(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int (*psnd_pcm_hw_params_set_access)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
	int (*psnd_pcm_hw_params_set_format)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
	int (*psnd_pcm_hw_params_set_channels)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
	int (*psnd_pcm_hw_params_set_rate_near)		(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
	int (*psnd_pcm_hw_params_set_period_size_near)	(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir);
	int (*psnd_pcm_hw_params)			(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int (*psnd_pcm_sw_params_current)		(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
	int (*psnd_pcm_sw_params_set_start_threshold)	(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
	int (*psnd_pcm_sw_params_set_stop_threshold)	(snd_pcm_t *pcm, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val);
	int (*psnd_pcm_sw_params)			(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
	int (*psnd_pcm_hw_params_get_buffer_size)	(const snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
	int (*psnd_pcm_hw_params_set_buffer_size_near)	(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val);
	snd_pcm_sframes_t (*psnd_pcm_avail_update)	(snd_pcm_t *pcm);
	int (*psnd_pcm_mmap_begin)			(snd_pcm_t *pcm, const snd_pcm_channel_area_t **areas, snd_pcm_uframes_t *offset, snd_pcm_uframes_t *frames);
	snd_pcm_sframes_t (*psnd_pcm_mmap_commit)	(snd_pcm_t *pcm, snd_pcm_uframes_t offset, snd_pcm_uframes_t frames);
	snd_pcm_state_t (*psnd_pcm_state)		(snd_pcm_t *pcm);
	int (*psnd_pcm_start)				(snd_pcm_t *pcm);

	size_t (*psnd_pcm_hw_params_sizeof)		(void);
	size_t (*psnd_pcm_sw_params_sizeof)		(void);
};


static int alsa_getdmapos(struct SoundCard *sc)
{
	struct alsa_private *p;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes;

	p = sc->driverprivate;

	nframes = sc->samples / sc->channels;

	p->psnd_pcm_avail_update(p->handle);
	p->psnd_pcm_mmap_begin(p->handle, &areas, &offset, &nframes);
	offset *= sc->channels;
	nframes *= sc->channels;
	sc->samplepos = offset;
	sc->buffer = areas->addr;
	return sc->samplepos;
}

static void alsa_shutdown(struct SoundCard *sc)
{
	struct alsa_private *p;

	p = sc->driverprivate;

	p->psnd_pcm_close(p->handle);

	dlclose(p->alsasharedobject);

	free(p);
}

static void alsa_submit(struct SoundCard *sc, unsigned int count)
{
	struct alsa_private *p;
	int state;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t nframes;
	snd_pcm_uframes_t offset;

	p = sc->driverprivate;

	nframes = count / sc->channels;

	p->psnd_pcm_avail_update(p->handle);
	p->psnd_pcm_mmap_begin(p->handle, &areas, &offset, &nframes);

	state = p->psnd_pcm_state(p->handle);

	switch (state)
	{
		case SND_PCM_STATE_PREPARED:
			p->psnd_pcm_mmap_commit(p->handle, offset, nframes);
			p->psnd_pcm_start(p->handle);
			break;
		case SND_PCM_STATE_RUNNING:
			p->psnd_pcm_mmap_commit(p->handle, offset, nframes);
			break;
		default:
			break;
	}
}

static qboolean Alsa_InitAlsa(struct alsa_private *p)
{
	qboolean ret;

	// Try alternative names of libasound, sometimes it is not linked correctly.
	p->alsasharedobject = dlopen("libasound.so.2", RTLD_LAZY|RTLD_LOCAL);
	if (p->alsasharedobject == 0)
	{
		p->alsasharedobject = dlopen("libasound.so", RTLD_LAZY|RTLD_LOCAL);
		if (p->alsasharedobject == 0)
		{
			return false;
		}
	}

	p->psnd_pcm_open = dlsym(p->alsasharedobject, "snd_pcm_open");
	p->psnd_pcm_close = dlsym(p->alsasharedobject, "snd_pcm_close");
	p->psnd_strerror = dlsym(p->alsasharedobject, "snd_strerror");
	p->psnd_pcm_hw_params_any = dlsym(p->alsasharedobject, "snd_pcm_hw_params_any");
	p->psnd_pcm_hw_params_set_access = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_access");
	p->psnd_pcm_hw_params_set_format = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_format");
	p->psnd_pcm_hw_params_set_channels = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_channels");
	p->psnd_pcm_hw_params_set_rate_near = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_rate_near");
	p->psnd_pcm_hw_params_set_period_size_near = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_period_size_near");
	p->psnd_pcm_hw_params = dlsym(p->alsasharedobject, "snd_pcm_hw_params");
	p->psnd_pcm_sw_params_current = dlsym(p->alsasharedobject, "snd_pcm_sw_params_current");
	p->psnd_pcm_sw_params_set_start_threshold = dlsym(p->alsasharedobject, "snd_pcm_sw_params_set_start_threshold");
	p->psnd_pcm_sw_params_set_stop_threshold = dlsym(p->alsasharedobject, "snd_pcm_sw_params_set_stop_threshold");
	p->psnd_pcm_sw_params = dlsym(p->alsasharedobject, "snd_pcm_sw_params");
	p->psnd_pcm_hw_params_get_buffer_size = dlsym(p->alsasharedobject, "snd_pcm_hw_params_get_buffer_size");
	p->psnd_pcm_avail_update = dlsym(p->alsasharedobject, "snd_pcm_avail_update");
	p->psnd_pcm_mmap_begin = dlsym(p->alsasharedobject, "snd_pcm_mmap_begin");
	p->psnd_pcm_state = dlsym(p->alsasharedobject, "snd_pcm_state");
	p->psnd_pcm_mmap_commit = dlsym(p->alsasharedobject, "snd_pcm_mmap_commit");
	p->psnd_pcm_start = dlsym(p->alsasharedobject, "snd_pcm_start");
	p->psnd_pcm_hw_params_sizeof = dlsym(p->alsasharedobject, "snd_pcm_hw_params_sizeof");
	p->psnd_pcm_sw_params_sizeof = dlsym(p->alsasharedobject, "snd_pcm_sw_params_sizeof");
	p->psnd_pcm_hw_params_set_buffer_size_near = dlsym(p->alsasharedobject, "snd_pcm_hw_params_set_buffer_size_near");

	ret = p->psnd_pcm_open
	   && p->psnd_pcm_close
	   && p->psnd_strerror
	   && p->psnd_pcm_hw_params_any
	   && p->psnd_pcm_hw_params_set_access
	   && p->psnd_pcm_hw_params_set_format
	   && p->psnd_pcm_hw_params_set_channels
	   && p->psnd_pcm_hw_params_set_rate_near
	   && p->psnd_pcm_hw_params_set_period_size_near
	   && p->psnd_pcm_hw_params
	   && p->psnd_pcm_sw_params_current
	   && p->psnd_pcm_sw_params_set_start_threshold
	   && p->psnd_pcm_sw_params_set_stop_threshold
	   && p->psnd_pcm_sw_params
	   && p->psnd_pcm_hw_params_get_buffer_size
	   && p->psnd_pcm_avail_update
	   && p->psnd_pcm_mmap_begin
	   && p->psnd_pcm_state
	   && p->psnd_pcm_mmap_commit
	   && p->psnd_pcm_start
	   && p->psnd_pcm_hw_params_sizeof
	   && p->psnd_pcm_sw_params_sizeof
	   && p->psnd_pcm_hw_params_set_buffer_size_near;

	if (!ret)
	{
		dlclose(p->alsasharedobject);
	}

	return ret;
}

static void alsa_cvarinit()
{
	Cvar_Register(&snd_alsa_device);
}

static qboolean alsa_init_internal(struct SoundCard *sc, const char *device, int rate, int channels, int bits)
{
	struct alsa_private *p;
	snd_pcm_t   *pcm;
	snd_pcm_uframes_t buffer_size;

	int					 err;
	snd_pcm_hw_params_t	*hw;
	snd_pcm_sw_params_t	*sw;
	snd_pcm_uframes_t	 frag_size;

	if (!*device)
		return false;

	p = malloc(sizeof(*p));
	if (p)
	{
		if (Alsa_InitAlsa(p))
		{
			hw = alloca(p->psnd_pcm_hw_params_sizeof());
			sw = alloca(p->psnd_pcm_sw_params_sizeof());
			memset(sw, 0, p->psnd_pcm_sw_params_sizeof());
			memset(hw, 0, p->psnd_pcm_hw_params_sizeof());

			//WARNING: 'default' as the default sucks arse. it adds about a second's worth of lag.

			Com_Printf("Initialising ALSA sound device \"%s\"\n", device);

			pcm = 0;
			err = p->psnd_pcm_open(&pcm, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
			if (0 > err)
			{
				Com_Printf ("Error: open error: %s\n", p->psnd_strerror(err));
				goto errorfoo;
			}
			Com_Printf ("ALSA: Using PCM %s.\n", device);

			err = p->psnd_pcm_hw_params_any(pcm, hw);
			if (0 > err) {
				Com_Printf ("ALSA: error setting hw_params_any. %s\n",
							p->psnd_strerror(err));
				goto error;
			}

			err = p->psnd_pcm_hw_params_set_access(pcm, hw,  SND_PCM_ACCESS_MMAP_INTERLEAVED);
			if (0 > err) 
			{
				Com_Printf ("ALSA: Failure to set noninterleaved PCM access. %s\n"
							"Note: Interleaved is not supported\n",
							p->psnd_strerror(err));
				goto error;
			}

			// get sample bit size
			{
				snd_pcm_format_t spft;
				if (bits == 16)
					spft = SND_PCM_FORMAT_S16;
				else
					spft = SND_PCM_FORMAT_U8;

				err = p->psnd_pcm_hw_params_set_format(pcm, hw, spft);
				while (err < 0)
				{
					if (spft == SND_PCM_FORMAT_S16)
					{
						bits = 8;
						spft = SND_PCM_FORMAT_U8;
					}
					else
					{
						Com_Printf ("ALSA: no usable formats. %s\n", p->psnd_strerror(err));
						goto error;
					}
					err = p->psnd_pcm_hw_params_set_format(pcm, hw, spft);
				}

				sc->samplebits = bits;
			}

			// get speaker channels
			err = p->psnd_pcm_hw_params_set_channels(pcm, hw, channels);
			while (err < 0) 
			{
				if (channels > 2)
					channels = 2;
				else if (channels > 1)
					channels = 1;
				else
				{
					Com_Printf ("ALSA: no usable number of channels. %s\n", p->psnd_strerror(err));
					goto error;
				}
				err = p->psnd_pcm_hw_params_set_channels(pcm, hw, channels);
			}

			sc->channels = channels;

			// get rate
			err = p->psnd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
			while (err < 0)
			{
				if (rate > 48000)
					rate = 48000;
				else if (rate > 44100)
					rate = 44100;
				else if (rate > 22150)
					rate = 22150;
				else if (rate > 11025)
					rate = 11025;
				else if (rate > 800)
					rate = 800;
				else
				{
					Com_Printf ("ALSA: no usable rates. %s\n", p->psnd_strerror(err));
					goto error;
				}
				err = p->psnd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
			}

			if (rate > 11025)
				frag_size = 8 * bits * rate / 11025;
			else
				frag_size = 8 * bits;

			err = p->psnd_pcm_hw_params_set_period_size_near(pcm, hw, &frag_size, 0);
			if (0 > err) {
				Com_Printf ("ALSA: unable to set period size near %i. %s\n",
							(int) frag_size, p->psnd_strerror(err));
				goto error;
			}
			err = p->psnd_pcm_hw_params(pcm, hw);
			if (0 > err) {
				Com_Printf ("ALSA: unable to install hw params: %s\n",
							p->psnd_strerror(err));
				goto error;
			}
			err = p->psnd_pcm_sw_params_current(pcm, sw);
			if (0 > err) {
				Com_Printf ("ALSA: unable to determine current sw params. %s\n",
							p->psnd_strerror(err));
				goto error;
			}
			err = p->psnd_pcm_sw_params_set_start_threshold(pcm, sw, ~0U);
			if (0 > err) {
				Com_Printf ("ALSA: unable to set playback threshold. %s\n",
					p->psnd_strerror(err));
				goto error;
			}
			err = p->psnd_pcm_sw_params_set_stop_threshold(pcm, sw, ~0U);
			if (0 > err) {
				Com_Printf ("ALSA: unable to set playback stop threshold. %s\n",
							p->psnd_strerror(err));
				goto error;
			}
			err = p->psnd_pcm_sw_params(pcm, sw);
			if (0 > err) {
				Com_Printf ("ALSA: unable to install sw params. %s\n",
							p->psnd_strerror(err));
				goto error;
			}

			sc->channels = channels;

			buffer_size = 32768 / channels;
			if (buffer_size)
			{
				err = p->psnd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buffer_size);
				if (err < 0)
				{
					Com_Printf ("ALSA: unable to set buffer size. %s\n", p->psnd_strerror(err));
					goto error;
				}
			}

			err = p->psnd_pcm_hw_params_get_buffer_size(hw, &buffer_size);
			if (0 > err) {
				Com_Printf ("ALSA: unable to get buffer size. %s\n",
							p->psnd_strerror(err));
				goto error;
			}

			sc->samples = buffer_size * sc->channels;		// mono samples in buffer
			sc->samplepos = 0;
			sc->speed = rate;
			sc->samplebits = bits;

			sc->driverprivate = p;
			p->handle = pcm;

			sc->Submit = alsa_submit;
			sc->Shutdown = alsa_shutdown;
			sc->GetDMAPos = alsa_getdmapos;

			alsa_getdmapos(sc);		// sets shm->buffer


			//alsa doesn't seem to like high mixahead values
			//(maybe it tells us above somehow...)
			//so force it lower
			//quake's default of 0.2 was for 10fps rendering anyway
			//so force it down to 0.1 which is the default for halflife at least, and should give better latency
			{
#if 0
				extern cvar_t _snd_mixahead;
				if (_snd_mixahead.value >= 0.2)
				{
					Com_Printf("Alsa Hack: _snd_mixahead forced lower\n");
					_snd_mixahead.value = 0.1;
				}
#endif
			}
			return true;

			error:
			p->psnd_pcm_close(pcm);
			errorfoo:
			dlclose(p->alsasharedobject);
		}

		free(p);
	}

	return false;
}

static qboolean alsa_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	qboolean ret;
	const char *prevattempt;

	prevattempt = snd_alsa_device.string;
	ret = alsa_init_internal(sc, snd_alsa_device.string, rate, channels, bits);
	if (ret == 0 && strcmp(snd_alsa_device.string, "hw") != 0)
	{
		Com_Printf("Opening \"%s\" failed, trying \"hw\"\n", snd_alsa_device.string);

		prevattempt = "hw";
		ret = alsa_init_internal(sc, "hw", rate, channels, bits);
        }
	if (ret == 0 && strcmp(snd_alsa_device.string, "default") != 0)
	{
		Com_Printf("Opening \"%s\" failed, trying \"default\"\n", prevattempt);

		ret = alsa_init_internal(sc, "default", rate, channels, bits);
        }

        return ret;
}

SoundCvarInitFunc ALSA_CvarInit = alsa_cvarinit;
SoundInitFunc ALSA_Init = alsa_init;

