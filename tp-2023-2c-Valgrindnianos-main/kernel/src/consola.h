#ifndef SRC_CONSOLA_H_
#define SRC_CONSOLA_H_

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
#include <readline/readline.h>
#include <readline/history.h>
#include<semaphore.h>


#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>
#include "planificador_largo_plazo.h"
#include "peticiones_cpu_dispatch.h"
#include "peticiones_fs.h"

extern sem_t despertar_planificacion_largo_plazo;
extern sem_t recibir_interrupcion;
extern t_dictionary* colas_de_procesos_bloqueados_para_cada_archivo;
extern int grado_max_multiprogramacion;
extern t_pcb* proceso_ejecutando;
extern pthread_mutex_t m_planificador_largo_plazo;
extern pthread_mutex_t m_planificador_corto_plazo;

extern int *recursos_disponible;
extern int cant_recursos;

void levantar_consola();
void destroy_proceso_ejecutando();


#endif /* SRC_CONSOLA_H_ */
