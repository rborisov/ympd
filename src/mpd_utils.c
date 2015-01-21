#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mpd_utils.h"

unsigned mpd_get_queue_length(struct mpd_connection *conn)
{
    struct mpd_status *status = mpd_run_status(conn);
    if (status == NULL)
        return 0;
    const unsigned length = mpd_status_get_queue_length(status);
    mpd_status_free(status);
    return length;
}

int mpd_insert ( struct mpd_connection *conn, char *song_path )
{
    struct mpd_status *status = mpd_run_status(conn);
    if (status == NULL)
        return 0;
    const unsigned from = mpd_status_get_queue_length(status);
    const int cur_pos = mpd_status_get_song_pos(status);
    mpd_status_free(status);

    if (mpd_run_add(conn, song_path) != true)
        return 0;

    /* check the new queue length to find out how many songs were
     *        appended  */
    const unsigned end = mpd_get_queue_length(conn);
    if (end == from)
        return 0;

    /* move those songs to right after the current one */
    return mpd_run_move_range(conn, from, end, cur_pos + 1);
}

void get_random_song(struct mpd_connection *conn, char *str, char *path)
{
    struct mpd_entity *entity;
    int listened0 = 65000;

    if (!mpd_send_list_meta(conn, path))
    {
        printf("error: mpd_send_list_meta\n");
        return;
    }

    srand((unsigned)time(0));

    while((entity = mpd_recv_entity(conn)) != NULL)
    {
        const struct mpd_song *song;
        if (mpd_entity_get_type(entity) == MPD_ENTITY_TYPE_SONG)
        {
            int listened;
            song = mpd_entity_get_song(entity);
            listened = db_get_song_numplayed(mpd_get_title(song),
                    mpd_get_artist(song));
            //printf("%i", listened);
            if (listened < listened0) {
                int probability = 50 + db_get_song_rating(mpd_get_title(song),
                        mpd_get_artist(song));
                bool Yes = (rand() % 100) < probability;
                if (Yes) {
                    sprintf(str, "%s", mpd_song_get_uri(song));
                    listened0 = listened;
                    printf("%s: li: %i pr: %i %s %s\n", __func__, listened, probability, 
                            mpd_get_title(song), mpd_get_artist(song));
                }
            }
        }
        mpd_entity_free(entity);
    }
    //TODO: look in / path if not found
}

