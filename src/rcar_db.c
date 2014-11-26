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
