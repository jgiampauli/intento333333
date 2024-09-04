#include "planificador_corto_plazo.h"
int pid_desalojado = 0;


void poner_a_ejecutar_otro_proceso(){

	sem_wait(&m_proceso_ejecutando);
	proceso_ejecutando = NULL;
	sem_post(&m_proceso_ejecutando);

	sem_wait(&m_cola_ready);
	sem_wait(&m_cola_new);

	if(queue_size(cola_ready) == 0 && queue_size(cola_new) > 0){
		sem_post(&m_cola_ready);
		sem_post(&m_cola_new);
		sem_post(&despertar_planificacion_largo_plazo);
	} else {
		sem_post(&m_cola_ready);
		sem_post(&m_cola_new);
		sem_post(&despertar_corto_plazo);
	}
}


void reordenar_cola_ready_prioridades(){
	// reodena de menor a mayor para que al hacer pop, saque al de menor proridad
	// cuando hace pop saca al primer elemento de la lista
	bool __proceso_mayor_prioridad(t_pcb* pcb_proceso1, t_pcb* pcb_proceso2){
		return pcb_proceso1->prioridad < pcb_proceso2->prioridad;
	}

	sem_wait(&m_cola_ready);
	list_sort(cola_ready->elements, (void *) __proceso_mayor_prioridad);
	sem_post(&m_cola_ready);
}

void notificar_desalojo_cpu_interrupt(char* motivo_del_desalojo){

	//el mensaje es el motivo del desalojo (usado solo para un log en cpu)
	enviar_mensaje(motivo_del_desalojo, socket_cpu_interrupt, INTERRUPCION);

	sem_wait(&espero_desalojo_CPU);
	sem_wait(&espero_actualizacion_pcb);
}

void crear_contexto_y_enviar_a_CPU(t_pcb* proceso_a_ejecutar){
	t_contexto_ejec* contexto_ejecucion = malloc(sizeof(t_contexto_ejec));
	contexto_ejecucion->instruccion = NULL;
	contexto_ejecucion->program_counter = proceso_a_ejecutar->program_counter;
	contexto_ejecucion->pid = proceso_a_ejecutar->PID;
	contexto_ejecucion->registros_CPU = proceso_a_ejecutar->registros_CPU;

	enviar_contexto_de_ejecucion_a(contexto_ejecucion, PETICION_CPU, socket_cpu_dispatch);


	contexto_ejecucion_destroy(contexto_ejecucion);
}

void poner_a_ejecutar_proceso(t_pcb* proceso_a_ejecutar){
	sem_wait(&m_proceso_ejecutando);
	proceso_ejecutando = proceso_a_ejecutar;
	if(strcmp(proceso_a_ejecutar->proceso_estado, "EXEC") != 0)
	{
		log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_ejecutar->PID, proceso_a_ejecutar->proceso_estado, "EXEC");
		actualizar_estado_a_pcb(proceso_a_ejecutar, "EXEC");
	}
	crear_contexto_y_enviar_a_CPU(proceso_a_ejecutar);
	sem_post(&m_proceso_ejecutando);
}

void planificar_corto_plazo_fifo() {
	sem_wait(&despertar_corto_plazo);
	pthread_mutex_lock(&m_planificador_corto_plazo);

	sem_wait(&m_cola_ready);
	if (queue_size(cola_ready) == 0) {
		sem_post(&m_cola_ready);
		pthread_mutex_unlock(&m_planificador_corto_plazo);
		return;
	}

	t_pcb *proceso_a_ejecutar = queue_pop(cola_ready);
	sem_post(&m_cola_ready);

	poner_a_ejecutar_proceso(proceso_a_ejecutar);

	pthread_mutex_unlock(&m_planificador_corto_plazo);
}

void planificar_corto_plazo_prioridades() {
	sem_wait(&despertar_corto_plazo);
	pthread_mutex_lock(&m_planificador_corto_plazo);
	sem_wait(&m_cola_ready);

	if (queue_size(cola_ready) == 0) {
		sem_post(&m_cola_ready);
		pthread_mutex_unlock(&m_planificador_corto_plazo);
		return;
	}
	sem_post(&m_cola_ready);

	reordenar_cola_ready_prioridades();

	//saca el de mayor prioridad
	sem_wait(&m_cola_ready);
	t_pcb *proceso_a_ejecutar = queue_pop(cola_ready);
	sem_post(&m_cola_ready);

	poner_a_ejecutar_proceso(proceso_a_ejecutar);

	pthread_mutex_lock(&m_pid_desalojado);
	pid_desalojado = 0;
	pthread_mutex_unlock(&m_pid_desalojado);

	pthread_mutex_unlock(&m_planificador_corto_plazo);
}

