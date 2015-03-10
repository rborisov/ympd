#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "mchar.h"
#include "debug.h"
#include "streamripper.h"
#include "mpd_client.h"
#include "mpd_utils.h"
#include "radio.h"

//static void catch_sig (int code);
static void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data);

static BOOL         m_started = FALSE;
static BOOL         m_alldone = FALSE;
//static BOOL         m_got_sig = FALSE;
static BOOL m_update = FALSE;
static BOOL m_new_track = FALSE;
static BOOL m_track_done = FALSE;

RIP_MANAGER_INFO *rmi = 0;
STREAM_PREFS prefs;
char chrbuff[128] = "";
//char filepath[128] = "";
char newsongname[128] = "";

void streamripper_set_url(char* url)
{
    const char *radio_url = url;

    if (url && strcmp(url, ""))
        goto done;

    if (!(&rcm.cfg)) {
        syslog(LOG_ERR, "%s: mpd is NULL\n", __func__);
        return;
    }
    if (!config_lookup_string(&rcm.cfg, "radio.url", &radio_url))
    {
        syslog(LOG_ERR, "%s: No 'radio.url' setting in configuration file.\n", __func__);
        return;
    }
done:
    setstream_streamripper(radio_url);
    syslog(LOG_INFO, "%s: url: %s\n", __func__, radio_url);
}

void streamripper_set_url_dest(char* dest)
{
    const char *radio_dest = dest,
         *music_path = NULL,
         *radio_path = NULL;
    config_setting_t *root, *setting;

    if (!(&rcm.cfg)) {
        syslog(LOG_ERR, "%s: rcm.cfg is NULL\n", __func__);
        return;
    }

    if (!config_lookup_string(&rcm.cfg, "application.music_path", &music_path))
    {
        syslog(LOG_ERR, "%s: No 'application.music_path' setting in configuration file.\n", __func__);
        return;
    }
    if (!config_lookup_string(&rcm.cfg, "radio.path", &radio_path))
    {
        syslog(LOG_ERR, "%s: No 'radio.path' setting in configuration file.\n", __func__);
        return;
    }
    rcm.image_update = 1;
    if (!radio_dest || strcmp(radio_dest, "") == 0) {
        if (!config_lookup_string(&rcm.cfg, "radio.current", &radio_dest))
        {
            syslog(LOG_ERR, "%s: No 'radio.dest' setting in configuration file.\n", __func__);
            return;
        }
    }
    else {
        root = config_root_setting(&rcm.cfg);
        setting = config_setting_get_member(root, "radio");
        if (setting) {
            int ret = config_setting_remove(setting, "current");
            if (ret == CONFIG_TRUE) {
                config_setting_t *current = config_setting_add(setting, "current", CONFIG_TYPE_STRING);
                config_setting_set_string(current, radio_dest);
                if(! config_write_file(&rcm.cfg, rcm.config_file_name))
                {
                    syslog(LOG_ERR, "%s: Error while writing file.\n", __func__);
                }
            }
        }
    }

    setting = config_lookup(&rcm.cfg, "radio.station");
    if(setting != NULL)
    {
        int count = config_setting_length(setting);
        int i;
        for(i = 0; i < count; ++i)
        {
            config_setting_t *station = config_setting_get_elem(setting, i);
            const char *name, *url;
            if(!(config_setting_lookup_string(station, "name", &name)
                        && config_setting_lookup_string(station, "url", &url)))
                continue;
            if (strcmp(name, radio_dest) == 0) {
                syslog(LOG_INFO, "%s: url = %s\n", __func__, url);
                setstream_streamripper(url);
                break;
            }
        }
    }

    strcpy(rcm.current_radio, radio_dest);

    sprintf(rcm.file_path, "%s%s", radio_path, radio_dest);
    syslog(LOG_INFO, "%s: rcm.file_path = %s\n", __func__, rcm.file_path);
    sprintf(chrbuff, "%s%s", music_path, rcm.file_path);
    setpath_streamuri(chrbuff);
    syslog(LOG_INFO, "%s: path: %s\n", __func__, chrbuff);
}

void setstream_streamripper(const char* streamuri)
{
    prefs_load ();
    prefs_get_stream_prefs (&prefs, (char *)streamuri);
    prefs_save ();
}

void setpath_streamuri(char* outpath)
{
    prefs_load ();
    strncpy(prefs.output_directory, outpath, SR_MAX_PATH);
    prefs_save ();
}

void init_streamripper()
{
    const char* incomplete;

    sr_set_locale ();
    debug_set_filename("streamripper.log");
    debug_enable();

    prefs_load ();
    prefs.overwrite = OVERWRITE_ALWAYS;
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    OPT_FLAG_SET(prefs.flags, OPT_TRUNCATE_DUPS, 1);
    OPT_FLAG_SET(prefs.flags, OPT_KEEP_INCOMPLETE, 0);
    prefs.dropcount = 1;
    strncpy (prefs.cs_opt.codeset_filesys, "UTF-8", MAX_CODESET_STRING);
//    strncpy (prefs.cs_opt.codeset_id3, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_metadata, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_relay, "UTF-8", MAX_CODESET_STRING);


    if (!(&rcm.cfg)) {
        syslog(LOG_ERR, "%s: mpd is NULL\n", __func__);
    } else {
        if (!config_lookup_string(&rcm.cfg, "radio.incomplete", &incomplete))
        {
            syslog(LOG_ERR, "%s: No 'radio.incomplete' setting in configuration file.\n", __func__);
        } else {
            strncpy(prefs.incomplete_directory, incomplete, SR_MAX_PATH);
        }
    }
    prefs_save ();

    rip_manager_init();

    start_streamripper();
}

