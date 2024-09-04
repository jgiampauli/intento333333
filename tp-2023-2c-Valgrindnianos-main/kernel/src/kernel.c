#include "kernel.h"

int socket_cpu_dispatch;
int socket_cpu_interrupt;
int socket_kernel;
int socket_memoria;
int grado_max_multiprogramacion;
char **recursos;
int *recursos_disponible;
t_list *recursos_totales;
int quantum;
char** instancias_recursos;
int cant_recursos;
t_dictionary *matriz_recursos_asignados;
t_dictionary *matriz_recursos_pendientes;
t_dictionary *tabla_global_de_archivos_abiertos;
char* ip_filesystem;
char* puerto_filesystem;

int main(int argc, char *argv[]) {

	//Declaraciones de variables para config:

	char* ip_memoria;
	char* puerto_memoria;
	char* ip_cpu;
	char* puerto_cpu_dispatch;
	char* puerto_cpu_interrupt;


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
	ip_memoria = config_get_string_value(config, "IP_MEMORIA");
	puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");

	ip_filesystem = config_get_string_value(config, "IP_FILESYSTEM");
	puerto_filesystem = config_get_string_value(config, "PUERTO_FILESYSTEM");

	ip_cpu = config_get_string_value(config, "IP_CPU");
	puerto_cpu_dispatch = config_get_string_value(config,
			"PUERTO_CPU_DISPATCH");
	puerto_cpu_interrupt = config_get_string_value(config,
			"PUERTO_CPU_INTERRUPT");

	algoritmo_planificacion = config_get_string_value(config,
			"ALGORITMO_PLANIFICACION");
	quantum = config_get_int_value(config, "QUANTUM");
	grado_max_multiprogramacion = config_get_int_value(config,
			"GRADO_MULTIPROGRAMACION_INI");

	recursos = config_get_array_value(config, "RECURSOS");
	instancias_recursos = config_get_array_value(config, "INSTANCIAS_RECURSOS");

	// Control archivo configuracion
	if (!ip_memoria || !puerto_memoria || !ip_filesystem || !puerto_filesystem
			|| !ip_cpu || !puerto_cpu_dispatch || !puerto_cpu_interrupt
			|| !algoritmo_planificacion || !quantum || !recursos
			|| !instancias_recursos) {
		log_error(logger,
				"Error al recibir los datos del archivo de configuracion del kernel");
		terminar_programa(logger, config);
	}

	/*-------------------------------CONEXIONES KERNEL---------------------------------------------------------------*/

	// Realizar las conexiones y probarlas
	int result_conexion_memoria = conectar_memoria(ip_memoria, puerto_memoria);

	if (result_conexion_memoria == -1) {
		log_error(logger, "No se pudo conectar con el modulo Memoria !!");
		terminar_programa(logger, config);
	}

	log_info(logger,
			"El Kernel se conecto con el modulo Memoria correctamente");

	/*
	int result_conexion_filesystem = conectar_fs(ip_filesystem, puerto_filesystem);

	if (result_conexion_filesystem == -1) {
		log_error(logger, "No se pudo conectar con el modulo filesystem !!");
		terminar_programa(logger, config);
	}
	log_info(logger,"El Kernel se conecto con el modulo Filesystem correctamente");
*/


	//PUERTO INTERRUPT
	int result_conexion_cpu_interrupt = conectar_cpu_interrupt(ip_cpu,
			puerto_cpu_interrupt);

	if (result_conexion_cpu_interrupt == -1) {
		log_error(logger, "No se pudo conectar con el interrupt de la CPU !!");
		terminar_programa(logger, config);
	}

	log_info(logger,
			"El Kernel se conecto con el interrupt de la CPU correctamente");

	//PUERTO DISPATCH
	int result_conexion_cpu_dispatch = conectar_cpu_dispatch(ip_cpu,
			puerto_cpu_dispatch);

	if (result_conexion_cpu_dispatch == -1) {
		log_error(logger, "No se pudo conectar con el dispatch de la CPU !!");
		terminar_programa(logger, config);
	}

	log_info(logger,
			"El Kernel se conecto con el dispatch de la CPU correctamente");

	// creo array de int de recursos disponibles

	cant_recursos = string_array_size(instancias_recursos);
	recursos_totales = list_create();
	recursos_disponible = malloc(sizeof(int) * cant_recursos);

	if (cant_recursos != 0) {
		for (int i = 0; i < cant_recursos; i++) {
			recursos_disponible[i] = atoi(instancias_recursos[i]);

			t_recurso* recurso_n = recurso_new(recursos[i]);
			recurso_n->instancias_en_posesion= atoi(instancias_recursos[i]);
			list_add(recursos_totales, recurso_n);
		}
	}

	inicializar_colas_y_semaforos();

	// inicializo diccionarios para recursos bloqueados y archivos
	recurso_bloqueado = dictionary_create();
	matriz_recursos_asignados = dictionary_create();
	matriz_recursos_pendientes = dictionary_create();
	tabla_global_de_archivos_abiertos = dictionary_create();
	colas_de_procesos_bloqueados_para_cada_archivo = dictionary_create();
	colas_de_procesos_bloqueados_por_pf =dictionary_create();
	proceso_ejecutando = NULL;

	void _iterar_recursos(char *nombre_recurso) {
		t_queue *cola_bloqueados = queue_create();

		dictionary_put(recurso_bloqueado, nombre_recurso, cola_bloqueados);
	}

	string_iterate_lines(recursos, _iterar_recursos);

	//levanto 4 hilos para recibir peticiones de forma concurrente de los modulos

	pthread_t hilo_peticiones_cpu_dispatch, hilo_peticiones_cpu_interrupt, hilo_peticiones_memoria, hilo_planificador_largo_plazo, hilo_planificador_corto_plazo;

	t_args_manejar_peticiones_modulos *args_dispatch = malloc(sizeof(t_args_manejar_peticiones_modulos));
	t_args_manejar_peticiones_modulos *args_interrupt = malloc(sizeof(t_args_manejar_peticiones_modulos));
	t_args_manejar_peticiones_modulos *args_memoria = malloc(sizeof(t_args_manejar_peticiones_modulos));

	args_dispatch->cliente_fd = socket_cpu_dispatch;

	args_interrupt->cliente_fd = socket_cpu_interrupt;

	args_memoria->cliente_fd = socket_memoria;

	pthread_create(&hilo_planificador_corto_plazo, NULL, planificar_nuevos_procesos_corto_plazo, NULL);
	pthread_create(&hilo_planificador_largo_plazo, NULL, planificar_nuevos_procesos_largo_plazo, NULL);
	pthread_create(&hilo_peticiones_cpu_dispatch, NULL, escuchar_peticiones_cpu_dispatch, args_dispatch);
	pthread_create(&hilo_peticiones_cpu_interrupt, NULL, manejar_peticiones_modulos, args_interrupt);
	pthread_create(&hilo_peticiones_memoria, NULL, manejar_peticiones_modulos, args_memoria);

	pthread_detach(hilo_peticiones_cpu_dispatch);
	pthread_detach(hilo_peticiones_cpu_interrupt);
	pthread_detach(hilo_planificador_largo_plazo);
	pthread_detach(hilo_peticiones_memoria);

	//espero peticiones por consola
	levantar_consola();

	free(args_dispatch);
	free(args_interrupt);
	free(args_memoria);
	terminar_programa(logger, config);

} //Fin del main

