#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/config.h>
#include <commons/collections/node.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <commons/bitarray.h>
#include <commons/txt.h>
#include <sys/mman.h>

#include <global/utils_cliente.h>
#include <global/utils_server.h>
#include <global/global.h>
#include "peticiones_memoria.h"
#include "peticiones_kernel.h"


int socket_fs;
int socket_memoria;
int socket_fs_solo_memoria;
FILE* bloques;
t_dictionary* fcb_por_archivo;
extern char* ip_memoria;
extern char* puerto_memoria;

typedef struct{
	int cliente_fd;
}t_arg_atender_cliente;


t_config* iniciar_config(void);
t_log* iniciar_logger(void);
void terminar_programa(t_log* logger, t_config* config);
void manejar_peticiones();
FILE* levantar_archivo_binario(char* path_archivo);
void *atender_cliente(void* args);
int conectar_memoria(char* ip_memoria, char* puerto_memoria);
void esperar_por_fs(int milisegundos_a_esperar);
#endif