void planificar_corto_plazo_round_robbin() {

	sem_wait(&despertar_corto_plazo);
	pthread_mutex_lock(&m_planificador_corto_plazo);
	sem_wait(&m_cola_ready);

	if (queue_size(cola_ready) == 0) {
		sem_post(&m_cola_ready);
		pthread_mutex_unlock(&m_planificador_corto_plazo);
		return;
	}

	t_pcb *proceso_a_ejecutar = queue_pop(cola_ready);
	sem_post(&m_cola_ready);

	poner_a_ejecutar_proceso(proceso_a_ejecutar);

	esperar_por(quantum);

	pthread_mutex_lock(&m_pid_desalojado);
	pid_desalojado = proceso_a_ejecutar->PID;
	pthread_mutex_unlock(&m_pid_desalojado);

	sem_wait(&m_proceso_ejecutando);
	if(proceso_ejecutando == NULL){
		sem_post(&m_proceso_ejecutando);
		sem_wait(&m_cola_ready);
		int tam_procesos_en_ready = queue_size(cola_ready);
		sem_post(&m_cola_ready);
		sem_wait(&m_cola_new);
		int tam_procesos_en_new = queue_size(cola_new);
		sem_post(&m_cola_new);

		if(tam_procesos_en_ready > 0){
			sem_post(&despertar_corto_plazo);
		} else if(tam_procesos_en_new >0){
			sem_post(&despertar_planificacion_largo_plazo);
		}

		pthread_mutex_unlock(&m_planificador_corto_plazo);
		return;
	} else{
		sem_post(&m_proceso_ejecutando);
	}

	sem_wait(&m_proceso_ejecutando);

	log_info(logger, "Fin de Quantum: “PID: %d - Desalojado por fin de Quantum“", proceso_a_ejecutar->PID);
	sem_post(&m_proceso_ejecutando);

	char* mensaje = malloc(300);
	pthread_mutex_lock(&m_pid_desalojado);
	sprintf(mensaje, "Desalojo por fin de quantum a %d", pid_desalojado);
	pthread_mutex_unlock(&m_pid_desalojado);

	notificar_desalojo_cpu_interrupt(mensaje);

	free(mensaje);

	sem_wait(&m_proceso_ejecutando);
	if(string_equals_ignore_case(proceso_a_ejecutar->proceso_estado,"EXEC")){//si el proceso esta en exec (osea no se bloqueo)
		log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_ejecutar->PID, "EXEC", "READY");
		actualizar_estado_a_pcb(proceso_a_ejecutar, "READY");

		proceso_ejecutando = NULL;
		sem_post(&m_proceso_ejecutando);

		pasar_a_ready(proceso_a_ejecutar);
	}else {
		sem_post(&m_proceso_ejecutando);
	}

	pthread_mutex_lock(&m_pid_desalojado);
	pid_desalojado = 0;
	pthread_mutex_unlock(&m_pid_desalojado);

	pthread_mutex_unlock(&m_planificador_corto_plazo);
}

void enviar_contexto_de_ejecucion_a(t_contexto_ejec* contexto_a_ejecutar, op_code opcode, int socket_cliente){

	t_paquete *paquete = crear_paquete(opcode);

	agregar_a_paquete_sin_agregar_tamanio(paquete, &(contexto_a_ejecutar->pid),
			sizeof(int));

	agregar_a_paquete_sin_agregar_tamanio(paquete,
			&(contexto_a_ejecutar->program_counter), sizeof(int));

	agregar_a_paquete_sin_agregar_tamanio(paquete,
			&(contexto_a_ejecutar->registros_CPU->AX), sizeof(uint32_t));
	agregar_a_paquete_sin_agregar_tamanio(paquete,
			&(contexto_a_ejecutar->registros_CPU->BX), sizeof(uint32_t));
	agregar_a_paquete_sin_agregar_tamanio(paquete,
			&(contexto_a_ejecutar->registros_CPU->CX), sizeof(uint32_t));
	agregar_a_paquete_sin_agregar_tamanio(paquete,
			&(contexto_a_ejecutar->registros_CPU->DX), sizeof(uint32_t));

	enviar_paquete(paquete, socket_cliente);

	eliminar_paquete(paquete);
}

