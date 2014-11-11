/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <libconfig.h>
#include <pwd.h>

#include "mongoose.h"
#include "http_server.h"
#include "mpd_client.h"
#include "config.h"
//#include "radio.h"
#include "streamripper.h"

#include <mpd/client.h>

//#define project_name "rcarmedia"
//#define ympd_conf_file "ympd.conf"

extern char *optarg;

int force_exit = 0;

void bye()
{
    force_exit = 1;
}

static int server_callback(struct mg_connection *c) {
    if (c->is_websocket)
    {
        c->content[c->content_len] = '\0';
        if(c->content_len)
            return callback_mpd(c);
        else
            return MG_CLIENT_CONTINUE;
    }
    else
        return callback_http(c);
}

int main(int argc, char **argv)
{
    int n, option_index = 0;
    struct mg_server *server = mg_create_server(NULL);
    unsigned int current_timer = 0, last_timer = 0;
    char *run_as_user = NULL;
    char radio_song_name[512];
    const char *radio_path = NULL;
    char *radio_url = NULL;
    char radio_added_song[512];
    config_t cfg;
    config_setting_t *settings;
    char config_file_name[512];
    struct passwd *pw = getpwuid(getuid());
    char *homedir = pw->pw_dir;

    sprintf(config_file_name, "%s/%s/%s", homedir, RCM_DIR_STR, RCM_CONF_FILE_STR);
    printf("conf = %s\n", config_file_name);

    atexit(bye);
    mg_set_option(server, "listening_port", "8080");
    mpd.port = 6600;
    strcpy(mpd.host, "127.0.0.1");

    config_init(&cfg);
    if(! config_read_file(&cfg, config_file_name))
    {
        printf("config file error %s:%d - %s\n", config_error_file(&cfg),
                config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return(EXIT_FAILURE);
    }
    if (!config_lookup_string(&cfg, "application.radio_path", &radio_path))
    {
        fprintf(stderr, "No 'radio_path' setting in configuration file.\n");
    }

    /* drop privilges at last to ensure proper port binding */
    if(run_as_user != NULL)
    {
        mg_set_option(server, "run_as_user", run_as_user);
        free(run_as_user);
    }

//    mg_set_option(server, "document_root", "/home/ruinrobo/rcarmedia/");
//    mg_set_option(server, "url_rewrites", "/images=/home/ruinrobo/rcarmedia");

    if (!config_lookup_string(&cfg, "streamripper.url", &radio_url))
    {
        fprintf(stderr, "No 'radio_url' setting in configuration file.\n");
    }
    printf("url = %s\n", radio_url);

    start_streamripper();
    printf("start_streamripper\n");

    mg_set_http_close_handler(server, mpd_close_handler);
    mg_set_request_handler(server, server_callback);

//    printf("DOCUMENT_ROOT %s\n", mg_get_option(server, "document_root"));
//    printf("url_rewrites %s\n", mg_get_option(server, "url_rewrites"));

    while (!force_exit) {
        current_timer = mg_poll_server(server, 200);
        if(current_timer - last_timer)
        {
            last_timer = current_timer;
            mpd_poll(server);
            if (poll_streamripper(radio_song_name))
            {
                sprintf(radio_added_song, "%s%s", "radio/", radio_song_name);
                printf("%s\n", radio_added_song);
                mpd_run_update(mpd.conn, radio_added_song);
                sleep(1);
                mpd_run_add(mpd.conn, radio_added_song);
            }
        }
    }

    stop_streamripper();
    mpd_disconnect();
    mg_destroy_server(&server);

    config_destroy(&cfg);

    return EXIT_SUCCESS;
}
