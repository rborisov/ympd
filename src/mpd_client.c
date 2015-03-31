/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org
   (c) 2014-2015 Roman Borisov <rborisoff@gmail.com>

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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <libgen.h>
#include <mpd/client.h>
#include <pwd.h>
#include <curl/curl.h>

#include "mpd_client.h"
#include "mpd_utils.h"
#include "config.h"
#include "json_encode.h"
#include "radio.h"
#include "sqlitedb.h"
#include "streamripper.h"

const char * mpd_cmd_strs[] = {
    MPD_CMDS(GEN_STR)
};

static int queue_is_empty = 0;
char outfn[128];
char outdir[128];

int mpd_get_track_info(char *buffer);
int mpd_put_current_radio(char *buffer);
int mpd_db_update_current_song_rating(int increase);
int mpd_put_artist_art(char *buffer, char *artist);

int mpd_crop(struct mpd_connection *conn)
{
    struct mpd_status *status = mpd_run_status(conn);
    if (status == 0)
        return 0;
    int length = mpd_status_get_queue_length(status) - 1;

    if (length < 0) {
        mpd_status_free(status);
        syslog(LOG_INFO, "%s: A playlist longer than 1 song in length is required to crop.\n", __func__);
    } else if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
            mpd_status_get_state(status) == MPD_STATE_PAUSE) {
        if (!mpd_command_list_begin(conn, false)) {
            syslog(LOG_ERR, "%s: mpd_command_list_begin failed\n", __func__);
            return 0; //printErrorAndExit(conn);
        }

        for (; length >= 0; --length)
            if (length != mpd_status_get_song_pos(status))
                mpd_send_delete(conn, length);

        mpd_status_free(status);

        if (!mpd_command_list_end(conn) || !mpd_response_finish(conn)) {
            syslog(LOG_ERR, "%s: mpd_command_list_end || mpd_response_finish failed\n", __func__);
            return 0; //printErrorAndExit(conn);
        }

        return 0;
    } else {
        mpd_status_free(status);
        syslog(LOG_INFO, "%s: You need to be playing to crop the playlist\n", __func__);
        return 0;
    }
    return 1;
}

static inline enum mpd_cmd_ids get_cmd_id(char *cmd)
{
    for(int i = 0; i < sizeof(mpd_cmd_strs)/sizeof(mpd_cmd_strs[0]); i++)
        if(!strncmp(cmd, mpd_cmd_strs[i], strlen(mpd_cmd_strs[i])))
            return i;

    return -1;
}

char* download_file(char* url, char* artist)
{
    CURL *curl;
    FILE *fp;
    CURLcode res;
    struct passwd *pw = getpwuid(getuid());
    char *out, *images_dir, *ext, *homedir = pw->pw_dir;
    curl = curl_easy_init();
    if (curl)
    {
        if (!config_lookup_string(&rcm.cfg, "application.images_path", &images_dir))
        {
            syslog(LOG_ERR, "%s: No 'application.images_path' setting in configuration file.\n", __func__);
            sprintf(outdir, "%s/%s/images%s", homedir, RCM_DIR_STR, artist);
        } else {
            sprintf(outdir, "%s%s", images_dir, artist);
        }
        sprintf(outfn, "%s%s", outdir, strrchr(url, '/' ));
        mkdir(images_dir, 0777);
        mkdir(outdir, 0777);
        //syslog(LOG_INFO, "%s\n", strrchr(outfn, '.'));
        if (access(outfn, F_OK) != -1) {
            syslog(LOG_INFO, "%s: file already exists %s\n", __func__, outfn);
            goto done;
        }
        syslog(LOG_INFO, "%s out_filename: %s\n", __func__, outfn);
        fp = fopen(outfn,"wb");
        if (fp) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            res = curl_easy_perform(curl);
            fclose(fp);
            curl_easy_cleanup(curl);
            if (res != 0) {
                syslog(LOG_DEBUG, "%s error!\n", __func__);
                return NULL;
            }
        }
    }
done:
    //out = strrchr(outfn, '/' )+1;
    sprintf(outdir, "%s%s", artist, strrchr(outfn, '/'));
    syslog(LOG_DEBUG, "%s downloaded %s\n", __func__, outdir);

    return outdir;
}

void db_put_album(char* charbuf)
{
    char song[128] = "", album[128] = "", artist[128] = "", *art;
    char *token, *art_str;
    char *str_copy = strdup(charbuf);

    token = strsep(&str_copy, "|");
    if (token)
        strncpy(song, token, strlen(token));
    else return;
    token = strsep(&str_copy, "|");
    if (token)
        strncpy(artist, token, strlen(token));
    else return;
    token = strsep(&str_copy, "|");
    if (token)
        strncpy(album, token, strlen(token));
    else return;
    db_update_song_album(song, artist, album);

    token = strsep(&str_copy, "|");
    if (token) {
        art_str = download_file(token, artist);
        db_update_album_art(artist, album, art_str);
    }

    free(str_copy);
}

