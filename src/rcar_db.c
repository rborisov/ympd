#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rcar_db.h"
#include <mysql.h>

#define DBNAME "rcardb"
#define DBUSER "pi"
#define DBPASS "raspberry"
#define DBSERVER "localhost"

MYSQL *con;
char strbuf[128];

int db_init()
{
    int errno;
    char query[2000];
    
    con = mysql_init(NULL);
    if (con == NULL)
    {
        fprintf(stderr, "db_init: %s\n", mysql_error(con));
        return 0;
    }
    
    if (mysql_real_connect(con, DBSERVER, DBUSER, DBPASS, DBNAME, 0, NULL, 0) == NULL) {
        errno = mysql_errno(con);
        if (errno != 1049 && errno != 1045) {
            fprintf(stderr, "Error %u connecting to Database: %s\n", errno, mysql_error(con));
            return 0;
        } else {
            printf("Database '%s' doesn't exist. Trying to create it...\n", DBNAME);
            if (mysql_real_connect(con, "localhost", "root", "root_db", NULL, 0, NULL, 0) == NULL) {
                fprintf(stderr, "Error %u connecting to Database: %s\n", mysql_errno(con), mysql_error(con));
                return 0;
            }
            sprintf(query, "CREATE DATABASE IF NOT EXISTS %s", DBNAME);
            if (mysql_query(con, query)) {
                fprintf(stderr, "Error %u creating Database: %s\n", mysql_errno(con), mysql_error(con));
                return 0;
            }
            sprintf(query, "CREATE USER %s@localhost IDENTIFIED BY '%s'", DBUSER, DBPASS);
            if (mysql_query(con, query)) {
                fprintf(stderr, "Error %u creating user: %s\n", mysql_errno(con), mysql_error(con));
            }
            sprintf(query, "GRANT ALL PRIVILEGES ON %s.* TO %s@localhost;", DBNAME, DBUSER);
            if (mysql_query(con, query)) {
                fprintf(stderr, "Error %u granting rights: %s\n", mysql_errno(con), mysql_error(con));
                return 0;
            }
            mysql_close(con);
            printf("Done. Reconnecting to newly created database... %s\n", DBNAME);
            con = mysql_init(NULL);
            if (con == NULL)
            {
                fprintf(stderr, "db_init: %s\n", mysql_error(con));
                return 0;
            }
            if (mysql_real_connect(con, DBSERVER, DBUSER, DBPASS, DBNAME, 0, NULL, 0) == NULL) {
                errno = mysql_errno(con);
                return 0;
            }
        }
    }
    
    if (mysql_query(con, 
                "CREATE TABLE IF NOT EXISTS Songs(Id INT KEY AUTO_INCREMENT,\
                Name TEXT, Artist TEXT, Album TEXT, Listened INT, Rating INT)")) {
        printf("CREATE TABLE Songs: %s\n", mysql_error(con));
        return 0;
    }

    if (mysql_query(con,
                "CREATE TABLE IF NOT EXISTS Albums(Id INT KEY AUTO_INCREMENT,\
                Name TEXT, Artist TEXT, Year TEXT, ArtUrl TEXT, ArtPath TEXT)")) {
        printf("CREATE TABLE Albums: %s\n", mysql_error(con));
        return 0;
    }

    printf("db_init successfull\n");

    return 1;
}

void db_close()
{
    if (con)
        mysql_close(con);
}

char* db_get_album(char* name, char* artist)
{
    char query[2000];
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;
    char realartist[128] = "", realname[128] = "";
    char *albumstr = NULL;

    if (con == NULL || !name || !artist) {
        fprintf(stderr, "db_get_album: error!\n");
        return NULL;
    }
    mysql_real_escape_string(con, realname, name, strlen(name));
    mysql_real_escape_string(con, realartist, artist, strlen(artist));
    sprintf(query, "SELECT Album FROM Songs WHERE Name='%s' AND Artist='%s'", 
            realname, realartist);
    if (mysql_query(con, query)) {
        goto error;
    }
    result = mysql_store_result(con);
    if (result == NULL) {
        goto error;
    }
    row = mysql_fetch_row(result);
    if (row) {
        if (strcmp(row[0],"") != 0) {
            strcpy(strbuf, row[0]);
            albumstr = strbuf;
        }
    } else {
        fprintf(stderr, "db_get_album: can't find album - %s - %s\n", 
                realname, realartist);
    }
    mysql_free_result(result);
    return albumstr;
error:
    fprintf(stderr, "db_get_album: %s\n", mysql_error(con));
    return albumstr;
}

