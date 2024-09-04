#ifndef MEMORIA_H_
#define MEMORIA_H_

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
#include <math.h>

#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>

#include "memoria_de_instrucciones.h"

extern int socket_memoria;
extern int socket_fs;

typedef struct{
	int marco;
	bool presencia;
	bool modificado;
	uint32_t posicion_swap;
}t_tabla_de_paginas;

typedef struct{
	uint64_t cliente_fd;
	char* algoritmo_reemplazo;
	uint64_t retardo_respuesta;
	uint64_t tam_pagina;
	uint64_t tam_memoria;
}t_arg_atender_cliente;

typedef struct{
	int pid;
	int numero_marco;
	bool esLibre;
	int posicion_inicio_marco;
}t_situacion_marco;

typedef struct{
	char* pid;
	int numero_pagina;
}t_referenciaXpid;

t_dictionary* lista_instrucciones_porPID;
t_config* iniciar_config(void);
t_log* iniciar_logger(void);
void terminar_programa(t_log* logger, t_config* config);

int conectar_fs(char* ip, char* puerto, int *socket_filesystem);
void manejar_pedidos_memoria(char* algoritmo_reemplazo,int retardo_respuesta,int tam_pagina,int tam_memoria);
void *atender_cliente(void* args);

/*Estas mover a otro archivo en un futuro*/
void finalizar_proceso();
void devolver_marco(int cliente_fd);
void manejar_pagefault(char* algoritmo_reemplazo,int cliente_fd,int tam_pagina);
void crear_proceso(int cliente_fd);
bool memoria_llena();
int aplicarFifo();
int aplicarLru();
void reemplazar_marco(void*contenido_bloque,int pid,t_tabla_de_paginas*pagina_a_actualizar,t_situacion_marco* marco_a_guardar);
int obtener_pagina_a_reemplazar(int numero_marco,int pid);
void read_memory(int cliente_fd);
void write_memory(int cliente_fd);
void write_memory_fs(int cliente_fd);
#endif
