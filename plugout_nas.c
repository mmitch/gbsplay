/* $Id: plugout_nas.c,v 1.3 2004/04/03 13:56:06 mitch Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2004 (C) by Christian Garbs <mitch@cgarbs.de>
 * Licensed under GNU GPL.
 *
 * NAS sound output plugin
 */



/*****************************************************************************
 * This code is heavily based on the XMMS NAS output plugin and the
 * audiooss library, which are both GNU GPL'ed.  See copyrights below:
 */

/* NAS layer of libaudiooss. Large portions stolen from the XMMS NAS plugin,
   which has the credits listed below. Adapted by Jon Trulson, further
   modified by Erik Inge Bolsø. Largely rewritten afterwards by
   Tobias Diedrich. Modifications during 2000-2002. */

/*\  XMMS - Cross-platform multimedia player
|*|  Copyright (C) 1998-1999  Peter Alm, Mikael Alm, Olle Hallnas,
|*|                           Thomas Nilsson and 4Front Technologies
|*|
|*|  Network Audio System driver by Willem Monsuwe (willem@stack.nl)
\*/

/*****************************************************************************/


#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <audio/audiolib.h>

#include "plugout.h"


#define DEBUG 1

#define FRAG_SIZE 4096
#define FRAG_COUNT 8
#define STARTUP_THRESHOLD       FRAG_SIZE * 4
#define MIN_BUFFER_SIZE FRAG_SIZE * FRAG_COUNT


/* global variables */
static AuServer      *nas_server;
static AuFlowID       nas_flow;
static AuDeviceID     nas_device;
static unsigned char  nas_format = AuFormatLinearSigned16LSB;
static int            nas_client_buffer_size = 0;
static int            nas_client_buffer_used = 0;
static char          *nas_client_buffer = NULL;
static int            nas_server_buffer_size = MIN_BUFFER_SIZE;
static int            nas_server_buffer_used = 0;

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



/* begin of DEBUG code */

#if DEBUG != 1
#define DPRINTF(format, args...)
#else
#define DPRINTF(format, args...)        fprintf(stderr, format, ## args); \
                                        fflush(stderr)

static char *nas_event_types[] = {
        "Undefined",
        "Undefined",
        "ElementNotify",
        "GrabNotify",
        "MonitorNotify",
        "BucketNotify",
        "DeviceNotify"
};

static char *nas_elementnotify_kinds[] = {
        "LowWater",
        "HighWater",
        "State",
        "Unknown"
};

static char *nas_states[] = {
        "Stop",
        "Start",
        "Pause",
        "Any"
};

static char *nas_reasons[] = {
        "User",
        "Underrun",
        "Overrun",
        "EOF",
        "Watermark",
        "Hardware",
        "Any"
};


static char* regparm nas_reason(unsigned int reason)
/* translate NAS reason */
{
        if (reason > 6) reason = 6;
        return nas_reasons[reason];
}

static char* regparm nas_elementnotify_kind(unsigned int kind)
/* translate NAS elementnotify kinf */
{
        if (kind > 2) kind = 3;
        return nas_elementnotify_kinds[kind];
}

static char* regparm nas_event_type(unsigned int type)
/* translate NAS event type */
{
        if (type > 6) type = 0;
        return nas_event_types[type];
}

static char* regparm nas_state(unsigned int state)
/* translate NAS state */
{
        if (state > 3) state = 3;
        return nas_states[state];
}

#endif
/* end of DEBUG code */


static char regparm *nas_error(AuStatus status)
/* convert AuStatus to error string */
{
        static char s[100];

        AuGetErrorText(nas_server, status, s, sizeof(s));

        return s;
}

static void regparm nas_wait_for_event()
/* wait for NAS event */
{
        AuEvent ev;

        AuNextEvent(nas_server, AuTrue, &ev);
        AuDispatchEvent(nas_server, &ev);
}


