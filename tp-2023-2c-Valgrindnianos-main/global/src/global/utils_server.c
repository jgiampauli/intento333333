#include "utils_server.h"


int iniciar_servidor(char* puerto_escucha)
{
	int socket_servidor;

	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(IP, puerto_escucha, &hints, &servinfo);

	socket_servidor = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);

	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);

	return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
	int socket_cliente = accept(socket_servidor, NULL, NULL);
	log_info(logger, "Se conecto un cliente!");

	return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}

void* recibir_buffer(int* size, int socket_cliente)
{
	void * buffer;

	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	buffer = malloc(*size);
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}

char* recibir_mensaje(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);
	int length_buffer = strlen(buffer);
	buffer = realloc(buffer, length_buffer + 2);
	buffer[length_buffer +1] = '\0';

	return buffer;
}

t_list* recibir_paquete(int socket_cliente)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}

t_list* recibir_paquete_instrucciones(int socket_cliente){
	int size;
		int desplazamiento = 0;
		void * buffer;
		t_list* instrucciones = list_create();
		int lista_length;

		buffer = recibir_buffer(&size, socket_cliente);

		 while(desplazamiento < size){
			 memcpy(&lista_length, buffer + desplazamiento, sizeof(int));
			desplazamiento+=sizeof(int);

			for(int i = 0; i< lista_length; i++){
				t_instruccion* instruccion = malloc(sizeof(t_instruccion));

				memcpy(&(instruccion->opcode_lenght), buffer + desplazamiento, sizeof(int));
				desplazamiento+=sizeof(int);
				instruccion->opcode = malloc(instruccion->opcode_lenght);
				memcpy(instruccion->opcode, buffer+desplazamiento, instruccion->opcode_lenght);
				desplazamiento+=instruccion->opcode_lenght;

				memcpy(&(instruccion->parametro1_lenght), buffer+desplazamiento, sizeof(int));
				desplazamiento+=sizeof(int);
				memcpy(&(instruccion->parametro2_lenght), buffer+desplazamiento, sizeof(int));
				desplazamiento+=sizeof(int);
				memcpy(&(instruccion->parametro3_lenght), buffer+desplazamiento, sizeof(int));
				desplazamiento+=sizeof(int);

				instruccion->parametros[0] = malloc(instruccion->parametro1_lenght);
				memcpy(instruccion->parametros[0], buffer + desplazamiento, instruccion->parametro1_lenght);
				desplazamiento += instruccion->parametro1_lenght;
				instruccion->parametros[1] = malloc(instruccion->parametro2_lenght);
				memcpy(instruccion->parametros[1], buffer + desplazamiento, instruccion->parametro2_lenght);
				desplazamiento += instruccion->parametro2_lenght;
				instruccion->parametros[2] = malloc(instruccion->parametro3_lenght);
				memcpy(instruccion->parametros[2], buffer + desplazamiento, instruccion->parametro3_lenght);
				desplazamiento += instruccion->parametro3_lenght;


				list_add(instrucciones, instruccion);
			}
		 }


		free(buffer);
		return instrucciones;
}

void recibir_handshake(int socket_cliente)
{
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);

	if(strcmp(buffer, "OK") == 0)
		enviar_mensaje("OK", socket_cliente, HANDSHAKE);
	else
		enviar_mensaje("ERROR", socket_cliente, HANDSHAKE);


	free(buffer);
}

void manejar_handshake_del_cliente(int socket_cliente){
	int size;
		char* buffer = recibir_buffer(&size, socket_cliente);

		if(strcmp(buffer, "OK") == 0){
			log_info(logger, "Se establecio la conexion con cpu correctamente");
		} else {
			log_info(logger, "No se pudo recibir el handshake de la CPU");
		}

	free(buffer);
}


t_contexto_ejec* recibir_contexto_de_ejecucion(int socket_cliente)
{

	int size;
	int desplazamiento = 0;
	void * buffer;
	int program_counter;

	t_contexto_ejec* contexto_ejecucion = malloc(sizeof(t_contexto_ejec));
	contexto_ejecucion->registros_CPU = malloc(sizeof(registros_CPU));

	buffer = recibir_buffer(&size, socket_cliente);


	while(desplazamiento < size )
	{

		memcpy(&(contexto_ejecucion->pid), buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);


		memcpy(&program_counter, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);


		memcpy(&(contexto_ejecucion->registros_CPU->AX), buffer + desplazamiento,sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->BX), buffer + desplazamiento,sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->CX), buffer + desplazamiento,sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->DX), buffer + desplazamiento,sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);

		if(desplazamiento < size){
			contexto_ejecucion->instruccion = deserializar_instruccion_en(buffer, &desplazamiento);
		} else {
			contexto_ejecucion->instruccion = NULL;
		}

	}

	contexto_ejecucion->program_counter = program_counter;


	free(buffer);
	return contexto_ejecucion;
}

