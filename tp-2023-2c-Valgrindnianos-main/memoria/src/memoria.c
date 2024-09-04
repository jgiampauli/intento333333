#include "memoria.h"

int socket_memoria;
void* espacio_usuario;
int tam_pagina;
char* path_instrucciones;
t_dictionary* paginas_por_PID;
t_list* situacion_marcos;
t_queue* referencias_paginas;
int retardo_respuesta;
char *ip_filesystem;
char *puerto_filesystem;


int main(int argc, char *argv[]) {

	//Declaraciones de variables para config:

	char *puerto_escucha;
	int tam_memoria;
	char* algoritmo_reemplazo;

	/*------------------------------LOGGER Y CONFIG--------------------------------------------------*/

	// Iniciar archivos de log y configuracion:
	t_config *config = iniciar_config();
	logger = iniciar_logger();

	// Verificacion de creacion archivo config
	if (config == NULL) {
		log_error(logger,
				"No fue posible iniciar el archivo de configuracion !!");
		terminar_programa(logger, config);
	}

	// Carga de datos de config en variable y archivo
	ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
	puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");

	puerto_escucha = config_get_string_value(config, "PUERTO_ESCUCHA");

	tam_memoria = config_get_int_value(config, "TAM_MEMORIA");
	tam_pagina = config_get_int_value(config, "TAM_PAGINA");

	path_instrucciones = config_get_string_value(config, "PATH_INSTRUCCIONES");
	retardo_respuesta = config_get_int_value(config, "RETARDO_RESPUESTA");

	algoritmo_reemplazo = config_get_string_value(config, "ALGORITMO_REEMPLAZO");

	// Control archivo configuracion
	if(!ip_filesystem || !puerto_filesystem || !puerto_escucha || !tam_memoria || !tam_pagina || !path_instrucciones || !retardo_respuesta || !algoritmo_reemplazo){

		log_error(logger,
				"Error al recibir los datos del archivo de configuracion de la memoria");
		terminar_programa(logger, config);
	}

	/*-------------------------------CONEXIONES MEMORIA---------------------------------------------------------------*/


	//Esperar conexion de FS
	socket_memoria = iniciar_servidor(puerto_escucha);

	log_info(logger, "La memoria esta lista para recibir peticiones");
	/*-------------------------------- CREAR STRUCTS -------------------------------*/

	//diccionario de paginas por PID
	paginas_por_PID=dictionary_create(); //----------------------------

	//lista de marcos y modificado
	situacion_marcos=list_create();

	int cantidad_marcos = tam_memoria / tam_pagina;

	for(int i =0; i<cantidad_marcos; i++){
		t_situacion_marco * marco_n = malloc(sizeof(t_situacion_marco));

		marco_n->numero_marco = i;
		marco_n->posicion_inicio_marco = i*tam_pagina;
		marco_n->esLibre = true;
		marco_n->pid=0;
		list_add(situacion_marcos, marco_n);
	}

	//espacio de usuario
	espacio_usuario = malloc(tam_memoria);
	lista_instrucciones_porPID=dictionary_create();

	//lista de referencias a las paginas
	referencias_paginas=queue_create();

	//manejo de peticiones
	manejar_pedidos_memoria(algoritmo_reemplazo,retardo_respuesta,tam_pagina,tam_memoria);

	terminar_programa(logger, config);
	return 0;
} //FIN DEL MAIN

//Iniciar archivo de log y de config

t_log* iniciar_logger(void) {
	t_log *nuevo_logger = log_create("memoria.log", "Memoria", true,
			LOG_LEVEL_INFO);
	return nuevo_logger;
}

t_config* iniciar_config(void) {
	t_config *nueva_config = config_create("memoria.config");
	return nueva_config;
}

//Finalizar el programa

void terminar_programa(t_log *logger, t_config *config) {
	log_destroy(logger);
	config_destroy(config);
	close(socket_memoria);
}

