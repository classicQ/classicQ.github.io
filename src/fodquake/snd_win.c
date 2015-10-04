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

#include "quakedef.h"
#include "winquake.h"
#include "sound.h"


// 64K is > 1 second at 16-bit, 22050 Hz
#define	WAV_BUFFERS				64
#define	WAV_MASK				0x3F
#define	WAV_BUFFER_SIZE			0x0400
#define SECONDARY_BUFFER_SIZE	0x10000

struct wav_private {
	int	samplebytes;
	int	snd_sent, snd_completed;

/* 
 * Global variables. Must be visible to window-procedure function 
 *  so it can unlock and free the data block after it has been played. 
 */ 


	HANDLE		hData;
	HPSTR		lpData;

	HGLOBAL		hWaveHdr;
	LPWAVEHDR	lpWaveHdr;
 
	HWAVEOUT    hWaveOut; 
};

/*
void S_BlockSound (void) {

	// DirectSound takes care of blocking itself
	if (snd_iswave) {
		snd_blocked++;

		if (snd_blocked == 1)
			waveOutReset (hWaveOut);
	}
}

void S_UnblockSound (void) {

	// DirectSound takes care of blocking itself
	if (snd_iswave)
		snd_blocked--;
}
*/

static void wav_shutdown (struct SoundCard *sc)
{
	struct wav_private *p = sc->driverprivate;
	int i;

	if (p->hWaveOut)
	{
		waveOutReset (p->hWaveOut);

		if (p->lpWaveHdr)
		{
			for (i = 0; i < WAV_BUFFERS; i++)
				waveOutUnprepareHeader (p->hWaveOut, p->lpWaveHdr+i, sizeof(WAVEHDR));
		}

		waveOutClose (p->hWaveOut);

		if (p->hWaveHdr)
		{
			GlobalUnlock(p->hWaveHdr); 
			GlobalFree(p->hWaveHdr);
		}

		if (p->hData)
		{
			GlobalUnlock(p->hData);
			GlobalFree(p->hData);
		}

	}

	p->hWaveOut = 0;
	p->hData = 0;
	p->hWaveHdr = 0;
	p->lpData = NULL;
	p->lpWaveHdr = NULL;
}


//return the current sample position (in mono samples read) inside the recirculating dma buffer,
//so the mixing code will know how many sample are required to fill it up.
static int wav_getdmapos(struct SoundCard *sc)
{
	struct wav_private *p = sc->driverprivate;
	int s;

	s = p->snd_sent * WAV_BUFFER_SIZE;

	s /= p->samplebytes;

	s &= (sc->samples-1);

	return s;
}

//Send sound to device if buffer isn't really the dma buffer
static void wav_submit(struct SoundCard *sc, unsigned int count)
{
	struct wav_private *p = sc->driverprivate;
	LPWAVEHDR h;
	int wResult;

	// find which sound blocks have completed
	while (1) {
		if ( p->snd_completed == p->snd_sent ) {
			Com_DPrintf ("Sound overrun\n");
			break;
		}

		if (!(p->lpWaveHdr[ p->snd_completed & WAV_MASK].dwFlags & WHDR_DONE))
			break;

		p->snd_completed++;	// this buffer has been played
	}

	// submit two new sound blocks
	while (((p->snd_sent - p->snd_completed) / p->samplebytes) < 4) {
		h = p->lpWaveHdr + ( p->snd_sent&WAV_MASK );

		p->snd_sent++;
		/* 
		 * Now the data block can be sent to the output device. The 
		 * waveOutWrite function returns immediately and waveform 
		 * data is sent to the output device in the background. 
		 */ 
		wResult = waveOutWrite(p->hWaveOut, h, sizeof(WAVEHDR)); 

		if (wResult != MMSYSERR_NOERROR) { 
			Com_Printf ("Failed to write block to device\n");
			wav_shutdown (sc);
			return; 
		} 
	}
}



