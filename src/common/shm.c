// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#define _POSIX_C_SOURCE 200809L
#include "shm.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>

void *shm_create_map(const char *name, size_t size, int prot)
{
    int fd = shm_open(name, O_CREAT | O_RDWR, 0600);
    if (fd == -1)
    {
        perror("shm_open create");
        return NULL;
    }
    if (ftruncate(fd, (off_t)size) == -1)
    {
        perror("ftruncate");
        close(fd);
        return NULL;
    }
    void *p = mmap(NULL, size, prot, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
    {
        perror("mmap create");
        close(fd);
        return NULL;
    }
    close(fd);
    return p;
}

void *shm_attach_map(const char *name, size_t *out_size, int prot)
{
    /* Si el mapeo requiere escritura debemos abrir con O_RDWR o mmap fallará con EACCES */
    int oflags = (prot & PROT_WRITE) ? O_RDWR : O_RDONLY;
    int fd = shm_open(name, oflags, 0600);
    if (fd == -1)
    {
        perror("shm_open attach");
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) == -1)
    {
        perror("fstat");
        close(fd);
        return NULL;
    }
    void *p = mmap(NULL, (size_t)st.st_size, prot, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED)
    {
        perror("mmap attach");
        close(fd);
        return NULL;
    }
    if (out_size)
        *out_size = (size_t)st.st_size;
    close(fd);
    return p;
}

int shm_remove_name(const char *name)
{
    return shm_unlink(name);
}
