#ifndef CPU_H_
#define CPU_H_

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

#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>
#include "cpu_instrucciones.h"


extern int socket_cpu_dispatch;
extern int socket_cpu_interrupt;
extern int socket_kernel;
extern int socket_memoria;

t_config* iniciar_config(void);
t_log* iniciar_logger(void);
void terminar_programa(t_log* logger, t_config* config);
int conectar_memoria(char* ip, char* puerto);
void* manejar_interrupciones(void* args);
void manejar_peticiones_instruccion();
void manejar_peticion_al_cpu();

t_instruccion* recibir_instruccion_memoria(int program_counter,int pid);
void recibir_interrupcion(int socket_kernel);
void devolver_a_kernel(t_contexto_ejec* contexto, op_code code, int socket_cliente);

#endif /* CPU_H_ */
