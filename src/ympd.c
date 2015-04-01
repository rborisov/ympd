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
#include <pwd.h>

#include "mongoose.h"
#include "http_server.h"
#include "mpd_client.h"
#include "config.h"
#include "streamripper.h"
#include "sqlitedb.h"
#include "mpd_utils.h"
#include "radio.h"

#include <mpd/client.h>

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
    struct mg_server *server = mg_create_server(NULL);
    unsigned int current_timer = 0, last_timer = 0;
    char *run_as_user = NULL;
    char radio_song_name[512] = "";
    struct passwd *pw = getpwuid(getuid());
    char *homedir = pw->pw_dir;

    rcm_init();
    rcm.last_timer = 0;

    sprintf(rcm.config_file_name, "%s/%s/%s", homedir, RCM_DIR_STR, RCM_CONF_FILE_STR);
    syslog(LOG_INFO, "conf = %s\n", rcm.config_file_name);

    atexit(bye);
    mg_set_option(server, "listening_port", "8080");
    mpd.port = 6600;
    strcpy(mpd.host, "127.0.0.1");

    config_init(&rcm.cfg);
    if(! config_read_file(&rcm.cfg, rcm.config_file_name))
    {
        syslog(LOG_INFO, "config file error %s:%d - %s\n", config_error_file(&rcm.cfg),
                config_error_line(&rcm.cfg), config_error_text(&rcm.cfg));
        config_destroy(&rcm.cfg);
        return(EXIT_FAILURE);
    }

    //TODO: read mpd.radio_status from config

    /* drop privilges at last to ensure proper port binding */
    if(run_as_user != NULL)
    {
        mg_set_option(server, "run_as_user", run_as_user);
        free(run_as_user);
    }

    db_init();

    rcm.radio_status = 0;
    if (cfg_get_radio_status() == 1) {
        streamripper_set_url_dest(NULL);
        init_streamripper();
        rcm.radio_status = 1;
        syslog(LOG_INFO, "init_streamripper\n");
    }

    mg_set_http_close_handler(server, mpd_close_handler);
    mg_set_request_handler(server, server_callback);

    while (!force_exit) {
        current_timer = mg_poll_server(server, 200);
        if(current_timer - last_timer)
        {
            last_timer = current_timer;
            mpd_poll(server);
            if (www_online()) {
                if (rcm.radio_status == 1) {
                    if (poll_streamripper(radio_song_name))
                    {
                        mpd_run_update(mpd.conn, radio_song_name);
                        sleep(1);
                        mpd_run_add(mpd.conn, radio_song_name);
                        //mpd_insert(mpd.conn, radio_song_name);
                    }
                }
            }
        }
    }

    if (rcm.radio_status == 1)
        stop_streamripper();
    db_close();

    rcm_close();

    mpd_disconnect();
    mg_destroy_server(&server);

    config_destroy(&rcm.cfg);

    return EXIT_SUCCESS;
}
