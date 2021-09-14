/*
 * gbsplay is a Gameboy sound player
 *
 * 2003-2021 (C) by Tobias Diedrich <ranma+gbsplay@tdiedrich.de>
 *                  Christian Garbs <mitch@cgarbs.de>
 *
 * Licensed under GNU GPL v1 or, at your option, any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

#include <xmms/plugin.h>
#include <gtk/gtk.h>

#define GBS_PRESERVE_TEXTDOMAIN 1

#include "common.h"
#include "gbhw.h"
#include "gbcpu.h"
#include "libgbs.h"
#include "cfgparser.h"

#define GBS_DEBUG 0

#ifdef DPRINTF
#undef DPRINTF
#endif

#if GBS_DEBUG == 1

#include <sys/types.h>
#include <linux/unistd.h>
_syscall0(pid_t, gettid);

static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

#define DPRINTF(...) do {                                   \
	pthread_mutex_lock(&debug_mutex);                   \
	fprintf(stderr, "%s(%d), tid %d, %s(): ",           \
	        __FILE__, __LINE__, gettid(), __func__);    \
	fprintf(stderr, __VA_ARGS__);                       \
	pthread_mutex_unlock(&debug_mutex);                 \
} while (0)
#else
#define DPRINTF(...) do { } while (0)
#endif

static const struct timespec TEN_MILLISECONDS = {
	.tv_sec = 0,
	.tv_nsec = 10000000
};

static InputPlugin gbs_ip;

static char *cfgfile = ".xmms/gbsxmmsrc";

static struct gbs *gbs;
static pthread_mutex_t gbs_mutex = PTHREAD_MUTEX_INITIALIZER;

static long subsong_gap = 2;
static long fadeout = 3;
static long silence_timeout = 2;
static long subsong_timeout = 2*60;
static long rate = 44100;

static pthread_t playthread;
static long stopthread = 1;
static long workunit;

static int16_t samples[4096];
static struct gbhw_buffer buffer = {
.data    = samples,
.bytes   = sizeof(samples),
.samples = sizeof(samples) / 4,
.pos     = 0,
};

static GtkWidget *dialog_fileinfo;
static GtkWidget *entry_filename;
static GtkWidget *entry_game;
static GtkWidget *entry_artist;
static GtkWidget *entry_copyright;
static GtkWidget *viewport1;

static char *file_info_title;
static char *file_info_filename;
static struct gbs *file_info_gbs;

static struct cfg_option options[] = {
	{ "rate", &rate, cfg_long },
	{ "subsong_timeout", &subsong_timeout, cfg_long },
	{ "silence_timeout", &silence_timeout, cfg_long },
	{ "subsong_gap", &subsong_gap, cfg_long },
	{ "fadeout", &fadeout, cfg_long },
	{ NULL, NULL, NULL }
};

enum gbs_state {
	STATE_STOPPED,
	STATE_RUNNING
} gbs_state = STATE_STOPPED;

/********************************************************************
 * libgbs interface
 *
 * These functions need to take the gbs_mutex before calling
 * into libgbs.
 ********************************************************************/

static long gbs_time(struct gbs *gbs, long subsong) {
	long res = 0;
	long i;

	if (!gbs) return 0;

	for (i=0; i<subsong && i<gbs->songs; i++) {
		if (gbs->subsong_info[i].len)
			res += (gbs->subsong_info[i].len * 1000) >> GBS_LEN_SHIFT;
		else res += subsong_timeout * 1000;
	}
	return res;
}

static void next_subsong(long flush)
{
	if (!(gbs_ip.output && gbs)) return;
	if (!flush) {
		gbs_ip.output->buffer_free();
		gbs_ip.output->buffer_free();
		while (gbs_ip.output->buffer_playing() && !stopthread) nanosleep(&TEN_MILLISECONDS, NULL);
	}
	DPRINTF("locking gbs_mutex\n");
	pthread_mutex_lock(&gbs_mutex);
	gbs->subsong++;
	gbs->subsong %= gbs->songs;
	gbs_init(gbs, gbs->subsong);
	pthread_mutex_unlock(&gbs_mutex);
	DPRINTF("unlocked gbs_mutex\n");
	gbs_ip.output->flush(gbs_time(gbs, gbs->subsong));
}

