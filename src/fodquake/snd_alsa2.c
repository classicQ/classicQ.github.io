/*
Copyright (C) 2010 Mark Olsen

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

#include <alsa/asoundlib.h>

#include "quakedef.h"
#include "sound.h"
#include <dlfcn.h>

#if 1
#define DEBUG(x...) do { } while(0)
#else
#define DEBUG(x...) fprintf(stderr, x)
#endif

#define PCM_RECOVER_VERBOSE 0

static cvar_t snd_alsa_device = { "snd_alsa_device", "default", CVAR_ARCHIVE };
static cvar_t snd_alsa_latency = { "snd_alsa_latency", "0.04", CVAR_ARCHIVE };

struct alsa_private
{
	void *alsasharedobject;
	snd_pcm_t *pcmhandle;
	void *buffer;
	unsigned int buffersamples;
	unsigned int bufferpos;
	unsigned int samplesize;

	snd_pcm_sframes_t (*snd_pcm_avail_update)(snd_pcm_t *pcm);
	int (*snd_pcm_close)(snd_pcm_t *pcm);
	int (*snd_pcm_drop)(snd_pcm_t *pcm);
	int (*snd_pcm_open)(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
	int (*snd_pcm_recover)(snd_pcm_t *pcm, int err, int silent);
	int (*snd_pcm_set_params)(snd_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_access_t access, unsigned int channels, unsigned int rate, int soft_resample, unsigned int latency);
	int (*snd_pcm_start)(snd_pcm_t *pcm);
	snd_pcm_sframes_t (*snd_pcm_writei)(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
	const char *(*snd_strerror)(int errnum);
};

static void alsa_alsasucksdonkeyballs(struct alsa_private *p, unsigned int ret, int dorestart)
{
	if (dorestart && ret != -EPIPE)
		dorestart = 0;

	ret = p->snd_pcm_recover(p->pcmhandle, ret, !PCM_RECOVER_VERBOSE);
	if (ret < 0)
		fprintf(stderr, "ALSA made a boo-boo: %d (%s)\n", (int)ret, p->snd_strerror(ret));

	if (dorestart)
		p->snd_pcm_start(p->pcmhandle);
}

static int alsa_reallygetavail(struct alsa_private *p)
{
	int ret;

	ret = p->snd_pcm_avail_update(p->pcmhandle);
	if (ret < 0)
	{
		alsa_alsasucksdonkeyballs(p, ret, 1);
		ret = p->snd_pcm_avail_update(p->pcmhandle);
		if (ret < 0)
		{
			DEBUG("Sorry, your ALSA is really totally utterly fucked. (%d - %s)\n", ret, p->snd_strerror(ret));
			return 0;
		}
	}

	ret *= 2;

	return ret;
}

static void alsa_writestuff(struct alsa_private *p, unsigned int max)
{
	unsigned int count;
	snd_pcm_sframes_t ret;
	int avail;

	while(max)
	{
		count = p->buffersamples - p->bufferpos;
		if (count > max)
			count = max;

		avail = alsa_reallygetavail(p);

		DEBUG("count: %d\n", count);
		DEBUG("Avail: %d\n", avail);

		/* This workaround is required to keep sound working on Ubuntu 10.04 and 10.10 (Yay for Ubuntu) */
		if (avail < 64)
			break;

		avail -= 64;

		if (count > avail)
			count = avail;

		if (count == 0)
			break;

		ret = p->snd_pcm_writei(p->pcmhandle, p->buffer + (p->samplesize * p->bufferpos), count/2);
		DEBUG("Ret was %d\n", ret);
		if (ret < 0)
		{
			alsa_alsasucksdonkeyballs(p, ret, 1);
			break;
		}

		if (ret <= 0)
			break;

		max -= ret * 2;
		p->bufferpos += ret * 2;
		if (p->bufferpos == p->buffersamples)
			p->bufferpos = 0;
	}
}

static int alsa_getdmapos(struct SoundCard *sc)
{
	struct alsa_private *p;

	p = sc->driverprivate;

	return p->bufferpos;
}

static int alsa_getavail(struct SoundCard *sc)
{
	struct alsa_private *p;

	p = sc->driverprivate;

	return alsa_reallygetavail(p);
}

static void alsa_shutdown(struct SoundCard *sc)
{
	struct alsa_private *p;

	p = sc->driverprivate;

	p->snd_pcm_drop(p->pcmhandle);
	p->snd_pcm_close(p->pcmhandle);

	dlclose(p->alsasharedobject);

	free(p);
}

