#include <stdio.h>
#include <mysql.h>
#include "rcar_db.h"

int db_create()
{
    MYSQL *con = mysql_init(NULL);

    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        return 0;
    }

    if (mysql_real_connect(con, "localhost", "root", "root_db", 
                NULL, 0, NULL, 0) == NULL) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return 0;
    }  

    if (mysql_query(con, "CREATE DATABASE rcardb")) 
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        mysql_close(con);
        return 0;
    }

    mysql_close(con);

    return 1;
}

int db_create_tables()
{
    MYSQL *con = mysql_init(NULL);

    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        return 0;
    }

    if (mysql_real_connect(con, "localhost", "pi", "raspberry", "rcardb", 0, 
                NULL, 0) == NULL)
       goto error; 
    if (mysql_query(con, "DROP TABLE IF EXISTS Songs"))
        goto error;
    if (mysql_query(con, "DROP TABLE IF EXISTS Albums"))
        goto error;
    if (mysql_query(con, "DROP TABLE IF EXISTS Artists"))
        goto error;
    if (mysql_query(con, "CREATE TABLE Songs(Id INT KEY AUTO_INCREMENT,\
        Name TEXT, Artist TEXT, Album TEXT, Listened INT, Rating INT)"))
        goto error;
    if (mysql_query(con, "CREATE TABLE Albums(Id INT KEY AUTO_INCREMENT,\
        Name TEXT, Artist TEXT, Art TEXT, Listened INT, Rating INT)"))
            goto error;
    if (mysql_query(con, "CREATE TABLE Artists(Id INT KEY AUTO_INCREMENT,\
        Name TEXT, Art TEXT, Listened INT, Rating INT)"))
        goto error;

    mysql_close(con);
    return 1;

error:
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    return 0;
}

int db_put_listen(char* name, char* artist, char* album)
{
    MYSQL *con = mysql_init(NULL);
    char query[128];
    int status = 0;

    if (con == NULL)
    {
        fprintf(stderr, "%s\n", mysql_error(con));
        return 0;
    }

    if (mysql_real_connect(con, "localhost", "pi", "raspberry", "rcardb", 0,
                NULL, 0) == NULL)
        goto error;

    sprintf(query, "SELECT Artists FROM Songs WHERE Name=%s", name);
    if (mysql_query(con, query))
        goto error;

    do {
        MYSQL_RES *result = mysql_store_result(con);
        if (result == NULL)
            goto error;
        MYSQL_ROW row = mysql_fetch_row(result);
        printf("%s\n", row[0]);
        //TODO: if there are Song and Artist: increase Listened
        //and check Album
        //...
        mysql_free_result(result);
        status = mysql_next_result(con);
        if (status > 0)
            goto error;
    } while (status == 0);

    mysql_close(con);
    return 1;

error:
    fprintf(stderr, "%s\n", mysql_error(con));
    mysql_close(con);
    return 0;
}
