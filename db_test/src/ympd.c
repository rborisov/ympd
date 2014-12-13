#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <pthread.h>
#include <libconfig.h>
#include <pwd.h>

//#include "rcar_db.h"
//#include <mysql.h>


#include <curl/curl.h>

int main(int argc, char **argv)
{
/*    db_init();

    db_put_song("song name", "artist name", "album name");

    db_close();*/

/*    char strr[] = "test string ' with '";
    char *ptr = strr;
    char chrr = 'i';
    int len;
    char out[128] = "";
    char *outptr = out;

    MYSQL *con;

    len = strlen(ptr);
    printf("%s %i\n", ptr, len);*/
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

    CURL *curl;
    FILE *fp;
    CURLcode res;
    char *url = "http://userserve-ak.last.fm/serve/300x300/75576296.jpg";//"http://stackoverflow.com";
    char outfilename[254] = "75576296.jpg";
    curl = curl_easy_init();                                                                                                                                                                                                                                                           
    if (curl)
    {   
        fp = fopen(outfilename,"wb");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
    }

    //printf("%s %i %i\n", out, outptr, strlen(out));

    return EXIT_SUCCESS;
}
