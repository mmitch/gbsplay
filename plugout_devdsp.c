/* $Id: plugout_devdsp.c,v 1.5 2004/03/20 20:41:26 mitch Exp $
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

static int fd;

void regparm devdsp_open(int endian, int rate)
{
	int c;
	int flags;
	
	if ((fd = open("/dev/dsp", O_WRONLY|O_NONBLOCK)) == -1) {
		fprintf(stderr, _("Could not open /dev/dsp: %s\n"), strerror(errno));
		exit(1);
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
		exit(1);
	}
	c = rate;
	if ((ioctl(fd, SNDCTL_DSP_SPEED, &c)) == -1) {
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_SPEED, %d) failed: %s\n"), rate, strerror(errno));
		exit(1);
	}
	if (c != rate) {
		fprintf(stderr, _("Requested rate %dHz, got %dHz.\n"), rate, c);
		rate = c;
	}
	c=1;
	if ((ioctl(fd, SNDCTL_DSP_STEREO, &c)) == -1) {
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_STEREO, %d) failed: %s\n"), c, strerror(errno));
		exit(1);
	}
	c=(4 << 16) + 11;
	if ((ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &c)) == -1)
		fprintf(stderr, _("ioctl(fd, SNDCTL_DSP_SETFRAGMENT, %08x) failed: %s\n"), c, strerror(errno));
}

ssize_t regparm devdsp_write(const void *buf, size_t count)
{
	return write(fd, buf, count);
}

void regparm devdsp_close()
{
	close(fd);
}
