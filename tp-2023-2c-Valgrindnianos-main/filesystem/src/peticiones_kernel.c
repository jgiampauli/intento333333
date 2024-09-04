#include "peticiones_kernel.h"

t_fcb* iniciar_fcb(t_config* config){

	// si no existe la config no hace nada
	if(config == NULL){
		log_error(logger, "El archivo ligado a la FCB que intentas abrir es erroneo o no existe");
		return NULL;
	}

	t_fcb* fcb = malloc(sizeof(t_fcb));

	fcb->nombre_archivo = strdup(config_get_string_value(config, "NOMBRE_ARCHIVO"));//duplico porque luego se libera el t_config
	fcb->tamanio_archivo = config_get_int_value(config, "TAMANIO_ARCHIVO");
	fcb->bloque_inicial = config_get_int_value(config, "BLOQUE_INICIAL");


	if(fcb->nombre_archivo==NULL ){
		log_error(logger, "El archivo ligado a la FCB que intentas abrir es erroneo o no existe");
		return NULL;
	}


	return fcb;
}


void abrir_archivo(int cliente_fd){

	//VARIABLES Y DATOS PARA LA FUNCION
	t_instruccion* instruccion = recibir_instruccion(cliente_fd);

	char* nombre_archivo = strdup(instruccion->parametros[0]);

	log_info(logger, "Apertura de Archivo: “Abrir Archivo: %s“", nombre_archivo);

	char* direccion_fcb = string_new();
	string_append(&direccion_fcb, path_fcb);
	string_append(&direccion_fcb, "/");
	string_append(&direccion_fcb, nombre_archivo);

	//CASO 1: La FCB esta en el diccionario (en memoria) => el archivo existe
	if(dictionary_has_key(fcb_por_archivo, nombre_archivo)){

		enviar_mensaje("OK", cliente_fd, ABRIR_ARCHIVO_RESPUESTA);

	//CASO 2: La FCB no esta en memoria pero si esta en el sistema
	}else {
		t_config* config_FCB = config_create(direccion_fcb);

		//CASO A: La FCB existe => el archivo tambien
		if(config_FCB != NULL){
			//Levanto la fcb
			t_fcb* fcb = malloc(sizeof(t_fcb));
			fcb = iniciar_fcb(config_FCB);


			config_destroy(config_FCB);

			dictionary_put(fcb_por_archivo, nombre_archivo, fcb);

			enviar_mensaje("OK", cliente_fd, ABRIR_ARCHIVO_RESPUESTA);


		//CASO B: La FCB no existe => el archivo tampoco
		}else{

			log_info(logger, "El archivo %s no se encontro ", nombre_archivo);
			//Doy aviso a Kernel
			enviar_mensaje("-1", cliente_fd, MENSAJE);

		}

	}

}


t_fcb* crear_fcb( t_instruccion* instruccion, char* path){
	t_config* config = malloc(sizeof(t_config));

	config->path = strdup(path);
	config->properties = dictionary_create();

	config_set_value(config, "NOMBRE_ARCHIVO", string_duplicate(instruccion->parametros[0]));
	config_set_value(config, "TAMANIO_ARCHIVO", string_itoa(0));
	config_set_value(config, "BLOQUE_INICIAL", "");

	config_save_in_file(config, path);

	t_fcb* fcb = malloc(sizeof(t_fcb));

	fcb->nombre_archivo = string_duplicate(config_get_string_value(config, "NOMBRE_ARCHIVO"));
	fcb->tamanio_archivo = config_get_int_value(config, "TAMANIO_ARCHIVO");
	fcb->bloque_inicial = config_get_int_value(config, "BLOQUE_INICIAL");

	config_destroy(config);
	return fcb;
}


void crear_archivo(int cliente_fd){
	//Cargamos las estructuras necesarias
	t_instruccion* instruccion = recibir_instruccion(cliente_fd);

	char* nombre_archivo = strdup(instruccion->parametros[0]);

	log_info(logger, "Crear Archivo: “Crear Archivo: %s“", nombre_archivo);

	char* direccion_fcb = string_new();
	string_append(&direccion_fcb, path_fcb);
	string_append(&direccion_fcb, "/");
	string_append(&direccion_fcb, nombre_archivo);

	log_info(logger, "Guardando el fcb del archivo %s en %s", nombre_archivo, direccion_fcb);

	//creamos la fcb
	t_fcb* fcb = malloc(sizeof(t_fcb));
	fcb = crear_fcb(instruccion, direccion_fcb);

	//Cargo estructuras resantes
	dictionary_put(fcb_por_archivo, nombre_archivo, fcb);
	enviar_mensaje("OK", cliente_fd, CREAR_ARCHIVO_RESPUESTA);
}



