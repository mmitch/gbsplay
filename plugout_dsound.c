/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2005 (C) by Christian Garbs <mitch@cgarbs.de>
 *                  Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 * Licensed under GNU GPL.
 *
 * header file for DirectSound output under Windows, aquanight <aquanight@gmail.com>
 * This is C++ because COM is a pain in pure C
 */

#include <initguid.h>
#include "common.h"
#include "plugout.h"
#include <windows.h>
#include <dsound.h>
#include <stdlib.h>
#include <unistd.h>

static LPDIRECTSOUND8 dsound_device;
static LPDIRECTSOUNDBUFFER8 dsound_buffer;

static DWORD writeOffset;    // Location of the last byte written (not the write cursor)
static DWORD writeMax;       // Maximum amount we can write at once.

static WAVEFORMATEX wfx;
static DSBUFFERDESC dsbdesc;

static long regparm dsound_open(enum plugout_endian endian, long rate)
{
	HRESULT hr;

	if (endian != PLUGOUT_ENDIAN_NATIVE)
	{
		fprintf(stderr, "Currently only native endianness is supported.");
		return -1;
	}

	hr = DirectSoundCreate8(NULL, &dsound_device, NULL);

	if (FAILED(hr))
	{
		switch (hr)
		{
			case DSERR_ALLOCATED:
				fprintf(stderr, "Failed to open DirectSound: the audio device is already in use by another program\n");
				break;
			case DSERR_NOAGGREGATION:
				fprintf(stderr, "Failed to open DirectSound: aggregation not supported.\n");
				break;
			case DSERR_NODRIVER:
				fprintf(stderr, "Failed to open DirectSound: no driver is available.\n");
				break;
			case DSERR_OUTOFMEMORY:
				fprintf(stderr, "Failed to open DirectSound: the system is out of memory.\n");
				break;
			default:
				fprintf(stderr, "Failed to open DirectSound: HRESULT = %lx\n", (unsigned long)hr);
				break;
		}
		return -1;
	}
	/* FUCKING FUCKITY FUCK I HAVE TO CREATE A WINDOW FOR THIS. */
	hr = IDirectSound8_SetCooperativeLevel(dsound_device, GetDesktopWindow(), DSSCL_NORMAL); // XXX VERY MUCH NOT ACCEPTABLE TO USE AN HWND THAT'S NOT OURS. But should work for initial trials at least.
	if (FAILED(hr))
	{
		switch (hr)
		{
			case DSERR_ALLOCATED:
				fprintf(stderr, "This shouldn't happen because we're not asking for anything fancy, so it should've already failed at device creation.\n");
				break;
			case DSERR_UNSUPPORTED:
			case DSERR_UNINITIALIZED:
				fprintf(stderr, "Wat. ");
			default:
				fprintf(stderr, "SetCooperativeLevel failed: HRESULT = %lx\n", (unsigned long)hr);
				break;
		}
		IDirectSound8_Release(dsound_device);
		return -1;
	}

	/* Needed to get a streaming secondary sound buffer... */

	memset(&wfx, 0, sizeof(WAVEFORMATEX));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = rate;
	wfx.nBlockAlign = 4;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.wBitsPerSample = 16;
	wfx.cbSize = 0; /* It's ignored anyway, but... */
	
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLPAN | DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY; /* Keep sound buffer going if inactive, use accurate play cursor */
	writeMax = dsbdesc.dwBufferBytes = wfx.nAvgBytesPerSec / 10;
	dsbdesc.lpwfxFormat = &wfx;

	return 0; /* We're open, rock and roll! */
}

static ssize_t regparm dsound_write(const void* buf, size_t count)
{
	size_t orig_count = count;
	DWORD playCursor, writeCursor;
	DWORD writeLimit;
	void* pBuf1;
	void* pBuf2;
	DWORD nBuf1, nBuf2;
	while (count > 0)
	{
		IDirectSoundBuffer8_GetCurrentPosition(dsound_buffer, &playCursor, &writeCursor);
		if (writeOffset == -1)
			writeOffset = writeCursor;
		if (writeCursor == 0 && playCursor == 0)
		{
			/* Starting write before DSound begins playing */
			writeLimit = writeMax - writeOffset;
		}
		else if (writeOffset <= playCursor)
		{
			/* We have wrapped behind the play cursor. */
			writeLimit = (playCursor - writeOffset);
		}
		else
		{
			writeLimit = writeMax - (writeOffset - playCursor);
		}
		if (writeLimit < 1)
		{
			continue;
		}
		if (count < writeLimit)
		{
			writeLimit = count;
		}
		IDirectSoundBuffer8_Lock(dsound_buffer, writeOffset, writeLimit, &pBuf1, &nBuf1, &pBuf2, &nBuf2, 0);
		memcpy(pBuf1, buf, nBuf1);
		if (nBuf1 < writeLimit)
		{
			memcpy(pBuf2, (void*)((char*)buf + nBuf1), nBuf2);
			writeOffset = nBuf2; /* NOT += */
		}
		else
			writeOffset += nBuf1;
		IDirectSoundBuffer8_Unlock(dsound_buffer, pBuf1, nBuf1, pBuf2, nBuf2);
		if (writeLimit < count)
		{
			count -= writeLimit;
			buf = (void*)((char*)buf + writeLimit);
		}
		else
			count = 0;
		IDirectSoundBuffer8_Play(dsound_buffer, 0, 0, DSBPLAY_LOOPING);
	}
	return orig_count;
}

static int regparm dsound_skip(int subsound)
{
	HRESULT hr;
	LPDIRECTSOUNDBUFFER pDsb = NULL;

	if (dsound_buffer)
	{
		IDirectSoundBuffer8_Stop(dsound_buffer); /* Well that should be simple enough. */
		IDirectSoundBuffer8_Release(dsound_buffer);
		dsound_buffer = NULL;
	}
	writeOffset = -1;
	hr = IDirectSound8_CreateSoundBuffer(dsound_device, &dsbdesc, &pDsb, NULL);
	if (FAILED(hr))
	{
		return -1;
	}
	IDirectSoundBuffer_QueryInterface(pDsb, &IID_IDirectSoundBuffer8, (LPVOID*)&dsound_buffer);
	IDirectSoundBuffer_Release(pDsb);
	return 0;
}

static int regparm dsound_pause(int pause_mode)
{
	if (pause_mode && dsound_buffer)
		IDirectSoundBuffer8_Stop(dsound_buffer);
	if (!pause_mode && dsound_buffer)
		IDirectSoundBuffer8_Play(dsound_buffer, 0, 0, DSBPLAY_LOOPING);
}

static void regparm dsound_close()
{
	if (dsound_buffer)
	{
		IDirectSoundBuffer8_Stop(dsound_buffer);
		IDirectSoundBuffer8_Release(dsound_buffer);
	}
	if (dsound_device)
		IDirectSound8_Release(dsound_device);
}

static char plugout_dsound_name[] = "dsound";
static char plugout_dsound_desc[] = "DirectSound sound driver";

const struct output_plugin plugout_dsound = {
	.name = plugout_dsound_name,
	.description = plugout_dsound_desc,
	.open = dsound_open,
	.write = dsound_write,
	.pause = dsound_pause,
	.skip = dsound_skip,
	.close = dsound_close
};
