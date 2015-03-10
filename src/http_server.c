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

#include "radio.h"
#include "http_server.h"
#include "config.h"

int callback_http(struct mg_connection *c)
{
    const struct embedded_file *req_file;
    FILE *fd;
    unsigned char *buf;
    char filename[MAX_LINE];
    struct passwd *pw = getpwuid(getuid());
    char *images_dir, *homedir = pw->pw_dir;

    syslog(LOG_INFO, "http: %s\n", c->uri);

    if(!strcmp(c->uri, "/"))
        req_file = find_embedded_file("/index.html");
    else
        req_file = find_embedded_file(c->uri);

    if(req_file)
    {
        mg_send_header(c, "Content-Type", req_file->mimetype);
        mg_send_data(c, req_file->data, req_file->size);
        return MG_REQUEST_PROCESSED;
    }

    if (!config_lookup_string(&rcm.cfg, "application.images_path", &images_dir))
    {
        syslog(LOG_ERR,  "%s: No 'application.images_path' setting in configuration file.\n", __func__);
        sprintf(filename, "%s/%s/images/%s", homedir, RCM_DIR_STR, c->uri);
    } else {
        sprintf(filename, "%s/%s", images_dir, c->uri);
    }

    fd = fopen(filename, "r");
    if(!fd)
        syslog(LOG_ERR, "Failed open file %s\n", filename);
    else
    {
        unsigned int bufsize;
        fseek(fd, 0, SEEK_END);
        bufsize = ftell(fd);
        fseek(fd, 0, SEEK_SET);
        buf = (unsigned char*)malloc(bufsize+1);

/*        while ((tmp = fgetc(fd)) != EOF)
        {
            buf[i] = (unsigned char)tmp;
            i++;
        }
        buf[i] = 0;*/

        if (buf) {
            fread(buf, bufsize, 1, fd);
            fclose(fd);

            mg_send_header(c, "Content-Type", "image/jpeg");
            mg_send_data(c, buf, bufsize);

            free(buf);
            return MG_REQUEST_PROCESSED;
        } else {
            fclose(fd);
            syslog(LOG_ERR, "%s: memory error\n", __func__);
        }
    }

    mg_send_status(c, 404);
    mg_printf_data(c, "Not Found");
    return MG_REQUEST_PROCESSED;
}