/*-----------------------DECLARACION DE FUNCIONES--------------------------------------------*/

//Iniciar archivo de log y de config

t_log* iniciar_logger(void) {
	// con "false" le decis que no muestres log logs por stdout
	t_log *nuevo_logger = log_create("kernel.log", "Kernel", false,
			LOG_LEVEL_INFO);
	return nuevo_logger;
}

t_config* iniciar_config(void) {
	t_config *nueva_config = config_create("kernel.config");
	return nueva_config;
}

//Finalizar el programa

void terminar_programa(t_log *logger, t_config *config) {
	log_destroy(logger);
	config_destroy(config);
	close(socket_cpu_dispatch);
	close(socket_cpu_interrupt);
	close(socket_memoria);
}

// CREAR CONEXIONES --------------------------------------------------------------------

int conectar_memoria(char *ip, char *puerto) {

	socket_memoria = crear_conexion(ip, puerto);

	//enviar handshake
	enviar_mensaje("OK", socket_memoria, HANDSHAKE);

	op_code cod_op = recibir_operacion(socket_memoria);
	if (cod_op != HANDSHAKE) {
		return -1;
	}

	int size;
	char *buffer = recibir_buffer(&size, socket_memoria);

	if (strcmp(buffer, "OK") != 0) {
		return -1;
	}

	return 0;
}

int conectar_fs(char *ip, char *puerto, int *socket_filesystem) {

	*socket_filesystem = crear_conexion(ip, puerto);

	//enviar handshake
	enviar_mensaje("OK", *socket_filesystem , HANDSHAKE);

	op_code cod_op = recibir_operacion(*socket_filesystem );

	if (cod_op != HANDSHAKE) {
		return -1;
	}

	int size;
	char *buffer = recibir_buffer(&size, *socket_filesystem );

	if (strcmp(buffer, "OK") != 0) {
		return -1;
	}

	return 0;
}

