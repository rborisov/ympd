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

int db_create();

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
        printf("CREATE TABLE: %s\n", mysql_error(con));
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
    sprintf(query, "SELECT Listened FROM Songs WHERE Name='%s' AND Artist='%s'", realname, realartist);
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
    } else {
        fprintf(stderr, "db_get_song_listened: can't find listened - 0 %s\n", mysql_error(con));
    }
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

    sprintf(query, "SELECT Id,Album,Listened FROM Songs WHERE Name='%s' AND Artist='%s'", realname, realartist);
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
        printf("numfields %i id %i albumupdate %i(%s) listened %i\n", numfields, id, albumupdate, row[1] ? row[1] : "NULL", listened);
#endif
        exists = 1;
    }
    mysql_free_result(result);

    if (exists) {
        printf("exists %i %i\n", exists, listened);
        /* increase listened */
        sprintf(query, "UPDATE Songs SET Listened='%i' WHERE Id='%i'", listened+1, id);
        if (mysql_query(con, query)) {
            goto error;
        }
        /* check album value and fill  */
        if (albumupdate) {
            sprintf(query, "UPDATE Songs SET Album='%s' WHERE Id='%i'", realalbum, id);
            if (mysql_query(con, query)) {
                goto error;
            }
        }
    } else {
        /* put new song, check for artist */
        sprintf(query, "INSERT INTO Songs VALUES(0,'%s','%s','%s',1,0)", realname, realartist, realalbum);
        if (mysql_query(con, query)) {
            goto error;
        }
    }

    return 1;

error:
    fprintf(stderr, "%s\n", mysql_error(con));
    return 0;
}
