#include "cpu_instrucciones.h"

int tamano_pagina, socket_memoria;

void setear_registro(t_contexto_ejec** contexto, char* registro, uint32_t valor);
uint32_t obtener_valor_del_registro(char* registro_a_leer, t_contexto_ejec** contexto_actual);
void guardar_valor_en(int direccion_fisica, uint32_t valor_a_guardar,  int pid);
uint32_t leer_valor(int direccion_fisica,  int pid);
int traducir_direccion_logica(int direccion_logica, int pid, t_contexto_ejec* contexto);

void manejar_instruccion_set(t_contexto_ejec** contexto, t_instruccion* instruccion)
{
	char* registro = strdup(instruccion->parametros[0]);
	uint32_t valor = atoi(instruccion->parametros[1]);
	setear_registro(contexto, registro, valor);

	free(registro);
}

void setear_registro(t_contexto_ejec** contexto, char* registro, uint32_t valor)
{
	if(strcmp(registro,"AX")==0)
	{
		(*contexto)->registros_CPU->AX=valor;
		log_info(logger, "Se setea en %s el valor: %d",registro,  (*contexto)->registros_CPU->AX);

	}else if(strcmp(registro,"BX")==0)
	{
		(*contexto)->registros_CPU->BX=valor;
		log_info(logger, "Se setea en %s el valor: %d",registro,  (*contexto)->registros_CPU->BX);
	}else if(strcmp(registro,"CX")==0)
	{
		(*contexto)->registros_CPU->CX=valor;
		log_info(logger, "Se setea en %s el valor: %d",registro,  (*contexto)->registros_CPU->CX);
	}else if(strcmp(registro,"DX")==0)
	{
		(*contexto)->registros_CPU->DX=valor;
		log_info(logger, "Se setea en %s el valor: %d",registro,  (*contexto)->registros_CPU->DX);
	}

}

void manejar_instruccion_sum(t_contexto_ejec** contexto_actual,t_instruccion* instruccion)
{
	 char* registro_destino = strdup(instruccion->parametros[0]);
	 char* registro_origen = strdup(instruccion->parametros[1]);

	 int valor_destino = obtener_valor_del_registro(registro_destino, contexto_actual);
	 int valor_origen = obtener_valor_del_registro(registro_origen, contexto_actual);

	 valor_destino=valor_destino+valor_origen;

	 setear_registro(contexto_actual, registro_destino, valor_destino);
	 free(registro_origen);
	 free(registro_destino);
}

void manejar_instruccion_sub(t_contexto_ejec** contexto_actual,t_instruccion* instruccion)
{
	 char* registro_destino = strdup(instruccion->parametros[0]);
	 char* registro_origen = strdup(instruccion->parametros[1]);

	 int valor_destino = obtener_valor_del_registro(registro_destino, contexto_actual);
	 int valor_origen = obtener_valor_del_registro(registro_origen, contexto_actual);

	 valor_destino=valor_destino-valor_origen;

	 setear_registro(contexto_actual, registro_destino, valor_destino);
	 free(registro_origen);
	 free(registro_destino);
}

void manejar_instruccion_jnz(t_contexto_ejec** contexto_actual, t_instruccion* instruccion){
	int numero_instruccion = atoi(instruccion->parametros[1]);
	char* registro_a_evaluar = strdup(instruccion->parametros[0]);

	if(obtener_valor_del_registro(registro_a_evaluar, contexto_actual) != 0){
		(*contexto_actual)->program_counter = numero_instruccion;
	}

	free(registro_a_evaluar);
}

void  manejar_mov_in(t_contexto_ejec** contexto_actual, t_instruccion*  instruccion){
	char* registro = string_duplicate(instruccion->parametros[0]);
	int direccion_fisica = atoi(instruccion->parametros[1]);

	uint32_t valor_a_guardar = leer_valor(direccion_fisica, (*contexto_actual)->pid);

	if(valor_a_guardar == -1){
		free(registro);
		return;
	}

	setear_registro(contexto_actual, registro, valor_a_guardar);

	free(registro);
}

void manejar_mov_out(t_contexto_ejec** contexto_actual, t_instruccion*  instruccion){
	int direccion_fisica = atoi(instruccion->parametros[0]);
	char* registro = string_duplicate(instruccion->parametros[1]);

	uint32_t valor_a_guardar = obtener_valor_del_registro(registro, contexto_actual);

	if(valor_a_guardar == -1){
		log_error(logger, "Error al obtener valor del registro %s ya que no existe un registro con ese nombre", registro);
		free(registro);
		return ;
	}

	guardar_valor_en(direccion_fisica, valor_a_guardar, (*contexto_actual)->pid);

	free(registro);
}

uint32_t obtener_valor_del_registro(char* registro_a_leer, t_contexto_ejec** contexto_actual){
	uint32_t valor_leido = -1; //valor devuelto si se escribe mal el nombre del registro

	if(strcmp(registro_a_leer,"AX")==0)
	{
		valor_leido= (*contexto_actual)->registros_CPU->AX;

	}else if(strcmp(registro_a_leer,"BX")==0)
	{

		valor_leido= (*contexto_actual)->registros_CPU->BX;

	}else if(strcmp(registro_a_leer,"CX")==0)
	{

		valor_leido= (*contexto_actual)->registros_CPU->CX;

	}else if(strcmp(registro_a_leer,"DX")==0)
	{
		valor_leido= (*contexto_actual)->registros_CPU->DX;
	}

	log_info(logger, "Se se lee en %s el valor: %d",registro_a_leer,  valor_leido);

	return valor_leido;
}

