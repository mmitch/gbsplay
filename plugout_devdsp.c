/* $Id: plugout_devdsp.c,v 1.6 2004/03/21 02:46:15 ranmachan Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003-2004 (C) by Christian Garbs <mitch@cgarbs.de>
 *                  Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 *
 * header file for /dev/dsp sound output plugin
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/soundcard.h>

#include "plugout.h"

static int fd;

static int regparm devdsp_open(int endian, int rate)
{
	int c;
	int flags;
	
	if ((fd = open("/dev/dsp", O_WRONLY|O_NONBLOCK)) == -1) {
		fprintf(stderr, _("Could not open /dev/dsp: %s\n"), strerror(errno));
		return -1;
	}
	if ((flags = fcntl(fd, F_GETFL)) == -1) {
		fprintf(stderr, _("fcntl(F_GETFL) failed: %s\n"), strerror(errno));
	} else if (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
		fprintf(stderr, _("fcntl(F_SETFL, flags&~O_NONBLOCK) failed: %s\n"), strerror(errno));
	}

	switch (endian) {
	case CFG_ENDIAN_BE: c = AFMT_S16_BE; break;
	case CFG_ENDIAN_LE: c = AFMT_S16_LE; break;
	case CFG_ENDIAN_NE: c = AFMT_S16_NE; break;
	}
	if ((ioctl(fd, SNDCTL_DSP_SETFMT, &c)) == -1) {
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_SETFMT, %d) failed: %s\n"), c, strerror(errno));
		return -1;
	}
	c = rate;
	if ((ioctl(fd, SNDCTL_DSP_SPEED, &c)) == -1) {
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_SPEED, %d) failed: %s\n"), rate, strerror(errno));
		return -1;
	}
	if (c != rate) {
		fprintf(stderr, _("Requested rate %dHz, got %dHz.\n"), rate, c);
		rate = c;
	}
	c=1;
	if ((ioctl(fd, SNDCTL_DSP_STEREO, &c)) == -1) {
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_STEREO, %d) failed: %s\n"), c, strerror(errno));
		return -1;
	}
	c=(4 << 16) + 11;
	if ((ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &c)) == -1)
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_SETFRAGMENT, %08x) failed: %s\n"), c, strerror(errno));

	return 0;
}

static ssize_t regparm devdsp_write(const void *buf, size_t count)
{
	return write(fd, buf, count);
}

static void regparm devdsp_close()
{
	close(fd);
}

struct output_plugin plugout_devdsp = {
	.name = "oss",
	.description = "OSS sound driver",
	.open = devdsp_open,
	.write = devdsp_write,
	.close = devdsp_close,
};
