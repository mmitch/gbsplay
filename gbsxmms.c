/* $Id: gbsxmms.c,v 1.24 2003/12/07 09:49:14 mitch Exp $
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
#include <gtk/gtk.h>

#include "gbhw.h"
#include "gbcpu.h"
#include "gbs.h"
#include "cfgparser.h"

InputPlugin gbs_ip;

GtkWidget *dialog_fileinfo;
GtkWidget *entry_filename;
GtkWidget *entry_game;
GtkWidget *entry_artist;
GtkWidget *entry_copyright;
GtkWidget *table2;
GtkWidget *viewport1;

char *fileinfo_filename;

static struct gbs *gbs;
static pthread_mutex_t gbs_mutex = PTHREAD_MUTEX_INITIALIZER;

static int subsong_gap = 2;
static int fadeout = 3;
static int silence_timeout = 2;
static int subsong_timeout = 2*60;
static int rate = 44100;

static int stopthread = 1;

static int16_t samples[4096];
static struct gbhw_buffer buffer = {
.data = samples,
.len  = sizeof(samples)/sizeof(int16_t),
.pos  = 0,
};

static struct cfg_option options[] = {
	{ "rate", &rate, cfg_int },
	{ "subsong_timeout", &subsong_timeout, cfg_int },
	{ "silence_timeout", &silence_timeout, cfg_int },
	{ "subsong_gap", &subsong_gap, cfg_int },
	{ "fadeout", &fadeout, cfg_int },
	{ NULL, NULL, NULL }
};

static int gbs_time(struct gbs *gbs, int subsong) {
	int res = 0;
	int i;

	if (!gbs) return 0;

	for (i=0; i<subsong && i<gbs->songs; i++) {
		if (gbs->subsong_info[i].len)
			res += (gbs->subsong_info[i].len * 1000) >> GBS_LEN_SHIFT;
		else res += subsong_timeout * 1000;
	}
	return res;
}

static void next_subsong(int flush)
{
	if (!(gbs_ip.output && gbs)) return;
	if (!flush) {
		gbs_ip.output->buffer_free();
		gbs_ip.output->buffer_free();
		while (gbs_ip.output->buffer_playing() && !stopthread) usleep(10000);
	}
	gbs->subsong++;
	gbs->subsong %= gbs->songs;
	gbs_init(gbs, gbs->subsong);
	gbs_ip.output->flush(gbs_time(gbs, gbs->subsong));
}

static void prev_subsong(void)
{
	if (!(gbs_ip.output && gbs)) return;
	gbs->subsong += gbs->songs-1;
	gbs->subsong %= gbs->songs;
	gbs_init(gbs, gbs->subsong);
	gbs_ip.output->flush(gbs_time(gbs, gbs->subsong));
}

static void configure(void)
{
	prev_subsong();
}

static void about(void)
{
	next_subsong(1);
}

char *file_info_title;
struct gbs *file_info_gbs;

static void file_info_box(char *filename)
{
	int titlelen = strlen(filename) + 12;

	if (fileinfo_filename) free(fileinfo_filename);
	fileinfo_filename = strdup(filename);
	if (file_info_title) free(file_info_title);
	file_info_title = malloc(titlelen);
	strcpy(file_info_title, "File Info: ");
	strcat(file_info_title, filename);
	gtk_window_set_title (GTK_WINDOW (dialog_fileinfo), file_info_title);
	gtk_entry_set_text(GTK_ENTRY(entry_filename), filename);

	if (file_info_gbs) gbs_close(file_info_gbs);
	if ((file_info_gbs = gbs_open(filename)) != NULL) {
		GtkWidget *label;
		int i;

		gtk_entry_set_text(GTK_ENTRY(entry_game), file_info_gbs->title);
		gtk_entry_set_text(GTK_ENTRY(entry_artist), file_info_gbs->author);
		gtk_entry_set_text(GTK_ENTRY(entry_copyright), file_info_gbs->copyright);

		if (table2) {
			gtk_container_remove (GTK_CONTAINER (viewport1), table2);
			gtk_widget_unref(GTK_WIDGET(table2));
		}

		table2 = gtk_table_new (file_info_gbs->songs + 1, 3, FALSE);
		gtk_widget_ref(table2);
		gtk_widget_show(table2);
		gtk_container_add (GTK_CONTAINER (viewport1), table2);
		gtk_container_set_border_width (GTK_CONTAINER (table2), 5);
		gtk_table_set_row_spacings (GTK_TABLE (table2), 5);
		gtk_table_set_col_spacings (GTK_TABLE (table2), 5);

		label = gtk_label_new("Subsong");
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table2), label,
		                 0, 1, 0, 1,
		                 (GtkAttachOptions) (GTK_FILL),
		                 (GtkAttachOptions) (0), 0, 0);
		label = gtk_label_new("Title");
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table2), label,
		                 1, 2, 0, 1,
		                 (GtkAttachOptions) (GTK_FILL),
		                 (GtkAttachOptions) (0), 0, 0);
		label = gtk_label_new("Length    ");
		gtk_widget_show(label);
		gtk_table_attach(GTK_TABLE(table2), label,
		                 2, 3, 0, 1,
		                 (GtkAttachOptions) (GTK_FILL),
		                 (GtkAttachOptions) (0), 0, 0);

		for (i=0; i<file_info_gbs->songs; i++) {
			char buf[5];
			GtkWidget *label;
			GtkWidget *entry = gtk_entry_new();
			GtkObject *sb_adj = gtk_adjustment_new(
			                     file_info_gbs->subsong_info[i].len >> GBS_LEN_SHIFT,
			                     0, 1800, 1, 10, 10);
			GtkWidget *spinbutton = gtk_spin_button_new(GTK_ADJUSTMENT(sb_adj), 1, 2);

			snprintf(buf, sizeof(buf), "%03d:", i);
			label = gtk_label_new(buf);

			if (file_info_gbs->subsong_info[i].title)
				gtk_entry_set_text(GTK_ENTRY(entry),
				                   file_info_gbs->subsong_info[i].title);
			gtk_widget_show(label);
			gtk_widget_show(entry);
			gtk_widget_show(spinbutton);
			gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
			gtk_table_attach(GTK_TABLE(table2), label,
			                 0, 1, i+1, i+2,
			                 (GtkAttachOptions) (GTK_FILL),
			                 (GtkAttachOptions) (0), 0, 0);
			gtk_table_attach(GTK_TABLE(table2), entry,
			                 1, 2, i+1, i+2,
			                 (GtkAttachOptions) (GTK_EXPAND|GTK_FILL),
			                 (GtkAttachOptions) (0), 0, 0);
			gtk_table_attach(GTK_TABLE(table2), spinbutton,
			                 2, 3, i+1, i+2,
			                 (GtkAttachOptions) (GTK_FILL),
			                 (GtkAttachOptions) (0), 0, 0);
		}
	}

	gtk_widget_show(dialog_fileinfo);
}

static void writebuffer(void)
{
	gbs_ip.add_vis_pcm(gbs_ip.output->written_time(),
	                   FMT_S16_NE, 2,
	                   buffer.pos*sizeof(int16_t), buffer.data);
	gbs_ip.output->write_audio(buffer.data, buffer.pos*sizeof(int16_t));
	buffer.pos = 0;
}

void tableenum(gpointer data, gpointer user_data)
{
	int i;
	GtkTableChild *child = (GtkTableChild*) data;

	i = child->top_attach;
	if (i>0) switch (child->left_attach) {
	case 1:
		file_info_gbs->subsong_info[i-1].title = strdup(
			gtk_entry_get_text(GTK_ENTRY(child->widget)));
		break;
	case 2:
		{
			float value = gtk_spin_button_get_value_as_float(GTK_SPIN_BUTTON(child->widget));
			value *= (float)GBS_LEN_DIV;
			file_info_gbs->subsong_info[i-1].len = (int) value;
		}
		break;
	}
}

void on_button_next_clicked(GtkButton *button, gpointer user_data)
{
	pthread_mutex_lock(&gbs_mutex);
	next_subsong(1);
	pthread_mutex_unlock(&gbs_mutex);
}

void on_button_prev_clicked(GtkButton *button, gpointer user_data)
{
	pthread_mutex_lock(&gbs_mutex);
	prev_subsong();
	pthread_mutex_unlock(&gbs_mutex);
}

void on_button_save_clicked(GtkButton *button, gpointer user_data)
{
	file_info_gbs->title = strdup(gtk_entry_get_text(GTK_ENTRY(entry_game)));
	file_info_gbs->author = strdup(gtk_entry_get_text(GTK_ENTRY(entry_artist)));
	file_info_gbs->copyright = strdup(gtk_entry_get_text(GTK_ENTRY(entry_copyright)));

	g_list_foreach(GTK_TABLE(table2)->children, tableenum, NULL);

	gbs_write(file_info_gbs, fileinfo_filename, 2);
}

void on_button_cancel_clicked(GtkButton *button, gpointer user_data)
{
	if (file_info_gbs) {
		gbs_close(file_info_gbs);
		file_info_gbs = NULL;
		if (table2) {
			gtk_container_remove (GTK_CONTAINER (viewport1), table2);
			gtk_widget_unref(GTK_WIDGET(table2));
			table2 = NULL;
		}

	}
	gtk_widget_hide(dialog_fileinfo);
}

static char *cfgfile = ".xmms/gbsxmmsrc";

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
  gtk_object_set_data (GTK_OBJECT (dialog_fileinfo), "dialog_fileinfo", dialog_fileinfo);
  gtk_window_set_title (GTK_WINDOW (dialog_fileinfo), "File Info");
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

  label1 = gtk_label_new ("Filename:");
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

  frame1 = gtk_frame_new ("GBS Info");
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

  label2 = gtk_label_new ("Game:");
  gtk_widget_ref (label2);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label2", label2,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label2);
  gtk_table_attach (GTK_TABLE (table1), label2, 0, 1, 0, 1,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label2), 0, 0.5);

  label3 = gtk_label_new ("Artist:");
  gtk_widget_ref (label3);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "label3", label3,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (label3);
  gtk_table_attach (GTK_TABLE (table1), label3, 0, 1, 1, 2,
                    (GtkAttachOptions) (GTK_FILL),
                    (GtkAttachOptions) (0), 0, 0);
  gtk_misc_set_alignment (GTK_MISC (label3), 0, 0.5);

  label4 = gtk_label_new ("Copyright:");
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

  frame2 = gtk_frame_new ("Extended Info");
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

  button_next = gtk_button_new_with_label ("Next");
  gtk_widget_ref (button_next);
  gtk_widget_show (button_next);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_next);
  GTK_WIDGET_SET_FLAGS (button_next, GTK_CAN_DEFAULT);

  button_prev = gtk_button_new_with_label ("Prev");
  gtk_widget_ref (button_prev);
  gtk_widget_show (button_prev);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_prev);
  GTK_WIDGET_SET_FLAGS (button_prev, GTK_CAN_DEFAULT);

  button_save = gtk_button_new_with_label ("Save");
  gtk_widget_ref (button_save);
  gtk_object_set_data_full (GTK_OBJECT (dialog_fileinfo), "button_save", button_save,
                            (GtkDestroyNotify) gtk_widget_unref);
  gtk_widget_show (button_save);
  gtk_container_add (GTK_CONTAINER (hbuttonbox1), button_save);
  GTK_WIDGET_SET_FLAGS (button_save, GTK_CAN_DEFAULT);

  button_cancel = gtk_button_new_with_label ("Cancel");
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

static void init(void)
{
	char *homedir = getenv("HOME");
	char *usercfg = malloc(strlen(homedir) + strlen(cfgfile) + 2);

	sprintf(usercfg, "%s/%s", homedir, cfgfile);
	cfg_parse(usercfg, options);

	create_dialogs();
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

static int workunit = 50; /* in msec */