//conexiones
int conectar_fs(char *ip, char *puerto, int *socket_filesystem) {

	*socket_filesystem = crear_conexion(ip, puerto);

	//enviar handshake
	enviar_mensaje("OK", *socket_filesystem, HANDSHAKE);

	op_code cod_op = recibir_operacion(*socket_filesystem);

	if (cod_op != HANDSHAKE) {
		return -1;
	}

	int size;
	char *buffer = recibir_buffer(&size, *socket_filesystem);

	if (strcmp(buffer, "OK") != 0) {
		return -1;
	}

	return 0;
}

// atiende las peticiones de kernel, cpu y filesystem de forma concurrente
void manejar_pedidos_memoria(char* algoritmo_reemplazo,int retardo_respuesta,int tam_pagina,int tam_memoria){

	while (1) {
		pthread_t thread;
		uint64_t cliente_fd = (uint64_t) esperar_cliente(socket_memoria);

		t_arg_atender_cliente *argumentos_atender_cliente = malloc(
				sizeof(t_arg_atender_cliente));
		argumentos_atender_cliente->cliente_fd = cliente_fd;
		argumentos_atender_cliente->algoritmo_reemplazo = algoritmo_reemplazo;
		argumentos_atender_cliente->retardo_respuesta = retardo_respuesta;
		argumentos_atender_cliente->tam_pagina = tam_pagina;
		argumentos_atender_cliente->tam_memoria = tam_memoria;

		pthread_create(&thread, NULL, atender_cliente, (void*) argumentos_atender_cliente);

		pthread_detach(thread);
	}

}

// en el handshake con CPU le envia el tamanio de pagina para la traduccion de direcciones logicas
void enviar_tam_pagina(int socket_cliente){
	int size;
	char* buffer = recibir_buffer(&size, socket_cliente);

	if(strcmp(buffer, "OK") == 0){
		t_paquete* paquete = crear_paquete(HANDSHAKE_TAM_MEMORIA);
		agregar_a_paquete_sin_agregar_tamanio(paquete, &tam_pagina, sizeof(tam_pagina));

		enviar_paquete(paquete, socket_cliente);

		eliminar_paquete(paquete);
	} else {
		enviar_mensaje("ERROR", socket_cliente, HANDSHAKE_TAM_MEMORIA);
	}


	free(buffer);
}

//peticiones de kernel, cpu y filesystem
void* atender_cliente(void *args) {
	t_arg_atender_cliente *argumentos = (t_arg_atender_cliente*) args;

	uint64_t cliente_fd = argumentos->cliente_fd;
	char* algoritmo_reemplazo = argumentos->algoritmo_reemplazo;
	uint64_t retardo_respuesta = argumentos->retardo_respuesta;
	uint64_t tam_pagina = argumentos->tam_pagina;

	while (1) {
		int cod_op = recibir_operacion(cliente_fd);

		switch(cod_op){
			case HANDSHAKE:
				recibir_handshake(cliente_fd);
				break;
			case HANDSHAKE_TAM_MEMORIA:
				enviar_tam_pagina(cliente_fd);
				break;
			case INICIAR_PROCESO:
				 leer_pseudo(cliente_fd);
				break;
			case INSTRUCCION:
				enviar_instruccion_a_cpu(cliente_fd,retardo_respuesta);
				break;
			case CREAR_PROCESO:
				crear_proceso(cliente_fd);
				break;
			case FINALIZAR_PROCESO_MEMORIA:
				finalizar_proceso(cliente_fd);
				break;
			case ACCESO_A_PAGINA:
				devolver_marco(cliente_fd);
				break;
			case PAGE_FAULT:
				manejar_pagefault(algoritmo_reemplazo,cliente_fd,tam_pagina);
				break;
			case READ_MEMORY:
				read_memory(cliente_fd);
				break;
			case WRITE_MEMORY:
				write_memory(cliente_fd);
				break;
			case WRITE_MEMORY_FS:
				write_memory_fs(cliente_fd);
				break;
			case -1:
				log_error(logger, "El cliente se desconecto. Terminando servidor");
				return NULL;
			default:
				log_warning(logger, "Operacion desconocida. No quieras meter la pata");
				break;
		}
	}

	return NULL;
}