int db_get_song_listened(char* name, char* artist)
{
    char query[2000];
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;
    int listened = 0;
    char realartist[128] = "", realname[128] = "";

    if (con == NULL) {
        fprintf(stderr, "db_get_song_listened: error!\n");
        return 0;
    }
    mysql_real_escape_string(con, realname, name, strlen(name));
    mysql_real_escape_string(con, realartist, artist, strlen(artist));
    sprintf(query, "SELECT Listened FROM Songs WHERE Name='%s' AND Artist='%s'", 
            realname, realartist);
    if (mysql_query(con, query)) {
        goto error;
    }
    result = mysql_store_result(con);
    if (result == NULL) {
        goto error;
    }
    row = mysql_fetch_row(result);
    if (row) {
        listened = atoi(row[0]);
    } /*else {
        fprintf(stderr, "db_get_song_listened: can't find listened - 0 %s\n", 
                mysql_error(con));
    }*/
    mysql_free_result(result);
    return listened;
error:
    fprintf(stderr, "db_get_song_listened: %s\n", mysql_error(con));
    return -1;
}

int db_listen_song(char* name, char* artist, char* album)
{
    char query[2000];
    int exists = 0;
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;
#ifdef DEBUGINFO
    int numfields, i;
#endif
    int id = 0;
    int listened = 0;
    int albumupdate = 0;
    char realartist[128] = "", 
         realname[128] = "", 
         realalbum[128] = "";

    if (con == NULL) {
        fprintf(stderr, "db_put_song: error!\n");
        return 0;
    }
        
    mysql_real_escape_string(con, realname, name, strlen(name));
    mysql_real_escape_string(con, realartist, artist, strlen(artist));
    mysql_real_escape_string(con, realalbum, album, strlen(album));

    sprintf(query, "SELECT Id,Album,Listened FROM Songs WHERE Name='%s' AND Artist='%s'", 
            realname, realartist);
    if (mysql_query(con, query)) {
        goto error;
    }
    
    result = mysql_store_result(con);
    if (result == NULL) {
        goto error;
    }
#ifdef DEBUGINFO 
    numfields = mysql_num_fields(result);
#endif
    row = mysql_fetch_row(result);
    if (row) {
#ifdef DEBUGINFO
        for (i = 0; i < numfields; i++) {
            if (row[i]) {
                printf("exists:%i %s\n", i, row[i] ? row[i] : "NULL");
            }

        }
#endif
        id = atoi(row[0]);
        albumupdate = strcmp(realalbum, row[1]) < 0;
        listened = atoi(row[2]);
#ifdef DEBUGINFO
        printf("numfields %i id %i albumupdate %i(%s) listened %i\n", 
                numfields, id, albumupdate, row[1] ? row[1] : "NULL", listened);
#endif
        exists = 1;
    }
    mysql_free_result(result);

    if (exists) {
        printf("exists %i %i\n", exists, listened);
        /* increase listened */
        sprintf(query, "UPDATE Songs SET Listened='%i' WHERE Id='%i'", 
                listened+1, id);
        if (mysql_query(con, query)) {
            goto error;
        }
        /* check album value and fill  */
        if (albumupdate) {
            sprintf(query, "UPDATE Songs SET Album='%s' WHERE Id='%i'", 
                    realalbum, id);
            if (mysql_query(con, query)) {
                goto error;
            }
        }
    } else {
        /* put new song, check for artist */
        sprintf(query, "INSERT INTO Songs VALUES(0,'%s','%s','%s',1,0)", 
                realname, realartist, realalbum);
        if (mysql_query(con, query)) {
            goto error;
        }
    }

    return 1;

error:
    fprintf(stderr, "db_listen_song: %s\n", mysql_error(con));
    return 0;
}

int db_get_realname_exists(char* table, char* realname, char* realartist)
{
    char query[2000];
    int exists = 0;
    MYSQL_RES *result = NULL;
    MYSQL_ROW row;

    if (con == NULL) {
        fprintf(stderr, "db_get_realname_exists: error!\n");
        return 0;
    }

    sprintf(query, "SELECT Id FROM Albums WHERE Name='%s' AND Artist='%s'",
            realname, realartist);
    if (mysql_query(con, query)) {
        goto error;
    }

    result = mysql_store_result(con);
    if (result == NULL) {
        goto error;
    }

    row = mysql_fetch_row(result);
    if (row) {
        exists = 1;
    }
    mysql_free_result(result);
    
    printf("%s exists %i\n", realname, exists);
    return exists;
error:
    fprintf(stderr, "db_listen_song: %s\n", mysql_error(con));    
    return 0;
}

