#ifndef MEMORIA_DE_INSTRUCCIONES_H_
#define MEMORIA_DE_INSTRUCCIONES_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <commons/log.h>
#include <commons/string.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>


#include <global/global.h>
#include <global/utils_cliente.h>
#include <global/utils_server.h>

extern char *path_instrucciones;
extern t_dictionary *lista_instrucciones_porPID;

void enviar_instruccion_a_cpu(int cliente_cpu,int retardo_respuesta);
void leer_pseudo(int cliente_fd);

#endif
