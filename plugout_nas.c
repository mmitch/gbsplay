/* $Id: plugout_nas.c,v 1.2 2004/04/02 14:25:34 mitch Exp $
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


/* global variables */
static AuServer      *nas_server;
static AuFlowID       nas_flow;
static AuDeviceID     nas_device;
static AuUint32       nas_buffer_free = 0;
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



static char *nas_error(AuStatus status)
/* convert AuStatus to error string */
{
        static char s[100];

        AuGetErrorText(nas_server, status, s, sizeof(s));

        return s;
}

static AuDeviceID nas_find_device(AuServer *aud)
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

static AuBool nas_event_handler(AuServer *srv, AuEvent *ev, AuEventHandlerRec *hnd)
/* handle NAS events */
{
	printf("EVENT!!!\n");
	switch (ev->type)
	{
	case AuEventTypeElementNotify: {
		AuElementNotifyEvent* event = (AuElementNotifyEvent *)ev;
		switch (event->kind)
		{
		case AuElementNotifyKindLowWater:
			printf("LOWWATER:%d\n", event->num_bytes);
			if (nas_buffer_free >= 0) {
				nas_buffer_free += event->num_bytes;
			} else {
				nas_buffer_free = event->num_bytes;
			}
			break;
		case AuElementNotifyKindState:
			printf("STATE:%d\n", event->num_bytes);
			switch (event->cur_state)
			{
			case AuStatePause:
				if (event->reason != AuReasonUser) {
					if (nas_buffer_free >= 0) {
						nas_buffer_free += event->num_bytes;
					} else {
						nas_buffer_free = event->num_bytes;
					}
				}
				break;
			}
			break;
		}
		break;
	}
	}
	
	return AuTrue;
}

static int regparm nas_open(int endian, int rate)
/* open NAS output */
{
	char *text;
	AuElement nas_elements[3];
	AuStatus  status;

	static const int buffer_size = 16384; /* 8192; */

	/* open server connection */
	nas_server = AuOpenServer(NULL, 0, NULL, 0, NULL, &text);
	if (!nas_server) {
		fprintf(stderr, _("%s: Can't open server: %s\n"), plugout_nas.name, text);
		return(-1);
	}

	/* search device with 2 channels */
        nas_device = nas_find_device(nas_server);
        if (nas_device == AuNone) {
		fprintf(stderr, _("%s: Can't find device with 2 channels\n"), plugout_nas.name);
		AuCloseServer(nas_server);
		return(-1);
	}

	/* create audio flow */
	nas_flow = AuCreateFlow(nas_server, NULL);
	if (!nas_flow) {
		fprintf(stderr, _("%s: Can't create audio flow\n"), plugout_nas.name);
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
		fprintf(stderr, _("%s: Can't set audio elements: %s\n"), plugout_nas.name, nas_error(status));
	}

	/* register event handler */
/*	AuRegisterEventHandler(nas_server, AuEventHandlerIDMask, 0, nas_flow,
			       nas_event_handler, (AuPointer) NULL);
	nas_buffer_free = -1;
*/
	/* start sound flow */
        AuStartFlow(nas_server, nas_flow, &status);
        if (status != AuSuccess) {
                fprintf(stderr, _("%s: Can't start audio flow: %s\n"), plugout_nas.name, nas_error(status));
                return -1;
        }
	
	return 0;
}

static ssize_t regparm nas_write(const void *buf, size_t count)
/* write audio data to NAS device */
{
	AuStatus status;

/*        while (count > nas_buffer_free) {
		/* not all data fits in buffer * /
                AuEvent ev;
                AuNextEvent(nas_server, AuTrue, &ev);
                AuDispatchEvent(nas_server, &ev);
		printf(".");
        }
*/
        nas_buffer_free -= count;
        AuWriteElement(nas_server, nas_flow, 0, count, (AuPointer*) buf, AuFalse, &status);
	printf("%s\n", nas_error(status));
	return count;
}

static void regparm nas_close()
/* close NAS device */
{
	if (nas_server) {
		AuStopFlow(nas_server, nas_flow, NULL);
		AuCloseServer(nas_server);
	}
}