void db_put_artist(char* charbuf)
{
    char artist[128] = "";
    char *token, *art_str;
    char *str_copy = strdup(charbuf);

    token = strsep(&str_copy, "|");
    if (token)
        strncpy(artist, token, strlen(token));
    else return;
    token = strsep(&str_copy, "|");
    if (token) {
        art_str = download_file(token, artist);
        db_update_artist_art(artist, art_str);
    } else {
        syslog(LOG_INFO, "%s: no art for %s\n", __func__, artist);
    }

    free(str_copy);
}

int callback_mpd(struct mg_connection *c)
{
    enum mpd_cmd_ids cmd_id = get_cmd_id(c->content);
    size_t n = 0;
    unsigned int uint_buf, uint_buf_2;
    int int_buf;
    char *p_charbuf = NULL;

    if(cmd_id == -1)
        return MG_CLIENT_CONTINUE;

    if(mpd.conn_state != MPD_CONNECTED && cmd_id != MPD_API_SET_MPDHOST &&
        cmd_id != MPD_API_GET_MPDHOST && cmd_id != MPD_API_SET_MPDPASS)
        return MG_CLIENT_CONTINUE;

    mpd_connection_set_timeout(mpd.conn, 10000);
    switch(cmd_id)
    {
        case MPD_API_LIST_ARTISTS:
            mpd_list_artists(mpd.conn);
            break;
        case MPD_API_LIKE:
            mpd_db_update_current_song_rating(5);
            break;
        case MPD_API_DISLIKE:
            mpd_db_update_current_song_rating(-5);
            mpd_run_next(mpd.conn);
            break;
        case MPD_API_DB_ALBUM:
            if(sscanf(c->content, "MPD_API_DB_ALBUM,%m[^\t\n]",
                        &p_charbuf) && p_charbuf != NULL)
            {
                db_put_album(p_charbuf);
                free(p_charbuf);
            }
            break;
        case MPD_API_DB_ARTIST:
            if(sscanf(c->content, "MPD_API_DB_ARTIST,%m[^\t\n]",
                        &p_charbuf) && p_charbuf != NULL)
            {
                syslog(LOG_INFO, "%s: MPD_API_DB_ARTIST: %s\n", __func__, p_charbuf);
                db_put_artist(p_charbuf);
                free(p_charbuf);
            }
            break;
        case MPD_API_DB_GET_ARTIST:
            if(sscanf(c->content, "MPD_API_DB_GET_ARTIST,%m[^\t\n]",
                        &p_charbuf) && p_charbuf != NULL)
            {
                syslog(LOG_INFO, "%s: MPD_API_DB_GET_ARTIST: %s\n", __func__, p_charbuf);
                n = mpd_put_artist_art(mpd.buf, p_charbuf);
                mg_websocket_write(c, 1, mpd.buf, n);
                free(p_charbuf);
            }
            break;
        case MPD_API_TOGGLE_RADIO:
            syslog(LOG_INFO, "%s: RADIO_TOGGLE_RADIO %i\n", __func__, radio_get_status());
            radio_toggle();
            break;
        case MPD_API_UPDATE_DB:
            mpd_run_update(mpd.conn, NULL);
            break;
        case MPD_API_SET_PAUSE:
            mpd_run_toggle_pause(mpd.conn);
            break;
        case MPD_API_SET_PREV:
            mpd_run_previous(mpd.conn);
            break;
        case MPD_API_SET_NEXT:
            //decrease rating before
            mpd_db_update_current_song_rating(-1);
            mpd_run_next(mpd.conn);
            break;
        case MPD_API_SET_PLAY:
            mpd_run_play(mpd.conn);
            break;
        case MPD_API_SET_STOP:
            mpd_run_stop(mpd.conn);
            break;
        case MPD_API_RM_ALL:
            //mpd_run_clear(mpd.conn);
            mpd_crop(mpd.conn);
            break;
        case MPD_API_RM_TRACK:
            if(sscanf(c->content, "MPD_API_RM_TRACK,%u", &uint_buf))
                mpd_run_delete_id(mpd.conn, uint_buf);
            break;
        case MPD_API_PLAY_TRACK:
            if(sscanf(c->content, "MPD_API_PLAY_TRACK,%u", &uint_buf)) {
                syslog(LOG_INFO, "%s: PLAY_TRACK id %i\n", __func__, uint_buf);
                mpd_run_play_id(mpd.conn, uint_buf);
            }
            break;
        case MPD_API_TOGGLE_RANDOM:
            if(sscanf(c->content, "MPD_API_TOGGLE_RANDOM,%u", &uint_buf))
                mpd_run_random(mpd.conn, uint_buf);
            break;
        case MPD_API_TOGGLE_REPEAT:
            if(sscanf(c->content, "MPD_API_TOGGLE_REPEAT,%u", &uint_buf))
                mpd_run_repeat(mpd.conn, uint_buf);
            break;
        case MPD_API_TOGGLE_CONSUME:
            if(sscanf(c->content, "MPD_API_TOGGLE_CONSUME,%u", &uint_buf))
                mpd_run_consume(mpd.conn, uint_buf);
            break;
        case MPD_API_TOGGLE_SINGLE:
            if(sscanf(c->content, "MPD_API_TOGGLE_SINGLE,%u", &uint_buf))
                mpd_run_single(mpd.conn, uint_buf);
            break;
        case MPD_API_SET_VOLUME:
            if(sscanf(c->content, "MPD_API_SET_VOLUME,%ud", &uint_buf) && uint_buf <= 100)
                mpd_run_set_volume(mpd.conn, uint_buf);
            break;
        case MPD_API_INCREASE_VOLUME:
            mpd.volume += 5;
            if (mpd.volume > 100)
                mpd.volume = 100;
            mpd_run_set_volume(mpd.conn, mpd.volume);
            break;
        case MPD_API_DECREASE_VOLUME:
            mpd.volume -= 5;
            if (mpd.volume < 0)
                mpd.volume = 0;
            mpd_run_set_volume(mpd.conn, mpd.volume);
            break;
        case MPD_API_SET_SEEK:
            if(sscanf(c->content, "MPD_API_SET_SEEK,%u,%u", &uint_buf, &uint_buf_2))
                mpd_run_seek_id(mpd.conn, uint_buf, uint_buf_2);
            break;
        case MPD_API_GET_QUEUE:
            if(sscanf(c->content, "MPD_API_GET_QUEUE,%u", &uint_buf))
                n = mpd_put_queue(mpd.buf, uint_buf);
            break;
        case MPD_API_GET_RADIO:
            if(sscanf(c->content, "MPD_API_GET_RADIO,%u", &uint_buf)) {
                printf("%s: offset %i\n", __func__, uint_buf);
                n = mpd_put_radio(mpd.buf, uint_buf);
            }
            break;
        case MPD_API_GET_BROWSE:
            if(sscanf(c->content, "MPD_API_GET_BROWSE,%u,%m[^\t\n]", &uint_buf, &p_charbuf) && p_charbuf != NULL)
            {
                n = mpd_put_browse(mpd.buf, p_charbuf, uint_buf);
                free(p_charbuf);
            }
            break;
        case MPD_API_SET_RADIO:
            if(sscanf(c->content, "MPD_API_SET_RADIO,%m[^\t\n]", &p_charbuf) && p_charbuf != NULL)
            {
                syslog(LOG_INFO, "%s set radio: %s\n", __func__, p_charbuf);
                printf("%s set radio: %s\n", __func__, p_charbuf);
/*                stop_streamripper();
                strcpy(rcm.current_radio, p_charbuf);
                streamripper_set_url_dest(rcm.current_radio);
                init_streamripper();*/
                free(p_charbuf);
            }
            break;
        case MPD_API_ADD_TRACK:
            if(sscanf(c->content, "MPD_API_ADD_TRACK,%m[^\t\n]", &p_charbuf) && p_charbuf != NULL)
            {
                mpd_run_add(mpd.conn, p_charbuf);
                free(p_charbuf);
            }
            break;
        case MPD_API_ADD_PLAY_TRACK:
            if(sscanf(c->content, "MPD_API_ADD_PLAY_TRACK,%m[^\t\n]", &p_charbuf) && p_charbuf != NULL)
            {
                int_buf = mpd_run_add_id(mpd.conn, p_charbuf);
                if(int_buf != -1)
                    mpd_run_play_id(mpd.conn, int_buf);
                free(p_charbuf);
            }
            break;
        case MPD_API_ADD_PLAYLIST:
            if(sscanf(c->content, "MPD_API_ADD_PLAYLIST,%m[^\t\n]", &p_charbuf) && p_charbuf != NULL)
            {
                mpd_run_load(mpd.conn, p_charbuf);
                free(p_charbuf);
            }
            break;
        case MPD_API_SEARCH:
            if(sscanf(c->content, "MPD_API_SEARCH,%m[^\t\n]", &p_charbuf) && p_charbuf != NULL)
            {
                n = mpd_search(mpd.buf, p_charbuf);
                free(p_charbuf);
            }
            break;
        case TEST_DELETE_FILE:
            delete_file_forever(NULL);
            break;
#ifdef WITH_MPD_HOST_CHANGE
        /* Commands allowed when disconnected from MPD server */
        case MPD_API_SET_MPDHOST:
            int_buf = 0;
            if(sscanf(c->content, "MPD_API_SET_MPDHOST,%d,%m[^\t\n ]", &int_buf, &p_charbuf) &&
                p_charbuf != NULL && int_buf > 0)
            {
                strncpy(mpd.host, p_charbuf, sizeof(mpd.host));
                free(p_charbuf);
                mpd.port = int_buf;
                mpd.conn_state = MPD_RECONNECT;
                return MG_CLIENT_CONTINUE;
            }
            break;
        case MPD_API_GET_MPDHOST:
            n = snprintf(mpd.buf, MAX_SIZE, "{\"type\":\"mpdhost\", \"data\": "
                "{\"host\" : \"%s\", \"port\": \"%d\", \"passwort_set\": %s}"
                "}", mpd.host, mpd.port, mpd.password ? "true" : "false");
            break;
        case MPD_API_SET_MPDPASS:
            if(sscanf(c->content, "MPD_API_SET_MPDPASS,%m[^\t\n ]", &p_charbuf))
            {
                if(mpd.password)
                    free(mpd.password);

                mpd.password = p_charbuf;
                mpd.conn_state = MPD_RECONNECT;
                return MG_CLIENT_CONTINUE;
            }
            break;
#endif
    }

    if(mpd.conn_state == MPD_CONNECTED && mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS)
    {
        n = snprintf(mpd.buf, MAX_SIZE, "{\"type\":\"error\", \"data\": \"%s\"}",
            mpd_connection_get_error_message(mpd.conn));

        /* Try to recover error */
        if (!mpd_connection_clear_error(mpd.conn))
            mpd.conn_state = MPD_FAILURE;
    }

    if(n > 0)
        mg_websocket_write(c, 1, mpd.buf, n);

    return MG_CLIENT_CONTINUE;
}

