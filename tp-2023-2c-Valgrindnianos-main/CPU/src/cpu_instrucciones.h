#ifndef SRC_CPU_INSTRUCCIONES_H_
#define SRC_CPU_INSTRUCCIONES_H_

#include <stdlib.h>
#include <math.h>
#include <global/global.h>
#include <global/utils_server.h>


void manejar_instruccion_set(t_contexto_ejec** contexto, t_instruccion* instruccion);
void manejar_instruccion_sum(t_contexto_ejec** contexto_actual,t_instruccion* instruccion);
void manejar_instruccion_sub(t_contexto_ejec** contexto_actual,t_instruccion* instruccion);
void manejar_instruccion_jnz(t_contexto_ejec** contexto_actual, t_instruccion* instruccion);
void manejar_mov_in(t_contexto_ejec** contexto_actual, t_instruccion*  instruccion);
void manejar_mov_out(t_contexto_ejec** contexto_actual, t_instruccion*  instruccion);

bool decodificar_direccion_logica(t_contexto_ejec** contexto_acutal,  int posicion_parametro_direccion_logica);

extern int tamano_pagina;
extern int socket_memoria;
extern int socket_kernel_client_fd;

#endif /* SRC_CPU_INSTRUCCIONES_H_ */
