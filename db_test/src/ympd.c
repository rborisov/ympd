#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <libconfig.h>
#include <pwd.h>

#include "rcar_db.h"
#include <mysql.h>

int main(int argc, char **argv)
{
/*    db_init();

    db_put_song("song name", "artist name", "album name");

    db_close();*/

    char strr[] = "test string ' with '";
    char *ptr = strr;
    char chrr = 'i';
    int len;
    char out[128] = "";
    char *outptr = out;

    MYSQL *con;

    len = strlen(ptr);
    printf("%s %i\n", ptr, len);
/*    strcpy(out, strr);
    while (ptr) {
        ptr = strchr(ptr, chrr);
        if (ptr) {
            strncmp(outptr + (ptr-strr+1), "ji", 2);
            printf("%s %i\n%s %i\n", ptr, ptr-strr, out, strlen(out));
            outptr = outptr + (ptr-strr) + 1;
           printf("%s\n", outptr); 
            ptr++;
        }
    }
*/
    con = mysql_init(NULL);
    outptr = mysql_real_escape_string(con, out, strr, 20);
    mysql_close(con);


    printf("%s %i %i\n", out, outptr, strlen(out));

    return EXIT_SUCCESS;
}