int mpd_close_handler(struct mg_connection *c)
{
    /* Cleanup session data */
    if(c->connection_param)
        free(c->connection_param);
    return 0;
}

static int mpd_notify_callback(struct mg_connection *c) {
    size_t n;

    if(!c->is_websocket)
        return MG_REQUEST_PROCESSED;

    if(c->callback_param)
    {
        /* error message? */
        n = snprintf(mpd.buf, MAX_SIZE, "{\"type\":\"error\",\"data\":\"%s\"}",
            (const char *)c->callback_param);

        mg_websocket_write(c, 1, mpd.buf, n);
        return MG_REQUEST_PROCESSED;
    }

    if(!c->connection_param)
        c->connection_param = calloc(1, sizeof(struct t_mpd_client_session));

    struct t_mpd_client_session *s = (struct t_mpd_client_session *)c->connection_param;

    if(mpd.conn_state != MPD_CONNECTED) {
        n = snprintf(mpd.buf, MAX_SIZE, "{\"type\":\"disconnected\"}");
        mg_websocket_write(c, 1, mpd.buf, n);
    }
    else
    {
        mg_websocket_write(c, 1, mpd.buf, mpd.buf_size);

        if (mpd.song_id == -1)
        {
            char str[128] = "";
            syslog(LOG_INFO, "%s song_id == %i\n", __func__, mpd.song_id);
            get_random_song(mpd.conn, str, rcm.file_path);
            if (strcmp(str, "") == 0)
               get_random_song(mpd.conn, str, "radio");
            syslog(LOG_INFO, "%s: add random song %s and play\n", __func__, str);
            mpd_run_add(mpd.conn, str);
            mpd_run_play(mpd.conn);

            return MG_REQUEST_PROCESSED;
        }

        if(s->song_id != mpd.song_id)
        {
            syslog(LOG_INFO, "%s ssong = %i mpdsong = %i\n", __func__, s->song_id, mpd.song_id);
            n = mpd_put_current_song(mpd.buf);
            mg_websocket_write(c, 1, mpd.buf, n);
            s->song_id = mpd.song_id;

            n = mpd_get_track_info(mpd.buf);
            mg_websocket_write(c, 1, mpd.buf, n);
        } /*else {
            n = mpd_put_next_song(mpd.buf, mpd.song_id);
            mg_websocket_write(c, 1, mpd.buf, n);
        }*/

        if(s->queue_version != mpd.queue_version)
        {
            n = snprintf(mpd.buf, MAX_SIZE, "{\"type\":\"update_queue\"}");
            mg_websocket_write(c, 1, mpd.buf, n);
            s->queue_version = mpd.queue_version;
        }

        //TODO: support different sources;
        if (((unsigned int)time(NULL) - rcm.last_timer) > 10) {
            n = mpd_put_current_radio(mpd.buf);
            if (n > 0)
                mg_websocket_write(c, 1, mpd.buf, n);
            rcm.last_timer = (unsigned int)time(NULL);
//            syslog(LOG_INFO, "%s %i\n", __func__, rcm.last_timer);
        }
    }

    return MG_REQUEST_PROCESSED;
}