//dado un tamanio en bytes, calcula cuantos bloques son
int calcular_cantidad_de_bloques(int tamanio_en_bytes){
	float numero_bloques_nuevo_aux = tamanio_en_bytes/(float)tam_bloque;

	double parte_fraccional, numero_bloques_nuevo;
	parte_fraccional = modf(numero_bloques_nuevo_aux, &numero_bloques_nuevo);


	if( parte_fraccional != 0)
		numero_bloques_nuevo=numero_bloques_nuevo+1;

	return (int)numero_bloques_nuevo;
}

void agregar_bloques(t_fcb* fcb_a_actualizar, int bloques_a_agregar){

	//Asigno a posicion_fat el bloque incial y recorro la fat hasta encontrar el final (UINT32_MAX)
	uint32_t posicion_fat = fcb_a_actualizar->bloque_inicial;

	if(posicion_fat == 0){
		posicion_fat = primer_bloque_fat;

		while(bits_fat[posicion_fat] != 0){ //Bucle auxiliar para encontrar un bloque libre
			esperar_por_fs(retardo_acceso_fat);
			log_info(logger, "Acceso a FAT: “Acceso FAT buscando bloque libre: - Entrada: %d - Valor: %d“", posicion_fat, bits_fat[posicion_fat]);
			posicion_fat ++;
		}

		fcb_a_actualizar->bloque_inicial= posicion_fat;
		esperar_por_fs(retardo_acceso_fat);
		bits_fat[posicion_fat]= UINT32_MAX;
	}

	while(bits_fat[posicion_fat] != UINT32_MAX){
		esperar_por_fs(retardo_acceso_fat);
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", posicion_fat, bits_fat[posicion_fat]);
		posicion_fat = bits_fat[posicion_fat];
	}

	//Creo un auxiliar para contar los bloques que me quedan por agregar y los voy agregando
	int bloques_restantes_por_agregar = bloques_a_agregar;
	uint32_t num_bloque_anterior = posicion_fat;

	//Inicio el bucle para agregar bloques
	while(bloques_restantes_por_agregar != 0){
		uint32_t aux_busqueda = primer_bloque_fat; //Voy a utilizar este auxiliar para recorrer la fat y encontrar los bloques libres

		//busco un bloque libre
		while(bits_fat[aux_busqueda] != 0){ //Bucle auxiliar para encontrar un bloque libre
			esperar_por_fs(retardo_acceso_fat);
			log_info(logger, "Acceso a FAT: “Acceso a FAT buscando bloque libre: - Entrada: %d - Valor: %d“", aux_busqueda, bits_fat[aux_busqueda]);
			aux_busqueda ++;
		}
		//Encontrado el bloque libre al anterior le asigno el encontrado en la fat (donde antes estaba el uint32_max )
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", num_bloque_anterior, bits_fat[num_bloque_anterior]);
		esperar_por_fs(retardo_acceso_fat);
		bits_fat[num_bloque_anterior] = aux_busqueda;
		bloques_restantes_por_agregar --;

		// y al encontrado un uint32_max por si es el ulimo, sino en la proxima iteracion del while lo cambia por el numero al siguiente bloque
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", aux_busqueda, bits_fat[aux_busqueda]);
		esperar_por_fs(retardo_acceso_fat);
		bits_fat[aux_busqueda] = UINT32_MAX;
		num_bloque_anterior=aux_busqueda;

	}

}