int conectar_cpu_dispatch(char *ip, char *puerto) {

	socket_cpu_dispatch = crear_conexion(ip, puerto);

	//enviar handshake
	enviar_mensaje("OK", socket_cpu_dispatch, HANDSHAKE);
	op_code cod_op = recibir_operacion(socket_cpu_dispatch);

	if (cod_op != HANDSHAKE) {
		return -1;
	}

	int size;
	char *buffer = recibir_buffer(&size, socket_cpu_dispatch);

	if(strcmp(buffer, "OK") != 0){
		return -1;
	}

	return 0;
}

int conectar_cpu_interrupt(char *ip, char *puerto) {

	socket_cpu_interrupt = crear_conexion(ip, puerto);

	//enviar handshake
	enviar_mensaje("OK", socket_cpu_interrupt, HANDSHAKE);
	op_code cod_op = recibir_operacion(socket_cpu_interrupt);

	if(cod_op != HANDSHAKE){
		return -1;
	}

	int size;
	char *buffer = recibir_buffer(&size, socket_cpu_interrupt);

	if(strcmp(buffer, "OK") != 0){
		return -1;
	}

	return 0;
}

//aca se maneja las peticiones de todos los modulos menos los de CPU
void* manejar_peticiones_modulos(void *args) {
	t_args_manejar_peticiones_modulos * args_hilo = (t_args_manejar_peticiones_modulos *) args;
	int cliente_fd = args_hilo->cliente_fd;

	while (1) {

		int cod_op = recibir_operacion(cliente_fd);

		switch(cod_op){
			case HANDSHAKE:
				recibir_handshake(cliente_fd);
				break;
			case INICIAR_PROCESO:
				char* mensaje = recibir_mensaje(cliente_fd);
				log_info(logger, "Se recibio un %s de memoria, procede planificador de largo plazo", mensaje);
				sem_post(&memoria_lista);
				free(mensaje);
				break;
			case CREAR_PROCESO:
				mensaje = recibir_mensaje(cliente_fd);
				log_info(logger, "Se recibio un %s de memoria, procede planificador de largo plazo", mensaje);
				sem_post(&proceso_creado_memoria);
				free(mensaje);
				break;
			case PAGE_FAULT:
				//aca deberia llegar un ok
	 			mensaje = recibir_mensaje(socket_memoria);
				log_info(logger, "Se recibio un %s de memoria, procede el manejo del page fault", mensaje);
				sem_post(&m_espero_respuesta_pf);
				free(mensaje);
				break;
			case INTERRUPCION:
				mensaje = recibir_mensaje(socket_cpu_interrupt);
				log_info(logger, "se recibio %s de cpu por cpu interrupt", mensaje);

				sem_post(&espero_desalojo_CPU);
				sem_post(&espero_actualizacion_pcb);

				free(mensaje);

				break;
			case -1:
				log_error(logger, "El cliente se desconecto. Terminando servidor");
				return NULL;
			default:
				log_warning(logger, "Operacion desconocida. No quieras meter la pata, cod_op modulos: %d", cod_op);
				break;
		}
	}
}

// ---------------------------- Peticiones CPU ----------------------------------------------


struct args_cliente_fd_y_funcion {
	int cliente_fd;
	void (*puntero_a_func_a_ejecutar)(int, t_contexto_ejec*);
	t_contexto_ejec* contexto;
};

void * manejo_instruccion(void* args){
	struct args_cliente_fd_y_funcion *args_hilo = args;

	args_hilo->puntero_a_func_a_ejecutar(args_hilo->cliente_fd,args_hilo->contexto);

	free(args);

	return NULL;
}