int start_streamripper()
{
    int ret;

    /* Launch the ripping thread */
    if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
        syslog(LOG_ERR, "%s: Couldn't connect to %s\n", __func__, prefs.url);
        rip_manager_stop(rmi);
    }
    sleep(1);
    syslog(LOG_INFO, "%s: rmi %d\n", __func__, rmi->started);

    return ret;
}

int streamripper_exists()
{
    return rmi->started;
}

int status_streamripper()
{
    return rmi->status;
}

int poll_streamripper(char* newfilename)
{
//    static char filename[MAX_TRACK_LEN];

    if (!rmi->started) {
        start_streamripper();
        return 0;
    }

    if (m_update) {
        switch (rmi->status)
        {
            case RM_STATUS_BUFFERING:
                strcpy(rcm.status_str, "buffering");
                //syslog(LOG_INFO, "%s: stream ripper: buffering... %s\n", __func__, rmi->filename);
                break;
            case RM_STATUS_RIPPING:
                if (rmi->track_count < rmi->prefs->dropcount) {
                    strcpy(rcm.status_str, "skipping");
                }
                else {
                    strcpy(rcm.status_str, "ripping");
                }
                format_byte_size(rcm.filesize_str, rmi->filesize);
                break;
            case RM_STATUS_RECONNECTING:
                strcpy(rcm.status_str, "reconnecting");
                //syslog(LOG_INFO, "%s: reconnecting...\n", __func__);
                break;

        }
        m_update = FALSE;
    }
    if (m_track_done) {
        if (newfilename == NULL)
        {
            syslog(LOG_ERR, "%s: BUG!\n", __func__);
            return 0;
        }
        mstrncpy(newfilename, newsongname, MAX_TRACK_LEN);
        syslog(LOG_INFO, "%s: track done %s\n", __func__, newfilename);
        m_track_done = FALSE;
        return 1;
    }
    if (m_new_track) {
        syslog(LOG_INFO, "%s: new track %s %lu\n", __func__, rmi->filename, rmi->filesize);
        m_new_track = FALSE;
    }

    return 0;
}

int stop_streamripper()
{
    syslog(LOG_INFO, "%s: stop_streamripper\n", __func__);
    rip_manager_stop (rmi);
    rip_manager_cleanup ();

    return 0;
}
/*
void catch_sig(int code)
{
//    print_to_console ("\n");
    if (!m_started)
        exit(2);
    m_got_sig = TRUE;
}
*/

void delete_file_forever(char* uri)
{
    if (!uri) {
        char *music_path = NULL;
        char path[128], song[128];
        if (!config_lookup_string(&rcm.cfg, "application.music_path", &music_path))
        {
            syslog(LOG_ERR, "%s: No 'application.music_path' setting in configuration file.\n", __func__);
            return;
        }
        get_worst_song(&mpd.conn, song);
        sprintf(path, "%s%s", music_path, song);
        syslog(LOG_INFO, "%s: %s\n", __func__, path);
        remove(path);
    } else {
        remove(uri);
    }
}


/*
 * This will get called whenever anything interesting happens with the
 * stream. Interesting are progress updates, error's, when the rip
 * thread stops (RM_DONE) starts (RM_START) and when we get a new track.
 *
 * for the most part this function just checks what kind of message we got
 * and prints out stuff to the screen.
 */
void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data)
{
    ERROR_INFO *err;
    switch(message)
    {
        case RM_UPDATE:
            m_update = TRUE;
            break;
        case RM_ERROR:
            err = (ERROR_INFO*)data;
            syslog(LOG_ERR, "\n%s: error %d [%s]\n", __func__, err->error_code, err->error_str);
            sprintf(rcm.status_str, "error [%s]", err->error_str);
            m_alldone = TRUE;
            switch (err->error_code)
            {
                case 0x24:
                case 0x25:
                    syslog(LOG_INFO, "%s: %s - lets free file storage\n", __func__, err->error_str);
                    delete_file_forever(NULL);
                    break;
            }
            break;
        case RM_DONE:
            m_alldone = TRUE;
            break;
        case RM_NEW_TRACK:
            m_new_track = TRUE;
            break;
        case RM_STARTED:
            m_started = TRUE;
            break;
        case RM_TRACK_DONE:
            m_track_done = TRUE;
            sprintf(newsongname, "%s/%s", rcm.file_path, strrchr(data, '/' )+1);
            syslog(LOG_INFO, "%s: RM_TRACK_DONE: (%s)%s\n", __func__, (char*)data, newsongname);
            break;
    }
}