void sacar_bloques(t_fcb* fcb_a_actualizar, int bloques_a_sacar, int bloques_actual){

	//Creo un bucle que se repite la cantidad de veces que tenga que eliminar bloques
	uint32_t *punteros_en_orden = malloc(sizeof(uint32_t)* bloques_actual);
	punteros_en_orden[0] = fcb_a_actualizar->bloque_inicial;

	//Al ingresar al bucle inicializo una variable auxiliar para buscar el final del archivo y una con la posicion inicial de la fat;
	uint32_t posicion_fat = fcb_a_actualizar->bloque_inicial;

	for(int i = 1; i<bloques_actual+1; i++){
		esperar_por_fs(retardo_acceso_fat);
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", posicion_fat, bits_fat[posicion_fat]);
		punteros_en_orden[i]= bits_fat[posicion_fat];
		posicion_fat++;
	}

	int num_ultimo_bloque = bloques_actual;
	int bloques_a_sacar_inicial = bloques_a_sacar;
	while(bloques_a_sacar != 0){
		esperar_por_fs(retardo_acceso_fat);
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", punteros_en_orden[num_ultimo_bloque], bits_fat[punteros_en_orden[num_ultimo_bloque]]);
		bits_fat[punteros_en_orden[num_ultimo_bloque]] = 0;
		esperar_por_fs(retardo_acceso_fat);
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", punteros_en_orden[num_ultimo_bloque-1], bits_fat[punteros_en_orden[num_ultimo_bloque-1]]);
		bits_fat[punteros_en_orden[num_ultimo_bloque-1]] = UINT32_MAX;


		//Decremento las variables de bloques a sacar  bloques actuales para ir actualizando los bucles
		bloques_a_sacar --;
		num_ultimo_bloque--;
	}

	//saco el ultimo bloque para que tenga 0 si debo sacarle todos
	if(bloques_a_sacar_inicial == bloques_actual){
		log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", punteros_en_orden[num_ultimo_bloque], bits_fat[punteros_en_orden[num_ultimo_bloque]]);
		esperar_por_fs(retardo_acceso_fat);
		bits_fat[punteros_en_orden[num_ultimo_bloque]] = 0;

		fcb_a_actualizar->bloque_inicial = -1;
	}


	free(punteros_en_orden);
}

void truncar_archivo(int cliente_fd){

		t_instruccion* instruccion_peticion = (t_instruccion*) recibir_instruccion(cliente_fd);

		char* nombre_archivo = strdup(instruccion_peticion->parametros[0]);
		int nuevo_tamano_archivo = atoi(instruccion_peticion->parametros[1]);


		log_info(logger, "Truncate de Archivo: “Truncar Archivo: %s - Tamaño: %d“", nombre_archivo, nuevo_tamano_archivo);
		t_fcb* fcb_a_truncar = dictionary_get(fcb_por_archivo, nombre_archivo);



		  //calculo de cantidad de bloques a truncar
	    int numero_de_bloques_nuevo = calcular_cantidad_de_bloques(nuevo_tamano_archivo);

	    //calculo de cantidad de bloques actuales (antes del truncado :D)
	    int numero_de_bloques_actual = calcular_cantidad_de_bloques(fcb_a_truncar->tamanio_archivo);

		log_info(logger, "Bloques actual %d vs Bloque nuevo %d", numero_de_bloques_actual, numero_de_bloques_nuevo);

	    if(numero_de_bloques_nuevo > numero_de_bloques_actual){
		   //ampliar tamanio
	    	int bloques_a_agregar = numero_de_bloques_nuevo - numero_de_bloques_actual;

	    	agregar_bloques(fcb_a_truncar, bloques_a_agregar);


	    } else if(numero_de_bloques_nuevo < numero_de_bloques_actual) {
		   //reducir tamanio

		  int bloques_a_sacar = numero_de_bloques_actual - numero_de_bloques_nuevo;
		  sacar_bloques(fcb_a_truncar, bloques_a_sacar, numero_de_bloques_actual);

	   }

	    // actualizo tamanio_fcb de memoria y de el archivo config
	   fcb_a_truncar->tamanio_archivo = nuevo_tamano_archivo;



	 	char* direccion_fcb = string_new();
		string_append(&direccion_fcb, path_fcb);
		string_append(&direccion_fcb, "/");
		string_append(&direccion_fcb, fcb_a_truncar->nombre_archivo);

	   	t_config* archivo_fcb = config_create(direccion_fcb);

		if(archivo_fcb == NULL){
			log_error(logger, "No se encontro el fcb del archivo %s", fcb_a_truncar->nombre_archivo);
			enviar_mensaje("ERROR", cliente_fd, TRUNCAR_ARCHIVO_RESPUESTA);
			return;

		}


		config_set_value(archivo_fcb, "TAMANIO_ARCHIVO", string_itoa(fcb_a_truncar->tamanio_archivo));
		config_set_value(archivo_fcb, "BLOQUE_INICIAL", string_itoa(fcb_a_truncar->bloque_inicial));

		config_save(archivo_fcb );


	   // respondo a kernel un OK
		enviar_mensaje("OK", cliente_fd, TRUNCAR_ARCHIVO_RESPUESTA);

		free(nombre_archivo);
}


