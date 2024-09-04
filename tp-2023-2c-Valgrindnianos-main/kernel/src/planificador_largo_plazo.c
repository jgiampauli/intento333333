#include "planificador_largo_plazo.h"

t_queue* cola_new;
t_queue* cola_ready;
t_queue* cola_exit;
t_pcb* proceso_ejecutando;
char* algoritmo_planificacion;


sem_t despertar_corto_plazo;
sem_t m_cola_ready;
sem_t m_cola_new;
sem_t m_cola_exit;
sem_t m_proceso_ejecutando;
sem_t m_recurso_bloqueado;
sem_t m_cola_de_procesos_bloqueados_para_cada_archivo;
sem_t m_colas_de_procesos_bloqueados_por_pf;
sem_t despertar_planificacion_largo_plazo;
sem_t memoria_lista;
sem_t proceso_creado_memoria;
sem_t espero_desalojo_CPU;
sem_t espero_actualizacion_pcb;
t_dictionary* colas_de_procesos_bloqueados_para_cada_archivo;
t_dictionary* colas_de_procesos_bloqueados_por_pf;
t_dictionary* recurso_bloqueado;
pthread_mutex_t m_planificador_largo_plazo;
pthread_mutex_t m_planificador_corto_plazo;
sem_t m_espero_respuesta_pf;
pthread_mutex_t m_matriz_recursos_asignados;
pthread_mutex_t m_matriz_recursos_pendientes;
pthread_mutex_t m_pid_desalojado;

void inicializar_colas_y_semaforos(){
	cola_new = queue_create();
	cola_ready = queue_create();
	cola_exit = queue_create();
	sem_init(&m_cola_ready,0,1);
	sem_init(&m_cola_new, 0, 1);
	sem_init(&m_cola_exit, 0, 1);
	sem_init(&m_proceso_ejecutando, 0, 1);
	sem_init(&despertar_corto_plazo,0,0);
	sem_init(&m_recurso_bloqueado, 0, 1);
	sem_init(&m_cola_de_procesos_bloqueados_para_cada_archivo, 0,1);
	sem_init(&despertar_planificacion_largo_plazo,0,0);
	sem_init(&memoria_lista,0,0);
	sem_init(&proceso_creado_memoria, 0, 0);
	sem_init(&espero_desalojo_CPU, 0, 0);
	sem_init(&espero_actualizacion_pcb, 0, 0);
	sem_init(&m_espero_respuesta_pf, 0, 0);
	sem_init(&m_colas_de_procesos_bloqueados_por_pf, 0, 1);
	pthread_mutex_init(&m_planificador_largo_plazo, NULL);
	pthread_mutex_init(&m_planificador_corto_plazo, NULL);
	pthread_mutex_init(&m_matriz_recursos_asignados, NULL);
	pthread_mutex_init(&m_matriz_recursos_pendientes, NULL);
	pthread_mutex_init(&m_pid_desalojado, NULL);
}

// si el proceso no es new, no es necesario el socket de memoria
void agregar_proceso_a_ready(int conexion_memoria, char* algoritmo_planificacion){
	sem_wait(&m_cola_new);
	t_pcb* proceso_new_a_ready = (t_pcb *) queue_pop(cola_new);
	sem_post(&m_cola_new);

	log_info(logger, "Cambio de Estado: “PID: %d - Estado Anterior: %s - Estado Actual: %s“", proceso_new_a_ready->PID, "NEW", "READY");

	actualizar_estado_a_pcb(proceso_new_a_ready, "READY");

	//envio a memoria de instrucciones
	t_paquete* paquete_instrucciones = crear_paquete(INICIAR_PROCESO);

	agregar_a_paquete(paquete_instrucciones, proceso_new_a_ready->comando->parametros[0],proceso_new_a_ready->comando->parametro1_lenght);
	agregar_a_paquete_sin_agregar_tamanio(paquete_instrucciones,&(proceso_new_a_ready->PID),sizeof(int));
	enviar_paquete(paquete_instrucciones,socket_memoria);

	eliminar_paquete(paquete_instrucciones);



	//espero a que termine memoria
	sem_wait(&memoria_lista);


	t_paquete* paquete_tamanio = crear_paquete(CREAR_PROCESO);

	//agrego el tamanio de proceso

	int tamanio_proceso =atoi(proceso_new_a_ready->comando->parametros[1]);

	agregar_a_paquete_sin_agregar_tamanio(paquete_tamanio,&tamanio_proceso,sizeof(int));
	agregar_a_paquete_sin_agregar_tamanio(paquete_tamanio,&(proceso_new_a_ready->PID),sizeof(int));

	enviar_paquete(paquete_tamanio,socket_memoria);

	eliminar_paquete(paquete_tamanio);

	free(proceso_new_a_ready->comando);//el comando luego no se va a usar, lo libero

	//espero a que termine memoria
	sem_wait(&proceso_creado_memoria);

	pasar_a_ready(proceso_new_a_ready);
}