void limpiar_referencias_proceso(int pid){
	char* pid_string = string_itoa(pid);
	t_list* lista_referencias = referencias_paginas->elements;

	bool esDeEsteProceso(void*args){
		t_referenciaXpid* referencia = (t_referenciaXpid*)args;

		return strcmp(referencia->pid, pid_string) == 0;
	}

	void limpiar_ref(void* args){
		t_referenciaXpid* referencia = (t_referenciaXpid*)args;
		free(referencia->pid);
		free(referencia);
	}

	list_remove_and_destroy_all_by_condition(lista_referencias, esDeEsteProceso, limpiar_ref);
}

void finalizar_proceso(int cliente_fd){
	//recibe la orden de la consola de kernel
	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int pid;
	memcpy(&pid,buffer,sizeof(int));

	char* pid_string=string_itoa(pid);

	t_list* lista_de_marcos = dictionary_get(paginas_por_PID,pid_string);


	//creo array de punteros para enviar a FS
	int cant_paginas_proceso = list_size(lista_de_marcos);
	uint32_t punteros[cant_paginas_proceso];

	t_list_iterator* iterador_lista = list_iterator_create(lista_de_marcos);

	while(list_iterator_has_next(iterador_lista)){
		t_tabla_de_paginas* entrada_tabla_n = list_iterator_next(iterador_lista);

		punteros[list_iterator_index(iterador_lista)] = entrada_tabla_n->posicion_swap;
	}
	list_iterator_destroy(iterador_lista);


	//limpiar las referencias a este proceso
	limpiar_referencias_proceso(pid);

	//destruir TP
	log_info(logger,"Destrucción de Tabla de Páginas: “PID: %d - Tamaño: %d”",pid,cant_paginas_proceso);

	list_destroy_and_destroy_elements(lista_de_marcos, free);

	dictionary_remove(paginas_por_PID,pid_string);


	//marcar como libres a los marcos
	void marcarLibre (void* args){
		t_situacion_marco* marco_x= (t_situacion_marco*)args;

		if(marco_x->pid==pid){
			marco_x->esLibre=true;
		}
	}

	list_iterate(situacion_marcos,marcarLibre);

	int socket_filesystem;
	int result_conexion_fs = conectar_fs(ip_filesystem, puerto_filesystem, &socket_filesystem);

	if (result_conexion_fs == -1) {
		log_error(logger, "No se pudo conectar con el modulo Filesystem !!");
		return;
	}

	log_info(logger, "Memoria se conecto con el modulo Filesystem correctamente");

	//avisar a FS para que libere los bloques
	t_paquete* paquete = crear_paquete(FINALIZAR_PROCESO_FS);

	agregar_a_paquete_sin_agregar_tamanio(paquete, &cant_paginas_proceso, sizeof(int));
	for(int i= 0; i<cant_paginas_proceso; i++){
		agregar_a_paquete_sin_agregar_tamanio(paquete, &(punteros[i]), sizeof(uint32_t));
	}

	enviar_paquete(paquete, socket_filesystem);

	eliminar_paquete(paquete);
	close(socket_filesystem);
	free(buffer);
}

