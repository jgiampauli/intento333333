#ifndef GLOBAL_H_
#define GLOBAL_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/queue.h>
#include <commons/temporal.h>
#include <commons/log.h>
#include <commons/string.h>

extern t_log* logger;


typedef enum
{
	MENSAJE,
	HANDSHAKE,
	HANDSHAKE_TAM_MEMORIA,
	PAQUETE,
	// CPU
	CREAR_PROCESO,
	TERMINAR_PROCESO, //Libera la pcb, avisa a memoria y a consola
	FINALIZAR_PROCESO,
	BLOQUEAR_PROCESO,
	PETICION_KERNEL,
	APROPIAR_RECURSOS,
	DESALOJAR_RECURSOS,
	DESALOJAR_PROCESO,
	SLEEP,
	PROCESAR_INSTRUCCION,
	INTERRUPCION,
	PETICION_CPU,
	// memoria
	ACCESO_A_PAGINA,
	PAGE_FAULT,
	FINALIZAR_PROCESO_MEMORIA,
	READ_MEMORY,
	READ_MEMORY_RESPUESTA,
	WRITE_MEMORY,
	WRITE_MEMORY_RESPUESTA,
	INSTRUCCION,
	TAMANO_PAGINA, //se usa?
	// filesystem
	NUEVO_PROCESO_FS,
	FINALIZAR_PROCESO_FS,
	LEER_CONTENIDO_PAGINA,
	ESCRIBIR_CONTENIDO_PAGINA,
	ABRIR_ARCHIVO,
	ABRIR_ARCHIVO_RESPUESTA,
	CERRAR_ARCHIVO,
	APUNTAR_ARCHIVO,
	TRUNCAR_ARCHIVO,
	TRUNCAR_ARCHIVO_RESPUESTA,
	LEER_ARCHIVO,
	LEER_ARCHIVO_RESPUESTA,
	ESCRIBIR_ARCHIVO,
	ESCRIBIR_ARCHIVO_RESPUESTA,
	CREAR_ARCHIVO,
	CREAR_ARCHIVO_RESPUESTA,
	WRITE_MEMORY_FS,
	WRITE_MEMORY_FS_RESPUESTA,
	//consola kernel (FINALIZAR PROCESO REUTILIZA EL DE ARRIBA)
	INICIAR_PROCESO,
	DETENER_PLANIFICACION,
	INICIAR_PLANIFICACION,
	MULTIPROGRAMACION,
	PROCESO_ESTADO,
}op_code;


typedef struct {
	int opcode_lenght;
	char* opcode;
	int parametro1_lenght;
	int parametro2_lenght;
	int parametro3_lenght;
	char* parametros[3];

}t_instruccion;


// Registros de 4 bytes
typedef struct {
    uint32_t AX;
    uint32_t BX;
    uint32_t CX;
    uint32_t DX;
} registros_CPU;



typedef struct
{
	int pid;
	t_instruccion* instruccion;
	int program_counter;

	registros_CPU* registros_CPU;

} t_contexto_ejec;

typedef struct {
	char* nombre_archivo;
	int tamanio_archivo;
	int bloque_inicial;
} t_fcb;

typedef struct{
	char* nombre_archivo;
	int puntero_posicion; // se usa para el fseek
	char* modo_apertura;
}t_tabla_de_archivos_por_proceso;

typedef struct
{
	int PID;
	int program_counter;
	char* proceso_estado; // pueden ser "NEW", "READY", "EXEC", "BLOCKED" y "EXIT"
	int64_t tiempo_llegada_ready;
	registros_CPU* registros_CPU;

	int prioridad;

	t_instruccion* comando;

	// lista de t_tabla_de_archivos_por_proceso
	t_list* tabla_archivos_abiertos_del_proceso;

} t_pcb;

typedef struct {
	int read_lock_active; //numero de locks de lectura activos
	int read_lock_count; // número de locks de lectura total
	int write_lock_count; // número de locks de escritura total
	t_pcb* proceso_write_lock; // proceso que tiene el lock de escritura
	t_list* lista_locks_read; // procesos que tienen locks de lectura
	t_queue* cola_locks; // procesos bloqueados por lock ya sea write o read
}t_file_lock;

typedef struct{
	int fileDescriptor;
	char* file;
	int open;//contador_de_aperturas
	t_file_lock *lock_de_archivo;
}t_tabla_global_de_archivos_abiertos;

typedef struct  {
	char* nombre_recurso;
	int instancias_en_posesion;
} t_recurso;

#endif /* GLOBAL_H_ */
