#include "../include/customdups.h"

int custom_dup (int oldfd) 
{
    int fd;

    /* get the minimum available file descriptor */
    fd = fcntl(oldfd, F_DUPFD, 0);
    if (fd == -1)
        return RETURN_FAILURE;
    
    return fd;
}

int custom_dup2 (int oldfd, int newfd) 
{
    int fd;
    
    /* check if oldfd is valid */
    if (fcntl(oldfd, F_GETFL) == -1) 
    {
        errno = EBADF;
        return RETURN_FAILURE;
    }

    /* check if newfd and oldfd are the same */
    if (newfd == oldfd)
        return newfd;

    /* check if newfd file is already open */
    if (fcntl(newfd, F_GETFL) != -1)
        close(newfd);

    fd = fcntl(oldfd, F_DUPFD, newfd);
    if (fd == -1)
        return RETURN_FAILURE;
    
    return fd;
}