static void regparm nas_check_event_queue()
/* check for audio events */
{
        AuEvent ev;
        AuBool result;

        do {
                result = AuScanForTypedEvent(nas_server, AuEventsQueuedAfterFlush,
					     AuTrue, AuEventTypeElementNotify, &ev);
                if (result == AuTrue)
                        AuDispatchEvent(nas_server, &ev);
        } while (result == AuTrue);
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

static void regparm nas_buffer_resize(int newlen)
/* resize the client buffer */
{
        void *newbuf = malloc(nas_client_buffer_size = MAX(newlen, MIN_BUFFER_SIZE));
        void *oldbuf = nas_client_buffer;

        memcpy(newbuf, oldbuf, nas_client_buffer_used);
        nas_client_buffer = newbuf;
        if (oldbuf != NULL)
                free(oldbuf);
}

static int regparm nas_readBuffer(int num)
/* get buffer size */
{
        AuStatus as;

        if (!nas_client_buffer)
                nas_buffer_resize(MIN_BUFFER_SIZE);

        DPRINTF("readBuffer(): num=%d client=%d/%d server=%d/%d\n",
		num,
		nas_client_buffer_used, nas_client_buffer_size,
		nas_server_buffer_used, nas_server_buffer_size);

        if (nas_client_buffer_used < num)
                num = nas_client_buffer_used;

        if (nas_client_buffer_used == 0) {
                return 0;
        }

        AuWriteElement(nas_server, nas_flow, 0, num, nas_client_buffer, AuFalse, &as);
        if (as == AuSuccess) {
                nas_client_buffer_used -= num;
                nas_server_buffer_used += num;
/*                gettimeofday(&last_tv, 0); */
                memmove(nas_client_buffer, nas_client_buffer + num, nas_client_buffer_used);
/*                bytes_written += num; */
        } else {
                fprintf(stderr, _("%s:readBuffer: AuWriteElement %s\n"), plugout_nas.name, nas_error(as));
                num = 0;
        }
        AuFlush(nas_server);

        return num;
}

static void regparm nas_writeBuffer(const void *data, int len)
/* write data to the server */
{
        if (len > nas_client_buffer_size)
                nas_buffer_resize(len);

        DPRINTF("writeBuffer(): len=%d client=%d/%d\n",
		len, nas_client_buffer_used, nas_client_buffer_size);

        memcpy(nas_client_buffer + nas_client_buffer_used, data, len);
        nas_client_buffer_used += len;

        if ((nas_server_buffer_used < nas_server_buffer_size) &&
            ((nas_server_buffer_used != 0) ||
             (nas_client_buffer_used > STARTUP_THRESHOLD))) {
                AuSync(nas_server, AuFalse);
                nas_check_event_queue();
                nas_readBuffer(nas_server_buffer_size - nas_server_buffer_used);
        }
}


static AuBool regparm nas_event_handler(AuServer* aud, AuEvent* ev, AuEventHandlerRec* hnd)
/* handle NAS events */
{
        switch (ev->type) {
        case AuEventTypeElementNotify: {
                AuElementNotifyEvent* event = (AuElementNotifyEvent *)ev;

                DPRINTF("event_handler(): kind %s state %s->%s reason %s numbytes %ld\n",
                        nas_elementnotify_kind(event->kind),
                        nas_state(event->prev_state),
                        nas_state(event->cur_state),
                        nas_reason(event->reason),
                        event->num_bytes);

                nas_server_buffer_used -= event->num_bytes;
                if (nas_server_buffer_used < 0)
                        nas_server_buffer_used = 0;

                switch (event->kind) {
                case AuElementNotifyKindLowWater:
                        nas_readBuffer(event->num_bytes);
                        break;
                case AuElementNotifyKindState:
                        if ((event->cur_state == AuStatePause) &&
                            (event->reason == AuReasonUnderrun))
                                /* buffer underrun -> refill buffer
                                   we may have data in flight on its way
                                   to the server, so do not write a full
                                   buffer, only what the server asks for */
/*                                if (!close) */ /* don't complain if we expect it */
/*                                        fprintf(stderr,
					  _("%s: buffer underrun\n"), plugout_nas.name); */
                                nas_readBuffer(event->num_bytes);
                        break;
                }
                break;
        }
        default:
#if DEBUG != 1
                fprintf(stderr,
                        _("%s: event_handler: unhandled event type %d\n"),
			plugout_nas.name,
                        ev->type);
                fflush(stderr);
#else
		DPRINTF ("%s: event_handler: unhandled event type %d (%s)\n",
			plugout_nas.name, ev->type, nas_event_type(ev->type));

#endif
                break;
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
	AuRegisterEventHandler(nas_server, AuEventHandlerIDMask | AuEventHandlerTypeMask, AuEventTypeElementNotify, nas_flow,
			       nas_event_handler, (AuPointer) NULL);

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
        int writelen = 0;
        int numwritten = 0;

        DPRINTF("nas_write(): len=%d client=%d/%d server=%d/%d\n",
		count, nas_client_buffer_used, nas_client_buffer_size,
		nas_server_buffer_used, nas_server_buffer_size);

        if (nas_client_buffer_size == 0)
                nas_buffer_resize(MIN_BUFFER_SIZE);

        while (numwritten < count) {
                while (nas_client_buffer_size == nas_client_buffer_used)
                        nas_wait_for_event();
                if ((count - numwritten) > (nas_client_buffer_size - nas_client_buffer_used))
                        writelen = nas_client_buffer_size - nas_client_buffer_used;
                else writelen = (count - numwritten);
                nas_writeBuffer(buf + numwritten, writelen);
                numwritten += writelen;
        }

        AuSync(nas_server, AuFalse);
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
