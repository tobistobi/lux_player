 
// gcc -o gtk_player gtk_player.c `pkg-config --libs gtk+-3.0 libvlc` `pkg-config --cflags gtk+-3.0 libvlc`
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

#define BORDER_WIDTH 6

void destroy(GtkWidget *widget, gpointer data);
void player_widget_on_realize(GtkWidget *widget, gpointer data);
void on_open(GtkWidget *widget, gpointer data);
void open_media(const char* uri);
void on_playpause(GtkWidget *widget, gpointer data);
void on_stop(GtkWidget *widget, gpointer data);
void play(void);
void pause_player(void);
gboolean _update_scale(gpointer data);
void on_value_change(GtkWidget *widget, gpointer data);

libvlc_media_t *media;
libvlc_media_player_t *media_player;
libvlc_instance_t *vlc_inst;
libvlc_state_t status;

void *pUserData = 0;

libvlc_event_manager_t *lux_event_manager;

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

float video_length, current_play_time, media_position, last_position = -5.0;
char media_pos_str[255];

//init vars for GPIO states
int sensor_event = 0;
int sensor_state = 1;
int idle_status = 1;
int ended = 0;

// init vars for config file
char fn_idle[255];
char fn_content[255];
int wait_thr;
int is_debug;
char debug_str[255];

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


void on_open(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;

    dialog = gtk_file_chooser_dialog_new("open file", GTK_WINDOW(widget), action, _("Cancel"), GTK_RESPONSE_CANCEL, _("Open"), GTK_RESPONSE_ACCEPT, NULL);

    if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *uri;
        uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(dialog));
        open_media(uri);
        g_free(uri);
    }
    gtk_widget_destroy(dialog);
}

void open_media(const char* uri) {
    media = libvlc_media_new_location(vlc_inst, uri);
    libvlc_media_player_set_media(media_player, media);

	current_play_time = 0.0f;
	gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
    play();
	g_timeout_add(500,_update_scale,process_scale);
    libvlc_media_release(media);
}

void on_playpause(GtkWidget *widget, gpointer data) {
    if(libvlc_media_player_is_playing(media_player) == 1) {
        pause_player();
    }
    else {
        play();
    }
}

void on_stop(GtkWidget *widget, gpointer data) {
    pause_player();
    libvlc_media_player_stop(media_player);
}


void on_value_change(GtkWidget *widget, gpointer data)
{
	float scale_value = gtk_adjustment_get_value(process_adjuest);
	//printf("%f\n",scale_value);
	libvlc_media_player_set_position(media_player, scale_value/100);
}

gboolean _update_scale(gpointer data){
	// 获取当前打开视频的长度，时间单位为ms
	video_length = libvlc_media_player_get_length(media_player);
	current_play_time = libvlc_media_player_get_time(media_player);

	g_signal_handlers_block_by_func(G_OBJECT(process_scale), on_value_change, NULL);
	gtk_adjustment_set_value(process_adjuest,current_play_time/video_length*100);
	g_signal_handlers_unblock_by_func(G_OBJECT(process_scale), on_value_change, NULL);
	return G_SOURCE_CONTINUE;
}

void play(void) {
    libvlc_media_player_play(media_player);
    pause_icon_image = gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(playpause_button), pause_icon_image);
}

