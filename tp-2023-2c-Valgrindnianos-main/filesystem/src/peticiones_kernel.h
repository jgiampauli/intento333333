#ifndef SRC_PETICIONES_KERNEL_H_
#define SRC_PETICIONES_KERNEL_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "filesystem.h"

extern int retardo_acceso_bloque;
extern int retardo_acceso_fat;
extern int tam_bloque;
extern uint32_t *bits_fat; //Array con tabla FAT
extern char* path_fcb;
extern int socket_fs;
extern FILE* bloques;
extern char** array_bloques;
extern int primer_bloque_fat;

void abrir_archivo(int cliente_fd);
void crear_archivo(int cliente_fd);
void truncar_archivo(int cliente_fd);
void leer_archivo_fs(int cliente_fd);
void escribir_archivo_fs(int cliente_fd);

#endif /* SRC_PETICIONES_KERNEL_H_ */
