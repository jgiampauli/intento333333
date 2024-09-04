#ifndef UTILS_SERVER_H_
#define UTILS_SERVER_H_


#include "global.h"
#include "utils_cliente.h"

#define IP "127.0.0.1"

void* recibir_buffer(int*, int);

int iniciar_servidor(char* puerto_escucha);
int esperar_cliente(int);
t_list* recibir_paquete(int);
t_list* recibir_paquete_instrucciones(int socket_cliente);
char* recibir_mensaje(int);
int recibir_operacion(int);
void recibir_handshake(int);
int responder_peticiones(int cliente_fd);
void manejar_handshake_del_cliente(int);
t_contexto_ejec* recibir_contexto_de_ejecucion(int socket_cliente);
void recibir_instruccion_con_dos_parametros_en(t_instruccion* instruccion,  char** nombre_modulo, int *pid, int cliente_fd);
void recibir_instruccion_con_dos_parametros_y_contenido_en(t_instruccion* instruccion, char** contenido_a_escribir,char** nombre_modulo, int *pid, int cliente_fd);
void deserializar_instruccion_con_dos_parametros_de(void* buffer, t_instruccion* instruccion, int *desplazamiento);
void recibir_programcounter(int socket_cliente, int *pid, int* program_counter);
void registro_cpu_destroy(registros_CPU* registro);
void contexto_ejecucion_destroy(t_contexto_ejec* contexto_ejecucion);
void instruccion_destroy(t_instruccion* instruccion);
t_instruccion *deserializar_instruccion_en(void *buffer, int* desplazamiento);
t_instruccion *recibir_instruccion(int cliente_fd);
void recibir_path_y_pid(int socket_cliente, char **path, int *pid);
t_contexto_ejec* deserializar_contexto_de_ejecucion(void *buffer, int size_buffer, int *desplazamiento);

#endif /* UTILS_SERVER_H_ */