int solicitar_marco(int numero_pagina, int pid){
	t_paquete* paquete = crear_paquete(ACCESO_A_PAGINA);

	agregar_a_paquete_sin_agregar_tamanio(paquete, &numero_pagina, sizeof(int));
	agregar_a_paquete_sin_agregar_tamanio(paquete, &pid, sizeof(int));

	enviar_paquete(paquete, socket_memoria);
	eliminar_paquete(paquete);

	int size;
	int marco_pagina;

	op_code cod_op = recibir_operacion(socket_memoria);


	if(cod_op == ACCESO_A_PAGINA){
		void* buffer = recibir_buffer(&size, socket_memoria);
		memcpy(&marco_pagina, buffer, sizeof(int));

		free(buffer);
		return marco_pagina;

	} else if(cod_op == PAGE_FAULT) { // sino significaria page fault

		char* mensaje = recibir_mensaje(socket_memoria);
		log_info(logger, "Se recibio: “%s“ de memoria", mensaje);
		free(mensaje);
		return -1;
	} else {
		log_error(logger, "Codigo de operacion inesperado de memoria: %d", cod_op);
		return -1;
	}
}

int traducir_direccion_logica(int direccion_logica, int pid, t_contexto_ejec* contexto){

	int numero_pagina = (int) floor(direccion_logica / tamano_pagina);

	int desplazamiento = direccion_logica - numero_pagina * tamano_pagina;

	int marco_pagina = solicitar_marco(numero_pagina,pid);

	if(marco_pagina == -1){
		log_info(logger, "Page Fault: “Page Fault PID: %d - Página: %d“", pid, numero_pagina);
		// AVISO A KERNEL PARA QUE MANEJE EL PAGE fAULT

		t_paquete *paquete_contexto = crear_paquete(PAGE_FAULT);


		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto, &numero_pagina,sizeof(int));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto, &(pid),sizeof(int));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto,&(contexto->program_counter), sizeof(int));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto,&(contexto->registros_CPU->AX), sizeof(uint32_t));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto,&(contexto->registros_CPU->BX), sizeof(uint32_t));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto,&(contexto->registros_CPU->CX), sizeof(uint32_t));
		agregar_a_paquete_sin_agregar_tamanio(paquete_contexto,&(contexto->registros_CPU->DX), sizeof(uint32_t));
		agregar_a_paquete(paquete_contexto, contexto->instruccion->opcode,contexto->instruccion->opcode_lenght);
		agregar_a_paquete(paquete_contexto, contexto->instruccion->parametros[0],contexto->instruccion->parametro1_lenght);
		agregar_a_paquete(paquete_contexto, contexto->instruccion->parametros[1],contexto->instruccion->parametro2_lenght);
		agregar_a_paquete(paquete_contexto, contexto->instruccion->parametros[2],contexto->instruccion->parametro3_lenght);

		enviar_paquete(paquete_contexto, socket_kernel_client_fd);

		eliminar_paquete(paquete_contexto);

		return -1; // page fault
	} else {
		log_info(logger, "Obtener Marco: “PID: %d - OBTENER MARCO - Página: %d - Marco: %d“", pid, numero_pagina, marco_pagina);
	}

	return desplazamiento + marco_pagina * tamano_pagina;
}

bool decodificar_direccion_logica(t_contexto_ejec** contexto_acutal, int posicion_parametro_direccion_logica){
	int direccion_logica = atoi((*contexto_acutal)->instruccion->parametros[posicion_parametro_direccion_logica]);
	free((*contexto_acutal)->instruccion->parametros[posicion_parametro_direccion_logica]);
	if(posicion_parametro_direccion_logica == 0){
		(*contexto_acutal)->instruccion->parametro1_lenght = 0;
	} else if(posicion_parametro_direccion_logica == 1){
		(*contexto_acutal)->instruccion->parametro2_lenght = 0;
	}

	int direccion_fisica = traducir_direccion_logica(direccion_logica, (*contexto_acutal)->pid, *contexto_acutal);

	if(direccion_fisica == -1){
		return true;
	}

	char *direccion_fisica_string = string_itoa(direccion_fisica);

	int tam_direccion_fisica_string = strlen(direccion_fisica_string) + 1;
	(*contexto_acutal)->instruccion->parametros[posicion_parametro_direccion_logica] = malloc(sizeof(char) * tam_direccion_fisica_string);
	strcpy((*contexto_acutal)->instruccion->parametros[posicion_parametro_direccion_logica], direccion_fisica_string);

	if(posicion_parametro_direccion_logica == 0){
		(*contexto_acutal)->instruccion->parametro1_lenght = tam_direccion_fisica_string;
	} else if(posicion_parametro_direccion_logica == 1){
		(*contexto_acutal)->instruccion->parametro2_lenght = tam_direccion_fisica_string;
	}

	free(direccion_fisica_string);
	return false;
}