t_contexto_ejec* deserializar_contexto_de_ejecucion(void *buffer, int size_buffer, int *desplazamiento){

	int program_counter;

	t_contexto_ejec* contexto_ejecucion = malloc(sizeof(t_contexto_ejec));
	contexto_ejecucion->registros_CPU = malloc(sizeof(registros_CPU));

	while(*desplazamiento < size_buffer )
	{

		memcpy(&(contexto_ejecucion->pid), buffer+*desplazamiento, sizeof(int));
		*desplazamiento+=sizeof(int);

		memcpy(&program_counter, buffer + *desplazamiento, sizeof(int));
		*desplazamiento+=sizeof(int);

		memcpy(&(contexto_ejecucion->registros_CPU->AX), buffer + *desplazamiento,sizeof(uint32_t));
		*desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->BX), buffer + *desplazamiento,sizeof(uint32_t));
		*desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->CX), buffer + *desplazamiento,sizeof(uint32_t));
		*desplazamiento+=sizeof(uint32_t);

		memcpy(&(contexto_ejecucion->registros_CPU->DX), buffer + *desplazamiento,sizeof(uint32_t));
		*desplazamiento+=sizeof(uint32_t);

		if(*desplazamiento < size_buffer){
			contexto_ejecucion->instruccion = deserializar_instruccion_en(buffer, desplazamiento);
		} else {
			contexto_ejecucion->instruccion = NULL;
		}

	}

	contexto_ejecucion->program_counter = program_counter;


	return contexto_ejecucion;
}


void deserializar_instruccion_con_dos_parametros_de(void* buffer, t_instruccion* instruccion, int *desplazamiento){
	memcpy(&(instruccion->opcode_lenght), buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	instruccion->opcode = malloc(instruccion->opcode_lenght);
	memcpy(instruccion->opcode, buffer+*desplazamiento, instruccion->opcode_lenght);
	*desplazamiento+=instruccion->opcode_lenght;

	memcpy(&(instruccion->parametro1_lenght), buffer+*desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	instruccion->parametros[0] = malloc(instruccion->parametro1_lenght);
	memcpy(instruccion->parametros[0], buffer + *desplazamiento, instruccion->parametro1_lenght);
	*desplazamiento += instruccion->parametro1_lenght;

	memcpy(&(instruccion->parametro2_lenght), buffer+*desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	instruccion->parametros[1] = malloc(instruccion->parametro2_lenght);
	memcpy(instruccion->parametros[1], buffer + *desplazamiento, instruccion->parametro2_lenght);
	*desplazamiento += instruccion->parametro2_lenght;

	 //esta linea es para el destroy de la instruccion
	 instruccion->parametro3_lenght = 0;
}

void recibir_instruccion_con_dos_parametros_y_contenido_en(t_instruccion* instruccion, char** contenido_a_escribir, char** nombre_modulo, int *pid, int cliente_fd){
	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);

	int despĺazamiento  = 0;

	while(despĺazamiento < size){

		memcpy(pid, buffer +despĺazamiento, sizeof(int));
		despĺazamiento += sizeof(int);

	    deserializar_instruccion_con_dos_parametros_de(buffer, instruccion, &despĺazamiento);

		int contenido_a_escribir_length;
		memcpy(&contenido_a_escribir_length, buffer + despĺazamiento, sizeof(int));
		despĺazamiento += sizeof(int);

		*contenido_a_escribir = malloc(contenido_a_escribir_length);
		memcpy(*contenido_a_escribir, buffer + despĺazamiento, contenido_a_escribir_length);
		despĺazamiento += contenido_a_escribir_length;

		int nombre_modulo_length;
		memcpy(&nombre_modulo_length, buffer + despĺazamiento, sizeof(int));
		despĺazamiento += sizeof(int);

		*nombre_modulo = malloc(nombre_modulo_length);
		memcpy(*nombre_modulo, buffer + despĺazamiento, nombre_modulo_length);
		despĺazamiento += nombre_modulo_length;
	}

	free(buffer);
}

void recibir_instruccion_con_dos_parametros_en(t_instruccion* instruccion, char** nombre_modulo, int *pid, int cliente_fd){
	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);

	int despĺazamiento  = 0;

	while(despĺazamiento < size){

		memcpy(pid, buffer +despĺazamiento, sizeof(int));
		despĺazamiento += sizeof(int);

	    deserializar_instruccion_con_dos_parametros_de(buffer, instruccion, &despĺazamiento);

	    int nombre_modulo_length;
		memcpy(&nombre_modulo_length, buffer + despĺazamiento, sizeof(int));
		despĺazamiento += sizeof(int);

		*nombre_modulo = malloc(nombre_modulo_length);
		memcpy(*nombre_modulo, buffer + despĺazamiento, nombre_modulo_length);
		despĺazamiento += nombre_modulo_length;
	}

	free(buffer);
}

t_instruccion *recibir_instruccion(int cliente_fd){
	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);

	int desplazamiento  = 0;
	t_instruccion* instruccion;

	while(desplazamiento < size){

		instruccion = deserializar_instruccion_en(buffer, &desplazamiento);
	}

	free(buffer);
	return instruccion;
}

