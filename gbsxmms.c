/* $Id: gbsxmms.c,v 1.7 2003/08/31 17:55:23 ranma Exp $
 *
 * gbsplay is a Gameboy sound player
 *
 * 2003 (C) by Tobias Diedrich <ranma@gmx.at>
 * Licensed under GNU GPL.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <xmms/plugin.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"

InputPlugin gbs_ip;

static struct gbs *gbs;

static int silencectr = 0;
static long long gbclock = 0;

static int gbs_subsong = 0;

static void file_info_box(char *filename)
{
	gbs_ip.output->flush(0);
	gbs_subsong++;
	if (gbs_subsong >= gbs->songs) gbs_subsong = 0;
	gbs_playsong(gbs, gbs_subsong);
}

static int stopthread = 1;

static void callback(void *buf, int len, void *priv)
{
	int time = gbclock / 4194304;

	while (gbs_ip.output->buffer_free() < len && !stopthread) usleep(10000);
	gbs_ip.output->write_audio(buf, len);
	gbs_ip.add_vis_pcm(gbs_ip.output->written_time(),
	                   FMT_S16_LE, 2, len/4, buf);


	if ((gbhw_ch[0].volume == 0 ||
	     gbhw_ch[0].master == 0) &&
	    (gbhw_ch[1].volume == 0 ||
	     gbhw_ch[1].master == 0) &&
	    (gbhw_ch[2].volume == 0 ||
	     gbhw_ch[2].master == 0) &&
	    (gbhw_ch[3].volume == 0 ||
	     gbhw_ch[3].master == 0)) {
		silencectr++;
	} else silencectr = 0;

	if (time > 2*60 || silencectr > 100) {
		file_info_box(NULL);
	}
}

static void init(void)
{
}

static int is_our_file(char *filename)
{
	int fd = open(filename, O_RDONLY);
	int res = 0;
	char id[4];

	read(fd, id, sizeof(id));
	close(fd);

	if (strncmp(id, "GBS\1", sizeof(id)) == 0 ||
	    strncmp(id, "GBS\2", sizeof(id)) == 0) res = 1;

	return res;
}

static pthread_t playthread;

void *playloop(void *priv)
{
	if (!gbs_ip.output->open_audio(FMT_S16_LE, 44100, 2)) {
		puts("Error opening output plugin.");
		return 0;
	}
	while (!stopthread) {
		int cycles = gbhw_step();
		if (cycles<0) {
			stopthread = 1;
		} else {
			gbclock += cycles;
			if (!((gbhw_ch[0].volume == 0 ||
			       gbhw_ch[0].master == 0) &&
			      (gbhw_ch[1].volume == 0 ||
			       gbhw_ch[1].master == 0) &&
			      (gbhw_ch[2].volume == 0 ||
			       gbhw_ch[2].master == 0) &&
			      (gbhw_ch[3].volume == 0 ||
			       gbhw_ch[3].master == 0)))
				silencectr = 0;
		}
	}
	gbs_ip.output->close_audio();
	return 0;
}

static void play_file(char *filename)
{
	if ((gbs = gbs_open(filename)) != NULL) {
		gbhw_init(gbs->rom, gbs->romsize);
		gbhw_setcallback(callback, NULL);
		gbhw_setrate(44100);
		gbs_subsong = gbs->defaultsong - 1;
		gbs_playsong(gbs, gbs_subsong);
		gbclock = 0;
		stopthread = 0;
		pthread_create(&playthread, 0, playloop, 0);
	}
}

static void stop(void)
{
	stopthread = 1;
	pthread_join(playthread, 0);
	if (gbs) {
		gbs_close(gbs);
		gbs = NULL;
	}
}

static int get_time(void)
{
	if (stopthread) return -1;
	return gbs_ip.output->output_time();
}

static void cleanup(void)
{
}

static void get_song_info(char *filename, char **title, int *length)
{
	int fd = open(filename, O_RDONLY);
	char buf[0x70];

	read(fd, buf, sizeof(buf));
	close(fd);

	*title = malloc(3*32+10);
	*length = -1;

	snprintf(*title, 3*32+9, "%.32s - %.32s (%.32s)",
	         &buf[0x10], &buf[0x30], &buf[0x50]);
	title[3*32+9] = 0;
}

InputPlugin gbs_ip = {
description:	"GBS Player",
init:		init,
is_our_file:	is_our_file,
play_file:	play_file,
stop:		stop,
get_time:	get_time,
cleanup:	cleanup,
get_song_info:	get_song_info,
file_info_box:	file_info_box,
};

InputPlugin *get_iplugin_info(void)
{
	return &gbs_ip;
}
