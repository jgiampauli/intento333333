#ifndef UTILS_CLIENTE_H_
#define UTILS_CLIENTE_H_


#include<signal.h>
#include "global.h"

typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

int crear_conexion(char* ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente, op_code codigo);
t_paquete* crear_paquete(op_code codigo);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void agregar_a_paquete_sin_agregar_tamanio(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
char* pasar_a_string(char** string_array);
void esperar_por(int milisegundos_a_esperar);
char *listar_pids_cola(t_queue* cola_con_pids);
char *listar_pids_cola_de_strings(t_queue* cola_con_pids);
char *listar_recursos_lista_recursos_por_condicion(t_list* lista_con_recursos, bool (*evaluar)(t_recurso *));
char* listar_recursos_disponibles(int* recursos_disponibles, int cantidad_de_recursos);

#endif /* UTILS_CLIENTE_H_ */