static void prev_subsong(void)
{
	if (!(gbs_ip.output && gbs)) return;
	DPRINTF("locking gbs_mutex\n");
	pthread_mutex_lock(&gbs_mutex);
	gbs->subsong += gbs->songs-1;
	gbs->subsong %= gbs->songs;
	gbs_init(gbs, gbs->subsong);
	pthread_mutex_unlock(&gbs_mutex);
	DPRINTF("unlocked gbs_mutex\n");
	gbs_ip.output->flush(gbs_time(gbs, gbs->subsong));
}

static void callback(struct gbhw_buffer *buf, void *priv)
{
	gbs_ip.add_vis_pcm(gbs_ip.output->written_time(),
	                   FMT_S16_NE, 2,
	                   buf->bytes, buf->data);
	gbs_ip.output->write_audio(buf->data, buf->bytes);
}

static void *playloop(void *priv)
{
	DPRINTF("playloop() entered\n");
	if (!gbs_ip.output->open_audio(FMT_S16_NE, rate, 2)) {
		puts(_("Error opening output plugin."));
		return 0;
	}
	while (!stopthread) {
		if (gbs_ip.output->buffer_free() < buffer.bytes) {
			struct timespec waittime = {
				.tv_sec = 0,
				.tv_nsec = workunit*1000
			};
			nanosleep(&waittime, NULL);
			continue;
		}

		DPRINTF("locking gbs_mutex\n");
		pthread_mutex_lock(&gbs_mutex);
		DPRINTF("gbs_step(workunit = %ld)\n", workunit);
		if (workunit == 0 ||
		    !gbs_step(gbs, workunit)) stopthread = true;
		pthread_mutex_unlock(&gbs_mutex);
		DPRINTF("unlocked gbs_mutex\n");
	}
	gbs_ip.output->close_audio();
	gbs_state = STATE_STOPPED;
	DPRINTF("state changed to STATE_STOPPED\n");
	DPRINTF("leaving playloop()\n");
	return 0;
}

static void set_song_info(struct gbs *gbs, char **title, int *length)
{
	long len = 13 + strlen(gbs->title) + strlen(gbs->author) + strlen(gbs->copyright);

	*title = malloc(len);
	*length = gbs_time(gbs, gbs->songs);

	snprintf(*title, len, "%s - %s (%s)",
	         gbs->title, gbs->author, gbs->copyright);
}

static void gbs_stop(void)
{
	if (gbs_state == STATE_RUNNING) {
		stopthread = true;
		pthread_join(playthread, 0);
	}

	if (gbs) {
		DPRINTF("locking gbs_mutex\n");
		pthread_mutex_lock(&gbs_mutex);
		gbs_close(gbs);
		gbs = NULL;
		pthread_mutex_unlock(&gbs_mutex);
		DPRINTF("unlocked gbs_mutex\n");
	}
}

static void gbs_play(char *filename)
{
	long len, length;
	char *title;

	if (gbs_state == STATE_RUNNING) gbs_stop();

	DPRINTF("locking gbs_mutex\n");
	pthread_mutex_lock(&gbs_mutex);
	if ((gbs = gbs_open(filename)) == NULL) {
		pthread_mutex_unlock(&gbs_mutex);
		DPRINTF("unlocked gbs_mutex\n");
		return;
	}

	len = 13 +
	      strlen(gbs->title) +
	      strlen(gbs->author) +
	      strlen(gbs->copyright);
	title = malloc(len);
	length = gbs_time(gbs, gbs->songs);

	snprintf(title, len, "%s - %s (%s)",
	         gbs->title, gbs->author, gbs->copyright);
	gbs_ip.set_info(title, length, 0, rate, 2);

	gbs_init(gbs, -1);
	gbhw_set_buffer(&buffer);
	gbhw_set_rate(rate);
	gbhw_set_callback(callback, NULL);
	gbs->subsong_timeout = subsong_timeout;
	gbs->gap = subsong_gap;
	gbs->silence_timeout = silence_timeout;
	gbs->fadeout = fadeout;
	DPRINTF("buffer.samples=%ld, rate=%ld\n", buffer.samples, rate);
	workunit = 1000*buffer.samples/rate;
	pthread_mutex_unlock(&gbs_mutex);
	DPRINTF("unlocked gbs_mutex\n");

	stopthread = false;
	gbs_state = STATE_RUNNING;
	DPRINTF("state changed to STATE_RUNNING\n");
	pthread_create(&playthread, 0, playloop, 0);
}

