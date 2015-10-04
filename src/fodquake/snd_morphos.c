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

#include <exec/exec.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#define USE_INLINE_STDARG
#include <proto/ahi.h>

#include "quakedef.h"
#include "sound.h"

struct AHIChannelInfo
{
	struct AHIEffChannelInfo aeci;
	ULONG offset;
};

struct ahi_private
{
	struct MsgPort *msgport;
	struct AHIRequest *ahireq;
	struct AHIAudioCtrl *audioctrl;
	void *samplebuffer;
	struct Hook EffectHook;
	struct AHIChannelInfo aci;
	unsigned int readpos;
};

ULONG EffectFunc()
{
	struct Hook *hook = (struct Hook *)REG_A0;
	struct AHIEffChannelInfo *aeci = (struct AHIEffChannelInfo *)REG_A1;

	struct ahi_private *p;

	p = hook->h_Data;

	p->readpos = aeci->ahieci_Offset[0];

	return 0;
}

static struct EmulLibEntry EffectFunc_Gate =
{
	TRAP_LIB, 0, (void (*)(void))EffectFunc
};

void ahi_shutdown(struct SoundCard *sc)
{
	struct ahi_private *p = sc->driverprivate;

	struct Library *AHIBase;

	AHIBase = (struct Library *)p->ahireq->ahir_Std.io_Device;

	p->aci.aeci.ahie_Effect = AHIET_CHANNELINFO|AHIET_CANCEL;
	AHI_SetEffect(&p->aci.aeci, p->audioctrl);
	AHI_ControlAudio(p->audioctrl,
	                 AHIC_Play, FALSE,
	                 TAG_END);

	AHI_FreeAudio(p->audioctrl);

	CloseDevice((struct IORequest *)p->ahireq);
	DeleteIORequest((struct IORequest *)p->ahireq);

	DeleteMsgPort(p->msgport);

	FreeVec(p->samplebuffer);

	FreeVec(p);
}

int ahi_getdmapos(struct SoundCard *sc)
{
	struct ahi_private *p = sc->driverprivate;

	sc->samplepos = p->readpos*sc->channels;

	return sc->samplepos;
}

void ahi_submit(struct SoundCard *sc, unsigned int count)
{
}

qboolean ahi_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	struct ahi_private *p;
	ULONG r;

	char name[64];

	struct Library *AHIBase;

	struct AHISampleInfo sample;

	p = AllocVec(sizeof(*p), MEMF_ANY);
	if (p)
	{
		p->msgport = CreateMsgPort();
		if (p->msgport)
		{
			p->ahireq = (struct AHIRequest *)CreateIORequest(p->msgport, sizeof(struct AHIRequest));
			if (p->ahireq)
			{
				r = !OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)p->ahireq, 0);
				if (r)
				{
					AHIBase = (struct Library *)p->ahireq->ahir_Std.io_Device;

					p->audioctrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
					                               AHIA_MixFreq, rate,
					                               AHIA_Channels, 1,
					                               AHIA_Sounds, 1,
					                               TAG_END);

					if (p->audioctrl)
					{
						AHI_GetAudioAttrs(AHI_INVALID_ID, p->audioctrl,
						                  AHIDB_BufferLen, sizeof(name),
						                  AHIDB_Name, (ULONG)name,
						                  AHIDB_MaxChannels, (ULONG)&channels,
						                  AHIDB_Bits, (ULONG)&bits,
						                  TAG_END);

						AHI_ControlAudio(p->audioctrl,
						                 AHIC_MixFreq_Query, (ULONG)&rate,
						                 TAG_END);

						if (bits == 8 || bits == 16)
						{
							if (channels > 2)
								channels = 2;

							sc->speed = rate;
							sc->samplebits = bits;
							sc->channels = channels;
							sc->samples = 16384*(rate/11025);

							p->samplebuffer = AllocVec(16384*(rate/11025)*(bits/8)*channels, MEMF_CLEAR);
							if (p->samplebuffer)
							{
								sc->buffer = p->samplebuffer;

								if (channels == 1)
								{
									if (bits == 8)
										sample.ahisi_Type = AHIST_M8S;
									else
										sample.ahisi_Type = AHIST_M16S;
								}
								else
								{
									if (bits == 8)
										sample.ahisi_Type = AHIST_S8S;
									else
										sample.ahisi_Type = AHIST_S16S;
								}

								sample.ahisi_Address = p->samplebuffer;
								sample.ahisi_Length = (16384*(rate/11025)*(bits/8))/AHI_SampleFrameSize(sample.ahisi_Type);

								r = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, p->audioctrl);
								if (r == 0)
								{
									r = AHI_ControlAudio(p->audioctrl,
									                     AHIC_Play, TRUE,
									                     TAG_END);

									if (r == 0)
									{
										AHI_Play(p->audioctrl,
										         AHIP_BeginChannel, 0,
										         AHIP_Freq, rate,
										         AHIP_Vol, 0x10000,
										         AHIP_Pan, 0x8000,
										         AHIP_Sound, 0,
										         AHIP_EndChannel, NULL,
										         TAG_END);

										p->aci.aeci.ahie_Effect = AHIET_CHANNELINFO;
										p->aci.aeci.ahieci_Func = &p->EffectHook;
										p->aci.aeci.ahieci_Channels = 1;

										p->EffectHook.h_Entry = (void *)&EffectFunc_Gate;
										p->EffectHook.h_Data = p;

										AHI_SetEffect(&p->aci, p->audioctrl);

										Com_Printf("Using AHI mode \"%s\" for audio output\n", name);
										Com_Printf("Channels: %d bits: %d frequency: %d\n", channels, bits, rate);

										sc->driverprivate = p;

										sc->GetDMAPos = ahi_getdmapos;
										sc->Submit = ahi_submit;
										sc->Shutdown = ahi_shutdown;

										return 1;
									}
								}
							}
							FreeVec(p->samplebuffer);
						}
						AHI_FreeAudio(p->audioctrl);
					}
					else
						Com_Printf("Failed to allocate AHI audio\n");

					CloseDevice((struct IORequest *)p->ahireq);
				}
				DeleteIORequest((struct IORequest *)p->ahireq);
			}
			DeleteMsgPort(p->msgport);
		}
		FreeVec(p);
	}

	return 0;
}

SoundInitFunc AHI_Init = ahi_init;

