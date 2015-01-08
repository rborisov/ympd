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
//#include <libconfig.h>
#include <pwd.h>

#include "mongoose.h"
#include "http_server.h"
#include "mpd_client.h"
#include "config.h"
#include "streamripper.h"
#include "sqlitedb.h"

#include <mpd/client.h>
//#include "ydebug.h"

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
    char radio_song_name[512] = "";
    const char *music_path = NULL, 
          *radio_path = NULL,
          *radio_url = NULL,
          *radio_dest = NULL;
    char chrbuff[512] = "";
//    char config_file_name[512] = "";
    struct passwd *pw = getpwuid(getuid());
    char *homedir = pw->pw_dir;

    rcm.last_timer = 0;

    sprintf(mpd.config_file_name, "%s/%s/%s", homedir, RCM_DIR_STR, RCM_CONF_FILE_STR);
    printf("conf = %s\n", mpd.config_file_name);

    atexit(bye);
    mg_set_option(server, "listening_port", "8080");
    mpd.port = 6600;
    strcpy(mpd.host, "127.0.0.1");

    config_init(&mpd.cfg);
    if(! config_read_file(&mpd.cfg, mpd.config_file_name))
    {
        printf("config file error %s:%d - %s\n", config_error_file(&mpd.cfg),
                config_error_line(&mpd.cfg), config_error_text(&mpd.cfg));
        config_destroy(&mpd.cfg);
        return(EXIT_FAILURE);
    }
/*    if (!config_lookup_string(&mpd.cfg, "application.music_path", &music_path))
    {
        fprintf(stderr, "No 'application.music_path' setting in configuration file.\n");
    }
*/
    /* drop privilges at last to ensure proper port binding */
    if(run_as_user != NULL)
    {
        mg_set_option(server, "run_as_user", run_as_user);
        free(run_as_user);
    }

/*    if (!config_lookup_string(&mpd.cfg, "radio.path", &radio_path))
    {
        fprintf(stderr, "No 'radio.path' setting in configuration file.\n");
    }*/
/*    if (!config_lookup_string(&mpd.cfg, "radio.url", &radio_url))
    {
        fprintf(stderr, "No 'radio.url' setting in configuration file.\n");
    }*/
/*    if (!config_lookup_string(&mpd.cfg, "radio.dest", &radio_dest))
    {
        fprintf(stderr, "No 'radio.dest' setting in configuration file.\n");
    }*/

    db_init();

//    setstream_streamripper(radio_url);
//    streamripper_set_url(NULL);
//    sprintf(chrbuff, "%s%s%s", music_path, radio_path, radio_dest);
//    setpath_streamuri(chrbuff);
//    printf("url: %s path: %s\n", radio_url, chrbuff);
    streamripper_set_url_dest(NULL);
    init_streamripper();
    mpd.radio_status = 1;
    printf("init_streamripper\n");
    
    mg_set_http_close_handler(server, mpd_close_handler);
    mg_set_request_handler(server, server_callback);

    while (!force_exit) {
        current_timer = mg_poll_server(server, 200);
        if(current_timer - last_timer)
        {
            last_timer = current_timer;
            mpd_poll(server);
            if (mpd.radio_status == 1)
                if (poll_streamripper(radio_song_name))
                {
//                    sprintf(chrbuff, "%s%s/%s", radio_path, radio_dest, radio_song_name);
//                    printf("%s\n", chrbuff);
                    mpd_run_update(mpd.conn, radio_song_name);
                    sleep(1);
                    mpd_run_add(mpd.conn, radio_song_name);
                }
        }
    }

    stop_streamripper();
    db_close();
    mpd_disconnect();
    mg_destroy_server(&server);

    config_destroy(&mpd.cfg);

    return EXIT_SUCCESS;
}
