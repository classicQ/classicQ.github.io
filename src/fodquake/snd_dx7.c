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
//if you get compile errors with this file, just take it out of the makefile

#include <dsound.h>

#include "quakedef.h"
#include "sound.h"

#warning Command line arguments are not tolerated.

#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS             64
#define	WAV_MASK                0x3F
#define	WAV_BUFFER_SIZE         0x0400
#define SECONDARY_BUFFER_SIZE   0x10000

struct dspriv {
	HINSTANCE hInstDS;
	HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
	HRESULT (WINAPI *pDirectSoundEnumerate)(LPDSENUMCALLBACKA pDSEnumCallback, LPVOID pContext);

	int wanteddevicenum;
	int currentenum;
	LPGUID dsdevice;


	int samplebytes;

	LPDIRECTSOUND pDS;
	LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;

	int gSndBufSize;

	DWORD lockedsize;
	MMTIME mmstarttime;
};

static BOOL CALLBACK DS_EnumDevices(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
	struct dspriv *p = lpContext;

	if (p->wanteddevicenum == p->currentenum++)
	{
		p->dsdevice = lpGUID;

		return FALSE;
	}
	else
	{
		return TRUE;
	}
}


static void ds7_shutdown (struct SoundCard *sc)
{
	struct dspriv *p = sc->driverprivate;

	p->pDSBuf->lpVtbl->Stop(p->pDSBuf);
	p->pDSBuf->lpVtbl->Release(p->pDSBuf);

	// only release primary buffer if it's not also the mixing buffer we just released
	if (p->pDSPBuf && p->pDSBuf != p->pDSPBuf)
		p->pDSPBuf->lpVtbl->Release(p->pDSPBuf);

	p->pDS->lpVtbl->SetCooperativeLevel (p->pDS, GetDesktopWindow(), DSSCL_NORMAL);
	p->pDS->lpVtbl->Release(p->pDS);

	FreeLibrary(p->hInstDS);

	free(p);
}

//return the current sample position (in mono samples read) inside the recirculating dma buffer,
//so the mixing code will know how many sample are required to fill it up.
static int ds7_getdmapos(struct SoundCard *sc)
{
	struct dspriv *p = sc->driverprivate;
	MMTIME mmtime;
	int s;
	DWORD dwWrite;

	mmtime.wType = TIME_SAMPLES;
	p->pDSBuf->lpVtbl->GetCurrentPosition(p->pDSBuf, &mmtime.u.sample, &dwWrite);
	s = mmtime.u.sample - p->mmstarttime.u.sample;

	s /= p->samplebytes;

	s &= (sc->samples-1);

	return s;
}

//Send sound to device if buffer isn't really the dma buffer
static void ds7_submit(struct SoundCard *sc, unsigned int count)
{
}

static void ds7_restore(struct SoundCard *sc)
{
	struct dspriv *p = sc->driverprivate;
	DWORD dwStatus;

	if (p->pDSBuf->lpVtbl->GetStatus (p->pDSBuf, &dwStatus) != DS_OK)
		Com_Printf("Couldn't get sound buffer status\n");
	
	if (dwStatus & DSBSTATUS_BUFFERLOST)
		p->pDSBuf->lpVtbl->Restore (p->pDSBuf);
	
	if (!(dwStatus & DSBSTATUS_PLAYING))
		p->pDSBuf->lpVtbl->Play(p->pDSBuf, 0, 0, DSBPLAY_LOOPING);
}

static void *ds7_lock(struct SoundCard *sc)
{
	struct dspriv *p = sc->driverprivate;

	int		reps;
	DWORD	dwSize,dwSize2;
	DWORD	*pbuf2;
	HRESULT	hresult;

	reps = 0;

	while ((hresult = p->pDSBuf->lpVtbl->Lock(p->pDSBuf, 0, p->gSndBufSize, &sc->buffer, &p->lockedsize, 
								   &pbuf2, &dwSize2, 0)) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Com_Printf("dsound7: Lock Sound Buffer Failed\n");
#if 0
			S_Shutdown ();
			S_Startup ();
#endif
			return NULL;
		}

		if (++reps > 10000)
		{
			Com_Printf("dsound7: couldn't restore buffer\n");
#if 0
			S_Shutdown ();
			S_Startup ();
#endif
			return NULL;
		}
	}
	return sc->buffer;
}

static void ds7_unlock(struct SoundCard *sc)
{
	struct dspriv *p = sc->driverprivate;
	p->pDSBuf->lpVtbl->Unlock(p->pDSBuf, sc->buffer, p->lockedsize, NULL, 0);
	sc->buffer = NULL;
}