void crear_proceso(int cliente_fd){

	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int tamanio;
	int pid;
	memcpy(&tamanio,buffer,sizeof(int));
	memcpy(&pid,buffer+sizeof(int),sizeof(int));

	int socket_filesystem;
	int result_conexion_fs = conectar_fs(ip_filesystem, puerto_filesystem, &socket_filesystem);

		if (result_conexion_fs == -1) {
			log_error(logger, "No se pudo conectar con el modulo Filesystem !!");
			return;
		}

		log_info(logger, "Memoria se conecto con el modulo Filesystem correctamente");

	 t_paquete* paquete = crear_paquete(INICIAR_PROCESO);

	 agregar_a_paquete_sin_agregar_tamanio(paquete,&tamanio,sizeof(int));

	 enviar_paquete(paquete,socket_filesystem);

	 eliminar_paquete(paquete);

	 int cod_op = recibir_operacion(socket_filesystem);

	 if(cod_op!=INICIAR_PROCESO){
		 log_error(logger, "No se pudo recibir bloques asignados. Terminando servidor");
		 return;
	 }

	void* buffer_fs = recibir_buffer(&size, socket_filesystem);
	int desplazamiento = 0;
	int cant_paginas;
	t_list* lista_de_marcos_x_procesos=list_create();

	memcpy(&cant_paginas, buffer_fs, sizeof(int));
	desplazamiento+= sizeof(int);

	uint32_t punteros[cant_paginas];

	for(int i = 0; i<cant_paginas; i++){
		memcpy(&(punteros[i]), buffer_fs+desplazamiento, sizeof(uint32_t));
		desplazamiento+=sizeof(uint32_t);

		//crea una entrada por cada pagina y coloca su posicion en swap correspondiente para cada pagina
		 t_tabla_de_paginas* tabla_por_proceso = malloc(sizeof(t_tabla_de_paginas));

		 //cuando ocurra un PF se modificara este marco
		 tabla_por_proceso->marco = 0;
		 tabla_por_proceso->presencia = false;
		 tabla_por_proceso->modificado = false;
		 tabla_por_proceso->posicion_swap = punteros[i];

		 list_add(lista_de_marcos_x_procesos,tabla_por_proceso);
	}

	log_info(logger,"Creación de Tabla de Páginas: “PID: %d - Tamaño: %d”",pid,cant_paginas);

	dictionary_put(paginas_por_PID,string_itoa(pid),lista_de_marcos_x_procesos);

	enviar_mensaje("OK", cliente_fd, CREAR_PROCESO);
	close(socket_filesystem);
	free(buffer);
	free(buffer_fs);
}

void devolver_marco(int cliente_fd){

	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int numero_pagina;
	int pid_entero;

		//recibo la pagina y pid
	memcpy(&numero_pagina,buffer,sizeof(int));
	memcpy(&pid_entero,buffer+sizeof(int),sizeof(int));

	//obtengo el valor
	char*pid_string=string_itoa(pid_entero);

	//creo y guardo la referencia a la pagina
	t_referenciaXpid* pagina_referenciada = malloc(sizeof(t_referenciaXpid));
	pagina_referenciada->numero_pagina = numero_pagina;
	pagina_referenciada->pid = strdup(pid_string);

	queue_push(referencias_paginas, pagina_referenciada);



	//busco en la tabla si esta presente o no
	t_list* lista_de_marcos = dictionary_get(paginas_por_PID,pid_string);

	t_tabla_de_paginas* entrada_de_pagina =list_get(lista_de_marcos,numero_pagina);

	//a cpu  por ACCESO_A_PAGINA
	if(entrada_de_pagina->presencia){
		t_paquete* paquete=crear_paquete(ACCESO_A_PAGINA);
		agregar_a_paquete_sin_agregar_tamanio(paquete,&(entrada_de_pagina->marco),sizeof(int));

		//log obligatorio solo cuando se encuentra
		log_info(logger,"Acceso a Tabla de Páginas: “PID: %d - Pagina: %d - Marco: %d”",pid_entero,numero_pagina,entrada_de_pagina->marco);


		esperar_por(retardo_respuesta);

		enviar_paquete(paquete,cliente_fd);
	}
	else{
		esperar_por(retardo_respuesta);

		enviar_mensaje("No se encontro en memoria el marco buscado :(",cliente_fd,PAGE_FAULT);

	}



	free(buffer);

}

