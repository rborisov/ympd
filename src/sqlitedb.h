#ifndef __SQLITE_DB_H__
#define __SQLITE_DB_H__

int db_init();
void db_close();
int db_listen_song(char*, char*, char*);
int db_get_song_numplayed(char*, char*);
int db_get_album_id(char*, char*);
char* db_get_song_album(char*, char*);
char* db_get_album_art(char*, char*);
char* db_get_artist_art(char*);
int db_update_song_album(char*, char*, char*);
int db_update_album_art(char*, char*, char*);
int db_update_artist_art(char*, char*);
void db_result_free(char*);
int db_update_song_rating(char*, char*, int);
int db_get_song_rating(char*, char*);

#endif