void guardar_valor_en(int direccion_fisica, uint32_t valor_a_guardar, int pid){
	t_paquete* paquete_a_enviar = crear_paquete(WRITE_MEMORY);

	t_instruccion* instruccion = malloc(sizeof(t_instruccion));

	//esto no se usa del otro lado, pero hay que hacerlo para evitar errores
	instruccion->opcode = 	string_new();
	string_append(&(instruccion->opcode), "MOV_OUT");
	instruccion->opcode_lenght = strlen(instruccion->opcode) +1;

	instruccion->parametros[0] = string_itoa(direccion_fisica);
	instruccion->parametro1_lenght = strlen(instruccion->parametros[0]) +1;

	char *bytes_a_escribir = string_itoa(sizeof(valor_a_guardar));
	instruccion->parametro2_lenght = strlen(bytes_a_escribir) + 1;
	instruccion->parametros[1] = bytes_a_escribir;

	instruccion->parametros[2] = NULL ;
	instruccion->parametro3_lenght = 0;

	agregar_a_paquete_sin_agregar_tamanio(paquete_a_enviar, &pid, sizeof(int));
	agregar_a_paquete(paquete_a_enviar, instruccion->opcode, instruccion->opcode_lenght );

	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[0], instruccion->parametro1_lenght);
	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[1], instruccion->parametro2_lenght);
	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[2], instruccion->parametro3_lenght);

	agregar_a_paquete_sin_agregar_tamanio(paquete_a_enviar, &valor_a_guardar, sizeof(valor_a_guardar));


	enviar_paquete(paquete_a_enviar, socket_memoria);

	instruccion_destroy(instruccion);
	eliminar_paquete(paquete_a_enviar);

	int cod_op = recibir_operacion(socket_memoria);

	if(cod_op == WRITE_MEMORY_RESPUESTA){

		// recibo el OK de memoria
		char* mensaje = recibir_mensaje(socket_memoria);

		if(strcmp(mensaje,"OK") == 0){
			log_info(logger,"Escritura Memoria: “PID: %d - Acción: ESCRIBIR - Dirección Física: %d - Valor: %d“", pid, direccion_fisica, valor_a_guardar);
		} else {
			// si esto se llega a ejecutar, debe haber algun error en memoria o aca o algo raro falla
			log_error(logger,"PID: %d - no se pudo escribir en memoria, esto no deberia pasar, poque recibi el mensaje: %s", pid, mensaje);
		}
		free(mensaje);
	} else {
		log_error(logger,"Se recibio cod_op: %d esto no deberia pasar", cod_op );
	}
}

uint32_t leer_valor(int direccion_fisica, int pid){
	t_paquete* paquete_a_enviar = crear_paquete(READ_MEMORY);

	t_instruccion* instruccion = malloc(sizeof(t_instruccion));

	//esto no se usa del otro lado, pero hay que hacerlo para evitar errores
	instruccion->opcode = 	string_new();
	string_append(&(instruccion->opcode), "MOV_IN");
	instruccion->opcode_lenght = strlen(instruccion->opcode) +1;

	instruccion->parametros[1] = string_itoa(direccion_fisica);
	instruccion->parametro2_lenght = strlen(instruccion->parametros[1]) +1;

	char *bytes_a_escribir = string_itoa(sizeof(uint32_t));
	instruccion->parametro1_lenght = strlen(bytes_a_escribir) + 1;
	instruccion->parametros[0] = bytes_a_escribir;

	instruccion->parametros[2] = NULL;
	instruccion->parametro3_lenght = 0;

	agregar_a_paquete_sin_agregar_tamanio(paquete_a_enviar, &pid, sizeof(int));
	agregar_a_paquete(paquete_a_enviar, instruccion->opcode, sizeof(char)*instruccion->opcode_lenght );

	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[0], instruccion->parametro1_lenght);
	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[1], instruccion->parametro2_lenght);
	agregar_a_paquete(paquete_a_enviar, instruccion->parametros[2], instruccion->parametro3_lenght);

	enviar_paquete(paquete_a_enviar, socket_memoria);

	instruccion_destroy(instruccion);
	eliminar_paquete(paquete_a_enviar);

	int cod_op = recibir_operacion(socket_memoria);

	void* valor_leido_sin_realizar;
	uint32_t valor_leido;

	if(cod_op == READ_MEMORY_RESPUESTA){
		// recibo el valor leido de memoria
		int size_recibido;
		valor_leido_sin_realizar = recibir_buffer(&size_recibido, socket_memoria);

		memcpy(&valor_leido, valor_leido_sin_realizar, sizeof(uint32_t));

		log_info(logger,"Lectura Memoria: “PID: %d - Acción: LEER - Dirección Física: %d - Valor: %d“", pid, direccion_fisica, valor_leido);

	} else {
		log_error(logger,"Se recibio cod_op: %d esto no deberia pasar", cod_op );

		return -1;
	}

	free(valor_leido_sin_realizar);
	return valor_leido;
}
