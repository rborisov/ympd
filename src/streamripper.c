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

static void catch_sig (int code);
static void rip_callback (RIP_MANAGER_INFO* rmi, int message, void *data);

static BOOL         m_started = FALSE;
static BOOL         m_alldone = FALSE;
static BOOL         m_got_sig = FALSE;
static BOOL m_update = FALSE;
static BOOL m_new_track = FALSE;
static BOOL m_track_done = FALSE;

RIP_MANAGER_INFO *rmi = 0;
STREAM_PREFS prefs;
char chrbuff[128] = "";
char filepath[128] = "";

void streamripper_set_url(char* url)
{
    char *radio_url = url;

    if (url && url != "")
        goto done;
    
    if (!(&mpd.cfg)) {
        fprintf(stderr, "%s: mpd is NULL\n", __func__);
        return;
    }
    if (!config_lookup_string(&mpd.cfg, "radio.url", &radio_url))
    {
        fprintf(stderr, "%s: No 'radio.url' setting in configuration file.\n", __func__);
        return;
    }
done:
    setstream_streamripper(radio_url);
    printf("%s: url: %s\n", __func__, radio_url);
}

void streamripper_set_url_dest(char* dest)
{
    char *radio_dest = dest,
         *music_path = NULL,
         *radio_path = NULL,
         *radio_url = NULL;
    config_setting_t *root, *setting;

    if (!(&mpd.cfg)) {
        fprintf(stderr, "%s: mpd is NULL\n", __func__);
        return;
    }
    
    if (!config_lookup_string(&mpd.cfg, "application.music_path", &music_path))
    {
        fprintf(stderr, "%s: No 'application.music_path' setting in configuration file.\n", __func__);
        return;
    }
    if (!config_lookup_string(&mpd.cfg, "radio.path", &radio_path))
    {
        fprintf(stderr, "%s: No 'radio.path' setting in configuration file.\n", __func__);
        return;
    }
    if (!radio_dest || radio_dest == "") {
        if (!config_lookup_string(&mpd.cfg, "radio.current", &radio_dest))
        {
            fprintf(stderr, "%s: No 'radio.dest' setting in configuration file.\n", __func__);
            return;
        }
    } //TODO: else set new current radio to cfg
    else {
        root = config_root_setting(&mpd.cfg);
        setting = config_setting_get_member(root, "radio");
        if (setting) {
            int ret = config_setting_remove(setting, "current");
            if (ret == CONFIG_TRUE) {
                config_setting_t *current = config_setting_add(setting, "current", CONFIG_TYPE_STRING);
                config_setting_set_string(current, radio_dest);
                if(! config_write_file(&mpd.cfg, mpd.config_file_name))
                {
                    fprintf(stderr, "%s: Error while writing file.\n", __func__);
                }
            }
        }
    }
    
    setting = config_lookup(&mpd.cfg, "radio.station");
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
                printf("%s: url = %s\n", __func__, url);
                setstream_streamripper(url);
                break;
            }
        }
    }

    sprintf(filepath, "%s%s/", radio_path, radio_dest);
    printf("%s: filepath = %s\n", __func__, filepath);
    sprintf(chrbuff, "%s%s", music_path, filepath);
    setpath_streamuri(chrbuff);
    printf("%s: path: %s\n", __func__, chrbuff);
}

void setstream_streamripper(char* streamuri)
{
    prefs_load ();
    prefs_get_stream_prefs (&prefs, streamuri);
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
    sr_set_locale ();
//    debug_set_filename("streamripper.log");
//    debug_enable();
    
    prefs_load ();
    prefs.overwrite = OVERWRITE_ALWAYS;
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    prefs.dropcount = 1;
    strncpy (prefs.cs_opt.codeset_filesys, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_id3, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_metadata, "UTF-8", MAX_CODESET_STRING);
    prefs_save ();
    
    rip_manager_init();

    start_streamripper();
}

int start_streamripper()
{
    int ret;
 
    /* Launch the ripping thread */
    if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
        fprintf(stderr, "%s: Couldn't connect to %s\n", __func__, prefs.url);
        rip_manager_stop(rmi);
    }
    sleep(1);
    printf("%s: rmi %d\n", __func__, rmi->started);

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
    static char filename[MAX_TRACK_LEN];

    if (!rmi->started) {
        start_streamripper();
        return 0;
    }

    if (m_update) {
        switch (rmi->status)
        {
            case RM_STATUS_BUFFERING:
                printf("%s: stream ripper: buffering... %s\n", __func__, rmi->filename);
                break;
            case RM_STATUS_RIPPING:
                if (rmi->track_count < rmi->prefs->dropcount) {
//                    printf("%i (%i) skipping... %s %i\n", rmi->track_count, rmi->prefs->dropcount, rmi->filename, rmi->filesize);
                }
                else {
//                    printf("ripping... %s %i\n", rmi->filename, rmi->filesize);
                }
                break;
            case RM_STATUS_RECONNECTING:
                printf("%s: reconnecting...\n", __func__);
                break;

        }
        m_update = FALSE;
    }
    if (m_track_done) {
        if (newfilename == NULL)
        {
            fprintf(stderr, "%s: BUG!\n", __func__);
            return 0;
        }
//        mstrncpy(newfilename, filename, MAX_TRACK_LEN);
        sprintf(newfilename, "%s", filename);
        printf("%s: track done %s\n", __func__, newfilename);
        m_track_done = FALSE;
        return 1;
    }
    if (m_new_track) {
        printf("%s: new track %s %i\n", __func__, rmi->filename, rmi->filesize);
        sprintf(filename, "%s%s%s", filepath, rmi->filename, ".mp3");
        m_new_track = FALSE;
    }
    
    return 0;
}

int stop_streamripper()
{
    printf("%s: stop_streamripper\n", __func__);
    rip_manager_stop (rmi);
    rip_manager_cleanup ();

    return 0;
}

void catch_sig(int code)
{
//    print_to_console ("\n");
    if (!m_started)
        exit(2);
    m_got_sig = TRUE;
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
            fprintf(stderr, "\n%s: error %d [%s]\n", __func__, err->error_code, err->error_str);
            m_alldone = TRUE;
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
            break;
    }
}

