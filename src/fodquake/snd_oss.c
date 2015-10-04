/*
Copyright (C) 2006-2007 Mark Olsen

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

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "quakedef.h"
#include "sound.h"

static const int rates[] = { 11025, 22050, 44100, 8000 };

#define NUMRATES (sizeof(rates)/sizeof(*rates))

cvar_t snd_oss_device = { "snd_oss_device", "/dev/dsp", CVAR_ARCHIVE };

struct oss_private
{
	int fd;
};

static int oss_getdmapos(struct SoundCard *sc)
{
	struct count_info count;
	struct oss_private *p = sc->driverprivate;
	
	if (ioctl(p->fd, SNDCTL_DSP_GETOPTR, &count) != -1)
		sc->samplepos = count.ptr/(sc->samplebits/8);
	else
		sc->samplepos = 0;

	return sc->samplepos;
}

static void oss_submit(struct SoundCard *sc, unsigned int count)
{
}

static void oss_shutdown(struct SoundCard *sc)
{
	struct oss_private *p = sc->driverprivate;

	munmap(sc->buffer, sc->samples*(sc->samplebits/8));
	close(p->fd);

	free(p);
}

static void oss_cvarinit()
{
	Cvar_Register(&snd_oss_device);
}

static qboolean oss_init_internal(struct SoundCard *sc, const char *device, int rate, int channels, int bits)
{
	int i;
	struct oss_private *p;

	int capabilities;
	int dspformats;
	struct audio_buf_info info;

	p = malloc(sizeof(*p));
	if (p)
	{
		p->fd = open(device, O_RDWR|O_NONBLOCK);
		if (p->fd != -1)
		{
			if ((ioctl(p->fd, SNDCTL_DSP_RESET, 0) >= 0)
			&& (ioctl(p->fd, SNDCTL_DSP_GETCAPS, &capabilities) != -1 && (capabilities&(DSP_CAP_TRIGGER|DSP_CAP_MMAP)) == (DSP_CAP_TRIGGER|DSP_CAP_MMAP))
			&& (ioctl(p->fd, SNDCTL_DSP_GETOSPACE, &info) != -1))
			{
				if ((sc->buffer = mmap(0, info.fragstotal * info.fragsize, PROT_WRITE, MAP_SHARED, p->fd, 0)) != MAP_FAILED)
				{
					ioctl(p->fd, SNDCTL_DSP_GETFMTS, &dspformats);

					if ((!(dspformats&AFMT_S16_LE) && (dspformats&AFMT_U8)) || bits == 8)
						sc->samplebits = 8;
					else if ((dspformats&AFMT_S16_LE))
						sc->samplebits = 16;
					else
						sc->samplebits = 0;

					if (sc->samplebits)
					{
						i = 0;
						do
						{
							if (ioctl(p->fd, SNDCTL_DSP_SPEED, &rate) == 0)
								break;

							rate = rates[i];
						} while(i++ != NUMRATES);

						if (i != NUMRATES+1)
						{
							if (channels == 1)
							{
								sc->channels = 1;
								i = 0;
							}
							else
							{
								sc->channels = 2;
								i = 1;
							}

							if (ioctl(p->fd, SNDCTL_DSP_STEREO, &i) >= 0)
							{
								if (sc->samplebits == 16)
									i = AFMT_S16_LE;
								else
									i = AFMT_S8;

								if (ioctl(p->fd, SNDCTL_DSP_SETFMT, &i) >= 0)
								{
									i = 0;
									if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
									{
										i = PCM_ENABLE_OUTPUT;
										if (ioctl(p->fd, SNDCTL_DSP_SETTRIGGER, &i) >= 0)
										{
											sc->samples = info.fragstotal * info.fragsize / (sc->samplebits/8);
											sc->samplepos = 0;
											sc->speed = rate;

											sc->driverprivate = p;

											sc->GetDMAPos = oss_getdmapos;
											sc->Submit = oss_submit;
											sc->Shutdown = oss_shutdown;

											return 1;
										}
									}
								}
							}
						}
					}

					munmap(sc->buffer, info.fragstotal * info.fragsize);
				}
			}

			close(p->fd);
		}

		free(p);
	}

	return 0;
}

static qboolean oss_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	qboolean ret;

	ret = oss_init_internal(sc, snd_oss_device.string, rate, channels, bits);
	if (ret == 0 && strcmp(snd_oss_device.string, "/dev/dsp") != 0)
	{
		Com_Printf("Opening \"%s\" failed, trying \"/dev/dsp\"\n", snd_oss_device.string);

		ret = oss_init_internal(sc, "/dev/dsp", rate, channels, bits);
	}

	return ret;
}

SoundCvarInitFunc OSS_CvarInit = oss_cvarinit;
SoundInitFunc OSS_Init = oss_init;

