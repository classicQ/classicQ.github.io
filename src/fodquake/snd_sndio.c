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

#include <sndio.h>
#include <dlfcn.h>
#include <poll.h>
#include <errno.h>

#include <string.h>
#include <stdlib.h>

#include "cvar.h"
#include "common.h"
#include "sound.h"

static cvar_t snd_sndio_device = { "snd_sndio_device", "aucat:0", CVAR_ARCHIVE };
static cvar_t snd_sndio_latency = { "snd_sndio_latency", "0.04", CVAR_ARCHIVE };

struct sndio_private
{
	void *sndiosharedobject;
	struct sio_hdl *handle;
	unsigned int sndiobufsize;
	unsigned int channels;
	unsigned int samplesize;

	struct pollfd *pollfds;

	void *buffer;
	unsigned int buffersamples;
	volatile unsigned int bufferreadpos;
	unsigned int bufferwritepos;

	void (*sndio_sio_close)(struct sio_hdl *hdl);
	int (*sndio_sio_eof)(struct sio_hdl *hdl);
	int (*sndio_sio_getpar)(struct sio_hdl *hdl, struct sio_par *par);
	void (*sndio_sio_initpar)(struct sio_par *par);
	int (*sndio_sio_nfds)(struct sio_hdl *hdl);
	void (*sndio_sio_onmove)(struct sio_hdl *hdl, void (*cb)(void *arg, int delta), void *arg);
	struct sio_hdl *(*sndio_sio_open)(const char *name, unsigned mode, int nbio_flag);
	int (*sndio_sio_pollfd)(struct sio_hdl *hdl, struct pollfd *pfd, int events);
	int (*sndio_sio_revents)(struct sio_hdl *hdl, struct pollfd *pfd);
	int (*sndio_sio_setpar)(struct sio_hdl *hdl, struct sio_par *par);
	int (*sndio_sio_start)(struct sio_hdl *hdl);
	size_t (*sndio_sio_write)(struct sio_hdl *hdl, const void *addr, size_t nbytes);
};

static qboolean sndio_initso(struct sndio_private *p)
{
	p->sndiosharedobject = dlopen("libsndio.so.4.0", RTLD_LAZY|RTLD_LOCAL);

	if (p->sndiosharedobject)
	{
		p->sndio_sio_close = dlsym(p->sndiosharedobject, "sio_close");
		p->sndio_sio_eof = dlsym(p->sndiosharedobject, "sio_eof");
		p->sndio_sio_getpar = dlsym(p->sndiosharedobject, "sio_getpar");
		p->sndio_sio_initpar = dlsym(p->sndiosharedobject, "sio_initpar");
		p->sndio_sio_nfds = dlsym(p->sndiosharedobject, "sio_nfds");
		p->sndio_sio_onmove = dlsym(p->sndiosharedobject, "sio_onmove");
		p->sndio_sio_open = dlsym(p->sndiosharedobject, "sio_open");
		p->sndio_sio_pollfd = dlsym(p->sndiosharedobject, "sio_pollfd");
		p->sndio_sio_revents = dlsym(p->sndiosharedobject, "sio_revents");
		p->sndio_sio_setpar = dlsym(p->sndiosharedobject, "sio_setpar");
		p->sndio_sio_start = dlsym(p->sndiosharedobject, "sio_start");
		p->sndio_sio_write = dlsym(p->sndiosharedobject, "sio_write");

		if (p->sndio_sio_close
		 && p->sndio_sio_eof
		 && p->sndio_sio_getpar
		 && p->sndio_sio_initpar
		 && p->sndio_sio_nfds
		 && p->sndio_sio_onmove
		 && p->sndio_sio_open
		 && p->sndio_sio_pollfd
		 && p->sndio_sio_revents
		 && p->sndio_sio_setpar
		 && p->sndio_sio_start
		 && p->sndio_sio_write)
		{
			return 1;
		}

		dlclose(p->sndiosharedobject);
	}

	return 0;
}

static void sndio_cvarinit()
{
	Cvar_Register(&snd_sndio_device);
	Cvar_Register(&snd_sndio_latency);
}

static void sndio_onmove_callback(void *private, int delta)
{
	struct sndio_private *p;

	p = private;

	p->bufferreadpos = (p->bufferreadpos + delta) % p->buffersamples;
}

static int sndio_refresh(struct sndio_private *p)
{
	int i;

	i = p->sndio_sio_pollfd(p->handle, p->pollfds, POLLOUT);
	while(poll(p->pollfds, i, 0) < 0 && errno == EINTR);

	i = (p->sndio_sio_revents(p->handle, p->pollfds) & POLLOUT)?1:0;

	return i;
}

static int sndio_getdmapos(struct SoundCard *sc)
{
	struct sndio_private *p;

	p = sc->driverprivate;

	return p->bufferwritepos * p->channels;
}

