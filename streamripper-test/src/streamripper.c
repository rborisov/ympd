#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <libconfig.h>
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

int start_streamripper()
{
    int ret;
    STREAM_PREFS prefs;
    
    debug_set_filename("streamripper.log");
    debug_enable();

    // Load prefs
    prefs_load ();
    prefs_get_stream_prefs (&prefs, "http://stream-ru1.radioparadise.com:9000/mp3-192");
    strncpy(prefs.output_directory, "/home/ruinrobo/Music/radio", SR_MAX_PATH);
    OPT_FLAG_SET(prefs.flags, OPT_SEPARATE_DIRS, 0);
    prefs.overwrite = OVERWRITE_ALWAYS;
    prefs_save ();

//    signal (SIGINT, catch_sig);
//    signal (SIGTERM, catch_sig);

    rip_manager_init();

    /* Launch the ripping thread */
    if ((ret = rip_manager_start (&rmi, &prefs, rip_callback)) != SR_SUCCESS) {
        fprintf(stderr, "Couldn't connect to %s\n", prefs.url);
        exit(EXIT_FAILURE);
    }
    sleep(1);
    printf("rmi %d\n", rmi->started);

    return 1;
}

int status_streamripper()
{
    return rmi->status;
}

int poll_streamripper(char* newfilename)
{
    STREAM_PREFS *prefs = rmi->prefs;

    if (m_update) {
        switch (rmi->status)
        {
            case RM_STATUS_BUFFERING:
                printf("stream ripper: buffering... %s\n", rmi->filename);
                break;
            case RM_STATUS_RIPPING:
                if (rmi->track_count < prefs->dropcount) {
//                    printf("skipping... %s %i\n", rmi->filename, rmi->filesize);
                }
                else {
//                    printf("ripping... %s %i\n", rmi->filename, rmi->filesize);
                }
                break;
            case RM_STATUS_RECONNECTING:
//                printf("reconnecting...\n");
                break;

        }
        m_update = FALSE;
    }
    if (m_new_track) {
        printf("new track %s %i\n", rmi->filename, rmi->filesize);
        m_new_track = FALSE;
    }
    if (m_track_done) {
        printf("track done %s %i\n", rmi->filename, rmi->filesize);
//        mstrncpy(newfilename, rmi->filename, MAX_TRACK_LEN);
        if (newfilename == NULL)
        {
            fprintf(stderr, "BUG!\n");
            return 0;
        }
        sprintf(newfilename, "%s%s", rmi->filename, ".mp3");
        m_track_done = FALSE;
        return 1;
    }
    
    return 0;
}

int stop_streamripper()
{
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

