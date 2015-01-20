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