void pause_player(void) {
    libvlc_media_player_pause(media_player);
    play_icon_image = gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(playpause_button), play_icon_image);
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
    char buff[255];

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

    // setup window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 2560, 1440);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    gtk_container_set_border_width (GTK_CONTAINER (window), 0);
    gtk_window_set_title(GTK_WINDOW(window), "LUXOOM");

    //setup box
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, FALSE);
    gtk_container_add(GTK_CONTAINER(window), vbox);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, FALSE);

    /*/setup menu
    menubar = gtk_menu_bar_new();
    filemenu = gtk_menu_new();
    fileitem = gtk_menu_item_new_with_label ("File");
    filemenu_openitem = gtk_menu_item_new_with_label("Open");
    gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), filemenu_openitem);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(fileitem), filemenu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), fileitem);

    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    g_signal_connect(filemenu_openitem, "activate", G_CALLBACK(on_open), window);*/

    //setup player widget
    player_widget = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(vbox), player_widget, TRUE, TRUE, 0);

    /*/setup controls
    playpause_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    stop_button = gtk_button_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_BUTTON);

    g_signal_connect(playpause_button, "clicked", G_CALLBACK(on_playpause), NULL);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop), NULL);

    hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), BORDER_WIDTH);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);

    gtk_box_pack_start(GTK_BOX(hbuttonbox), playpause_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbuttonbox), stop_button, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(hbox), hbuttonbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);*/
    
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
    //libvlc_set_fullscreen(media_player, TRUE);
    
	//setup scale
	process_adjuest = gtk_adjustment_new(0.00, 0.00, 100.00, 1.00, 0.00, 0.00);
	process_scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL,process_adjuest);
	gtk_box_pack_start(GTK_BOX(hbox), process_scale, TRUE, TRUE, 0);
	gtk_scale_set_draw_value (GTK_SCALE(process_scale), FALSE);
	gtk_scale_set_has_origin (GTK_SCALE(process_scale), TRUE);
	gtk_scale_set_value_pos(GTK_SCALE(process_scale), 5);
	g_signal_connect(G_OBJECT(process_scale),"value_changed", G_CALLBACK(on_value_change), NULL);

    gtk_widget_show_all(window);
    gtk_window_fullscreen(GTK_WINDOW(window));
    
    //start idle loop as initial state
    if (idle_status == 1)
    {
	media = libvlc_media_new_path(vlc_inst, fn_idle);
	libvlc_media_player_set_media(media_player, media);

	current_play_time = 0.0f;
	gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
	play();
	g_timeout_add(500,_update_scale,process_scale);
	libvlc_media_release(media);
    }
    
    // Setup GPIO
    wiringPiSetup();
    pinMode(1, INPUT);
    // Pull up to 3.3V,make GPIO1 a stable level
    pullUpDnControl(1, PUD_UP);
}

int main(int argc, char *argv[]) {


    if (!startup)
	{
	    startup = 1;
	    XInitThreads();
	    gtk_init (&argc, &argv);
	    init_run();
	}
	
    //making the control loop
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
		    strcpy(debug_str, "Idle Mode. Play Pos : ");
		} else {
		    strcpy(debug_str, "Content Mode. Plaz Pos : ");
		}
	    strcat(debug_str, media_pos_str);
	    gtk_label_set_text(GTK_LABEL(g_lbl_hello), debug_str);
	}
	
	//get play status to check for 6 == media ended
	status = libvlc_media_get_state(media);
	
	if (status == 6) {
	    if (idle_status == 1)
	    {
		
		media = libvlc_media_new_path(vlc_inst, fn_idle);
		libvlc_media_player_set_media(media_player, media);

		current_play_time = 0.0f;
		gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
		play();
		g_timeout_add(500,_update_scale,process_scale);
		libvlc_media_release(media);
	    } else {
		
		media = libvlc_media_new_path(vlc_inst, fn_content);
		libvlc_media_player_set_media(media_player, media);
	
		current_play_time = 0.0f;
		gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
		play();
		g_timeout_add(500,_update_scale,process_scale);
		libvlc_media_release(media);
	    }
	}
	
	if (sensor_event == 1)
	{
	    sensor_event = 0;
	    if (idle_status == 1)
	    {
		idle_status = 0;
		media = libvlc_media_new_path(vlc_inst, fn_content);
		libvlc_media_player_set_media(media_player, media);

		current_play_time = 0.0f;
		gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
		play();
		g_timeout_add(500,_update_scale,process_scale);
		libvlc_media_release(media);
	    } else {
		idle_status = 1;
		media = libvlc_media_new_path(vlc_inst, fn_idle);
		libvlc_media_player_set_media(media_player, media);
	
		current_play_time = 0.0f;
		gtk_scale_set_value_pos(GTK_SCALE(process_scale), current_play_time/video_length*100);
		play();
		g_timeout_add(500,_update_scale,process_scale);
		libvlc_media_release(media);
	    }
	}
    }


    
    return 0;
}