static void alsa_submit(struct SoundCard *sc, unsigned int count)
{
	struct alsa_private *p;

	DEBUG("Submit(%d)\n", count);

	p = sc->driverprivate;

	alsa_writestuff(p, count);
}

static qboolean alsa_initso(struct alsa_private *p)
{
	p->alsasharedobject = dlopen("libasound.so.2", RTLD_LAZY|RTLD_LOCAL);
	if (p->alsasharedobject == 0)
	{
		p->alsasharedobject = dlopen("libasound.so", RTLD_LAZY|RTLD_LOCAL);
	}

	if (p->alsasharedobject)
	{
		p->snd_pcm_avail_update = dlsym(p->alsasharedobject, "snd_pcm_avail_update");
		p->snd_pcm_close = dlsym(p->alsasharedobject, "snd_pcm_close");
		p->snd_pcm_drop = dlsym(p->alsasharedobject, "snd_pcm_drop");
		p->snd_pcm_open = dlsym(p->alsasharedobject, "snd_pcm_open");
		p->snd_pcm_recover = dlsym(p->alsasharedobject, "snd_pcm_recover");
		p->snd_pcm_set_params = dlsym(p->alsasharedobject, "snd_pcm_set_params");
		p->snd_pcm_start = dlsym(p->alsasharedobject, "snd_pcm_start");
		p->snd_pcm_writei = dlsym(p->alsasharedobject, "snd_pcm_writei");
		p->snd_strerror = dlsym(p->alsasharedobject, "snd_strerror");

		if (p->snd_pcm_avail_update
		 && p->snd_pcm_close
		 && p->snd_pcm_drop
		 && p->snd_pcm_open
		 && p->snd_pcm_recover
		 && p->snd_pcm_set_params
		 && p->snd_pcm_writei
		 && p->snd_strerror)
		{
			return 1;
		}

		dlclose(p->alsasharedobject);
	}

	return 0;
}

static void alsa_cvarinit()
{
	Cvar_Register(&snd_alsa_device);
	Cvar_Register(&snd_alsa_latency);
}

static qboolean alsa_init_internal(struct SoundCard *sc, const char *device, int rate, int channels, int bits)
{
	struct alsa_private *p;

	if (!*device)
		return false;

	p = malloc(sizeof(*p));
	if (p)
	{
		if (alsa_initso(p))
		{
			if (p->snd_pcm_open(&p->pcmhandle, device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) >= 0)
			{
				if (p->snd_pcm_set_params(p->pcmhandle, bits==8?SND_PCM_FORMAT_S8:SND_PCM_FORMAT_S16, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, snd_alsa_latency.value * 1000000) >= 0)
				{
					p->buffer = malloc(65536);
					if (p->buffer)
					{
						memset(p->buffer, 0, 65536);

						p->bufferpos = 0;
						p->samplesize = (bits/8);
						p->buffersamples = 65536/p->samplesize;

						sc->driverprivate = p;

						sc->GetDMAPos = alsa_getdmapos;
						sc->GetAvail = alsa_getavail;
						sc->Submit = alsa_submit;
						sc->Shutdown = alsa_shutdown;

						sc->channels = channels;
						sc->samples = p->buffersamples;
						sc->samplepos = 0;
						sc->samplebits = bits;
						sc->speed = rate;
						sc->buffer = p->buffer;

						alsa_writestuff(p, p->buffersamples);

						p->snd_pcm_start(p->pcmhandle);

						return true;
					}
				}

				p->snd_pcm_close(p->pcmhandle);
			}

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
	if (ret == 0 && strcmp(snd_alsa_device.string, "default") != 0)
	{
		Com_Printf("ALSA: Opening \"%s\" failed, trying \"default\"\n", snd_alsa_device.string);

		prevattempt = "default";
		ret = alsa_init_internal(sc, "default", rate, channels, bits);
	}
	if (ret == 0 && strcmp(snd_alsa_device.string, "hw") != 0)
	{
		Com_Printf("ALSA: Opening \"%s\" failed, trying \"hw\"\n", prevattempt);

		ret = alsa_init_internal(sc, "hw", rate, channels, bits);
	}

	return ret;
}

SoundCvarInitFunc ALSA_CvarInit = alsa_cvarinit;
SoundInitFunc ALSA_Init = alsa_init;