void *planificar_nuevos_procesos_corto_plazo(void *arg){

	while(1){
		if(string_equals_ignore_case(algoritmo_planificacion, "FIFO")){
			planificar_corto_plazo_fifo();
		}else if(string_equals_ignore_case(algoritmo_planificacion, "PRIORIDADES")){
			planificar_corto_plazo_prioridades();
		}else {
			planificar_corto_plazo_round_robbin();
		}
	}

	return NULL;
}


void aviso_planificador_corto_plazo_proceso_en_ready(t_pcb* proceso_en_ready){
	if(string_equals_ignore_case(algoritmo_planificacion, "PRIORIDADES")){
		//asumo que ya esta el proceso en la cola de ready
		reordenar_cola_ready_prioridades();

		sem_wait(&m_cola_ready);
		t_pcb* proceso_con_mayor_prioridad = queue_peek(cola_ready);//obtengo el proceso sin sacarlo de la cola
		sem_post(&m_cola_ready);

		log_info(logger, "Manejo ingreso a ready");
		sem_wait(&m_proceso_ejecutando);
		if(proceso_ejecutando == NULL){//si no hay nadie ejecutando
			sem_post(&m_proceso_ejecutando);
			log_info(logger, "No hay nadie ejecutando, pongo a ejecutar a otro proceso");

			poner_a_ejecutar_otro_proceso();

		}else if(proceso_con_mayor_prioridad != NULL && proceso_con_mayor_prioridad->prioridad < proceso_ejecutando->prioridad){//si hay alguien ejecutando pero es de menor prioridad
			sem_post(&m_proceso_ejecutando);
			log_info(logger, "Hay un proceso ejecutando, pero el que esta en ready es de mayor prioridad, lo desalojo");
			char* mensaje = malloc(300);
			sem_wait(&m_proceso_ejecutando);
			sprintf(mensaje, "Desalojo por proceso de mayor prioridad a %d", proceso_ejecutando->PID);
			sem_post(&m_proceso_ejecutando);

			notificar_desalojo_cpu_interrupt(mensaje);
			free(mensaje);

			sem_wait(&m_proceso_ejecutando);
			t_pcb* proceso_a_desalojar = proceso_ejecutando;
			proceso_ejecutando = proceso_con_mayor_prioridad;
			sem_post(&m_proceso_ejecutando);

			if(strcmp(proceso_a_desalojar->proceso_estado, "EXEC") == 0){
				log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_a_desalojar->PID, proceso_a_desalojar->proceso_estado,"READY");
				actualizar_estado_a_pcb(proceso_a_desalojar, "READY");

				sem_wait(&m_cola_ready);
				queue_push(cola_ready, proceso_a_desalojar);
				sem_post(&m_cola_ready);

				aviso_planificador_corto_plazo_proceso_en_ready(proceso_a_desalojar);

				char *pids = listar_pids_cola(cola_ready);
				log_info(logger, "Ingreso a Ready: “Cola Ready %s: [%s]“",algoritmo_planificacion, pids);
				free(pids);
			}


			sem_wait(&m_cola_ready);
			t_pcb* proceso_a_ejecutar = queue_pop(cola_ready);//saco el proceso de la cola
			sem_post(&m_cola_ready);

			sem_wait(&m_proceso_ejecutando);
			pthread_mutex_lock(&m_pid_desalojado);
			pid_desalojado= proceso_a_desalojar->PID;
			pthread_mutex_unlock(&m_pid_desalojado);
			sem_post(&m_proceso_ejecutando);

			log_info(logger, "Proceso de menor prioridad desalojado, pongo a ejecutar al proceso de mayor prioridad");
			poner_a_ejecutar_proceso(proceso_a_ejecutar);

		} else {//si hay alguien ejecutando y es el proceso no hago nada
			log_info(logger, "Hay alguien ejecutando y es el de mayor prioridad, no hago nada");
			sem_post(&m_proceso_ejecutando);
		}
	}else {// si es FIFO Y RR
		log_info(logger, "Manejo ingreso a ready");
		sem_wait(&m_proceso_ejecutando);
		if(proceso_ejecutando == NULL){//si no hay nadie ejecutando
			sem_post(&m_proceso_ejecutando);
			log_info(logger, "Manejo ingreso a ready - no hay nadie ejecutando, pongo a ejecutar a otro proceso");
			poner_a_ejecutar_otro_proceso();
		} else {
			sem_post(&m_proceso_ejecutando);
			log_info(logger, "Manejo ingreso a ready - hay alguien ejecutando, no hago nada");
		}
	}
}

void manejar_desalojo_en_bloc(t_pcb* proceso_en_bloc){
	pthread_mutex_lock(&m_pid_desalojado);
	if(pid_desalojado != proceso_en_bloc->PID){
		log_info(logger, "Manejo de bloc -El desalojado no es el proceso en bloc, pongo a ejecutar a otro proceso");
		poner_a_ejecutar_otro_proceso();
	}
	pthread_mutex_unlock(&m_pid_desalojado);
}

