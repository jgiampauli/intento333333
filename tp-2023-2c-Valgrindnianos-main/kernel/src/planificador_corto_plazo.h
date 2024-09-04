#ifndef SRC_PLANIFICADOR_CORTO_PLAZO_H_
#define SRC_PLANIFICADOR_CORTO_PLAZO_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/temporal.h>
#include<semaphore.h>
#include<commons/collections/dictionary.h>

#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>

#include "planificador_largo_plazo.h"// para traerme variables globales

extern pthread_mutex_t m_planificador_corto_plazo;

void *planificar_nuevos_procesos_corto_plazo(void *arg);

void enviar_contexto_de_ejecucion_a(t_contexto_ejec* contexto_a_ejecutar, op_code opcode, int socket_cliente);

void aviso_planificador_corto_plazo_proceso_en_ready(t_pcb* proceso_en_ready);

void aviso_planificador_corto_plazo_proceso_en_bloc(t_pcb* proceso_en_bloc);

void aviso_planificador_corto_plazo_proceso_en_exec(t_pcb* proceso_en_exec);

void aviso_planificador_corto_plazo_proceso_en_exit(int pid_proceso_exit);

void poner_a_ejecutar_otro_proceso();

#endif /* SRC_PLANIFICADOR_CORTO_PLAZO_H_ */
