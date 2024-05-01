#ifndef _PIPE_H
#define _PIPE_H

#include <device.h>

void pipe_init();
int kpipe(int pipefd[2]);

#endif