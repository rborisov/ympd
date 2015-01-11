/* ympd
   (c) 2013-2014 Andrew Karpow <andy@ndyk.de>
   This project's homepage is: http://www.ympd.org
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/types.h>

#include "mpd_client.h"
#include "http_server.h"
#include "config.h"

int callback_http(struct mg_connection *c)
{
    const struct embedded_file *req_file;
    FILE *fd;
    int i = 0;
    unsigned char buf[500000];
    char filename[256]; 
    int tmp;
    struct passwd *pw = getpwuid(getuid());
    char *images_dir, *homedir = pw->pw_dir;

    printf("http: %s\n", c->uri);

    if(!strcmp(c->uri, "/"))
        req_file = find_embedded_file("/index.html");
    else
        req_file = find_embedded_file(c->uri);

    if(req_file)
    {
        mg_send_header(c, "Content-Type", req_file->mimetype);
        mg_send_data(c, req_file->data, req_file->size);
//        printf("%s %i\n", c->uri, req_file->size);    
        return MG_REQUEST_PROCESSED;
    }


//    printf("%s ", c->uri);
    if (!config_lookup_string(&mpd.cfg, "application.images_path", &images_dir))
    {
        fprintf(stderr, "%s: No 'application.images_path' setting in configuration file.\n", __func__);
        sprintf(filename, "%s/%s/images/%s", homedir, RCM_DIR_STR, c->uri);
    } else {
        sprintf(filename, "%s/%s", images_dir, c->uri);
    }

    fd = fopen(filename, "r");
    if(!fd)
        printf("Failed open file %s\n", filename);
    else 
    {
        while ((tmp = fgetc(fd)) != EOF)
        {
            buf[i] = (unsigned char)tmp;
            i++;
        }
        buf[i] = 0;
//        printf(" i %i\n", i);
        fclose(fd);
        mg_send_header(c, "Content-Type", "image/jpeg");
        mg_send_data(c, buf, i);

        return MG_REQUEST_PROCESSED;
    }

    mg_send_status(c, 404);
    mg_printf_data(c, "Not Found");
    return MG_REQUEST_PROCESSED;
}
