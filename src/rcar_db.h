#ifndef __RCAR_DB_H__
#define __RCAR_DB_H__

//#include <mysql.h>

int db_init();
void db_close();
int db_listen_song(char* name, char* artist, char* album);
int db_get_song_listened(char* name, char* artist);

#endif //__RCAR_DB_H__
