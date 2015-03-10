#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include "sqlitedb.h"
#include "sql.h"
#include "radio.h"

sqlite3 *conn;
char *sqlchar0, *sqlchar1, *sqlchar2;

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
    char dbpath[128];
    sprintf(dbpath, "%srcardb.sqlite", cfg_get_db_path());

    char create_Songs_table_query[] = "CREATE TABLE IF NOT EXISTS Songs \
                                (id INTEGER PRIMARY KEY, song TEXT, \
                                 artist TEXT, album TEXT, style TEXT, \
                                 numplayed INTEGER, added DATETIME, \
                                 played DATETIME, rating INTEGER);";
    char create_Albums_table_query[] = "CREATE TABLE IF NOT EXISTS Albums \
                                        (id INTEGER PRIMARY KEY, album TEXT, \
                                         artist TEXT, artpath TEXT)";
    char create_Artists_table_query[] = "CREATE TABLE IF NOT EXISTS Artists \
                                         (id INTEGER PRIMARY KEY, artist TEXT, \
                                          artpath TEXT)";
    char create_Radio_table_query[] = "CREATE TABLE IF NOT EXISTS Radios \
                                       (id INTEGER PRIMARY KEY, name TEXT, \
                                        time DATETIME, rating INTEGER, style TEXT, url TEXT)";
    char create_RadioUrl_table_query[] = "CREATE TABLE IF NOT EXISTS RadioUrls \
                                       (id INTEGER PRIMARY KEY, radioid INTEGER, url TEXT, \
                                        time DATETIME, bitrate INTEGER)";
    rc = sqlite3_open(dbpath, &conn);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_Songs_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_Albums_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_Artists_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_Radio_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    rc = sqlite3_exec(conn, create_RadioUrl_table_query, 0, 0, 0);
    if (rc != SQLITE_OK)
        goto error;
    return rc;
error:
    syslog(LOG_DEBUG, "%s, %s\n", __func__, sqlite3_errmsg(conn));
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
    //printf("%s found numplayed %i\n", __func__, np);
    return np;
}

int db_get_album_id(char* artist, char* album)
{
    convert_str(album);
    convert_str(artist);

    int id = sql_get_int_field(conn, "SELECT id FROM Albums WHERE "
            "album = '%s' AND artist = '%s'", album, artist);
    return (id>0) ? id : 0;
}

int db_get_artist_id(char* artist)
{
    convert_str(artist);

    int id = sql_get_int_field(conn, "SELECT id FROM Artists WHERE "
            "artist = '%s'", artist);
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

char* db_get_artist_art(char* artist)
{
    convert_str(artist);
    sqlite3_free(sqlchar2);
    sqlchar2 = sql_get_text_field(conn, "SELECT artpath FROM Artists WHERE "
            "artist = '%s'", artist);
    return sqlchar2;
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
        syslog(LOG_DEBUG, "%s update %s album...\n", __func__, album);
        if (sql_exec(conn, "UPDATE Songs SET album = '%s' WHERE song = '%s' AND artist = '%s'",
                    album, song, artist) == SQLITE_OK)
            rc = 1;
    }
    return rc;
}

int db_get_song_rating(char* song, char* artist)
{
    convert_str(song);
    convert_str(artist);
    int rating = sql_get_int_field(conn, "SELECT rating FROM Songs WHERE "
            "song = '%s' AND artist = '%s'", song, artist);
    return rating;
}

int db_update_song_rating(char* song, char* artist, int increase)
{
    int rc, rating = db_get_song_rating(song, artist) + increase;
    printf("%s update %s, %s rating... %i\n", __func__, song, artist, rating);
    rc = sql_exec(conn, "UPDATE Songs SET rating = '%i' "
            "WHERE song = '%s' AND artist = '%s'", rating, song, artist);
    return (rc == SQLITE_OK) ? rating : 0;
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

int db_update_artist_art(char* artist, char* art)
{
    int id, rc;
    if (!artist||!art)
        return 0;
    id = db_get_artist_id(artist);
    if (id) {
        rc = sql_exec(conn, "UPDATE Artists SET artpath = '%s' "
                "WHERE id = '%i'", art, id);
    } else {
        rc = sql_exec(conn, "INSERT INTO Artists (artist, artpath)"
                " VALUES ('%s', '%s')", artist, art);
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
        syslog(LOG_DEBUG, "%s found %i updating...\n", __func__, np);
        rc = sql_exec(conn, "UPDATE Songs SET numplayed = '%i' "
                "WHERE song = '%s' AND artist = '%s'", np, song, artist);
        if (rc == SQLITE_OK)
            rc = sql_exec(conn, "UPDATE Songs SET played = DATETIME('NOW', 'LOCALTIME') "
                    "WHERE song = '%s' AND artist = '%s'", song, artist);
    } else {
        syslog(LOG_DEBUG, "%s %s %s doesn't exist. adding...\n", __func__, song, artist);
        rc = sql_exec(conn, "INSERT INTO Songs (song, artist, added, played, numplayed, rating)"
                " VALUES ('%s', '%s', DATETIME('NOW', 'LOCALTIME'), DATETIME('NOW', 'LOCALTIME'), 1, 0)",
                song, artist);
    }
    if (rc != SQLITE_OK)
        return 0;

    db_update_song_album(song, artist, album);
/*    if (album)
    {
        syslog(LOG_DEBUG, "%s update %s album...\n", __func__, album);
        sql_exec(conn, "UPDATE Songs SET album = '%s' WHERE song = '%s' AND artist = '%s'", album, song, artist);
    }*/

    return np;
}

void db_close()
{
    sqlite3_free(sqlchar0);
    sqlite3_free(sqlchar1);
    sqlite3_free(sqlchar2);
    sqlite3_close(conn);
}