void mpd_poll(struct mg_server *s)
{
    char str[256] = "";
    switch (mpd.conn_state) {
        case MPD_DISCONNECTED:
            /* Try to connect */
            syslog(LOG_INFO, "MPD Connecting to %s:%d\n", mpd.host, mpd.port);
            mpd.conn = mpd_connection_new(mpd.host, mpd.port, 3000);
            if (mpd.conn == NULL) {
                syslog(LOG_ERR, "Out of memory.");
                mpd.conn_state = MPD_FAILURE;
                return;
            }

            if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS) {
                syslog(LOG_ERR, "MPD connection: %s\n", mpd_connection_get_error_message(mpd.conn));
                mg_iterate_over_connections(s, mpd_notify_callback,
                    (void *)mpd_connection_get_error_message(mpd.conn));
                mpd.conn_state = MPD_FAILURE;
                return;
            }

            if(mpd.password && !mpd_run_password(mpd.conn, mpd.password))
            {
                syslog(LOG_ERR, "MPD connection: %s\n", mpd_connection_get_error_message(mpd.conn));
                mg_iterate_over_connections(s, mpd_notify_callback,
                    (void *)mpd_connection_get_error_message(mpd.conn));
                mpd.conn_state = MPD_FAILURE;
                return;
            }

            syslog(LOG_ERR, "MPD connected.\n");
            mpd.conn_state = MPD_CONNECTED;
            break;

        case MPD_FAILURE:
            syslog(LOG_ERR, "MPD connection failed.\n");

        case MPD_DISCONNECT:
        case MPD_RECONNECT:
            if(mpd.conn != NULL)
                mpd_connection_free(mpd.conn);
            mpd.conn = NULL;
            mpd.conn_state = MPD_DISCONNECTED;
            break;

        case MPD_CONNECTED:
            mpd.buf_size = mpd_put_state(mpd.buf, &mpd.song_id, &mpd.queue_version);
            mg_iterate_over_connections(s, mpd_notify_callback, NULL);
            if (queue_is_empty) {
                get_random_song(mpd.conn, str, rcm.file_path);
                if (strcmp(str, "") == 0)
                    get_random_song(mpd.conn, str, "radio");
                syslog(LOG_DEBUG, "%s: add random song %s\n", __func__, str);
                mpd_run_add(mpd.conn, str);
                queue_is_empty = 0;
            }
            break;
        default:
            syslog(LOG_ERR, "mpd.conn_state %i\n", mpd.conn_state);
    }
}

