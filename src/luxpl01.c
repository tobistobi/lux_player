// Thanks to all code and dev sources for enabling this. 
// 修改自：http://git.videolan.org/?p=vlc.git;a=blob;f=doc/libvlc/gtk_player.c

#include <stdlib.h>
#include <stdio.h>
#include <wiringPi.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <vlc/vlc.h>
#include <X11/Xlib.h>

void destroy(GtkWidget *widget, gpointer data);
void player_widget_on_realize(GtkWidget *widget, gpointer data);
void play(void);
void pause_player(void);

libvlc_media_t *media;
libvlc_media_player_t *media_player;
libvlc_instance_t *vlc_inst;
libvlc_state_t status;

GtkWidget *playpause_button,*play_icon_image,*pause_icon_image,*stop_icon_image,
		  *process_scale;
GtkAdjustment *process_adjuest;

GtkWidget *window,
              *vbox,
			  *hbox,
              *menubar,
              *filemenu,
              *fileitem,
              *filemenu_openitem,
              *player_widget,
              *hbuttonbox,
              *stop_button,
	      *g_lbl_hello;
	      
GdkRectangle workarea = {0};

float video_length, current_play_time, media_position;
char media_pos_str[255];

//init vars for GPIO states
int sensor_event = 0;
int sensor_state = 1;
int idle_status = 1;
int ended = 0;

// init vars for config file
char fn_idle[100];
char fn_content[100];
int wait_thr;
int is_debug;
char debug_str[512];

//setup startup mode
int startup = 0;

void destroy(GtkWidget *widget, gpointer data) {
    gtk_main_quit();
    libvlc_media_player_release(media_player);
    libvlc_release(vlc_inst);
    exit(0);
}

void player_widget_on_realize(GtkWidget *widget, gpointer data) {
    libvlc_media_player_set_xwindow((libvlc_media_player_t*)data, GDK_WINDOW_XID(gtk_widget_get_window(widget)));
    
}

void play(void) {
    libvlc_media_player_play(media_player);
}

void pause_player(void) {
    libvlc_media_player_pause(media_player);
}

//GPPIO check
void sensor_read()
{
    if(digitalRead(1) == 1)
    {
	if (sensor_state == 1)
	{
	    sensor_state = 0;
	    sensor_event = 1;
	}
    } else {
	if (sensor_state == 0)
	{
	    sensor_state = 1;
	    sensor_event = 1;
	}
    }
    return;
}

// read player setup from config
void read_config()
{
    FILE *fp;
    char buff[100];

    fp = fopen("luxpl.config", "r");
    fscanf(fp, "%s", buff);
    printf("1 : %s\n", buff);
    fscanf(fp, "%s", fn_idle);
    printf("idle : %s\n", fn_idle);
    fscanf(fp, "%s", buff);
    printf("2 : %s\n", buff);
    fscanf(fp, "%s", fn_content);
    printf("content : %s\n", fn_content);
    fscanf(fp, "%s", buff);
    printf("3 : %s\n", buff);  
    fscanf(fp, "%s", buff);
    wait_thr = atoi(buff);
    printf("wait threshold (ms) : %s : %d\n", buff, wait_thr); 
    fscanf(fp, "%s", buff);
    printf("4 : %s\n", buff);  
    fscanf(fp, "%s", buff);
    is_debug = atoi(buff);
    printf("wait threshold (ms) : %s : %d\n", buff, is_debug);
    fclose(fp);
    return;
}

void init_run()
{
    //query primary screen resolution
    gdk_monitor_get_geometry(gdk_display_get_primary_monitor(gdk_display_get_default()), &workarea);

    // setup window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), workarea.width, workarea.height);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_title(GTK_WINDOW(window), "PLAYER");

    //setup box
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, FALSE);
    gtk_container_add(GTK_CONTAINER(window), vbox);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, FALSE);

    //setup player widget
    player_widget = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(vbox), player_widget, TRUE, TRUE, 0);
    
     //get config info
    read_config();
    
    if (is_debug)
    {
	g_lbl_hello = gtk_label_new (NULL);
	gtk_box_pack_start(GTK_BOX(vbox), g_lbl_hello, FALSE, TRUE, 0); //trial
	gtk_label_set_text(GTK_LABEL(g_lbl_hello), "Debug Mode.");
    }
    
    //setup vlc
    vlc_inst = libvlc_new(0, NULL);
    media_player = libvlc_media_player_new(vlc_inst);
    g_signal_connect(G_OBJECT(player_widget), "realize", G_CALLBACK(player_widget_on_realize), media_player);

    //initialize gtk window
    gtk_widget_show_all(window);
    gtk_window_fullscreen(GTK_WINDOW(window));
    
    //start idle loop as initial state
    if (idle_status == 1)
    {
	media = libvlc_media_new_path(vlc_inst, fn_idle);
	libvlc_media_player_set_media(media_player, media);
	play();
	libvlc_media_release(media);
    }
    
    // Setup GPIO
    wiringPiSetup();
    pinMode(1, INPUT);
    // Pull up to 3.3V,make GPIO1 a stable level
    pullUpDnControl(1, PUD_UP);
}

int main(int argc, char *argv[]) {

    //run once at startup
    if (!startup)
	{
	    startup = 1;
	    XInitThreads(); //without the XInitThreads() the video doesn't scale large enough due to performance issues
	    gtk_init (&argc, &argv);
	    init_run();
	}
	
    //control loop
     while (1)
    {   
       while (gtk_events_pending())
        {
            gtk_main_iteration();
        }
        sensor_read();

	if (is_debug)
	{
	    media_position = libvlc_media_player_get_position(media_player);
	    gcvt(media_position, 10, media_pos_str);
	    if (idle_status)
		{
		    sprintf(debug_str, "W: %u H: %u - %s - Idle Mode. Play Pos : %s ", workarea.width, workarea.height, fn_idle, media_pos_str);
		} else {
		    sprintf(debug_str, "W: %u H: %u - %s - Content Mode. Play Pos : %s ", workarea.width, workarea.height, fn_content, media_pos_str);
		}
	    gtk_label_set_text(GTK_LABEL(g_lbl_hello), debug_str);
	}
	
	//AUTOLOOP - get play status to check for 6 == media ended
	status = libvlc_media_get_state(media);	
	if (status == 6) {
	    if (idle_status == 1)
	    {
		
		media = libvlc_media_new_path(vlc_inst, fn_idle);
		libvlc_media_player_set_media(media_player, media);
		play();
		libvlc_media_release(media);
	    } else {
		
		media = libvlc_media_new_path(vlc_inst, fn_content);
		libvlc_media_player_set_media(media_player, media);
		play();
		libvlc_media_release(media);
	    }
	}
	
	//Check for GPIO Events and act
	if (sensor_event == 1)
	{
	    sensor_event = 0;
	    if (idle_status == 1)
	    {
		idle_status = 0;
		media = libvlc_media_new_path(vlc_inst, fn_content);
		libvlc_media_player_set_media(media_player, media);
		play();
		libvlc_media_release(media);
	    } else {
		idle_status = 1;
		media = libvlc_media_new_path(vlc_inst, fn_idle);
		libvlc_media_player_set_media(media_player, media);
		play();
		libvlc_media_release(media);
	    }
	}
    }

    return 0;
}