t_situacion_marco* obtener_situacion_marco(int numero_marco){
	bool esMarco_busado(void *args){
		t_situacion_marco* situacion_marco = (t_situacion_marco*) args;

		return situacion_marco->numero_marco == numero_marco;
	}

	return list_find(situacion_marcos, esMarco_busado);
}

void *obtener_contenido_de_marco(int numero_marco, int pid){
	void* contenido = malloc(tam_pagina);

	t_situacion_marco* situacion_marco = obtener_situacion_marco(numero_marco);
	log_info(logger,"Acceso a espacio de usuario: “PID: %d - Accion: LEER - Direccion fisica Marco: %d“",pid,situacion_marco->posicion_inicio_marco);

	memcpy(contenido, espacio_usuario+situacion_marco->posicion_inicio_marco, tam_pagina);
	return contenido;
}

void manejar_pagefault(char* algoritmo_reemplazo,int cliente_fd,int tam_pagina){

	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int numero_pagina;
	int pid;
	//recibo la pagina y pid
	memcpy(&numero_pagina,buffer,sizeof(int));
	memcpy(&pid,buffer+sizeof(int),sizeof(int));

	t_list* paginas_del_proceso =dictionary_get(paginas_por_PID,string_itoa(pid));
	t_tabla_de_paginas* pagina_a_actualizar =list_get(paginas_del_proceso,numero_pagina);
	int socket_filesystem;
	int result_conexion_fs = conectar_fs(ip_filesystem, puerto_filesystem, &socket_filesystem);

		if (result_conexion_fs == -1) {
			log_error(logger, "No se pudo conectar con el modulo Filesystem !!");
			return;
		}

	log_info(logger, "Memoria se conecto con el modulo Filesystem correctamente");

	 t_paquete* paquete = crear_paquete(LEER_CONTENIDO_PAGINA);

	 agregar_a_paquete_sin_agregar_tamanio(paquete,&(pagina_a_actualizar->posicion_swap),sizeof(int));

	 enviar_paquete(paquete,socket_filesystem);

	 eliminar_paquete(paquete);

	 int cod_op = recibir_operacion(socket_filesystem);

	 if(cod_op!=LEER_CONTENIDO_PAGINA){
		 log_error(logger, "No se pudo recibir el contenido del bloque. Terminando servidor ");
		 return;
	 }

	 void* contenido_bloque = recibir_buffer(&size, socket_filesystem);

	if(memoria_llena()){
		log_info(logger, "memoria llena, proceso a reemplazar por algun marco");//TODO borrar log
		int numero_marco;

		if(strcmp(algoritmo_reemplazo,"FIFO")==0){
			numero_marco=aplicarFifo();

		}else{
			numero_marco=aplicarLru();
		}

		log_info(logger, "Marco seleccionado para reemplazo: %d", numero_marco);//TODO borrar log

		t_tabla_de_paginas* marco;
		void esMarco(char* pid, void* marcos){

			t_list* lista_marcos=marcos;
			bool esMarcoBuscado(void* args){
				t_tabla_de_paginas* marco_x =(t_tabla_de_paginas*)args;
				return numero_marco==marco_x->marco;
			}

			t_tabla_de_paginas* marco_buscado=list_find(lista_marcos,esMarcoBuscado);
			if((marco_buscado!=NULL) && (marco_buscado->presencia==true) ){
				log_info(logger, "Encontre la entrada de la TP del marco %d", marco_buscado->marco);//TODO borrar log
				marco=marco_buscado;
			}

		}

		dictionary_iterator(paginas_por_PID,esMarco);

		t_situacion_marco* marco_a_guardar = obtener_situacion_marco(marco->marco);

		int pagina_a_reemplazar=obtener_pagina_a_reemplazar(marco->marco,marco_a_guardar->pid);

		if(marco->modificado)
		{
			//actualiza en el bloque de swap la pagina modificada de memoria

			log_info(logger,"Escritura de Página en SWAP: “SWAP OUT -  PID: %d - Marco: %d - Page Out: %d-%d”",pid,marco->marco,marco_a_guardar->pid,pagina_a_reemplazar);


			void *contenido_acutualizado = obtener_contenido_de_marco(marco->marco, pid);
			t_paquete* paquete = crear_paquete(ESCRIBIR_CONTENIDO_PAGINA);
			agregar_a_paquete_sin_agregar_tamanio(paquete, &(marco->posicion_swap), sizeof(uint32_t));
			agregar_a_paquete_sin_agregar_tamanio(paquete, contenido_acutualizado, tam_pagina);

			enviar_paquete(paquete, socket_filesystem);

			eliminar_paquete(paquete);

			int cod_op = recibir_operacion(socket_filesystem);

			 if(cod_op!=ESCRIBIR_CONTENIDO_PAGINA){
				 log_error(logger, "No se pudo actualizar el bloque swap de la pagina a reeemplazar");
				 return;
			 }

			 char* mensaje = recibir_mensaje(socket_filesystem);
			 log_info(logger, "Se recibio un %s de filesystem, procedo con el remplazo de pagina", mensaje);
		}

		//modifico el hallado
		marco->presencia=false;
		marco->modificado=false;



		log_info(logger,"Reemplazo Página: “REEMPLAZO - Marco: %d - Page Out: %d-%d - Page In: %d-%d“",numero_marco,marco_a_guardar->pid,pagina_a_reemplazar,pid,numero_pagina);

		log_info(logger,"Lectura de Página de SWAP: “SWAP IN -  PID: %d - Marco: %d - Page In: %d-%d“",pid,marco_a_guardar->numero_marco,pid,numero_pagina);

		reemplazar_marco(contenido_bloque,pid,pagina_a_actualizar,marco_a_guardar);
	}else{
		log_info(logger, "memoria no llena, uso primer marco libre");//TODO borrar log
		bool esMarcoLibre(void* args){
			t_situacion_marco* marco_x =(t_situacion_marco*)args;
			return marco_x->esLibre;
		}
		t_situacion_marco* marco_a_guardar = list_find(situacion_marcos,esMarcoLibre);

		log_info(logger,"Lectura de Página de SWAP: “SWAP IN -  PID: %d - Marco: %d - Page In: %d-%d“",pid,marco_a_guardar->numero_marco,pid,numero_pagina);

		reemplazar_marco(contenido_bloque,pid,pagina_a_actualizar,marco_a_guardar);

	}



	esperar_por(retardo_respuesta);

	enviar_mensaje("OK",cliente_fd,PAGE_FAULT);
	close(socket_filesystem);
}