void escribir_archivo_fs(int cliente_fd){

	int size;
	void * buffer = recibir_buffer(&size ,cliente_fd);
	int pid, puntero, desplazamiento  = 0;
	t_instruccion* instruccion ;

	memcpy(&pid, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	instruccion = deserializar_instruccion_en(buffer, &desplazamiento);
	memcpy(&puntero, buffer+desplazamiento, sizeof(int));

	char* nombre_archivo = strdup(instruccion->parametros[0]);

	log_info(logger, "Escritura de Archivo: “Escribir Archivo: %s - Puntero: %d - Memoria: %s”",nombre_archivo, puntero, instruccion->parametros[1]);

	t_fcb *fcb = dictionary_get(fcb_por_archivo, nombre_archivo);

	int result_conexion_memoria = conectar_memoria(ip_memoria, puerto_memoria);

	if (result_conexion_memoria == -1) {
		log_error(logger, "No se pudo conectar con el modulo Memoria !!");
		return;
	}

	t_paquete *paquete = crear_paquete(READ_MEMORY);
	agregar_a_paquete_sin_agregar_tamanio(paquete, &pid, sizeof(int));
	agregar_a_paquete(paquete, instruccion->opcode,instruccion->opcode_lenght);
	agregar_a_paquete(paquete, instruccion->parametros[0],instruccion->parametro1_lenght);
	agregar_a_paquete(paquete, instruccion->parametros[1],instruccion->parametro2_lenght);
	agregar_a_paquete(paquete, instruccion->parametros[2],instruccion->parametro3_lenght);
	enviar_paquete(paquete, socket_memoria);

	op_code cod_op = recibir_operacion(socket_memoria);

	if(cod_op != READ_MEMORY_RESPUESTA){
		log_error(logger, "No se pudo leer el contenido del archivo en la memoria");
		return;
	}
	int size_recibido;
	void* buffer_recibido = recibir_buffer(&size_recibido, socket_memoria);

	void * contenido_a_escribir = malloc(tam_bloque);
	memcpy(contenido_a_escribir, buffer_recibido, tam_bloque);

	if(dictionary_has_key(fcb_por_archivo, nombre_archivo)){

		int bloque_a_escribir = puntero/tam_bloque;
		int i = 0;
		int aux_busqueda_fat = fcb->bloque_inicial;

		while(i < bloque_a_escribir){
			esperar_por_fs(retardo_acceso_fat);
			log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", aux_busqueda_fat, bits_fat[aux_busqueda_fat]);
			aux_busqueda_fat = bits_fat[aux_busqueda_fat];
			i++;
		}
		log_info(logger, "Acceso a Bloque Archivo: “Acceso a bloque - Archivo: %s - Bloque archivo: %d - Bloque FS: %d“", nombre_archivo, bloque_a_escribir, aux_busqueda_fat);

		esperar_por_fs(retardo_acceso_bloque);
		memcpy(array_bloques[aux_busqueda_fat], contenido_a_escribir, tam_bloque);

		log_info(logger, "Archivo escrito: %s - Puntero: %d - Memoria: %s", nombre_archivo, puntero, instruccion->parametros[1]);

		enviar_mensaje("OK", cliente_fd, ESCRIBIR_ARCHIVO_RESPUESTA);

	}else{
		log_error(logger, "El archivo no se encuentra en el FS");
	}


	free(nombre_archivo);
	instruccion_destroy(instruccion);
	close(socket_memoria);
	free(buffer);
}

/*Esta operación leerá la información correspondiente al bloque a partir del puntero.
La información se deberá enviar al módulo Memoria para ser escrita a partir de la dirección física
recibida por parámetro, una vez recibida la confirmación por parte del módulo Memoria, se informará al
 módulo Kernel del éxito de la operación.*/
void leer_archivo_fs(int cliente_fd){
	int size;
	void * buffer = recibir_buffer(&size ,cliente_fd);
	int puntero, pid, desplazamiento = 0 ;

	t_instruccion* instruccion ;
	memcpy(&pid, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);
	instruccion = deserializar_instruccion_en(buffer, &desplazamiento);
	memcpy(&puntero, buffer+desplazamiento, sizeof(int));

	char* nombre_archivo = strdup(instruccion->parametros[0]);

	log_info(logger, "Lectura de Archivo: “Leer Archivo: %s - Puntero: %d - Memoria: %s”", nombre_archivo, puntero, instruccion->parametros[1]);

	t_fcb *fcb = dictionary_get(fcb_por_archivo, nombre_archivo);

	if(dictionary_has_key(fcb_por_archivo, nombre_archivo)){

		int bloque_a_leer = puntero/tam_bloque;
		int i = 0;
		int aux_busqueda_fat = fcb->bloque_inicial;

		while(i < bloque_a_leer){
			log_info(logger, "Acceso a FAT: “Acceso a FAT - Entrada: %d - Valor: %d“", aux_busqueda_fat, bits_fat[aux_busqueda_fat]);
			esperar_por_fs(retardo_acceso_fat);
			aux_busqueda_fat = bits_fat[aux_busqueda_fat];
			i++;
		}

		log_info(logger, "Acceso a Bloque Archivo: “Acceso a bloque - Archivo: %s - Bloque archivo: %d - Bloque FS: %d“", nombre_archivo, bloque_a_leer, aux_busqueda_fat);
		esperar_por_fs(retardo_acceso_bloque);
		void *contenido_leido = array_bloques[aux_busqueda_fat];

		char* aux_direccion_fisica = strdup(instruccion->parametros[0]);
		instruccion->parametros[0] = strdup(instruccion->parametros[1]);
		instruccion->parametros[1] = aux_direccion_fisica;

		int aux_len_direccion_fisica = instruccion->parametro1_lenght;
		instruccion->parametro1_lenght = instruccion->parametro2_lenght;
		instruccion->parametro2_lenght = aux_len_direccion_fisica;

		int result_conexion_memoria = conectar_memoria(ip_memoria, puerto_memoria);

		if (result_conexion_memoria == -1) {
			log_error(logger, "No se pudo conectar con el modulo Memoria !!");
			return;
		}

		t_paquete *paquete = crear_paquete(WRITE_MEMORY_FS);
		agregar_a_paquete_sin_agregar_tamanio(paquete, &pid, sizeof(int));
		agregar_a_paquete(paquete, instruccion->opcode,instruccion->opcode_lenght);
		agregar_a_paquete(paquete, instruccion->parametros[0],instruccion->parametro1_lenght);
		agregar_a_paquete(paquete, instruccion->parametros[1],instruccion->parametro2_lenght);
		agregar_a_paquete(paquete, instruccion->parametros[2],instruccion->parametro3_lenght);
		agregar_a_paquete_sin_agregar_tamanio(paquete, contenido_leido, tam_bloque);
		enviar_paquete(paquete, socket_memoria);

		eliminar_paquete(paquete);

		log_info(logger, "Archivo leido: %s - Puntero: %d - Memoria: %s", nombre_archivo, puntero, instruccion->parametros[1]);

		op_code cod_op = recibir_operacion(socket_memoria);

		if(cod_op != WRITE_MEMORY_FS_RESPUESTA){
			log_error(logger, "No se pudo escribir el contenido del archivo en la memoria");
			return;
		}

		char* mensaje = recibir_mensaje(socket_memoria);
		if(strcmp(mensaje,"OK") == 0){
			log_info(logger, "se recibio un  %s de memoria", mensaje);

			enviar_mensaje(mensaje, cliente_fd, LEER_ARCHIVO_RESPUESTA);
			free(mensaje);
		}

	}else{
		log_error(logger, "El archivo no se encuentra en el FS");
	}


	free(nombre_archivo);
	free(buffer);
	instruccion_destroy(instruccion);
	close(socket_memoria);
}