static qboolean ds7_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	qboolean	primary_format_set;
	HPSTR		lpData;


	struct dspriv *p = NULL;
	DSBUFFERDESC dsbuf;
	DSBCAPS dsbcaps;
	DWORD dwSize, dwWrite;
	DSCAPS dscaps;
	WAVEFORMATEX format, pformat; 
	HRESULT hresult;
	int reps, temp, devicenum;
	int tempresult;

	p = malloc(sizeof(*p));
	if (p)
	{
		memset(p, 0, sizeof(*p));

		p->hInstDS = LoadLibrary("dsound.dll");
		if (p->hInstDS)
		{
			p->pDirectSoundCreate = (void *)GetProcAddress(p->hInstDS,"DirectSoundCreate");
			p->pDirectSoundEnumerate = (void *) GetProcAddress(p->hInstDS, "DirectSoundEnumerateA");

			if (p->pDirectSoundCreate && p->pDirectSoundEnumerate)
			{
				memset (&format, 0, sizeof(format));
				format.wFormatTag = WAVE_FORMAT_PCM;
				format.nChannels = channels;
				format.wBitsPerSample = bits;
				format.nSamplesPerSec = rate;
				format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
				format.cbSize = 0;
				format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 

				p->dsdevice = NULL;
				if ((temp = COM_CheckParm("-snddev")) && temp + 1 < com_argc)
				{
#warning do this properly
					p->wanteddevicenum = Q_atoi(com_argv[temp + 1]);
					p->currentenum = 0;
					if ((hresult = p->pDirectSoundEnumerate(DS_EnumDevices, p)) != DS_OK)
					{
						Com_Printf("Couldn't open preferred sound device. Falling back to primary sound device.\n");
						p->dsdevice = NULL;
					}
				}

				hresult = p->pDirectSoundCreate(p->dsdevice, &p->pDS, NULL);
				if (hresult != DS_OK && p->dsdevice)
				{
					Com_Printf("Couldn't open preferred sound device. Falling back to primary sound device.\n");
					p->dsdevice = NULL;
					hresult = p->pDirectSoundCreate(p->dsdevice, &p->pDS, NULL);
				}

				if (hresult != DS_OK)
				{
					if (hresult == DSERR_ALLOCATED)
					{
						Com_Printf("DirectSoundCreate failed, hardware already in use\n");
					}
					else
					{
						Com_Printf("DirectSound create failed\n");
					}
				}
				else
				{
					dscaps.dwSize = sizeof(dscaps);

					if (DS_OK != p->pDS->lpVtbl->GetCaps(p->pDS, &dscaps))
						Com_Printf("Couldn't get DS caps\n");

					if (dscaps.dwFlags & DSCAPS_EMULDRIVER)
					{
						Com_Printf("No DirectSound driver installed\n");
					}
					else
					{
						if (DS_OK != p->pDS->lpVtbl->SetCooperativeLevel(p->pDS, GetDesktopWindow(), DSSCL_EXCLUSIVE))
						{
							Com_Printf("Set coop level failed\n");
						}
						else
						{
							// get access to the primary buffer, if possible, so we can set the sound hardware format
							memset (&dsbuf, 0, sizeof(dsbuf));
							dsbuf.dwSize = sizeof(DSBUFFERDESC);
							dsbuf.dwFlags = DSBCAPS_PRIMARYBUFFER|DSBCAPS_GLOBALFOCUS;
							dsbuf.dwBufferBytes = 0;
							dsbuf.lpwfxFormat = NULL;

							memset(&dsbcaps, 0, sizeof(dsbcaps));
							dsbcaps.dwSize = sizeof(dsbcaps);
							primary_format_set = false;

							if (!COM_CheckParm ("-snoforceformat"))
							{
								if (DS_OK == p->pDS->lpVtbl->CreateSoundBuffer(p->pDS, &dsbuf, &p->pDSPBuf, NULL))
								{
									pformat = format;

									if (DS_OK == p->pDSPBuf->lpVtbl->SetFormat(p->pDSPBuf, &pformat))
									{
										primary_format_set = true;
									}
								}
							}

							tempresult = 0;

							if (!primary_format_set || !COM_CheckParm ("-primarysound"))
							{
								// create the secondary buffer we'll actually work with
								memset (&dsbuf, 0, sizeof(dsbuf));
								dsbuf.dwSize = sizeof(DSBUFFERDESC);
								dsbuf.dwFlags = DSBCAPS_CTRLFREQUENCY | DSBCAPS_LOCSOFTWARE|DSBCAPS_GLOBALFOCUS;
								dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
								dsbuf.lpwfxFormat = &format;

								memset(&dsbcaps, 0, sizeof(dsbcaps));
								dsbcaps.dwSize = sizeof(dsbcaps);

								if (DS_OK != p->pDS->lpVtbl->CreateSoundBuffer(p->pDS, &dsbuf, &p->pDSBuf, NULL))
								{
									Com_Printf("DS:CreateSoundBuffer Failed");
								}
								else
								{
									sc->driverprivate = p;
									channels = format.nChannels;
									bits = format.wBitsPerSample;
									rate = format.nSamplesPerSec;

									if (DS_OK != p->pDSBuf->lpVtbl->GetCaps(p->pDSBuf, &dsbcaps))
									{
										Com_Printf("DS:GetCaps failed\n");
									}
									else
										tempresult = 1;
								}
							}
							else
							{
								if (DS_OK != p->pDS->lpVtbl->SetCooperativeLevel(p->pDS, GetDesktopWindow(), DSSCL_WRITEPRIMARY))
								{
									Com_Printf("Set coop level failed\n");
								}
								else
								{
									if (DS_OK != p->pDSPBuf->lpVtbl->GetCaps(p->pDSPBuf, &dsbcaps))
									{
										Com_Printf("DS:GetCaps failed\n");
									}
									else
									{
										p->pDSBuf = p->pDSPBuf;
										tempresult = 1;
									}
								}
							}

							if (tempresult)
							{
								// Make sure mixer is active
								p->pDSBuf->lpVtbl->Play(p->pDSBuf, 0, 0, DSBPLAY_LOOPING);

								p->gSndBufSize = dsbcaps.dwBufferBytes;

								// initialize the buffer
								reps = 0;

								tempresult = 1;
								while ((hresult = p->pDSBuf->lpVtbl->Lock(p->pDSBuf, 0, p->gSndBufSize, &lpData, &dwSize, NULL, NULL, 0)) != DS_OK)
								{
									if (hresult != DSERR_BUFFERLOST)
									{
										Com_Printf("SNDDMA_InitDirect: DS::Lock Sound Buffer Failed\n");
										tempresult = 0;
										break;
									}

									Com_Printf("SNDDMA_InitDirect: relocking...\n");

									if (++reps > 10000)
									{
										Com_Printf("SNDDMA_InitDirect: DS: couldn't restore buffer\n");
										tempresult = 0;
										break;
									}
								}

								if (tempresult)
								{
									memset(lpData, 0, dwSize);
									//		lpData[4] = lpData[5] = 0x7f;	// force a pop for debugging

									p->pDSBuf->lpVtbl->Unlock(p->pDSBuf, lpData, dwSize, NULL, 0);

									/* we don't want anyone to access the buffer directly w/o locking it first. */
									lpData = NULL; 

									p->pDSBuf->lpVtbl->Stop(p->pDSBuf);
									p->pDSBuf->lpVtbl->GetCurrentPosition(p->pDSBuf, &p->mmstarttime.u.sample, &dwWrite);
									p->pDSBuf->lpVtbl->Play(p->pDSBuf, 0, 0, DSBPLAY_LOOPING);

									sc->speed = rate;
									sc->samplebits = bits;
									sc->channels = channels;

									sc->samples = p->gSndBufSize/(sc->samplebits/8);
									sc->samplepos = 0;
									sc->buffer = NULL;
									p->samplebytes = (sc->samplebits/8);

									sc->GetDMAPos = ds7_getdmapos;
									sc->Submit = ds7_submit;
									sc->Shutdown = ds7_shutdown;
									sc->Restore = ds7_restore;
									sc->Lock = ds7_lock;
									sc->Unlock = ds7_unlock;

									return true;
								}
							}

							if (p->pDSBuf)
							{
								p->pDSBuf->lpVtbl->Stop(p->pDSBuf);
								p->pDSBuf->lpVtbl->Release(p->pDSBuf);

								if (p->pDSPBuf && p->pDSBuf != p->pDSPBuf)
									p->pDSPBuf->lpVtbl->Release(p->pDSPBuf);
							}
						}
					}

					p->pDS->lpVtbl->Release(p->pDS);
				}
			}

			FreeLibrary(p->hInstDS);
		}

		free(p);
	}

	return false;
}

SoundInitFunc DS7_Init = ds7_init;

