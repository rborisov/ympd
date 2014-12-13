#ifndef __RCAR_DB_H__
#define __RCAR_DB_H__

//#include <mysql.h>

int db_init();
void db_close();
int db_listen_song(char* name, char* artist, char* album);
int db_get_song_listened(char* name, char* artist);
char* db_get_album(char* name, char* artist);
int db_put_album(char* name, char* artist, char*album);
int db_put_album1(char* pcharbuf);
int db_put_album_art1(char* pcharbuf);

#endif //__RCAR_DB_H__