int obtener_pagina_a_reemplazar(int numero_marco,int pid){


	t_list* elementos = dictionary_get(paginas_por_PID,string_itoa(pid));
	t_list_iterator *iterador_elementos = list_iterator_create(elementos);
	int pagina_a_reemplazar;

	while(list_iterator_has_next(iterador_elementos)){

		t_tabla_de_paginas* entrada_n = list_iterator_next(iterador_elementos);

		if(entrada_n->presencia && entrada_n->marco == numero_marco){
			pagina_a_reemplazar=list_iterator_index(iterador_elementos);
		}
	}

	return pagina_a_reemplazar;
}

void reemplazar_marco(void*contenido_bloque,int pid,t_tabla_de_paginas*pagina_a_actualizar,t_situacion_marco* marco_a_guardar){

	log_info(logger,"Acceso a espacio de usuario: “PID: %d - Accion: ESCRIBIR - Direccion fisica Marco: %d“",pid,marco_a_guardar->posicion_inicio_marco);

	memcpy(espacio_usuario+marco_a_guardar->posicion_inicio_marco,contenido_bloque,tam_pagina);
	marco_a_guardar->esLibre=false;
	marco_a_guardar->pid=pid;

	pagina_a_actualizar->marco=marco_a_guardar->numero_marco;
	pagina_a_actualizar->presencia=true;
}