/********************************************************************
 * gtk glue code
 ********************************************************************/

static void on_button_next_clicked(GtkButton *button, gpointer user_data)
{
	next_subsong(1);
}

static void on_button_prev_clicked(GtkButton *button, gpointer user_data)
{
	prev_subsong();
}

static void on_button_save_clicked(GtkButton *button, gpointer user_data)
{
	file_info_gbs->title = strdup(gtk_entry_get_text(GTK_ENTRY(entry_game)));
	file_info_gbs->author = strdup(gtk_entry_get_text(GTK_ENTRY(entry_artist)));
	file_info_gbs->copyright = strdup(gtk_entry_get_text(GTK_ENTRY(entry_copyright)));

	gbs_write(file_info_gbs, file_info_filename);
}

static void on_button_cancel_clicked(GtkButton *button, gpointer user_data)
{
	if (file_info_gbs) {
		gbs_close(file_info_gbs);
		file_info_gbs = NULL;
	}
	gtk_widget_hide(dialog_fileinfo);
}

static void create_dialogs(void)
{
  GtkWidget *dialog_vbox1;
  GtkWidget *vbox1;
  GtkWidget *hbox1;
  GtkWidget *label1;
  GtkWidget *frame1;
  GtkWidget *frame2;
  GtkWidget *table1;
  GtkWidget *label2;
  GtkWidget *label3;
  GtkWidget *label4;
  GtkWidget *scrolledwindow1;
  GtkWidget *dialog_action_area1;
  GtkWidget *hbuttonbox1;
  GtkWidget *button_save;
  GtkWidget *button_cancel;
  GtkWidget *button_next;
  GtkWidget *button_prev;

  dialog_fileinfo = gtk_dialog_new ();
  gtk_widget_ref (dialog_fileinfo);
  gtk_object_set_data (GTK_OBJECT (dialog_fileinfo), "dialog_fileinfo", dialog_fileinfo);
  gtk_window_set_title (GTK_WINDOW (dialog_fileinfo), _("File Info"));
  gtk_window_set_policy (GTK_WINDOW (dialog_fileinfo), TRUE, TRUE, FALSE);

  dialog_vbox1 = GTK_DIALOG (dialog_fileinfo)->vbox;
  gtk_object_set_data (GTK_OBJECT (dialog_fileinfo), "dialog_vbox1", dialog_vbox1);
  gtk_widget_show (dialog_vbox1);

  vbox1 = gtk_vbox_new (FALSE, 0);
  gtk_widget_ref (vbox1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "vbox1", vbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (vbox1);
  gtk_box_pack_start (GTK_BOX (dialog_vbox1), vbox1, TRUE, TRUE, 0);

  hbox1 = gtk_hbox_new (FALSE, 5);
  gtk_widget_ref (hbox1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "hbox1", hbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbox1);
  gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox1), 5);

  label1 = gtk_label_new (_("Filename:"));
  gtk_widget_ref (label1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label1", label1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label1);
  gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0);

  entry_filename = gtk_entry_new ();
  gtk_widget_ref (entry_filename);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "entry_filename", entry_filename,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_filename);
  gtk_box_pack_start (GTK_BOX (hbox1), entry_filename, TRUE, TRUE, 0);
  gtk_entry_set_editable (GTK_ENTRY (entry_filename), FALSE);

  frame1 = gtk_frame_new (_("GBS Info"));
  gtk_widget_ref (frame1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "frame1", frame1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame1);
  gtk_box_pack_start (GTK_BOX (vbox1), frame1, FALSE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame1), 5);

  table1 = gtk_table_new (3, 2, FALSE);
  gtk_widget_ref (table1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "table1", table1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (table1);
  gtk_container_add (GTK_CONTAINER (frame1), table1);
  gtk_container_set_border_width (GTK_CONTAINER (table1), 5);
  gtk_table_set_row_spacings (GTK_TABLE (table1), 5);
  gtk_table_set_col_spacings (GTK_TABLE (table1), 5);

  label2 = gtk_label_new (_("Game:"));
  gtk_widget_ref (label2);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label2", label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  label3 = gtk_label_new (_("Artist:"));
  gtk_widget_ref (label3);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label3", label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label3);
  gtk_table_attach (GTK_TABLE (table1), label3, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label3), 0, 0.5);

  label4 = gtk_label_new (_("Copyright:"));
  gtk_widget_ref (label4);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label4", label4,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label4);
  gtk_table_attach (GTK_TABLE (table1), label4, 0, 1, 2, 3,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label4), 0, 0.5);

  entry_game = gtk_entry_new ();
  gtk_widget_ref (entry_game);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "entry_game", entry_game,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_game);
  gtk_table_attach (GTK_TABLE (table1), entry_game, 1, 2, 0, 1,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  entry_artist = gtk_entry_new ();
  gtk_widget_ref (entry_artist);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "entry_artist", entry_artist,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_artist);
  gtk_table_attach (GTK_TABLE (table1), entry_artist, 1, 2, 1, 2,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  entry_copyright = gtk_entry_new ();
  gtk_widget_ref (entry_copyright);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "entry_copyright", entry_copyright,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (entry_copyright);
  gtk_table_attach (GTK_TABLE (table1), entry_copyright, 1, 2, 2, 3,
                    (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);

  frame2 = gtk_frame_new (_("Extended Info"));
  gtk_widget_ref (frame2);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "frame2", frame2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (frame2);
  gtk_box_pack_start (GTK_BOX (vbox1), frame2, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (frame2), 5);

  scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_ref (scrolledwindow1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "scrolledwindow1", scrolledwindow1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (scrolledwindow1);
  gtk_container_add (GTK_CONTAINER (frame2), scrolledwindow1);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

  viewport1 = gtk_viewport_new (NULL, NULL);
  gtk_widget_ref (viewport1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "viewport1", viewport1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (viewport1);
  gtk_container_add (GTK_CONTAINER (scrolledwindow1), viewport1);
  gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport1), GTK_SHADOW_NONE);

  dialog_action_area1 = GTK_DIALOG (dialog_fileinfo)->action_area;
  gtk_object_set_data (GTK_OBJECT (dialog_fileinfo), "dialog_action_area1", dialog_action_area1);
  gtk_widget_show (dialog_action_area1);
  gtk_container_set_border_width (GTK_CONTAINER (dialog_action_area1), 10);

  hbuttonbox1 = gtk_hbutton_box_new ();
  gtk_widget_ref (hbuttonbox1);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "hbuttonbox1", hbuttonbox1,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (hbuttonbox1);
  gtk_box_pack_start (GTK_BOX (dialog_action_area1), hbuttonbox1, TRUE, TRUE, 0);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox1), GTK_BUTTONBOX_END);

  button_next = gtk_button_new_with_label (_("Next"));
  gtk_widget_ref (button_next);
  gtk_widget_show (button_next);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_next);
  GTK_WIDGET_SET_FLAGS (button_next, GTK_CAN_DEFAULT);

  button_prev = gtk_button_new_with_label (_("Prev"));
  gtk_widget_ref (button_prev);
  gtk_widget_show (button_prev);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_prev);
  GTK_WIDGET_SET_FLAGS (button_prev, GTK_CAN_DEFAULT);

  button_save = gtk_button_new_with_label (_("Save"));
  gtk_widget_ref (button_save);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "button_save", button_save,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_save);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_save);
  GTK_WIDGET_SET_FLAGS (button_save, GTK_CAN_DEFAULT);

  button_cancel = gtk_button_new_with_label (_("Cancel"));
  gtk_widget_ref (button_cancel);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "button_cancel", button_cancel,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_cancel);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_cancel);
  GTK_WIDGET_SET_FLAGS (button_cancel, GTK_CAN_DEFAULT);

  gtk_signal_connect (GTK_OBJECT (button_save), "clicked",
                      GTK_SIGNAL_FUNC (on_button_save_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_cancel), "clicked",
                      GTK_SIGNAL_FUNC (on_button_cancel_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_next), "clicked",
                      GTK_SIGNAL_FUNC (on_button_next_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (button_prev), "clicked",
                      GTK_SIGNAL_FUNC (on_button_prev_clicked),
                      NULL);
  gtk_signal_connect (GTK_OBJECT (dialog_fileinfo), "delete_event",
                      GTK_SIGNAL_FUNC (gtk_true),
                      NULL);
}