int calcular_procesos_en_memoria(int procesos_en_ready){

	int procesos_bloqueados = 0;

	void _calcular_procesos_bloqueados(char* key, void* value){
		t_queue* cola_bloqueados_recurso_n = (t_queue*) value;

		if(queue_size(cola_bloqueados_recurso_n) != 0){
			procesos_bloqueados += queue_size(cola_bloqueados_recurso_n);
		}
	}

	dictionary_iterator(recurso_bloqueado, _calcular_procesos_bloqueados);


	sem_wait(&m_cola_de_procesos_bloqueados_para_cada_archivo);
	void _calcular_procesos_bloqueados_por_archivo(char* key, void* value){
		t_queue* cola_bloqueados_archivo_n = (t_queue*) value;

		if(queue_size(cola_bloqueados_archivo_n) != 0){
			procesos_bloqueados += queue_size(cola_bloqueados_archivo_n);
		}
	}

	dictionary_iterator(colas_de_procesos_bloqueados_para_cada_archivo, _calcular_procesos_bloqueados_por_archivo);
	sem_post(&m_cola_de_procesos_bloqueados_para_cada_archivo);

	sem_wait(&m_colas_de_procesos_bloqueados_por_pf);
	procesos_bloqueados += dictionary_size(colas_de_procesos_bloqueados_por_pf);
	sem_post(&m_colas_de_procesos_bloqueados_por_pf);

	sem_wait(&m_proceso_ejecutando);
	if(proceso_ejecutando != NULL){
		sem_post(&m_proceso_ejecutando);
		procesos_bloqueados ++;
	} else {
		sem_post(&m_proceso_ejecutando);
	}


	return procesos_bloqueados + procesos_en_ready;
}

void *planificar_nuevos_procesos_largo_plazo(void *arg){



	while(1){
		sem_wait(&despertar_planificacion_largo_plazo);
		pthread_mutex_lock(&m_planificador_largo_plazo);
		sem_wait(&m_cola_ready);
		int tamanio_cola_ready = queue_size(cola_ready);
		sem_post(&m_cola_ready);
		sem_wait(&m_cola_new);
		int tamanio_cola_new = queue_size(cola_new);
		sem_post(&m_cola_new);

		int procesos_en_memoria_total = calcular_procesos_en_memoria(tamanio_cola_ready);

		// sumo uno para simular si agrego a ready el proceso qeu esta en new
		procesos_en_memoria_total ++;


		if(tamanio_cola_new != 0 && procesos_en_memoria_total <= grado_max_multiprogramacion){

			//verificar si se lo puede admitir a la cola de ready
			agregar_proceso_a_ready(socket_memoria, algoritmo_planificacion);
		}
		pthread_mutex_unlock(&m_planificador_largo_plazo);
	}

	return NULL;
}


void pasar_a_ready(t_pcb* proceso_bloqueado){

	sem_wait(&m_cola_ready);

	queue_push(cola_ready, proceso_bloqueado);

	char *pids = listar_pids_cola(cola_ready);
	sem_post(&m_cola_ready);


	log_info(logger, "Ingreso a Ready: “Cola Ready %s: [%s]“",algoritmo_planificacion, pids);

	free(pids);

	aviso_planificador_corto_plazo_proceso_en_ready(proceso_bloqueado);

}


void agregar_cola_new(t_pcb** pcb_proceso){
	sem_wait(&m_cola_new);
	queue_push(cola_new, *pcb_proceso);
	sem_post(&m_cola_new);

	log_info(logger, "Creación de Proceso: “Se crea el proceso %d en NEW“", (*pcb_proceso)->PID);
}

void pcb_args_destroy(t_pcb* pcb_a_destruir){
	free(pcb_a_destruir->proceso_estado);
	free(pcb_a_destruir->registros_CPU); // ver si rompe con contexto ejec

	if(pcb_a_destruir->tabla_archivos_abiertos_del_proceso != NULL){
		 void destroy_entrada_tabla_arch_abiertos_proceso(void*args){
			 t_tabla_de_archivos_por_proceso* entrada_n = (t_tabla_de_archivos_por_proceso*) args;

			 free(entrada_n->modo_apertura);
			 free(entrada_n->nombre_archivo);
			 free(entrada_n);
		 }

		list_destroy_and_destroy_elements(pcb_a_destruir->tabla_archivos_abiertos_del_proceso, destroy_entrada_tabla_arch_abiertos_proceso);
	}
}

void actualizar_estado_a_pcb(t_pcb* a_actualizar_estado, char* estado){

	free(a_actualizar_estado->proceso_estado);
	a_actualizar_estado->proceso_estado = string_new();
	string_append(&(a_actualizar_estado->proceso_estado), estado);
}