bool memoria_llena(){

	bool es_marco_libre(void *elemento){
		t_situacion_marco *situacion_marco_n = (t_situacion_marco*) elemento;
		return situacion_marco_n->esLibre;
	}

	t_list* marcos_libres = list_filter(situacion_marcos, es_marco_libre);

	int cant_marcos_libres = list_size(marcos_libres);

	list_destroy(marcos_libres);

	return cant_marcos_libres == 0;
}


int aplicarFifo(){

	int marco_victima=-1;

	for(int i=0;queue_size(referencias_paginas)>i;i++){

		t_referenciaXpid* referencia_victima=(t_referenciaXpid*)queue_pop(referencias_paginas);
		int numero_pagina_a_buscar=referencia_victima->numero_pagina;
		char* pid_pagina_a_buscar=referencia_victima->pid;

		log_info(logger,"PID: %s - Pagina a buscar: %d",pid_pagina_a_buscar,numero_pagina_a_buscar);


		void esPaginaPresente(char*pid,void*args){
			t_list* entradas_x=(t_list*)args;
			t_tabla_de_paginas* entrada_pagina=list_get(entradas_x, numero_pagina_a_buscar);


			if(entrada_pagina->presencia && (strcmp(pid_pagina_a_buscar,pid)==0)){
				marco_victima=entrada_pagina->marco;
			}

		}


		dictionary_iterator(paginas_por_PID, esPaginaPresente);

		if(marco_victima!=-1){
			break;
		}
	}


	return marco_victima;
}


int aplicarLru(){

	struct auxiliar{
		int marco;
	};


	t_queue* cola_auxiliar=queue_create();
	t_list* lista_referencias_paginas=referencias_paginas->elements ;

	for(int i=0;queue_size(referencias_paginas)>i;i++){



		t_referenciaXpid* referencia_victima=(t_referenciaXpid*)list_get(lista_referencias_paginas,i);
		int numero_pagina_a_buscar=referencia_victima->numero_pagina;
		char* pid_pagina_a_buscar=referencia_victima->pid;


		void esPaginaPresente(char*pid,void*args){
			t_list* entradas_x=(t_list*)args;
			t_tabla_de_paginas* entrada_pagina=list_get(entradas_x, numero_pagina_a_buscar);


			if(entrada_pagina->presencia && (strcmp(pid_pagina_a_buscar,pid)==0)){

				t_list* ultimas_referencias=list_slice(lista_referencias_paginas, i, (list_size(lista_referencias_paginas)-i));

				bool esMismaReferencia(void*args){
					t_referenciaXpid* referencia_x=(t_referenciaXpid*)args;

					return referencia_x->numero_pagina == referencia_victima->numero_pagina &&
							(strcmp(referencia_x->pid,referencia_victima->pid)==0) ;
				}

				int cantidad_referencias=list_count_satisfying(ultimas_referencias, esMismaReferencia);

				if(cantidad_referencias==1){
					struct auxiliar* marco_auxiliar=malloc(sizeof(struct auxiliar));
					marco_auxiliar->marco=entrada_pagina->marco;
					queue_push(cola_auxiliar,(void*)marco_auxiliar);
				}

			}

		}


		dictionary_iterator(paginas_por_PID, esPaginaPresente);


	}

		struct auxiliar* marco_auxiliar=queue_pop(cola_auxiliar);

	return marco_auxiliar->marco;

}



