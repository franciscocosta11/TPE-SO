#ifndef SHM_HELPERS_H
#define SHM_HELPERS_H
#include <stddef.h>

/**
 * @brief Crea (o trunca) y mapea un objeto de memoria compartida.
 * @param name Nombre del objeto POSIX shm (ej. "/game_state").
 * @param size Tamaño en bytes a reservar/mmapear.
 * @param prot Flags de protección (p.ej. PROT_READ|PROT_WRITE).
 * @return puntero al mapeo en memoria en éxito, NULL en error (errno seteado).
 */
void* shm_create_map(const char *name, size_t size, int prot);

/**
 * @brief Abre y mapea un objeto de memoria compartida existente.
 * @param name Nombre del objeto POSIX shm.
 * @param out_size Si no es NULL, recibe el tamaño del objeto mapeado.
 * @param prot Flags de protección usadas en mmap (p.ej. PROT_READ).
 * @return puntero al mapeo en memoria en éxito, NULL en error (errno seteado).
 */
void* shm_attach_map(const char *name, size_t *out_size, int prot);

/**
 * @brief Elimina el nombre del objeto de memoria compartida (shm_unlink).
 * @param name Nombre del objeto POSIX shm a eliminar.
 * @return 0 en éxito, -1 en error (errno seteado).
 */
int shm_remove_name(const char *name);


#endif