void *playloop(void *priv)
{
	if (!gbs_ip.output->open_audio(FMT_S16_NE, rate, 2)) {
		puts("Error opening output plugin.");
		return 0;
	}
	while (!stopthread) {
		if (gbs_ip.output->buffer_free() < buffer.len*sizeof(int16_t)) {
			usleep(workunit*1000);
			continue;
		}

		pthread_mutex_lock(&gbs_mutex);
		if (!gbs_step(gbs, workunit)) stopthread = true;
		pthread_mutex_unlock(&gbs_mutex);

		if (!stopthread) writebuffer();
	}
	gbs_ip.output->close_audio();
	return 0;
}

static void play_file(char *filename)
{
	if ((gbs = gbs_open(filename)) != NULL) {
		int len = 13 +
			strlen(gbs->title) +
			strlen(gbs->author) +
			strlen(gbs->copyright);
		char *title = malloc(len);
		int length = gbs_time(gbs, gbs->songs);

		snprintf(title, len, "%s - %s (%s)",
		         gbs->title, gbs->author, gbs->copyright);
		gbs_ip.set_info(title, length, 0, rate, 2);

		gbhw_init(gbs->rom, gbs->romsize);
		gbhw_setbuffer(&buffer);
		gbhw_setrate(rate);
		workunit = 1000*(buffer.len/2)/rate;
		gbs_init(gbs, -1);
		gbs->subsong_timeout = subsong_timeout;
		gbs->gap = subsong_gap;
		gbs->silence_timeout = silence_timeout;
		gbs->fadeout = fadeout;
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
	gtk_widget_unref(dialog_fileinfo);
}

static void get_song_info(char *filename, char **title, int *length)
{
	struct gbs *gbs = gbs_open(filename);
	int len = 13 + strlen(gbs->title) + strlen(gbs->author) + strlen(gbs->copyright);

	*title = malloc(len);
	*length = gbs_time(gbs, gbs->songs);

	snprintf(*title, len, "%s - %s (%s)",
	         gbs->title, gbs->author, gbs->copyright);

	gbs_close(gbs);
}

static void seek(int time)
{
	pthread_mutex_lock(&gbs_mutex);
	if (time > get_time()/1000) next_subsong(1);
	else prev_subsong();
	pthread_mutex_unlock(&gbs_mutex);
}

static void pause_file(short paused)
{
       gbs_ip.output->pause(paused);
}

InputPlugin gbs_ip = {
description:	"GBS Player",
init:		init,
is_our_file:	is_our_file,
configure:	configure,
about:		about,
play_file:	play_file,
pause:		pause_file,
stop:		stop,
get_time:	get_time,
cleanup:	cleanup,
get_song_info:	get_song_info,
file_info_box:	file_info_box,
seek:		seek,
};

InputPlugin *get_iplugin_info(void)
{
	return &gbs_ip;
}