void* escuchar_peticiones_cpu_dispatch(void *args) {

	t_args_manejar_peticiones_modulos * arg_hilo = (t_args_manejar_peticiones_modulos *) args;
	int cliente_fd = arg_hilo->cliente_fd;
	int cantidad_de_recursos = string_array_size(instancias_recursos);

	while(1){
		int cod_op = recibir_operacion(cliente_fd);

			switch (cod_op) {
				case HANDSHAKE:
					recibir_handshake(cliente_fd);
					break;
				case FINALIZAR_PROCESO:
					t_contexto_ejec* contexto = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_fin_proceso;
					struct args_cliente_fd_y_funcion *args_hilo_fin_proceso = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_fin_proceso->cliente_fd = cliente_fd;
					args_hilo_fin_proceso->puntero_a_func_a_ejecutar = finalinzar_proceso;
					args_hilo_fin_proceso->contexto = contexto;
					pthread_create(&hilo_manejo_fin_proceso, NULL, manejo_instruccion, args_hilo_fin_proceso);
					pthread_detach(hilo_manejo_fin_proceso);
					break;
				case APROPIAR_RECURSOS:
					apropiar_recursos(cliente_fd, recursos, recursos_disponible, cantidad_de_recursos);
					break;
				case DESALOJAR_RECURSOS:
					desalojar_recursos(cliente_fd, recursos, recursos_disponible, cantidad_de_recursos);
					break;
				case SLEEP:
					manejar_sleep(cliente_fd);
					break;
				case ABRIR_ARCHIVO:
					t_contexto_ejec* contexto_abrir_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_abrir_archivo;
					struct args_cliente_fd_y_funcion *args_hilo_abrir_archivo = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_abrir_archivo->cliente_fd = cliente_fd;
					args_hilo_abrir_archivo->puntero_a_func_a_ejecutar = enviar_a_fs_crear_o_abrir_archivo;
					args_hilo_abrir_archivo->contexto = contexto_abrir_archivo;
					pthread_create(&hilo_manejo_abrir_archivo, NULL, manejo_instruccion, args_hilo_abrir_archivo);
					pthread_detach(hilo_manejo_abrir_archivo);
					break;
				case CERRAR_ARCHIVO:
					t_contexto_ejec* contexto_cerrar_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_cerrar_archivo;
					struct args_cliente_fd_y_funcion *args_hilo_cerrar_archivo = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_cerrar_archivo->cliente_fd = cliente_fd;
					args_hilo_cerrar_archivo->puntero_a_func_a_ejecutar = cerrar_archivo;
					args_hilo_cerrar_archivo->contexto=contexto_cerrar_archivo;

					pthread_create(&hilo_manejo_cerrar_archivo, NULL, manejo_instruccion, args_hilo_cerrar_archivo);
					pthread_detach(hilo_manejo_cerrar_archivo);
					break;
				case TRUNCAR_ARCHIVO:
					t_contexto_ejec* contexto_truncar_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_truncar_archivo;
					struct args_cliente_fd_y_funcion *args_hilo_truncar_archivo = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_truncar_archivo->cliente_fd = cliente_fd;
					args_hilo_truncar_archivo->puntero_a_func_a_ejecutar = enviar_a_fs_truncar_archivo;
					args_hilo_truncar_archivo->contexto=contexto_truncar_archivo;

					pthread_create(&hilo_manejo_truncar_archivo, NULL, manejo_instruccion, args_hilo_truncar_archivo);
					pthread_detach(hilo_manejo_truncar_archivo);
					break;
				case APUNTAR_ARCHIVO:
					t_contexto_ejec* contexto_apuntar_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					reposicionar_puntero(cliente_fd, contexto_apuntar_archivo);
					break;
				case LEER_ARCHIVO:
					t_contexto_ejec* contexto_leer_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_leer_archivo;
					struct args_cliente_fd_y_funcion *args_hilo_leer_archivo = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_leer_archivo->cliente_fd = cliente_fd;
					args_hilo_leer_archivo->puntero_a_func_a_ejecutar = leer_archivo;
					args_hilo_leer_archivo->contexto=contexto_leer_archivo;
					pthread_create(&hilo_manejo_leer_archivo, NULL, manejo_instruccion, args_hilo_leer_archivo);
					pthread_detach(hilo_manejo_leer_archivo);
					break;
				case ESCRIBIR_ARCHIVO:
					t_contexto_ejec* contexto_escribir_archivo = recibir_contexto_de_ejecucion(cliente_fd);
					pthread_t hilo_manejo_escribir_archivo;
					struct args_cliente_fd_y_funcion *args_hilo_escribir_archivo = malloc(sizeof(struct args_cliente_fd_y_funcion ));
					args_hilo_escribir_archivo->cliente_fd = cliente_fd;
					args_hilo_escribir_archivo->puntero_a_func_a_ejecutar = escribir_archivo;
					args_hilo_escribir_archivo->contexto = contexto_escribir_archivo;

					pthread_create(&hilo_manejo_escribir_archivo, NULL, manejo_instruccion, args_hilo_escribir_archivo);
					pthread_detach(hilo_manejo_escribir_archivo);
					break;
				case PAGE_FAULT:
					manejar_page_fault(cliente_fd);//dentro levanta su propio hilo
					break;
				case INTERRUPCION:
					t_contexto_ejec *contexto_interrupcion = recibir_contexto_de_ejecucion(socket_cpu_dispatch);
					log_info(logger, "program_counter: %d", contexto_interrupcion->program_counter);


					sem_post(&espero_desalojo_CPU);

					sem_wait(&m_proceso_ejecutando);
					actualizar_pcb(contexto_interrupcion, proceso_ejecutando);
					sem_post(&m_proceso_ejecutando);

					contexto_ejecucion_destroy(contexto_interrupcion);

					sem_post(&espero_actualizacion_pcb);
					break;
				case -1:
					log_error(logger, "La CPU se desconecto. Terminando servidor ");
					free(recursos_disponible);
					return NULL;
				default:
					log_warning(logger,"CPU Operacion desconocida. No quieras meter la pata. cod_op: %d", cod_op );
					break;
			}
	}

	return NULL;
}