char* mpd_get_album(struct mpd_song const *song)
{
    char *str;

    str = (char *)mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
    if(str == NULL){
/*        syslog(LOG_DEBUG, "%s: song %s; artist %s\n", __func__,
                mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
                mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));*/
        str = db_get_song_album(mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
                mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));
    }
    /*if (str)
        syslog(LOG_DEBUG, "%s: %s\n", __func__, str);
*/
    return str;
}

int image_exists(char* name)
{
    struct passwd *pw = getpwuid(getuid());
    char *images_dir, *homedir = pw->pw_dir;
    if (!config_lookup_string(&rcm.cfg, "application.images_path", &images_dir))
    {
        syslog(LOG_ERR, "%s: No 'application.images_path' setting in configuration file.\n", __func__);
        sprintf(outdir, "%s/%s/images/", homedir, RCM_DIR_STR);
    } else {
        sprintf(outdir, "%s", images_dir);
    }
    sprintf(outfn, "%s/%s", outdir, name);
    syslog(LOG_INFO, "%s looking for %s ", __func__, outfn);
    if (access(outfn, F_OK) != -1) {
        syslog(LOG_INFO, "%s: ok", __func__);
        return 1;
    }
    else {
        syslog(LOG_INFO, "%s: no", __func__);
        return 0;
    }
}

char* mpd_get_art(struct mpd_song const *song)
{
    char *str = db_get_album_art(mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
            db_get_song_album(mpd_song_get_tag(song, MPD_TAG_TITLE, 0),
                mpd_song_get_tag(song, MPD_TAG_ARTIST, 0)));
    if (str) {
        syslog(LOG_DEBUG, "%s: %s\n", __func__, str);
        //check if file exists
        if (!image_exists(str))
            return NULL;
    }
    return str;
}

char* mpd_get_artist_art(char* artist)
{
    char *str = db_get_artist_art(artist);
    if (str)
        syslog(LOG_INFO, "%s: %s\n", __func__, str);
    if (!image_exists(str))
        return NULL;
    return str;
}

int mpd_put_state(char *buffer, int *current_song_id, unsigned *queue_version)
{
    struct mpd_status *status;
    int len;
    unsigned queue_len;
    int song_pos, next_song_pos;

    status = mpd_run_status(mpd.conn);
    if (!status) {
        syslog(LOG_ERR, "MPD mpd_run_status: %s\n", mpd_connection_get_error_message(mpd.conn));
        mpd.conn_state = MPD_FAILURE;
        return 0;
    }

    song_pos = mpd_status_get_song_pos(status);
    next_song_pos = song_pos+1; //TODO: mpd_status_get_next_song_pos(status);
    queue_len = mpd_status_get_queue_length(status);
    mpd.volume = mpd_status_get_volume(status);
    len = snprintf(buffer, MAX_SIZE,
        "{\"type\":\"state\", \"data\":{"
        " \"state\":%d, \"volume\":%d, \"repeat\":%d,"
        " \"single\":%d, \"consume\":%d, \"random\":%d, "
        " \"songpos\":%d, \"nextsongpos\":%d, "
        " \"elapsedTime\":%d, \"totalTime\":%d, "
        " \"currentsongid\":%d, \"radio_status\":%d, \"queue_len\":%d, "
        " \"songrating\":%d"
        "}}",
        mpd_status_get_state(status),
        mpd.volume,
        mpd_status_get_repeat(status),
        mpd_status_get_single(status),
        mpd_status_get_consume(status),
        mpd_status_get_random(status),
        song_pos, next_song_pos,
        mpd_status_get_elapsed_time(status),
        mpd_status_get_total_time(status),
        mpd_status_get_song_id(status),
        radio_get_status(),
        queue_len,
        get_current_song_rating());

    *current_song_id = mpd_status_get_song_id(status);
    *queue_version = mpd_status_get_queue_version(status);

    if (song_pos+1 >= queue_len)
    {
        queue_is_empty = 1;
        syslog(LOG_DEBUG, "%s: queue is empty\n", __func__);
    }

    mpd.song_pos = song_pos;
    mpd_status_free(status);
    return len;
}

