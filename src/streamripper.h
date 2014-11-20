#ifndef __STREAMRIPPER_H__
#define __STREAMRIPPER_H__

#include <libstreamripper.h>

int stop_streamripper();
int start_streamripper();
int status_streamripper();
int streamripper_exists();
void setstream_streamripper(char*);
void setpath_streamuri(char*);
void init_streamripper();

#endif