static int sndio_getavail(struct SoundCard *sc)
{
	struct sndio_private *p;
	unsigned int readpos;
	unsigned int writepos;
	unsigned int avail;
	unsigned int used;

	p = sc->driverprivate;

	sndio_refresh(p);

	readpos = p->bufferreadpos;
	writepos = p->bufferwritepos;

	if (readpos <= writepos)
		used = writepos - readpos;
	else
		used = p->buffersamples + writepos - readpos;

	avail = (p->sndiobufsize)-used;

	return avail * p->channels;
}

static void sndio_shutdown(struct SoundCard *sc)
{
	struct sndio_private *p;

	p = sc->driverprivate;

	p->sndio_sio_close(p->handle);

	free(p->buffer);
	free(p->pollfds);

	dlclose(p->sndiosharedobject);

	free(p);
}

static void sndio_submit(struct SoundCard *sc, unsigned int count)
{
	struct sndio_private *p;
	unsigned int avail;
	unsigned int i;

	p = sc->driverprivate;

	if (!sndio_refresh(p))
		return;

	count /= p->channels;

	avail = sndio_getavail(sc);

	if (count > avail)
		count = avail;

	while(count)
	{
		i = count;
		if (i > p->buffersamples - p->bufferwritepos)
			i = p->buffersamples - p->bufferwritepos;

		i = p->sndio_sio_write(p->handle, p->buffer + p->bufferwritepos * p->samplesize, i * p->samplesize);
		if (i == 0)
			break;

		i /= p->samplesize;

		p->bufferwritepos += i;
		if (p->bufferwritepos == p->buffersamples)
			p->bufferwritepos = 0;

		count -= i;
	}
}

static qboolean sndio_init_internal(struct SoundCard *sc, const char *device, int rate, int channels, int bits)
{
	struct sndio_private *p;
	struct sio_par par;

	p = malloc(sizeof(*p));
	if (p)
	{
		if (sndio_initso(p))
		{
			p->handle = p->sndio_sio_open(device, SIO_PLAY, 1);
			if (p->handle)
			{
				p->sndio_sio_initpar(&par);

				par.bits = bits;
				par.sig = bits==16?1:0;
				par.le = SIO_LE_NATIVE;
				par.pchan = channels;
				par.rate = rate;
				par.appbufsz = snd_sndio_latency.value * rate;
				par.xrun = SIO_IGNORE;

				if (p->sndio_sio_setpar(p->handle, &par) && p->sndio_sio_getpar(p->handle, &par) && par.pchan <= 2 && par.bits <= 16 && par.sig == (par.bits==16?1:0))
				{
					p->pollfds = malloc(sizeof(*p->pollfds) * p->sndio_sio_nfds(p->handle));
					if (p->pollfds)
					{
						p->sndiobufsize = par.bufsz;

						p->channels = par.pchan;
						p->samplesize = par.pchan*(par.bits/8);

						p->buffer = malloc(65536);
						if (p->buffer)
						{
							memset(p->buffer, 0, 65536);

							p->buffersamples = 65536 / p->samplesize;
							p->bufferreadpos = 0;
							p->bufferwritepos = 0;

							sc->driverprivate = p;

							sc->GetDMAPos = sndio_getdmapos;
							sc->GetAvail = sndio_getavail;
							sc->Submit = sndio_submit;
							sc->Shutdown = sndio_shutdown;

							sc->channels = par.pchan;
							sc->samples = p->buffersamples * p->channels;
							sc->samplepos = 0;
							sc->samplebits = par.bits;
							sc->speed = par.rate;
							sc->buffer = p->buffer;

							p->sndio_sio_onmove(p->handle, sndio_onmove_callback, p);
							if (p->sndio_sio_start(p->handle))
							{
								return true;
							}

							free(p->buffer);
						}

						free(p->pollfds);
					}
				}

				p->sndio_sio_close(p->handle);
			}

			dlclose(p->sndiosharedobject);
		}

		free(p);
	}

	return false;
}

static qboolean sndio_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	qboolean ret;
	const char *prevattempt;

	prevattempt = snd_sndio_device.string;
	ret = sndio_init_internal(sc, snd_sndio_device.string, rate, channels, bits);
	if (ret == 0 && strcmp(snd_sndio_device.string, "aucat:0") != 0)
	{
		Com_Printf("SNDIO: Opening \"%s\" failed, trying \"aucat:0\"\n", snd_sndio_device.string);

		prevattempt = "aucat:0";
		ret = sndio_init_internal(sc, "aucat:0", rate, channels, bits);
	}
	if (ret == 0 && strcmp(snd_sndio_device.string, "sun:0") != 0)
	{
		Com_Printf("SNDIO: Opening \"%s\" failed, trying \"sun:0\"\n", prevattempt);

		ret = sndio_init_internal(sc, "sun:0", rate, channels, bits);
	}

	return ret;
}

SoundCvarInitFunc SNDIO_CvarInit = sndio_cvarinit;
SoundInitFunc SNDIO_Init = sndio_init;