int mpd_put_artist_art(char *buffer, char *artist)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;

    if (artist == NULL)
        return;
    cur += json_emit_raw_str(cur, end - cur, "{\"type\": \"artist_info\", \"data\":{\"artist\":");
    cur += json_emit_quoted_str(cur, end - cur, artist);
    if (mpd_get_artist_art(artist)) {
        cur += json_emit_raw_str(cur, end - cur, ",\"art\":");
        cur += json_emit_quoted_str(cur, end - cur, mpd_get_artist_art(artist));
    }
    cur += json_emit_raw_str(cur, end - cur, "}}");

    mpd_response_finish(mpd.conn);

    return cur - buffer;
}

int mpd_db_update_current_song_rating(int increase)
{
    int rating = 0;
    struct mpd_song *song;

    song = mpd_run_current_song(mpd.conn);
    if(song == NULL)
        return 0;

    rating = db_update_song_rating(mpd_get_title(song),
            mpd_song_get_tag(song, MPD_TAG_ARTIST, 0), increase);

    mpd_song_free(song);

    return rating;
}

int mpd_put_current_song(char *buffer)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    struct mpd_song *song;

    song = mpd_run_current_song(mpd.conn);
    if(song == NULL)
        return 0;

    cur += json_emit_raw_str(cur, end - cur, "{\"type\": \"song_change\", \"data\":{\"pos\":");
    cur += json_emit_int(cur, end - cur, mpd_song_get_pos(song));
    cur += json_emit_raw_str(cur, end - cur, ",\"title\":");
    cur += json_emit_quoted_str(cur, end - cur, mpd_get_title(song));

    if(mpd_song_get_tag(song, MPD_TAG_ARTIST, 0) != NULL)
    {
        cur += json_emit_raw_str(cur, end - cur, ",\"artist\":");
        cur += json_emit_quoted_str(cur, end - cur, mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));
    }

    if(mpd_get_album(song) != NULL)
    {
        cur += json_emit_raw_str(cur, end - cur, ",\"album\":");
        cur += json_emit_quoted_str(cur, end - cur, mpd_get_album(song));
    }

    if (mpd_get_art(song) != NULL)
    {
        syslog(LOG_INFO, "%s ART\n", __func__);
        cur += json_emit_raw_str(cur, end - cur, ",\"art\":");
        cur += json_emit_quoted_str(cur, end - cur, mpd_get_art(song));
    }

    cur += json_emit_raw_str(cur, end - cur, "}}");

    db_listen_song(mpd_get_title(song),
            mpd_song_get_tag(song, MPD_TAG_ARTIST, 0),
            mpd_get_album(song));

    mpd_song_free(song);
    mpd_response_finish(mpd.conn);

    return cur - buffer;
}

int mpd_get_track_info(char *buffer)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    struct mpd_song *song;

    song = mpd_run_current_song(mpd.conn);
    if(song == NULL)
        return 0;
    //TODO: find better criteria to get track info
    syslog(LOG_DEBUG, "get_track_info\n");
    if(mpd_get_album(song) != NULL)
        return 0;
    syslog(LOG_DEBUG, "do get_track_info\n");
    cur += json_emit_raw_str(cur, end - cur, "{\"type\": \"track_info\", \"data\":{\"pos\":");
    cur += json_emit_int(cur, end - cur, mpd_song_get_pos(song));
    cur += json_emit_raw_str(cur, end - cur, ",\"title\":");
    cur += json_emit_quoted_str(cur, end - cur, mpd_get_title(song));

    if(mpd_song_get_tag(song, MPD_TAG_ARTIST, 0) != NULL)
    {
        cur += json_emit_raw_str(cur, end - cur, ",\"artist\":");
        cur += json_emit_quoted_str(cur, end - cur, mpd_song_get_tag(song, MPD_TAG_ARTIST, 0));
    }
    cur += json_emit_raw_str(cur, end - cur, "}}");
    mpd_song_free(song);
    mpd_response_finish(mpd.conn);

    return cur - buffer;
}