/********************************************************************
 * xmms plugin methods
 ********************************************************************/

static void configure(void)
{
	DPRINTF("called by xmms\n");
	prev_subsong();
}

static void about(void)
{
	DPRINTF("called by xmms\n");
	next_subsong(1);
}

static void file_info_box(char *filename)
{
	long titlelen = strlen(filename) + 12;
	DPRINTF("called by xmms\n");

	if (file_info_filename) free(file_info_filename);
	file_info_filename = strdup(filename);
	if (file_info_title) free(file_info_title);
	file_info_title = malloc(titlelen);
	strcpy(file_info_title, _("File Info: "));
	strcat(file_info_title, filename);
	gtk_window_set_title (GTK_WINDOW (dialog_fileinfo), file_info_title);
	gtk_entry_set_text(GTK_ENTRY(entry_filename), filename);

	if (file_info_gbs) gbs_close(file_info_gbs);
	if ((file_info_gbs = gbs_open(filename)) != NULL) {
		GtkWidget *label;
		long i;

		gtk_entry_set_text(GTK_ENTRY(entry_game), file_info_gbs->title);
		gtk_entry_set_text(GTK_ENTRY(entry_artist), file_info_gbs->author);
		gtk_entry_set_text(GTK_ENTRY(entry_copyright), file_info_gbs->copyright);
	}

	gtk_widget_show(dialog_fileinfo);
}