int db_put_album_art1(char* pcharbuf)
{
        char query[2000];
        char realartist[128] = "",
             realart[128] = "",
             realname[128] = "";
        char *token;
        if (con == NULL) {
            fprintf(stderr, "db_put_album_arturl1: error!\n");
            return 0;
        }
        char *str_copy = strdup(pcharbuf);
        token = strsep(&str_copy, "*");
        if (token)
            mysql_real_escape_string(con, realname, token, strlen(token));
        else return 0;
        token = strsep(&str_copy, "*");
        if (token)
            mysql_real_escape_string(con, realartist, token, strlen(token));
        else return 0;
        token = strsep(&str_copy, "*");
        if (token)
            mysql_real_escape_string(con, realart, token, strlen(token));
        else return 0;
        printf("name:%s\nartist:%s\nart:%s\n", realname, realartist, realart);

        if (db_get_realname_exists("Albums", realname, realartist)) {
            printf("exists\n");
            sprintf(query, "UPDATE Albums SET Album='%s' WHERE Name='%s' AND Artist='%s'",
                    realart, realname, realartist);
        } else {
            printf("doesn't exist\n");
            sprintf(query, "INSERT INTO Albums VALUES(0,'%s','%s','%s','%s','%s')",
                    realname, realartist, "", realart, "");
        }

        free(str_copy);

        if (mysql_query(con, query)) {
            goto error;
        }

        return 1;
error:
        printf("error\n");
        fprintf(stderr, "db_put_album_art1: %s\n", mysql_error(con));
        return 0;
}

int db_put_album1(char* pcharbuf)
{
    char query[2000];
    char realartist[128] = "",
         realname[128] = "",
         realalbum[128] = "",
         realart[128] = "";
    char *token;
    if (con == NULL) {
        fprintf(stderr, "db_put_album: error!\n");
        return 0;
    }
    char *str_copy = strdup(pcharbuf);
    token = strsep(&str_copy, "*");
    if (token)
        mysql_real_escape_string(con, realname, token, strlen(token));
    else return 0;
    token = strsep(&str_copy, "*");
    if (token)
        mysql_real_escape_string(con, realartist, token, strlen(token));
    else return 0;
    token = strsep(&str_copy, "*");
    if (token)
        mysql_real_escape_string(con, realalbum, token, strlen(token));
    else return 0;
    token = strsep(&str_copy, "*");
    if (token)
        mysql_real_escape_string(con, realart, token, strlen(token));
    //this is not required //else return 0;
    
    printf("name:%s\nartist:%s\nalbum:%s\nart:%s\n", realname, realartist, realalbum, realart);

    if (db_get_realname_exists("Songs", realname, realartist)) {    
        sprintf(query, "UPDATE Songs SET Album='%s' WHERE Name='%s' AND Artist='%s'",
                realalbum, realname, realartist);
    } else {
        return 0;
    }    
    if (mysql_query(con, query)) {
        goto error;
    }

    if (db_get_realname_exists("Albums", realname, realartist)) {
        printf("exists\n");
        sprintf(query, "UPDATE Albums SET Album='%s' WHERE Name='%s' AND Artist='%s'",
                realart, realname, realartist);
    } else {
        printf("doesn't exist\n");
        sprintf(query, "INSERT INTO Albums VALUES(0,'%s','%s','%s','%s','%s')",
                realalbum, realartist, "", realart, "");
    }
    if (mysql_query(con, query)) {
        goto error;
    }

    free(str_copy);
    
    return 1;
error:
    fprintf(stderr, "db_put_album: %s\n", mysql_error(con));
    return 0;
}

int db_put_album(char* name, char* artist, char*album)
{
    char query[2000];
    char realartist[128] = "",
         realname[128] = "",
         realalbum[128] = "";

    if (con == NULL) {
        fprintf(stderr, "db_put_album: error!\n");
        return 0;
    }

    mysql_real_escape_string(con, realname, name, strlen(name));
    mysql_real_escape_string(con, realartist, artist, strlen(artist));
    mysql_real_escape_string(con, realalbum, album, strlen(album));

    sprintf(query, "UPDATE Songs SET Album='%s' WHERE Name='%s' AND Artist='%s'", 
            realalbum, realname, realartist);
    if (mysql_query(con, query)) {
        goto error;
    }
    return 1;
error:
    fprintf(stderr, "db_put_album: %s\n", mysql_error(con));
    return 0;
}
