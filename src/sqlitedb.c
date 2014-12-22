#include <string.h>
#include <sqlite3.h>
#include "sqlitedb.h"
//#include "ydebug.h"
#include "sql.h"

#define ydebug_printf printf

sqlite3 *conn;
char *sqlchar0, *sqlchar1;
//char rsong[128], ralbum[128], rartist[128];

void convert_str(char *instr)
{
    char *out = instr;

    if (!instr)
        return;

    while (*out) {
        if (*out == '\'')
            *out = '\"';
        out++;
    }
    *out = '\0';
}

int db_init()
{
    int rc;

    char create_Songs_table_query[] = "CREATE TABLE IF NOT EXISTS Songs \
                                (id INTEGER PRIMARY KEY, song TEXT, \
                                 artist TEXT, album TEXT, style TEXT, \
                                 numplayed INTEGER, added DATETIME, \
                                 played DATETIME, rating INTEGER);";
    char create_Albums_table_query[] = "CREATE TABLE IF NOT EXISTS Albums \
                                        (id INTEGER PRIMARY KEY, album TEXT, \
                                         artist TEXT, artpath TEXT)";
    rc = sqlite3_open("rcardb.sqlite", &conn);
    if (rc != SQLITE_OK) 
        goto error;
    rc = sqlite3_exec(conn, create_Songs_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_Albums_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    
    return rc;
error:
    ydebug_printf("%s, %s\n", __func__, sqlite3_errmsg(conn));
    sqlite3_close(conn);
    return rc;
}

/*
 * get numplayed from Songs or 0 when error
 */
int db_get_song_numplayed(char* song, char* artist)
{
    convert_str(song);
    convert_str(artist);
    int np = sql_get_int_field(conn, "SELECT numplayed FROM Songs WHERE "
            "song = '%s' AND artist = '%s'", song, artist);
    ydebug_printf("%s found numplayed %i\n", __func__, np);
    return (np>0) ? np : 0;
}

int db_get_album_id(char* artist, char* album)
{
        convert_str(album);
            convert_str(artist);

    int id = sql_get_int_field(conn, "SELECT id FROM Albums WHERE "
            "album = '%s' AND artist = '%s'", album, artist);
    return (id>0) ? id : 0;
}

/*
 * return: album or NULL
 * note: sqlite3_free(result) is required
 */
char* db_get_song_album(char* song, char* artist)
{
    convert_str(song);
    convert_str(artist);

    sqlite3_free(sqlchar0);
    sqlchar0 = sql_get_text_field(conn, "SELECT album FROM Songs WHERE "
            "song = '%s' AND artist = '%s'", song, artist);
    return sqlchar0;
}

char* db_get_album_art(char* artist, char* album)
{
    convert_str(album);
    convert_str(artist);
    sqlite3_free(sqlchar1);
    sqlchar1 = sql_get_text_field(conn, "SELECT artpath FROM Albums WHERE "
            "album = '%s' AND artist = '%s'", album, artist);
    return sqlchar1;
}

void db_result_free(char* buf)
{
    sqlite3_free(buf);
}

/*
 * update album in Songs table
 * return: 1-success; 0-failure;
 */
int db_update_song_album(char* song, char* artist, char* album)
{
    int rc = 0;
    if (album)
    {
        convert_str(artist);
        convert_str(song);
        convert_str(album);
        ydebug_printf("%s update %s album...\n", __func__, album);
        if (sql_exec(conn, "UPDATE Songs SET album = '%s' WHERE song = '%s' AND artist = '%s'", 
                    album, song, artist) == SQLITE_OK)
            rc = 1;
    }
    return rc;
}

int db_update_album_art(char* artist, char* album, char* art)
{
    int id, rc;
    if (!artist||!album||!art)
        return 0;
//    convert_str(album);
//    convert_str(artist);
    printf("%s\n", art);
    id = db_get_album_id(artist, album);
    if (id)
    {
        rc = sql_exec(conn, "UPDATE Albums SET artpath = '%s' "
                "WHERE id = '%i'", art, id);
    } else {
        rc = sql_exec(conn, "INSERT INTO Albums (album, artist, artpath)"
                " VALUES ('%s', '%s', '%s')", album, artist, art);
    }
    return (rc == SQLITE_OK) ? 1 : 0;
}

/*
 * add song and/or update album and/or increase numplayed
 * return new numplayed or 0 when error
 */
int db_listen_song(char* song, char* artist, char* album)
{
    int rc, np;
 
    if (!song||!artist)
        return 0;
//    convert_str(song);
    convert_str(artist);
//    convert_str(album);
    np = db_get_song_numplayed(song, artist);   
    if (np) 
    {
        np = np + 1;
        ydebug_printf("%s found %i updating...\n", __func__, np);
        rc = sql_exec(conn, "UPDATE Songs SET numplayed = '%i' "
                "WHERE song = '%s' AND artist = '%s'", np, song, artist);
        if (rc == SQLITE_OK)
            rc = sql_exec(conn, "UPDATE Songs SET played = DATETIME('NOW', 'LOCALTIME') "
                    "WHERE song = '%s' AND artist = '%s'", song, artist);
    } else {
        ydebug_printf("%s %s %s doesn't exist. adding...\n", __func__, song, artist);
        rc = sql_exec(conn, "INSERT INTO Songs (song, artist, numplayed, added, played, rating)"
                " VALUES ('%s', '%s', DATETIME('NOW', 'LOCALTIME'), DATETIME('NOW', 'LOCALTIME'), 1, 0)", 
                song, artist);
    }
    if (rc != SQLITE_OK)
        return 0;

    db_update_song_album(song, artist, album);
/*    if (album)
    {
        ydebug_printf("%s update %s album...\n", __func__, album);
        sql_exec(conn, "UPDATE Songs SET album = '%s' WHERE song = '%s' AND artist = '%s'", album, song, artist);
    }*/

    return np;
}

void db_close()
{
    sqlite3_free(sqlchar0);
    sqlite3_free(sqlchar1);
    sqlite3_close(conn);
}
