#ifndef CUSTOMDUPS_H
#define CUSTOMDUPS_H

#include <fcntl.h>
#include <unistd.h>
#include "mycommon.h"
#include <errno.h>

/* 
 * custom dup function which is implemented using fcntl
 * returns new file descriptor on success
 * returns -1 on failure and sets errno 
*/
int custom_dup(int oldfd);

/* 
 * custom dup2 function which is implemented using fcntl 
 * returns new file descriptor on success
 * returns -1 on failure and sets errno
*/
int custom_dup2(int oldfd, int newfd);

#endif