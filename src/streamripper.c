#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "streamripper.h"

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

void setstream_streamripper(char* streamuri)
{
//    prefs_get_stream_prefs (&prefs, "http://stream-ru1.radioparadise.com:9000/mp3-192");
    prefs_load ();
    prefs_get_stream_prefs (&prefs, streamuri);
    prefs_save ();
}

void setpath_streamuri(char* outpath)
{
//    strncpy(prefs.output_directory, "/home/ruinrobo/Music/radio", SR_MAX_PATH);
    prefs_load ();
    strncpy(prefs.output_directory, outpath, SR_MAX_PATH);
    prefs_save ();
}

void init_streamripper()
{
    sr_set_locale ();
    debug_set_filename("streamripper.log");
    debug_enable();
    
    prefs_load ();
    prefs.overwrite = OVERWRITE_ALWAYS;
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    prefs.dropcount = 1;
/*    strncpy (prefs.cs_opt.codeset_filesys, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_id3, "UTF-8", MAX_CODESET_STRING);
    strncpy (prefs.cs_opt.codeset_metadata, "UTF-8", MAX_CODESET_STRING);*/
    prefs_save ();
    
    rip_manager_init();

    start_streamripper();
}

int start_streamripper()
{
    int ret;
 
/*    sr_set_locale ();
   
    debug_set_filename("streamripper.log");
    debug_enable();

    // Load prefs
    prefs_load ();
//    prefs_get_stream_prefs (&prefs, "http://stream-ru1.radioparadise.com:9000/mp3-192");
//    prefs_get_stream_prefs (&prefs, "http://stream-uk1.radioparadise.com/mp3-192");
//    strncpy(prefs.output_directory, "/home/ruinrobo/Music/radio", SR_MAX_PATH);
    prefs_save ();
    prefs.overwrite = OVERWRITE_ALWAYS;
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    prefs.dropcount = 1;

    rip_manager_init();*/

    /* Launch the ripping thread */
    if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
        fprintf(stderr, "Couldn't connect to %s\n", prefs.url);
        rip_manager_stop(rmi);
//        rip_manager_cleanup();
    }
    sleep(1);
    printf("rmi %d\n", rmi->started);

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
                printf("stream ripper: buffering... %s\n", rmi->filename);
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
                printf("reconnecting...\n");
                break;

        }
        m_update = FALSE;
    }
    if (m_track_done) {
        if (newfilename == NULL)
        {
            fprintf(stderr, "BUG!\n");
            return 0;
        }
//        mstrncpy(newfilename, filename, MAX_TRACK_LEN);
        sprintf(newfilename, "%s", filename);
        printf("track done %s\n", newfilename);
        m_track_done = FALSE;
        return 1;
    }
    if (m_new_track) {
        printf("new track %s %i\n", rmi->filename, rmi->filesize);
        sprintf(filename, "%s%s", rmi->filename, ".mp3");
        m_new_track = FALSE;
    }
    
    return 0;
}

int stop_streamripper()
{
    printf("stop_streamripper\n");
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
            fprintf(stderr, "\nerror %d [%s]\n", err->error_code, err->error_str);
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