t_instruccion *deserializar_instruccion_en(void *buffer, int* desplazamiento){
	t_instruccion * instruccion = malloc(sizeof(t_instruccion));

	memcpy(&(instruccion->opcode_lenght), buffer + *desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	instruccion->opcode = malloc( instruccion->opcode_lenght);
	memcpy( instruccion->opcode, buffer+*desplazamiento,  instruccion->opcode_lenght);
	*desplazamiento+= instruccion->opcode_lenght;

	memcpy(&( instruccion->parametro1_lenght), buffer+*desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	 instruccion->parametros[0] = malloc( instruccion->parametro1_lenght);
	memcpy( instruccion->parametros[0], buffer + *desplazamiento,  instruccion->parametro1_lenght);
	*desplazamiento +=  instruccion->parametro1_lenght;

	memcpy(&( instruccion->parametro2_lenght), buffer+*desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	 instruccion->parametros[1] = malloc( instruccion->parametro2_lenght);
	memcpy( instruccion->parametros[1], buffer + *desplazamiento,  instruccion->parametro2_lenght);
	*desplazamiento +=  instruccion->parametro2_lenght;


	memcpy(&( instruccion->parametro3_lenght), buffer+*desplazamiento, sizeof(int));
	*desplazamiento+=sizeof(int);
	 instruccion->parametros[2] = malloc( instruccion->parametro3_lenght);
	memcpy( instruccion->parametros[2], buffer + *desplazamiento,  instruccion->parametro3_lenght);
	*desplazamiento +=  instruccion->parametro3_lenght;

	return instruccion;
}

void instruccion_destroy(t_instruccion* instruccion){
    free(instruccion->opcode);

    if(instruccion->parametro1_lenght != 0 && instruccion->parametro2_lenght != 0 && instruccion->parametro3_lenght != 0){

    	free(instruccion->parametros[0]);
		free(instruccion->parametros[1]);
		free(instruccion->parametros[2]);

    } else if(instruccion->parametro1_lenght != 0 && instruccion->parametro2_lenght != 0 ){

    	free(instruccion->parametros[0]);
		free(instruccion->parametros[1]);

    } else if(instruccion->parametro1_lenght != 0 ){

    	free(instruccion->parametros[0]);

    }


    free(instruccion);
}

void contexto_ejecucion_destroy(t_contexto_ejec* contexto_ejecucion){

	if(contexto_ejecucion->instruccion != NULL){
		instruccion_destroy(contexto_ejecucion->instruccion);
	}

	free(contexto_ejecucion);
}



void registro_cpu_destroy(registros_CPU* registro){
	free(registro);
}


void recibir_programcounter(int socket_cliente, int *pid,int* program_counter ){
	int size;
	int desplazamiento = 0;
	void * buffer;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(program_counter, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);

		memcpy(pid, buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
	}
	free(buffer);

}


void recibir_path_y_pid(int socket_cliente, char **path, int *pid)
{
	int size;
	int desplazamiento = 0;
	void * buffer;
	int tamanio;

	buffer = recibir_buffer(&size, socket_cliente);
	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		*path = malloc(tamanio);

		memcpy(*path, buffer+desplazamiento, tamanio);
		desplazamiento+=tamanio;

		memcpy(pid, buffer+desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
	}
	free(buffer);
}