int mpd_put_current_radio(char *buffer)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    config_setting_t *setting;

    cur += json_emit_raw_str(cur, end - cur, "{\"type\": \"current_radio\", \"data\":{\"name\":");
    cur += json_emit_quoted_str(cur, end - cur, rcm.current_radio);
    cur += json_emit_raw_str(cur, end - cur, ",\"status\":");
    cur += json_emit_quoted_str(cur, end - cur, rcm.status_str);
    cur += json_emit_raw_str(cur, end - cur, ",\"size\":");
    cur += json_emit_quoted_str(cur, end - cur, rcm.filesize_str);
    if (rcm.image_update) {
        setting = config_lookup(&rcm.cfg, "radio.station");
        if (setting != NULL) {
            int count = config_setting_length(setting);
            int i;
            for (i = 0; i < count; i++) {
                config_setting_t *station = config_setting_get_elem(setting, i);
                const char *name, *logo;
                if (!config_setting_lookup_string(station, "name", &name))
                    continue;
                if (strcmp(name, rcm.current_radio) == 0) {
                    if (config_setting_lookup_string(station, "logo", &logo)) {
                        cur += json_emit_raw_str(cur, end - cur, ",\"logo\":");
                        cur += json_emit_quoted_str(cur, end - cur, logo);
                    }
                    break;
                }
            }
        }
        rcm.image_update = 0;
    }

    cur += json_emit_raw_str(cur, end - cur, "}}");
    return cur - buffer;
}

int mpd_put_radio(char *buffer, unsigned int offset)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    config_setting_t *setting;
    int numelements = 0;

    setting = config_lookup(&rcm.cfg, "radio.station");
    if(setting != NULL)
    {
        int count = config_setting_length(setting);
        int i;

        cur += json_emit_raw_str(cur, end  - cur, "{\"type\":\"radio\",\"data\":[ ");

        syslog(LOG_INFO, "%s: %i %i\n", __func__, offset, count);

        for(i = 0; i < count; ++i)
        {
            config_setting_t *station = config_setting_get_elem(setting, i);

            /* Only output the record if all of the expected fields are present. */
            const char *name, *url;

            if(!(config_setting_lookup_string(station, "name", &name)
                && config_setting_lookup_string(station, "url", &url)))
                    continue;
			numelements++;
			if (numelements > offset) {
                syslog(LOG_INFO, "%s: %i %-30s  %-30s\n", __func__, numelements, name, url);
                cur += json_emit_raw_str(cur, end - cur, "{\"name\":");
                cur += json_emit_quoted_str(cur, end - cur, name);
                cur += json_emit_raw_str(cur, end - cur, ",\"url\":");
                cur += json_emit_quoted_str(cur, end - cur, url);
                cur += json_emit_raw_str(cur, end - cur, "},");
            }
            if (numelements > offset+MAX_ELEMENTS_PER_PAGE)
                break;
        }

        putchar('\n');

        /* remove last ',' */
        cur--;
        cur += json_emit_raw_str(cur, end - cur, "]}");
    }

    return cur - buffer;
}

