/* $Id: plugout_nas.c,v 1.6 2004/04/04 19:30:30 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * NAS sound output plugin
 *
 * Based on the libaudiooss nas backend, which was largely rewritten
 * by me (Tobias Diedrich), I'd dare to say the only function left from
 * the xmms code is nas_find_device, but I did not check that. :-)
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <audio/audiolib.h>

#include "plugout.h"

#define NAS_BUFFER_SAMPLES 8192

/* global variables */
static AuServer      *nas_server;
static AuFlowID       nas_flow;
static unsigned char  nas_format = AuFormatLinearSigned16LSB;

/* forward function declarations */
static int     regparm nas_open(int endian, int rate);
static ssize_t regparm nas_write(const void *buf, size_t count);
static void    regparm nas_close();

/* descripton of this plugin */
struct output_plugin plugout_nas = {
	.name = "nas",
	.description = "NAS sound driver",
	.open = nas_open,
	.write = nas_write,
	.close = nas_close,
};

static char regparm *nas_error(AuStatus status)
/* convert AuStatus to error string */
{
	static char s[100];

	AuGetErrorText(nas_server, status, s, sizeof(s));

	return s;
}

static AuDeviceID regparm nas_find_device(AuServer *aud)
/* look for a NAS device that supports 2 channels (we need stereo sound) */
{
	int i;
	for (i=0; i < AuServerNumDevices(aud); i++) {
		AuDeviceAttributes *dev = AuServerDevice(aud, i);
		if ((AuDeviceKind(dev) == AuComponentKindPhysicalOutput) &&
		    AuDeviceNumTracks(dev) == 2) {
			return AuDeviceIdentifier(dev);
		}
	}
	return AuNone;
}


static int regparm nas_open(int endian, int rate)
/* open NAS output */
{
	char *text;
	AuElement nas_elements[3];
	AuStatus  status;
	AuDeviceID nas_device;

	/* open server connection */
	nas_server = AuOpenServer(NULL, 0, NULL, 0, NULL, &text);
	if (!nas_server) {
		fprintf(stderr, _("%s: Can't open server: %s\n"), plugout_nas.name, text);
		goto err;
	}

	/* search device with 2 channels */
	nas_device = nas_find_device(nas_server);
	if (nas_device == AuNone) {
		fprintf(stderr, _("%s: Can't find device with 2 channels\n"), plugout_nas.name);
		goto err_close;
	}

	/* create audio flow */
	nas_flow = AuCreateFlow(nas_server, NULL);
	if (!nas_flow) {
		fprintf(stderr, _("%s: Can't create audio flow\n"), plugout_nas.name);
		goto err_close;
	}

	/* create elements(?) */
	AuMakeElementImportClient(nas_elements, rate, nas_format, 2, AuTrue,
	                          NAS_BUFFER_SAMPLES, NAS_BUFFER_SAMPLES / 2,
	                          0, NULL);
	AuMakeElementExportDevice(nas_elements+1, 0, nas_device, rate,
	                          AuUnlimitedSamples, 0, NULL);
	AuSetElements(nas_server, nas_flow, AuTrue, 2, nas_elements, &status);
	if (status != AuSuccess) {
		fprintf(stderr, _("%s: Can't set audio elements: %s\n"), plugout_nas.name, nas_error(status));
		goto err_close;
	}

	/* start sound flow */
	AuStartFlow(nas_server, nas_flow, &status);
	if (status != AuSuccess) {
		fprintf(stderr, _("%s: Can't start audio flow: %s\n"), plugout_nas.name, nas_error(status));
		goto err_close;
	}

	return 0;
err_close:
	AuCloseServer(nas_server);
	nas_server = NULL;
err:
	return -1;
}

static ssize_t regparm nas_write(const void *buf, size_t count)
/* write audio data to NAS device */
{
	int maxlen = NAS_BUFFER_SAMPLES * 2;
	int numwritten = 0;

	while (numwritten < count) {
		AuStatus as;
		int writelen = count - numwritten;

		if (writelen > maxlen) writelen = maxlen;

		AuWriteElement(nas_server, nas_flow, 0, writelen, (void*)buf, AuFalse, &as);
		AuSync(nas_server, AuTrue);
		if (as == AuBadValue) {
			/*
			 * Does not fit in remaining buffer space,
			 * wait a bit and try again
			 */
			usleep(1000);
			continue;
		}
		if (as != AuSuccess) {
			/*
			 * Unexpected error happened
			 */
			nas_error(as);
			break;
		}
		numwritten += writelen;
		buf += writelen;
	}

	return numwritten;
}

static void regparm nas_close()
/* close NAS device */
{
	if (nas_server) {
		AuStopFlow(nas_server, nas_flow, NULL);
		AuCloseServer(nas_server);
	}
}
