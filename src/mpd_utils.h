#ifndef __MPD_UTILS_H__
#define __MPD_UTILS_H__

#include <mpd/client.h>

unsigned mpd_get_queue_length(struct mpd_connection *);
int mpd_insert (struct mpd_connection *, char * );
void get_random_song(struct mpd_connection *, char *, char *);
void delete_file_forever(char* uri);

#endif
