#ifndef __RADIO_H__
#define __RADIO_H__

#include <libconfig.h>

struct t_rcm {
    char status_str[128];
    char filesize_str[64];
    char current_radio[128];
    char file_path[128];

    int radio_status; //0,1
    int image_update; //0,1

    unsigned int current_timer, last_timer;

    config_t cfg;
    char config_file_name[512];
} rcm;

char* cfg_get_db_path();

#endif