static void init(void)
{
	char *usercfg = get_userconfig(cfgfile);
	DPRINTF("called by xmms\n");
	cfg_parse(usercfg, options);
	free(usercfg);

	create_dialogs();
}

static int is_our_file(char *filename)
{
	long fd = open(filename, O_RDONLY);
	long res = false;
	char id[4];
	DPRINTF("called by xmms\n");

	read(fd, id, sizeof(id));
	close(fd);

	if (strncmp(id, "GBS\1", sizeof(id)) == 0) res = true;

	return res;
}

static void play_file(char *filename)
{
	DPRINTF("called by xmms\n");
	gbs_play(filename);
}

static void stop(void)
{
	DPRINTF("called by xmms\n");
	gbs_stop();
}

static int get_time(void)
{
	static int old_length = 0;
	char *title;
	int length;
	DPRINTF("called by xmms\n");
	set_song_info(gbs, &title, &length);
	if (length != old_length)
		gbs_ip.set_info(title, length, 0, 44100, 2);
	old_length = length;
	if (stopthread) return -1;
	return gbs_ip.output->output_time();
}

static void cleanup(void)
{
	DPRINTF("called by xmms\n");
	gtk_widget_unref(dialog_fileinfo);
}

static void get_song_info(char *filename, char **title, int *length)
{
	struct gbs *gbs = gbs_open(filename);
	DPRINTF("called by xmms\n");

	set_song_info(gbs, title, length);
	gbs_close(gbs);
}

static void seek(int time)
{
	DPRINTF("called by xmms\n");
	if (time > get_time()/1000) next_subsong(1);
	else prev_subsong();
}

static void pause_file(short paused)
{
	DPRINTF("called by xmms\n");
	gbs_ip.output->pause(paused);
}

static InputPlugin gbs_ip = {
	.description =	"GBS Player",
	.init =		init,
	.about =	about,
	.configure =	configure,
	.is_our_file =	is_our_file,
	.play_file =	play_file,
	.stop =		stop,
	.pause =	pause_file,
	.seek =		seek,
	.get_time =	get_time,
	.cleanup =	cleanup,
	.get_song_info =get_song_info,
	.file_info_box =file_info_box,
};

InputPlugin *get_iplugin_info(void)
{
	return &gbs_ip;
}