int mpd_put_queue(char *buffer, unsigned int offset)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    struct mpd_entity *entity;

    offset = mpd.song_pos; //TODO: +=
    syslog(LOG_INFO, "%s %i\n", __func__, offset);
    if (!mpd_send_list_queue_range_meta(mpd.conn, offset, offset+MAX_ELEMENTS_PER_PAGE))
        RETURN_ERROR_AND_RECOVER("mpd_send_list_queue_meta");

    cur += json_emit_raw_str(cur, end  - cur, "{\"type\":\"queue\",\"data\":[ ");

    while((entity = mpd_recv_entity(mpd.conn)) != NULL) {
        const struct mpd_song *song;

        if(mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG) {
            song = mpd_entity_get_song(entity);

            cur += json_emit_raw_str(cur, end - cur, "{\"id\":");
            cur += json_emit_int(cur, end - cur, mpd_song_get_id(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"pos\":");
            cur += json_emit_int(cur, end - cur, mpd_song_get_pos(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"duration\":");
            cur += json_emit_int(cur, end - cur, mpd_song_get_duration(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"title\":");
            cur += json_emit_quoted_str(cur, end - cur, mpd_get_title(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"artist\":");
            cur += json_emit_quoted_str(cur, end - cur, mpd_get_artist(song));
//            cur += json_emit_raw_str(cur, end - cur, ",\"album\":");
//            cur += json_emit_quoted_str(cur, end - cur, mpd_get_album(song)?mpd_get_album(song):"");
            cur += json_emit_raw_str(cur, end - cur, "},");
        }
        mpd_entity_free(entity);
    }

    /* remove last ',' */
    cur--;

    cur += json_emit_raw_str(cur, end - cur, "]}");
    return cur - buffer;
}

int mpd_put_browse(char *buffer, char *path, unsigned int offset)
{
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    struct mpd_entity *entity;
    unsigned int entity_count = 0;

    if (!mpd_send_list_meta(mpd.conn, path))
        RETURN_ERROR_AND_RECOVER("mpd_send_list_meta");

    cur += json_emit_raw_str(cur, end  - cur, "{\"type\":\"browse\",\"data\":[ ");

    while((entity = mpd_recv_entity(mpd.conn)) != NULL) {
        const struct mpd_song *song;
        const struct mpd_directory *dir;
        const struct mpd_playlist *pl;

        if(offset > entity_count)
        {
            mpd_entity_free(entity);
            entity_count++;
            continue;
        }
        else if(offset + MAX_ELEMENTS_PER_PAGE - 1 < entity_count)
        {
            mpd_entity_free(entity);
            cur += json_emit_raw_str(cur, end  - cur, "{\"type\":\"wrap\",\"count\":");
            cur += json_emit_int(cur, end - cur, entity_count);
            cur += json_emit_raw_str(cur, end  - cur, "} ");
            break;
        }

        switch (mpd_entity_get_type(entity)) {
            case MPD_ENTITY_TYPE_UNKNOWN:
                break;

            case MPD_ENTITY_TYPE_SONG:
                song = mpd_entity_get_song(entity);
                cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"song\",\"uri\":");
                cur += json_emit_quoted_str(cur, end - cur, mpd_song_get_uri(song));
                cur += json_emit_raw_str(cur, end - cur, ",\"duration\":");
                cur += json_emit_int(cur, end - cur, mpd_song_get_duration(song));
                cur += json_emit_raw_str(cur, end - cur, ",\"title\":");
                cur += json_emit_quoted_str(cur, end - cur, mpd_get_title(song));
/*                cur += json_emit_raw_str(cur, end - cur, ",\"artist\":");
                cur += json_emit_quoted_str(cur, end - cur, mpd_get_artist(song));*/
                cur += json_emit_raw_str(cur, end - cur, "},");
                break;

            case MPD_ENTITY_TYPE_DIRECTORY:
                dir = mpd_entity_get_directory(entity);

                cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"directory\",\"dir\":");
                cur += json_emit_quoted_str(cur, end - cur, mpd_directory_get_path(dir));
                cur += json_emit_raw_str(cur, end - cur, "},");
                break;

            case MPD_ENTITY_TYPE_PLAYLIST:
                pl = mpd_entity_get_playlist(entity);
                cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"playlist\",\"plist\":");
                cur += json_emit_quoted_str(cur, end - cur, mpd_playlist_get_path(pl));
                cur += json_emit_raw_str(cur, end - cur, "},");
                break;
        }
        mpd_entity_free(entity);
        entity_count++;
    }

    if (mpd_connection_get_error(mpd.conn) != MPD_ERROR_SUCCESS || !mpd_response_finish(mpd.conn)) {
        syslog(LOG_ERR, "MPD mpd_send_list_meta: %s\n", mpd_connection_get_error_message(mpd.conn));
        mpd.conn_state = MPD_FAILURE;
        return 0;
    }

    /* remove last ',' */
    cur--;

    cur += json_emit_raw_str(cur, end - cur, "]}");
    return cur - buffer;
}

int mpd_random_song()
{
    struct mpd_stats *stats;
    int rnd, num_songs;

    stats = mpd_run_stats(mpd.conn);

    num_songs = mpd_stats_get_number_of_songs(stats);
    rnd = (int)(rand() % num_songs);
    syslog(LOG_DEBUG, "num_songs = %i; add %i\n", num_songs, rnd);

    mpd_stats_free(stats);

    return rnd;
}

int mpd_search(char *buffer, char *searchstr)
{
    int i = 0;
    char *cur = buffer;
    const char *end = buffer + MAX_SIZE;
    struct mpd_song *song;

    if(mpd_search_db_songs(mpd.conn, false) == false)
        RETURN_ERROR_AND_RECOVER("mpd_search_db_songs");
    else if(mpd_search_add_any_tag_constraint(mpd.conn, MPD_OPERATOR_DEFAULT, searchstr) == false)
        RETURN_ERROR_AND_RECOVER("mpd_search_add_any_tag_constraint");
    else if(mpd_search_commit(mpd.conn) == false)
        RETURN_ERROR_AND_RECOVER("mpd_search_commit");
    else {
        cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"search\",\"data\":[ ");

        while((song = mpd_recv_song(mpd.conn)) != NULL) {
            cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"song\",\"uri\":");
            cur += json_emit_quoted_str(cur, end - cur, mpd_song_get_uri(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"duration\":");
            cur += json_emit_int(cur, end - cur, mpd_song_get_duration(song));
            cur += json_emit_raw_str(cur, end - cur, ",\"title\":");
            cur += json_emit_quoted_str(cur, end - cur, mpd_get_title(song));
            cur += json_emit_raw_str(cur, end - cur, "},");
            syslog(LOG_INFO, "%s: %s\n", __func__, mpd_get_title(song));
            mpd_song_free(song);
            /* Maximum results */
            if(i++ >= 300)
            {
                cur += json_emit_raw_str(cur, end - cur, "{\"type\":\"wrap\"},");
                break;
            }
        }

        /* remove last ',' */
        cur--;

        cur += json_emit_raw_str(cur, end - cur, "]}");
    }
    return cur - buffer;
}


void mpd_disconnect()
{
    mpd.conn_state = MPD_DISCONNECT;
    mpd_poll(NULL);
}
