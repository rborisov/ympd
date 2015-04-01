#ifndef __RADIO_H__
#define __RADIO_H__

#include <libconfig.h>
#include <syslog.h>

#define MAX_LINE 128

struct t_rcm {
    char status_str[MAX_LINE];
    char filesize_str[64];
    char current_radio[MAX_LINE];
    char file_path[MAX_LINE];

    int radio_status; //0,1
    int image_update; //0,1
    int online;

    unsigned int current_timer, last_timer;

    config_t cfg;
    char config_file_name[MAX_LINE];
} rcm;

int rcm_init();
void rcm_close();

char* cfg_get_db_path();
int cfg_get_radio_status();
void cfg_set_radio_status(int status);

//check internet connection
void *www_online();

void radio_start();
void radio_stop();
int radio_toggle();
int radio_get_status();

#endif