void manejar_proceso_en_bloc(t_pcb* proceso_en_bloc){
	if(string_equals_ignore_case(algoritmo_planificacion, "FIFO")){
		poner_a_ejecutar_otro_proceso();
	}else if(string_equals_ignore_case(algoritmo_planificacion, "PRIORIDADES")){
		pthread_mutex_lock(&m_pid_desalojado);
		if(pid_desalojado){// si justo se desaloja un proceso en este instante
			pthread_mutex_unlock(&m_pid_desalojado);
			log_info(logger, "Manejo de bloc -Hay alguien desalojado");
			manejar_desalojo_en_bloc(proceso_en_bloc);
		}else {
			pthread_mutex_unlock(&m_pid_desalojado);
			log_info(logger, "Manejo de bloc - no hay naide desalojado, pongo a ejecutar a otro proceso");
			poner_a_ejecutar_otro_proceso();
		}

	}else {// si es RR
		pthread_mutex_lock(&m_pid_desalojado);
		if(pid_desalojado){// si justo se desaloja un proceso en este instante
			pthread_mutex_unlock(&m_pid_desalojado);
			log_info(logger, "Manejo de bloc -Hay alguien desalojado");
			manejar_desalojo_en_bloc(proceso_en_bloc);
		}else {
			pthread_mutex_unlock(&m_pid_desalojado);
			log_info(logger, "Manejo de bloc - no hay naide desalojado, pongo a ejecutar a otro proceso");
			poner_a_ejecutar_otro_proceso();
		}

	}
}

void aviso_planificador_corto_plazo_proceso_en_bloc(t_pcb* proceso_en_bloc){
	sem_wait(&m_proceso_ejecutando);
	if(proceso_ejecutando != NULL && proceso_ejecutando->PID == proceso_en_bloc->PID){
		actualizar_estado_a_pcb(proceso_en_bloc, "BLOC");
		sem_post(&m_proceso_ejecutando);
	} else {
		sem_post(&m_proceso_ejecutando);
		actualizar_estado_a_pcb(proceso_en_bloc, "BLOC");
	}

	manejar_proceso_en_bloc(proceso_en_bloc);
}



void aviso_planificador_corto_plazo_proceso_en_exec(t_pcb* proceso_en_exec){
	sem_wait(&m_proceso_ejecutando);
	pthread_mutex_lock(&m_pid_desalojado);
	if(!pid_desalojado || pid_desalojado != proceso_en_exec->PID){
		pthread_mutex_unlock(&m_pid_desalojado);
		sem_post(&m_proceso_ejecutando);
		log_info(logger, "Manejo de exec de %d-Hay alguien desalojado y no es el proceso ejecutando o no hay nadie desalojado, continuo ejecutando el proceso ejecutando", proceso_en_exec->PID);
		poner_a_ejecutar_proceso(proceso_en_exec);
	}else if(pid_desalojado &&  pid_desalojado == proceso_en_exec->PID){
		pthread_mutex_unlock(&m_pid_desalojado);
		sem_post(&m_proceso_ejecutando);
		log_info(logger, "Manejo de exec de %d -Hay alguien desalojado y es el proceso ejecutando, no hago nada", proceso_en_exec->PID);

	}else{
		pthread_mutex_unlock(&m_pid_desalojado);
		sem_post(&m_proceso_ejecutando);
		log_info(logger, "Manejo de exec de %d - No deberia entrar aca entonces no hago nada", proceso_en_exec->PID);
	}


}

void aviso_planificador_corto_plazo_proceso_en_exit(int pid_proceso_exit){

	if(string_equals_ignore_case(algoritmo_planificacion, "PRIORIDADES") || string_equals_ignore_case(algoritmo_planificacion, "RR")){
		pthread_mutex_lock(&m_pid_desalojado);
		if(!pid_desalojado || pid_desalojado != pid_proceso_exit){
			log_info(logger, "Manejo de exit - no hay nadie desalojado o hay a alguien desalojado pero no es el proceso en exit, pongo a ejecutar a otro proceso");
			poner_a_ejecutar_otro_proceso();
		}else{
			log_info(logger, "Manejo de exit - Hay alguien desalojado y es el proceso en exit, no hago nada");
		}
		pthread_mutex_unlock(&m_pid_desalojado);
	}else {
		poner_a_ejecutar_otro_proceso();
	}
}