void read_memory(int cliente_fd){

	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int pid;
	t_instruccion* instruccion;
	int desplazamiento=0;
	int direccion_fisica;

	memcpy(&pid,buffer,sizeof(int));
	desplazamiento+=sizeof(int);
	instruccion=deserializar_instruccion_en(buffer, &desplazamiento);

	direccion_fisica=atoi(instruccion->parametros[1]);

	void* contenido = malloc(tam_pagina+1);


	log_info(logger,"Acceso a espacio de usuario: “PID: %d - Accion: LEER - Direccion fisica: %d“",pid,direccion_fisica);

	memcpy(contenido, espacio_usuario+direccion_fisica, tam_pagina);


	t_paquete* paquete = crear_paquete(READ_MEMORY_RESPUESTA);
	agregar_a_paquete_sin_agregar_tamanio(paquete, contenido, tam_pagina);

	esperar_por(retardo_respuesta);

	enviar_paquete(paquete, cliente_fd);

	eliminar_paquete(paquete);

	free(buffer);
	instruccion_destroy(instruccion);
}


//escribe solo un uint32_t no la pagina entera
void write_memory(int cliente_fd){

	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int pid;
	t_instruccion* instruccion;
	int desplazamiento=0;
	int direccion_fisica;
	uint32_t valor_a_escribir ;



	memcpy(&pid,buffer,sizeof(int));
	desplazamiento+=sizeof(int);
	instruccion=deserializar_instruccion_en(buffer, &desplazamiento);

	memcpy(&valor_a_escribir,buffer+desplazamiento,sizeof(uint32_t));

	direccion_fisica=atoi(instruccion->parametros[0]);


	//obtengo numero de pagina
	int numero_marco = (int) floor(direccion_fisica / tam_pagina);

	t_list* lista_de_TP =dictionary_get(paginas_por_PID,string_itoa(pid));

	bool esMarcoBuscado(void* args){
		t_tabla_de_paginas* marco_x =(t_tabla_de_paginas*)args;
		return numero_marco==marco_x->marco;
	}


	t_tabla_de_paginas* entrada_TP=list_find(lista_de_TP,esMarcoBuscado);
	entrada_TP->modificado=true;

	log_info(logger,"Acceso a espacio de usuario: “PID: %d - Accion: ESCRIBIR - Direccion fisica: %d“",pid,direccion_fisica);

	memcpy(espacio_usuario+direccion_fisica, &valor_a_escribir, sizeof(uint32_t));

	esperar_por(retardo_respuesta);

	enviar_mensaje("OK",cliente_fd,WRITE_MEMORY_RESPUESTA);


	free(buffer);
	instruccion_destroy(instruccion);
}

//es como el write memory pero escribe una pagina entera
void write_memory_fs(int cliente_fd){
	int size;
	void* buffer = recibir_buffer(&size, cliente_fd);
	int pid;
	t_instruccion* instruccion;
	int desplazamiento=0;
	int direccion_fisica;
	void* valor_a_escribir = malloc(tam_pagina) ;



	memcpy(&pid,buffer,sizeof(int));
	desplazamiento+=sizeof(int);
	instruccion=deserializar_instruccion_en(buffer, &desplazamiento);

	memcpy(valor_a_escribir,buffer+desplazamiento,tam_pagina);

	direccion_fisica=atoi(instruccion->parametros[0]);


	//obtengo numero de pagina
	int numero_marco = (int) floor(direccion_fisica / tam_pagina);

	t_list* lista_de_TP =dictionary_get(paginas_por_PID,string_itoa(pid));

	bool esMarcoBuscado(void* args){
		t_tabla_de_paginas* marco_x =(t_tabla_de_paginas*)args;
		return numero_marco==marco_x->marco;
	}


	t_tabla_de_paginas* entrada_TP=list_find(lista_de_TP,esMarcoBuscado);
	entrada_TP->modificado=true;


	log_info(logger,"Acceso a espacio de usuario: “PID: %d - Accion: ESCRIBIR - Direccion fisica: %d“",pid,direccion_fisica);

	memcpy(espacio_usuario+direccion_fisica, valor_a_escribir, tam_pagina);

	esperar_por(retardo_respuesta);

	enviar_mensaje("OK",cliente_fd,WRITE_MEMORY_FS_RESPUESTA);


	free(buffer);
	instruccion_destroy(instruccion);
}
