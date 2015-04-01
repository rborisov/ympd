#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>

#include "radio.h"

int rcm_init()
{
    openlog("rcm", 0, LOG_USER);
    return 1;
}

void rcm_close()
{
    closelog();
}

char* cfg_get_db_path()
{
    char *db_path = NULL;

    if (!(&rcm.cfg)) {
        syslog(LOG_ERR, "%s: rcm.cfg is NULL\n", __func__);
        return NULL;
    }

    if (!config_lookup_string(&rcm.cfg, "application.db_path", &db_path))
    {
        syslog(LOG_ERR, "%s: No 'application.db_path' setting in configuration file.\n", __func__);
        //TODO: fill the default db_path to config
    }

    return db_path;
}

int cfg_get_radio_status()
{
    int status = 0;

    if (!(&rcm.cfg)) {
        syslog(LOG_ERR, "%s: rcm.cfg is NULL\n", __func__);
        return 0;
    }

    if (!config_lookup_int(&rcm.cfg, "radio.on", &status))
    {
        syslog(LOG_ERR, "%s: No 'radio.on' setting in configuration file.\n", __func__);
    }

    return status;
}

void cfg_set_radio_status(int status)
{
    if (cfg_get_radio_status() != status) {
        config_setting_t *root, *setting;
        root = config_root_setting(&rcm.cfg);
        setting = config_setting_get_member(root, "radio");
        if (setting) {
            int ret = config_setting_remove(setting, "on");
            if (ret == CONFIG_TRUE) {
                config_setting_t *current = config_setting_add(setting, "on", CONFIG_TYPE_INT);
                config_setting_set_int(current, status);
                if(! config_write_file(&rcm.cfg, rcm.config_file_name))
                {
                    syslog(LOG_ERR, "%s: Error while writing file.\n", __func__);
                }
            }
        }
    }
}

/*
* http://stackoverflow.com/questions/10535255/check-internet-connection-with-sockets
*/
void *www_online()
{
    int sockfd,val;
    char buffer[MAX_LINE];
    struct hostent *google_ent=NULL;
    struct sockaddr_in google_addr;

    sockfd = -1;

    if((google_ent = gethostbyname("www.google.com")) != NULL)
    {
        if((sockfd = socket(google_ent->h_addrtype,SOCK_STREAM,IPPROTO_TCP)) != -1)
        {
            val = 1;
            if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR, (char *) &val, sizeof(val)) == 0
            && setsockopt(sockfd,IPPROTO_TCP,TCP_NODELAY, (char *) &val, sizeof(val)) == 0)
            {
                google_addr.sin_family = google_ent->h_addrtype;
                memcpy(&(google_addr.sin_addr), google_ent->h_addr, google_ent->h_length);
                google_addr.sin_port = htons(80);
                if( connect(sockfd,(struct sockaddr *) &google_addr,sizeof(google_addr)) == 0)
                {
                    if(write(sockfd,"GET /index.html HTTP/1.1\r\n\r\n", 29) >= 28)
                    {
                        shutdown(sockfd, SHUT_WR);
                        if(read(sockfd, buffer, MAX_LINE) != -1) // all right!
                        {
                            close(sockfd);
                            rcm.online = 1;
                            return (void *) 1;
                        }
                        else
                        syslog(LOG_ERR, "%s: read()\n", __func__);
                            //report_error("read()",1,0,verbose);
                    }
                    else
                    syslog(LOG_ERR, "%s: write()\n", __func__);
                        //report_error("write()",1,0,verbose);
                }
                else
                syslog(LOG_ERR, "%s: connect()\n", __func__);
                    //report_error("connect()",1,0,verbose);
            }
            else
            syslog(LOG_ERR, "%s: setsockopt()\n", __func__);
                //report_error("setsockopt()",1,0,verbose);
        }
        else
        syslog(LOG_ERR, "%s: socket()\n", __func__);
            //report_error("socket()",1,0,verbose);
    }
    else
    syslog(LOG_ERR, "%s: cannot resolve IP for mom Google.\n", __func__);
        //report_error("cannot resolve IP for mom Google.",0,0,error); // this is is the most common error.

    if(sockfd!=-1)
        close(sockfd);
    rcm.online = 0;
    return (void *) 0; // no internet
}

void radio_start()
{
    streamripper_set_url_dest(NULL);
    init_streamripper();
    rcm.radio_status = 1;
    cfg_set_radio_status(1);
}

void radio_stop()
{
    stop_streamripper();
    rcm.radio_status = 0;
    cfg_set_radio_status(0);
}

int radio_toggle()
{
    if (rcm.radio_status == 0)
        radio_start();
    else
        radio_stop();
    return rcm.radio_status;
}

int radio_get_status()
{
    return rcm.radio_status;
}
