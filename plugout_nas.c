/* $Id: plugout_nas.c,v 1.1 2004/04/02 10:40:44 mitch Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 *             Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 *
 * NAS sound output plugin
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include <audio/audiolib.h>

#include "plugout.h"

static AuServer      *nas_server;
static AuFlowID       nas_flow;
static AuDeviceID     nas_device;
static unsigned char  nas_format = AuFormatLinearSigned16LSB;

static char *nas_error(AuStatus status)
{
        static char s[100];

        AuGetErrorText(nas_server, status, s, sizeof(s));

        return s;
}

static AuDeviceID nas_find_device(AuServer *aud)
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
{
	char *text;
	AuElement nas_elements[3];
	AuStatus  status;

	static const int buffer_size = 8192;

	/* open server connection */
	nas_server = AuOpenServer(NULL, 0, NULL, 0, NULL, &text);
	if (!nas_server) {
		fprintf(stderr, _("NAS: Can't open server: %s\n"), text);
		return(-1);
	}

	/* search device with 2 channels */
        nas_device = nas_find_device(nas_server);
        if (nas_device == AuNone) {
		fprintf(stderr, _("NAS: Can't find device with 2 channels\n"));
		AuCloseServer(nas_server);
		return(-1);
	}

	/* create audio flow */
	nas_flow = AuCreateFlow(nas_server, NULL);
	if (!nas_flow) {
		fprintf(stderr, _("NAS: Can't create audio flow\n"));
		AuCloseServer(nas_server);
		return(-1);
	}

	/* create elements(?) */
        AuMakeElementImportClient(nas_elements, rate, nas_format, 2, AuTrue,
				  buffer_size, buffer_size / 2, 0, NULL);
        AuMakeElementExportDevice(nas_elements+1, 0, nas_device, rate,
				  AuUnlimitedSamples, 0, NULL);
        AuSetElements(nas_server, nas_flow, AuTrue, 2, nas_elements, &status);
	if (status != AuSuccess) {
		fprintf(stderr, _("NAS: Can't set audio elements: %s\n"), nas_error(status));
	}

	/* start sound flow */
        AuStartFlow(nas_server, nas_flow, &status);
        if (status != AuSuccess) {
                fprintf(stderr, _("NAS: Can't start audio flow: %s\n"), nas_error(status));
                return -1;
        }
	
	return 0;
}

static ssize_t regparm nas_write(const void *buf, size_t count)
{
/*	return write(fd, buf, count); */
	AuStatus status;
        AuWriteElement(nas_server, nas_flow, 0, count, buf, AuFalse, &status);
	return status;
}

static void regparm nas_close()
{
	AuCloseServer(nas_server);
}

struct output_plugin plugout_nas = {
	.name = "nas",
	.description = "NAS sound driver",
	.open = nas_open,
	.write = nas_write,
	.close = nas_close,
};
