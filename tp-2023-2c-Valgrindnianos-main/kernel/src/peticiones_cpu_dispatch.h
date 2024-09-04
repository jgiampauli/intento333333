#ifndef SRC_PETICIONES_CPU_DISPATCH_H_
#define SRC_PETICIONES_CPU_DISPATCH_H_

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
#include <pthread.h>


#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>

#include "planificador_largo_plazo.h"
#include "planificador_corto_plazo.h"
#include "peticiones_fs.h"

extern t_dictionary* recurso_bloqueado;
extern t_dictionary *matriz_recursos_asignados;
extern t_dictionary *matriz_recursos_pendientes;
extern t_list *recursos_totales;
extern char	**recursos;
extern int cant_recursos;

void manejar_sleep(int socket_cliente);
void apropiar_recursos(int socket_cliente, char** recursos, int* recurso_disponible, int cantidad_de_recursos);
void desalojar_recursos(int socket_cliente, char** recursos, int* recurso_disponible, int cantidad_de_recursos);
void finalinzar_proceso(int socket_cliente, t_contexto_ejec* contexto);
void deteccion_de_deadlock();

t_list *duplicar_lista_recursos(t_list *a_duplicar);
int obtener_indice_recurso(char** recursos, char* recurso_a_buscar);
void incrementar_recurso_en_matriz(t_dictionary **matriz, char *nombre_recurso, char *pid, int cantidad_de_recursos);
void decrementar_recurso_en_matriz(t_dictionary **matriz, char *nombre_recurso, char *pid, int cantidad_de_recursos);

void destroy_lista_de_recursos(t_list* lista_recursos);
void destroy_proceso_en_matriz(t_dictionary *matriz, char *pid);
void destroy_matriz(t_dictionary *matriz);
void manejar_page_fault(int socket_cliente);

t_recurso *recurso_new(char *nombre_recurso);


#endif /* SRC_PETICIONES_CPU_DISPATCH_H_ */