static qboolean wav_init(struct SoundCard *sc, int rate, int channels, int bits)
{
	struct wav_private *p;

	WAVEFORMATEX format; 
	int i;
	HRESULT hr;
	UINT_PTR devicenum;
	int temp;
	DWORD	gSndBufSize;


	sc->driverprivate = p = malloc(sizeof(*p));
	if (!p)
		return false;
	memset(p, 0, sizeof(*p));

	sc->channels = 2;
	sc->samplebits = 16;
	sc->speed = rate;
#warning need to try some other rates too

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = sc->channels;
	format.wBitsPerSample = sc->samplebits;
	format.nSamplesPerSec = sc->speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign; 

	devicenum = WAVE_MAPPER;
	if ((temp = COM_CheckParm("-snddev")) && temp + 1 < com_argc)
		devicenum = Q_atoi(com_argv[temp + 1]);

	hr = waveOutOpen((LPHWAVEOUT) &p->hWaveOut, devicenum, &format, 0, 0L, CALLBACK_NULL);
	if (hr != MMSYSERR_NOERROR && devicenum != WAVE_MAPPER) {
		Com_Printf ("Couldn't open preferred sound device. Falling back to primary sound device.\n");
		hr = waveOutOpen((LPHWAVEOUT)&p->hWaveOut, WAVE_MAPPER, &format, 0, 0L, CALLBACK_NULL);
	}
	
	/* Open a waveform device for output using window callback. */ 
	if (hr != MMSYSERR_NOERROR) {
		if (hr == MMSYSERR_ALLOCATED)
			Com_Printf ("waveOutOpen failed, hardware already in use\n");
		else
			Com_Printf ("waveOutOpen failed\n");

		return false;
	} 

	/* 
	 * Allocate and lock memory for the waveform data. The memory 
	 * for waveform data must be globally allocated with 
	 * GMEM_MOVEABLE and GMEM_SHARE flags. 
	*/ 
	gSndBufSize = WAV_BUFFERS*WAV_BUFFER_SIZE;
	p->hData = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, gSndBufSize); 
	if (!p->hData) { 
		Com_Printf ("Sound: Out of memory.\n");
		wav_shutdown (sc);
		return false; 
	}
	p->lpData = GlobalLock(p->hData);
	if (!p->lpData)
	{
		Com_Printf ("Sound: Failed to lock.\n");
		wav_shutdown (sc);
		return false;
	}
	memset (p->lpData, 0, gSndBufSize);

	/* 
	 * Allocate and lock memory for the header. This memory must 
	 * also be globally allocated with GMEM_MOVEABLE and 
	 * GMEM_SHARE flags. 
	 */ 
	p->hWaveHdr = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE,
		(DWORD) sizeof(WAVEHDR) * WAV_BUFFERS);

	if (p->hWaveHdr == NULL)
	{
		Com_Printf ("Sound: Failed to Alloc header.\n");
		wav_shutdown (sc);
		return false;
	}

	p->lpWaveHdr = (LPWAVEHDR) GlobalLock(p->hWaveHdr);

	if (p->lpWaveHdr == NULL)
	{ 
		Com_Printf ("Sound: Failed to lock header.\n");
		wav_shutdown (sc);
		return false; 
	}

	memset (p->lpWaveHdr, 0, sizeof(WAVEHDR) * WAV_BUFFERS);

	/* After allocation, set up and prepare headers. */ 
	for (i = 0; i < WAV_BUFFERS; i++)
	{
		p->lpWaveHdr[i].dwBufferLength = WAV_BUFFER_SIZE; 
		p->lpWaveHdr[i].lpData = p->lpData + i*WAV_BUFFER_SIZE;

		if (waveOutPrepareHeader(p->hWaveOut, p->lpWaveHdr+i, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			Com_Printf ("Sound: failed to prepare wave headers\n");
			wav_shutdown (sc);
			return false;
		}
	}

	sc->samples = gSndBufSize/(sc->samplebits/8);
	sc->samplepos = 0;
	sc->buffer = (unsigned char *) p->lpData;
	p->samplebytes = (sc->samplebits/8);

	sc->GetDMAPos = wav_getdmapos;
	sc->Submit = wav_submit;
	sc->Shutdown = wav_shutdown;

	return true;
}

SoundInitFunc WaveOut_Init = wav_init;











