#include "peticiones_cpu_dispatch.h"

sem_t esperar_proceso_ejecutando;

void* simular_sleep(void* arg){
	struct t_arg_tiempo{
		uint32_t tiempo_sleep;
		t_pcb* proceso;
	} *arg_tiempo = arg ;

	uint32_t tiempo_sleep = arg_tiempo->tiempo_sleep;

	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_en_sleep = arg_tiempo->proceso;
	sem_post(&m_proceso_ejecutando);

	sem_post(&esperar_proceso_ejecutando);

	esperar_por((tiempo_sleep) *1000);

	sem_wait(&m_proceso_ejecutando);
	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_en_sleep->PID, "BLOC","READY");

	actualizar_estado_a_pcb(proceso_en_sleep, "READY");
	sem_post(&m_proceso_ejecutando);

	sem_wait(&m_colas_de_procesos_bloqueados_por_pf);
	dictionary_remove(colas_de_procesos_bloqueados_por_pf,string_itoa(proceso_en_sleep->PID));
	sem_post(&m_colas_de_procesos_bloqueados_por_pf);

	pasar_a_ready(proceso_en_sleep);

	free(arg_tiempo);
	return NULL;
}

void manejar_sleep(int socket_cliente){
	t_contexto_ejec* contexto = (t_contexto_ejec*) recibir_contexto_de_ejecucion(socket_cliente);

	t_instruccion* instruccion = contexto->instruccion;

	//se actualiza el program_counter por las dudas
	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_operar= proceso_ejecutando;
	actualizar_pcb(contexto, proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	pthread_t hilo_simulacion;

	sem_init(&esperar_proceso_ejecutando, 0, 0);

	uint32_t tiempo_sleep = atoi(instruccion->parametros[0]);

	struct t_arg_tiempo{
		uint32_t tiempo_sleep;
		t_pcb* proceso;
	} *arg_tiempo = malloc(sizeof(struct t_arg_tiempo)) ;

	arg_tiempo->tiempo_sleep = tiempo_sleep;
	arg_tiempo->proceso = proceso_a_operar;


	pthread_create(&hilo_simulacion, NULL, simular_sleep, (void *) arg_tiempo);

	pthread_detach(hilo_simulacion);



	sem_wait(&esperar_proceso_ejecutando);

	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", contexto->pid, "EXEC","BLOC");
	log_info(logger, "Motivo de Bloqueo: “PID: %d - Bloqueado por: SLEEP“", contexto->pid);


	sem_wait(&m_proceso_ejecutando);

	//lo agrego a la cola para que lo encuentre el proceso_estado de la consola
	sem_wait(&m_colas_de_procesos_bloqueados_por_pf);
	dictionary_put(colas_de_procesos_bloqueados_por_pf,string_itoa(contexto->pid),proceso_a_operar);
	sem_post(&m_colas_de_procesos_bloqueados_por_pf);
	sem_post(&m_proceso_ejecutando);

	aviso_planificador_corto_plazo_proceso_en_bloc(proceso_a_operar);

	//destruyo el contexto de ejecucion
	contexto_ejecucion_destroy(contexto);
}

// si no lo encuentra devuelve -1
int obtener_indice_recurso(char** recursos, char* recurso_a_buscar){

	int indice_recurso = 0;
	int tamanio_recursos = string_array_size(recursos);

	if(string_array_is_empty(recursos)){
		return -1;
	}

	while(indice_recurso < tamanio_recursos && !string_equals_ignore_case(recursos[indice_recurso], recurso_a_buscar)){
		indice_recurso++;
	}
	if(indice_recurso == tamanio_recursos){
		return -1;
	}

	return indice_recurso;
}

t_recurso *obtener_recurso_con_nombre(t_list *recursos, char* recurso_a_buscar){

	bool es_recurso(void* recurso_sin_parsear){
		t_recurso *recurso = (t_recurso *) recurso_sin_parsear;
		return string_equals_ignore_case(recurso->nombre_recurso, recurso_a_buscar);
	}


	return list_find(recursos, es_recurso);
}

bool es_proceso_finalizado(void* recurso_sin_parsear){
	t_recurso *recurso = (t_recurso *) recurso_sin_parsear;
	return recurso->instancias_en_posesion == 0;
}

char *obtener_proceso_que_puede_finalizar(t_list *recusos_disponible, t_dictionary *matriz_necesidad, t_dictionary *matriz_asignados){
	t_list *procesos_posibles = list_create();

	void buscar_procesos(char *pid, void *recursos_pendientes_sin_parsear){
		t_list *recursos_pendientes = (t_list *) recursos_pendientes_sin_parsear;
		t_list *recursos_asignados_del_proceso = dictionary_get(matriz_asignados, pid);

		bool puede_sasitfacer_peticion(void* recurso_sin_parsear){
			t_recurso *recurso = (t_recurso *)recurso_sin_parsear;
			t_recurso* recurso_disponible = obtener_recurso_con_nombre(recusos_disponible, recurso->nombre_recurso);
			return recurso->instancias_en_posesion <= recurso_disponible->instancias_en_posesion;
		}

		if(list_all_satisfy(recursos_pendientes, puede_sasitfacer_peticion) && (
				!list_all_satisfy(recursos_asignados_del_proceso, es_proceso_finalizado) ||
				!list_all_satisfy(recursos_pendientes, es_proceso_finalizado)
		)){
			list_add(procesos_posibles, pid);
		}
	}

	dictionary_iterator(matriz_necesidad, buscar_procesos);

	if(list_size(procesos_posibles) == 0){
		return NULL;
	}

	return list_get(procesos_posibles, 0);
}

bool puede_finalizar_algun_proceso(t_list *recusos_disponible, t_dictionary *matriz_necesidad, t_dictionary *matriz_asignados){
	return obtener_proceso_que_puede_finalizar(recusos_disponible, matriz_necesidad, matriz_asignados) != NULL;
}


bool finalizaron_todos_los_procesos(t_dictionary *matriz_necesidad, t_dictionary *matriz_asignados){
	int procesos_finalizados = 0;

	void verificar_finalizo_proceso(char *pid, void *recursos_pendientes_del_proceso_sin_parsear){
		t_list *recursos_pendientes_del_proceso = (t_list *) recursos_pendientes_del_proceso_sin_parsear;
		t_list *recursos_asignados_del_proceso = dictionary_get(matriz_asignados, pid);

		if(list_all_satisfy(recursos_asignados_del_proceso, es_proceso_finalizado) && list_all_satisfy(recursos_pendientes_del_proceso, es_proceso_finalizado)){
			procesos_finalizados ++;
		}
	}


	dictionary_iterator(matriz_necesidad, verificar_finalizo_proceso);

	return procesos_finalizados == dictionary_size(matriz_necesidad);
}

t_recurso *recurso_new(char *nombre_recurso){
	t_recurso *recurso = malloc(sizeof(t_recurso));
	recurso->nombre_recurso = strdup(nombre_recurso);
	recurso->instancias_en_posesion = 0;
	return recurso;
}

void duplicar_diccionario(t_dictionary** duplicado, t_dictionary *a_duplicar){

	t_list* elementos = dictionary_elements(a_duplicar);
	t_list *keys = dictionary_keys(a_duplicar);

	t_list_iterator *iterador_elementos = list_iterator_create(elementos);

	while(list_iterator_has_next(iterador_elementos)){
		t_list *elemento_n = list_iterator_next(iterador_elementos);
		char *key_n = list_get(keys, list_iterator_index(iterador_elementos));

		t_list *elemento_n_dup = list_create();

		void  duplicar_recursos(void* args){
			t_recurso *recurso_n = (t_recurso *) args;
			t_recurso *recurso_n_dup = recurso_new(recurso_n->nombre_recurso);
			recurso_n_dup->instancias_en_posesion = recurso_n->instancias_en_posesion;

			list_add(elemento_n_dup, recurso_n_dup);
		}

		list_iterate(elemento_n, duplicar_recursos);


		dictionary_put(*duplicado, strdup(key_n), elemento_n_dup);
	}

	list_iterator_destroy(iterador_elementos);
}

void loggear_deadlock(void *pid_sin_parsear){
	char *pid = (char *) pid_sin_parsear;

	pthread_mutex_lock(&m_matriz_recursos_asignados);
	t_list *recursos_asignados = dictionary_get(matriz_recursos_asignados, pid);
	pthread_mutex_lock(&m_matriz_recursos_pendientes);
	t_list *recursos_pendientes = dictionary_get(matriz_recursos_pendientes, pid);

	bool tiene_instancias(t_recurso *recurso_n){
		return recurso_n->instancias_en_posesion > 0 ;
	}

	char *lista_recursos_asignados =listar_recursos_lista_recursos_por_condicion(recursos_asignados, tiene_instancias);
	pthread_mutex_unlock(&m_matriz_recursos_asignados);

	bool es_recurso_solicitado(void *recurso_sin_parsear){
		t_recurso *recurso = (t_recurso *) recurso_sin_parsear;
		return recurso->instancias_en_posesion > 0;
	}

	t_recurso *recurso_requerido = list_find(recursos_pendientes, es_recurso_solicitado);
	pthread_mutex_unlock(&m_matriz_recursos_pendientes);

	log_info(logger, "Detección de deadlock: “Deadlock detectado: %s - Recursos en posesión %s - Recurso requerido: %s“", pid, lista_recursos_asignados, recurso_requerido->nombre_recurso);

	free(lista_recursos_asignados);
}

t_list *obtener_procesos_en_deadlock(t_dictionary *matriz_necesidad, t_dictionary *matriz_asignados){
	// lista de pids de procesos en deadlock
	t_list * procesos_en_deadlock = list_create();

	void buscar_procesos_no_finalizados(char *pid, void *recursos_pendientes_del_proceso_sin_parsear){
		t_list *recursos_pendientes_del_proceso = (t_list *) recursos_pendientes_del_proceso_sin_parsear;
		t_list *recursos_asignados_del_proceso = dictionary_get(matriz_asignados, pid);

		bool es_proceso_finalizado(void* recurso_sin_parsear){
			t_recurso *recurso = (t_recurso *) recurso_sin_parsear;
			return recurso->instancias_en_posesion == 0;
		}

		if(!list_all_satisfy(recursos_asignados_del_proceso, es_proceso_finalizado) || !list_all_satisfy(recursos_pendientes_del_proceso, es_proceso_finalizado)){
			list_add(procesos_en_deadlock, pid);
		}
	}


	dictionary_iterator(matriz_necesidad, buscar_procesos_no_finalizados);

	return procesos_en_deadlock;
}


void calcular_recursos_asignados(t_list **recursos_asignados){
	void contar_recursos_asignados(char *pid, void *recursos_sin_parsear){
		t_list *recursos = (t_list *) recursos_sin_parsear;
		void contar_recursos(void *recurso_sin_parsear){
			t_recurso *recurso = (t_recurso *) recurso_sin_parsear;

			t_recurso *recurso_asignado_buscado = obtener_recurso_con_nombre(*recursos_asignados, recurso->nombre_recurso);

			if(recurso_asignado_buscado == NULL){
				t_recurso *recurso_asignado = recurso_new(recurso->nombre_recurso);

				recurso_asignado->instancias_en_posesion += recurso->instancias_en_posesion;

				list_add(*recursos_asignados, recurso_asignado);
			} else {
				recurso_asignado_buscado->instancias_en_posesion += recurso->instancias_en_posesion;
			}
		}

		list_iterate(recursos, contar_recursos);
	}

	pthread_mutex_lock(&m_matriz_recursos_asignados);
	dictionary_iterator(matriz_recursos_asignados, contar_recursos_asignados);
	pthread_mutex_unlock(&m_matriz_recursos_asignados);
}

void calcular_recursos_disponible(t_list **recursos_disponible, t_list *recursos_asignados){
	t_list_iterator *iterador_recursos_asignados = list_iterator_create(recursos_asignados);

	while(list_iterator_has_next(iterador_recursos_asignados)){
		t_recurso* recurso_asignado = list_iterator_next(iterador_recursos_asignados);

		t_recurso *recurso_disponible = recurso_new(recurso_asignado->nombre_recurso);

		t_recurso* recurso_total_n = list_get(recursos_totales, list_iterator_index(iterador_recursos_asignados));
		recurso_disponible->instancias_en_posesion = recurso_total_n->instancias_en_posesion - recurso_asignado->instancias_en_posesion;

		list_add(*recursos_disponible, recurso_disponible);
	}
}


void deteccion_de_deadlock(){
	// matriz de asignados
	// vector de recursos totales

	log_info(logger, "ANÁLISIS DE DETECCIÓN DE DEADLOCK");

	t_dictionary *matriz_asignados_cop = dictionary_create();
	t_dictionary *matriz_necesidad_cop = dictionary_create();
	t_list *recursos_disponible_cop = list_create();
	t_list *recursos_asignados = list_create();

	pthread_mutex_lock(&m_matriz_recursos_asignados);
	duplicar_diccionario(&matriz_asignados_cop, matriz_recursos_asignados);
	pthread_mutex_unlock(&m_matriz_recursos_asignados);

	pthread_mutex_lock(&m_matriz_recursos_pendientes);
	duplicar_diccionario(&matriz_necesidad_cop, matriz_recursos_pendientes);
	pthread_mutex_unlock(&m_matriz_recursos_pendientes);

	calcular_recursos_asignados(&recursos_asignados);
	calcular_recursos_disponible(&recursos_disponible_cop, recursos_asignados);

	//inicio simulacion
	while(puede_finalizar_algun_proceso(recursos_disponible_cop, matriz_necesidad_cop, matriz_asignados_cop)){
		char *pid_proceso_puede_finalizar = obtener_proceso_que_puede_finalizar(recursos_disponible_cop, matriz_necesidad_cop, matriz_asignados_cop);

		t_list* recursos_asignados_finalizar = dictionary_get(matriz_asignados_cop, pid_proceso_puede_finalizar);
		t_list* recursos_pendientes_finalizar = dictionary_get(matriz_necesidad_cop, pid_proceso_puede_finalizar);

		t_list_iterator *iterador_recursos_asignados_finalizar = list_iterator_create(recursos_asignados_finalizar);

		while(list_iterator_has_next(iterador_recursos_asignados_finalizar)){
			t_recurso* recurso_asignado_finalizar = list_iterator_next(iterador_recursos_asignados_finalizar);
			t_recurso* recurso_disponible_a_actualizar = list_get(recursos_disponible_cop, list_iterator_index(iterador_recursos_asignados_finalizar));
			t_recurso* recurso_pendiente_finalizar = list_get(recursos_pendientes_finalizar, list_iterator_index(iterador_recursos_asignados_finalizar));

			recurso_disponible_a_actualizar->instancias_en_posesion += recurso_asignado_finalizar->instancias_en_posesion;
			recurso_asignado_finalizar->instancias_en_posesion = 0;
			recurso_pendiente_finalizar->instancias_en_posesion = 0;
		}
	}


	if(!finalizaron_todos_los_procesos(matriz_necesidad_cop,matriz_asignados_cop)){
		//hay deadlock

		t_list* procesos_en_deadlock = obtener_procesos_en_deadlock(matriz_necesidad_cop,matriz_asignados_cop);


		list_iterate(procesos_en_deadlock, loggear_deadlock);

		list_destroy(procesos_en_deadlock);
	}

	destroy_matriz(matriz_asignados_cop);
	destroy_matriz(matriz_necesidad_cop);
	destroy_lista_de_recursos(recursos_disponible_cop);
	destroy_lista_de_recursos(recursos_asignados);
}

t_list *obtener_recursos_en_base_a_pid_en_matriz(t_dictionary **matriz, char *pid, int cantidad_de_recursos){
	t_list *recursos_a_devolver =  dictionary_get(*matriz, pid);

	if(recursos_a_devolver == NULL){ // si es un archivo, deberia ya estar en la matriz y aca no entraria
		recursos_a_devolver = list_create();

		for(int i = 0; i<cantidad_de_recursos; i++){
			t_recurso * recurso_n = recurso_new(recursos[i]);

			list_add(recursos_a_devolver, recurso_n);
		}

		//Entra en condicion de carrera cuando destruye al proceso de la matriz
		dictionary_put(*matriz,pid, recursos_a_devolver);
	}

	return recursos_a_devolver;
}

void incrementar_recurso_en_matriz(t_dictionary **matriz, char *nombre_recurso, char *pid, int cantidad_de_recursos){
	t_list *recursos = obtener_recursos_en_base_a_pid_en_matriz(matriz, pid, cantidad_de_recursos);

	t_recurso *recurso_a_incrementar = obtener_recurso_con_nombre(recursos, nombre_recurso);
	recurso_a_incrementar->instancias_en_posesion ++;//si esta la lista creada, recurso_a_incrementar no deberia ser NULL en caso de que sea de un archivo
}

void decrementar_recurso_en_matriz(t_dictionary **matriz, char *nombre_recurso, char *pid, int cantidad_de_recursos){
	t_list *recursos = obtener_recursos_en_base_a_pid_en_matriz(matriz, pid, cantidad_de_recursos);

	t_recurso *recurso_a_decrementar = obtener_recurso_con_nombre(recursos, nombre_recurso);
	recurso_a_decrementar->instancias_en_posesion --;//si esta la lista creada, recurso_a_incrementar no deberia ser NULL en caso de que sea de un archivo
}

void bloquear_proceso_por_recurso(t_pcb* proceso_a_bloquear, char* nombre_recurso, int cantidad_de_recursos){

	char *pid = string_itoa(proceso_a_bloquear->PID);

	pthread_mutex_lock(&m_matriz_recursos_pendientes);
	incrementar_recurso_en_matriz(&matriz_recursos_pendientes, nombre_recurso, pid, cantidad_de_recursos);
	pthread_mutex_unlock(&m_matriz_recursos_pendientes);

	deteccion_de_deadlock();

	log_info(logger, "Cambio de Estado: “PID: %s - Estado Anterior: %s - Estado Actual: %s“", pid, "EXEC","BLOC");
	log_info(logger, "Motivo de Bloqueo: “PID: %s - Bloqueado por: %s“", pid, nombre_recurso);

	t_queue* cola_bloqueados = dictionary_get(recurso_bloqueado,nombre_recurso);

	queue_push(cola_bloqueados,proceso_a_bloquear);
}

void finalizar_por_invalid_resource_proceso_ejecutando(t_contexto_ejec** contexto, t_pcb* proceso_a_operar){
	sem_wait(&m_proceso_ejecutando);
	actualizar_estado_a_pcb(proceso_a_operar, "EXIT");


	t_paquete* paquete = crear_paquete(FINALIZAR_PROCESO_MEMORIA);
	agregar_a_paquete_sin_agregar_tamanio(paquete, &(proceso_a_operar->PID), sizeof(int));
	enviar_paquete(paquete, socket_memoria);

	eliminar_paquete(paquete);

	log_info(logger, "Fin de Proceso: “Finaliza el proceso %d - Motivo: INVALID_RESOURCE“", proceso_a_operar->PID);
	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_operar->PID, "EXEC","EXIT");



	int pid_proceso_exit = proceso_a_operar->PID;
	sem_wait(&m_cola_exit);
	queue_push(cola_exit, string_itoa(pid_proceso_exit));
	sem_post(&m_cola_exit);
	sem_post(&m_proceso_ejecutando);


	contexto_ejecucion_destroy(*contexto);

	sem_wait(&m_proceso_ejecutando);
	pcb_args_destroy(proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	aviso_planificador_corto_plazo_proceso_en_exit(pid_proceso_exit);
}

void logear_instancias(char* pid, char* nombre_recurso, int *recurso_disponible, int cantidad_de_recursos){
	char* recursos_disponibles_string = listar_recursos_disponibles(recurso_disponible, cantidad_de_recursos);
	log_info(logger, "Wait: “PID: %s - Wait: %s - Instancias: [%s]“", pid, nombre_recurso, recursos_disponibles_string);

	free(recursos_disponibles_string);
}

void apropiar_recursos(int socket_cliente, char** recursos, int* recurso_disponible, int cantidad_de_recursos){
	t_contexto_ejec* contexto = (t_contexto_ejec*) recibir_contexto_de_ejecucion(socket_cliente);

	t_instruccion* instruccion = contexto->instruccion;
	char *nombre_recurso = instruccion->parametros[0];
	char *pid = string_itoa(contexto->pid);

	log_info(logger, "Inicio Wait al recurso %s",nombre_recurso);

	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_operar= proceso_ejecutando;
	actualizar_pcb(contexto, proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	int indice_recurso = obtener_indice_recurso(recursos, nombre_recurso);


	// si no existe el recurso finaliza
	if(indice_recurso == -1){
		finalizar_por_invalid_resource_proceso_ejecutando(&contexto, proceso_a_operar);
		return;
	}

	recurso_disponible[indice_recurso] --;

	if(recurso_disponible[indice_recurso] < 0){

		//lamo a esta funcion para inicializar la matriz de recursos asignados para la deteccion de deadlock
		pthread_mutex_lock(&m_matriz_recursos_asignados);
		obtener_recursos_en_base_a_pid_en_matriz(&matriz_recursos_asignados, pid, cantidad_de_recursos);
		pthread_mutex_unlock(&m_matriz_recursos_asignados);

		sem_wait(&m_proceso_ejecutando);
		bloquear_proceso_por_recurso(proceso_a_operar, recursos[indice_recurso], cantidad_de_recursos);
		sem_post(&m_proceso_ejecutando);

		logear_instancias(pid, recursos[indice_recurso], recurso_disponible, cantidad_de_recursos);

		aviso_planificador_corto_plazo_proceso_en_bloc(proceso_a_operar);

		contexto_ejecucion_destroy(contexto);

		return;
	}

	pthread_mutex_lock(&m_matriz_recursos_asignados);
	incrementar_recurso_en_matriz(&matriz_recursos_asignados, nombre_recurso, pid, cantidad_de_recursos);
	pthread_mutex_unlock(&m_matriz_recursos_asignados);

	logear_instancias(pid, recursos[indice_recurso], recurso_disponible, cantidad_de_recursos);


	aviso_planificador_corto_plazo_proceso_en_exec(proceso_a_operar);

	//destruyo el contexto de ejecucion
	contexto_ejecucion_destroy(contexto);
}

void desalojar_recursos(int socket_cliente,char** recursos, int* recurso_disponible, int cantidad_de_recursos){
	t_contexto_ejec* contexto = (t_contexto_ejec*) recibir_contexto_de_ejecucion(socket_cliente);

	t_instruccion* instruccion = contexto->instruccion;
	char *nombre_recurso = instruccion->parametros[0];
	char *pid = string_itoa(contexto->pid);

	log_info(logger, "Inicio Signal al recurso %s",nombre_recurso);

	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_operar= proceso_ejecutando;
	actualizar_pcb(contexto, proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	int indice_recurso = obtener_indice_recurso(recursos, nombre_recurso);

	// si no existe el recurso finaliza
	if(indice_recurso == -1){
		finalizar_por_invalid_resource_proceso_ejecutando(&contexto, proceso_a_operar);

		return;
	}

	recurso_disponible[indice_recurso] ++;

	pthread_mutex_lock(&m_matriz_recursos_asignados);
	t_list *recursos_del_proceso = obtener_recursos_en_base_a_pid_en_matriz(&matriz_recursos_asignados, pid, cantidad_de_recursos);
	pthread_mutex_unlock(&m_matriz_recursos_asignados);

	t_recurso *recurso_buscado = obtener_recurso_con_nombre(recursos_del_proceso, nombre_recurso);

	if(recurso_buscado->instancias_en_posesion > 0){ // si este proceso solicito el recurso
		pthread_mutex_lock(&m_matriz_recursos_asignados);
		decrementar_recurso_en_matriz(&matriz_recursos_asignados, nombre_recurso, pid, cantidad_de_recursos);
		pthread_mutex_unlock(&m_matriz_recursos_asignados);

	} else { //este proceso no solicito una instancia del recurso
		log_info(logger, "Finaliza el proceso %d por recurso no tomado", contexto->pid);
		finalizar_por_invalid_resource_proceso_ejecutando(&contexto, proceso_a_operar);
		return;
	}

	t_queue* cola_bloqueados= (t_queue*) dictionary_get(recurso_bloqueado,recursos[indice_recurso]);

	int cantidad_procesos_bloqueados = queue_size(cola_bloqueados);

	if(cantidad_procesos_bloqueados > 0){
		t_pcb* proceso_desbloqueado = queue_pop(cola_bloqueados);
		char * pid_desbloqueado = string_itoa(proceso_desbloqueado->PID);

		log_info(logger, "Cambio de Estado: “PID: %s - Estado Anterior: %s - Estado Actual: %s“", pid_desbloqueado, "BLOC","READY");

		actualizar_estado_a_pcb(proceso_desbloqueado, "READY");

		//actualizo los recursos disponibles para que no se le actualize a otro proceso
		recurso_disponible[indice_recurso] --;

		pthread_mutex_lock(&m_matriz_recursos_pendientes);
		decrementar_recurso_en_matriz(&matriz_recursos_pendientes, nombre_recurso, pid_desbloqueado, cantidad_de_recursos);
		pthread_mutex_unlock(&m_matriz_recursos_pendientes);

		pthread_mutex_lock(&m_matriz_recursos_asignados);
		incrementar_recurso_en_matriz(&matriz_recursos_asignados, nombre_recurso, pid_desbloqueado, cantidad_de_recursos);
		pthread_mutex_unlock(&m_matriz_recursos_asignados);

		pasar_a_ready(proceso_desbloqueado);
	}

	logear_instancias(pid, recursos[indice_recurso], recurso_disponible, cantidad_de_recursos);

	//continua ejecutandose el mismo proceso
	aviso_planificador_corto_plazo_proceso_en_exec(proceso_a_operar);

	//destruyo el contexto de ejecucion
	contexto_ejecucion_destroy(contexto);
}


void finalinzar_proceso(int socket_cliente, t_contexto_ejec* contexto){
	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_operar= proceso_ejecutando;
	actualizar_pcb(contexto, proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	sem_wait(&m_proceso_ejecutando);
	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_operar->PID, "EXEC","EXIT");

	actualizar_estado_a_pcb(proceso_a_operar, "EXIT");


	t_paquete *paquete = crear_paquete(FINALIZAR_PROCESO_MEMORIA);
	agregar_a_paquete_sin_agregar_tamanio(paquete,&(proceso_a_operar->PID),sizeof(int));
	enviar_paquete(paquete,socket_memoria);
	eliminar_paquete(paquete);

	int pid_proceso_exit = proceso_a_operar->PID;
	sem_post(&m_proceso_ejecutando);

	log_info(logger, "Fin de Proceso: “Finaliza el proceso %d - Motivo: SUCCESS“", pid_proceso_exit);


	sem_wait(&m_cola_exit);
	queue_push(cola_exit, string_itoa(pid_proceso_exit));
	sem_post(&m_cola_exit);

	contexto_ejecucion_destroy(contexto);

	sem_wait(&m_proceso_ejecutando);
	pcb_args_destroy(proceso_a_operar);
	free(proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	aviso_planificador_corto_plazo_proceso_en_exit(pid_proceso_exit);
}




void destroy_lista_de_recursos(t_list* lista_recursos){
	void eliminar_recurso(void *arg){
		t_recurso *recurso_n = (t_recurso *)arg;

		free(recurso_n->nombre_recurso);
		free(recurso_n);
	}

	list_destroy_and_destroy_elements(lista_recursos, eliminar_recurso);
}

void destroy_proceso_en_matriz(t_dictionary *matriz, char *pid){
	void eliminar_recursos(void *args){
		t_list *recusos_a_borrar = (t_list *) args;

		destroy_lista_de_recursos(recusos_a_borrar);
	}

	dictionary_remove_and_destroy(matriz, pid, eliminar_recursos);
}
void destroy_matriz(t_dictionary *matriz){

	t_list *pids_procesos = dictionary_keys(matriz);

	void destruir_proceso(void *args){
		char *pid = (char *)args;

		destroy_proceso_en_matriz(matriz, pid);
	}

	list_iterate(pids_procesos, destruir_proceso);

	list_destroy(pids_procesos);
	dictionary_destroy(matriz);
}

t_list *duplicar_lista_recursos(t_list *a_duplicar){
	t_list *recursos_del_proceso_dup = list_create();

	void duplicar_recursos(void *arg){
		t_recurso *recurso_n = (t_recurso *)arg;

		t_recurso *recuso_n_dup = recurso_new(recurso_n->nombre_recurso);
		recuso_n_dup->instancias_en_posesion = recurso_n->instancias_en_posesion;

		list_add(recursos_del_proceso_dup, recuso_n_dup);
	}

	list_iterate(a_duplicar, duplicar_recursos);

	return recursos_del_proceso_dup;
}


void* hilo_que_maneja_pf(void* args){

	struct t_arg_page_fault {
		int numero_pagina;
		int pid;
		t_pcb* proceso;
	}*args_page_fault = args;


	int numero_pagina = args_page_fault->numero_pagina;
	int pid = args_page_fault->pid;

	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_bloquear = args_page_fault->proceso;
	sem_post(&m_proceso_ejecutando);

	char* pid_del_bloqueado=string_itoa(proceso_a_bloquear->PID);
	sem_wait(&m_colas_de_procesos_bloqueados_por_pf);
	dictionary_put(colas_de_procesos_bloqueados_por_pf,pid_del_bloqueado,proceso_a_bloquear);
	sem_post(&m_colas_de_procesos_bloqueados_por_pf);

	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", pid, "EXEC","BLOC");
	log_info(logger, "Motivo de Bloqueo: “PID: %d - Bloqueado por: PAGE_FAULT“", proceso_a_bloquear->PID);
	log_info(logger, "Page Fault: “Page Fault PID: %d - Pagina: %d“", pid,numero_pagina);

	aviso_planificador_corto_plazo_proceso_en_bloc(proceso_a_bloquear);

	t_paquete* paquete= crear_paquete(PAGE_FAULT);

	agregar_a_paquete_sin_agregar_tamanio(paquete,&numero_pagina,sizeof(int));
	agregar_a_paquete_sin_agregar_tamanio(paquete,&pid,sizeof(int));
	enviar_paquete(paquete,socket_memoria);

	eliminar_paquete(paquete);


	sem_wait(&m_espero_respuesta_pf);
	 
	sem_wait(&m_colas_de_procesos_bloqueados_por_pf);
	t_pcb* proceso_a_ready = dictionary_remove(colas_de_procesos_bloqueados_por_pf, pid_del_bloqueado);
	sem_post(&m_colas_de_procesos_bloqueados_por_pf);

	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_ready->PID, "BLOC","READY");

	actualizar_estado_a_pcb(proceso_a_ready, "READY");
	pasar_a_ready(proceso_a_ready);
	 

	 free(args_page_fault);
   return NULL;
}

void manejar_page_fault(int socket_cliente){

	int size;
	void *buffer = recibir_buffer(&size, socket_cliente);
	int numero_pagina;
	int desplazamiento = 0;

	memcpy(&numero_pagina, buffer+desplazamiento, sizeof(int));
	desplazamiento+=sizeof(int);

	t_contexto_ejec* contexto = deserializar_contexto_de_ejecucion(buffer, size, &desplazamiento);

	sem_wait(&m_proceso_ejecutando);
	t_pcb* proceso_a_operar= proceso_ejecutando;
	actualizar_pcb(contexto, proceso_a_operar);
	sem_post(&m_proceso_ejecutando);

	pthread_t hilo_pf;

	struct t_arg_page_fault {
		int numero_pagina;
		int pid;
		t_pcb* proceso;
	}*args_page_fault = malloc(sizeof(struct t_arg_page_fault));
	args_page_fault->numero_pagina=numero_pagina;
	args_page_fault->pid=contexto->pid;
	args_page_fault->proceso = proceso_a_operar;

	pthread_create(&hilo_pf,NULL,hilo_que_maneja_pf,(void*)args_page_fault);
	pthread_detach(hilo_pf);

	contexto_ejecucion_destroy(contexto);
	free(buffer);
}

