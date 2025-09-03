#ifndef SHM_HELPERS_H
#define SHM_HELPERS_H
#include <stddef.h>

void* shm_create_map(const char *name, size_t size, int prot);           // crea + ftruncate + mmap
void* shm_attach_map(const char *name, size_t *out_size, int prot);      // abre + fstat + mmap
int   shm_remove_name(const char *name);                                  // shm_unlink wrapper

#endif
