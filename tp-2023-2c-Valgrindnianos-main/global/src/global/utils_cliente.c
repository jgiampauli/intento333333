#include <global/utils_cliente.h>


void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}

int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	int socket_cliente = socket(server_info->ai_family,server_info->ai_socktype, server_info->ai_protocol);

	connect(socket_cliente, server_info->ai_addr, server_info->ai_addrlen);

	freeaddrinfo(server_info);

	return socket_cliente;
}

void enviar_mensaje(char* mensaje, int socket_cliente, op_code codigo)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = codigo;

	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) +1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void crear_buffer(t_paquete* paquete)
{
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = 0;
	paquete->buffer->stream = NULL;
}


t_paquete* crear_paquete(op_code codigo)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = codigo;
	crear_buffer(paquete);
	return paquete;
}

void agregar_a_paquete_sin_agregar_tamanio(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio);

	memcpy(paquete->buffer->stream + paquete->buffer->size , valor, tamanio);

	paquete->buffer->size += tamanio;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
	paquete->buffer->stream = realloc(paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

	memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
	memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor, tamanio);

	paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
}

void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

// en base a un array de strings, lo pasa a un string
char* pasar_a_string(char** string_array){
	char* string = string_new();
	void _crear_string(char *contenido_n){
	    string_append(&string, contenido_n);
	}

	string_iterate_lines(string_array,_crear_string);

	return string;
}

char *listar_pids_cola(t_queue* cola_con_pids){
	char* pids = string_new();

	t_list* lista_cola = cola_con_pids->elements;

	void unir_pids(void *arg_pcb_n){
		t_pcb *pcb_n = (t_pcb *) arg_pcb_n;

		if(string_length(pids) == 0){
			string_append_with_format(&pids, "%d", pcb_n->PID);
		} else {
			string_append_with_format(&pids, ", %d", pcb_n->PID);
		}
	}

	list_iterate(lista_cola, unir_pids);

	return pids ;
}

char *listar_pids_cola_de_strings(t_queue* cola_con_pids){
	char* pids = string_new();

	t_list* lista_cola = cola_con_pids->elements;

	void unir_pids(void *arg_pcb_n){
		char *pid_n = (char *) arg_pcb_n;

		if(string_length(pids) == 0){
			string_append_with_format(&pids, "%s", pid_n);
		} else {
			string_append_with_format(&pids, ", %s", pid_n);
		}
	}

	list_iterate(lista_cola, unir_pids);

	return pids ;
}

char *listar_recursos_lista_recursos_por_condicion(t_list* lista_con_recursos, bool (*evaluar)(t_recurso *)){
	char *nombres = string_new();

	void apendear_recurso_si_cumple_condicion(void *arg_recurso){
		t_recurso *recurso_n = (t_recurso *) arg_recurso;

		if(evaluar(recurso_n)){
			if(string_length(nombres) == 0){
				string_append_with_format(&nombres, "%s", recurso_n->nombre_recurso);
			} else {
				string_append_with_format(&nombres, ", %s", recurso_n->nombre_recurso);
			}
		}
	}

	list_iterate(lista_con_recursos, apendear_recurso_si_cumple_condicion);

	return nombres;
}

char* listar_recursos_disponibles(int* recursos_disponibles, int cantidad_de_recursos){
	char* lista_recursos_disponibles= string_new();

	for(int i =0; i< cantidad_de_recursos; i++){
		int diponibilidad_recurso_n = recursos_disponibles[i];

		if(string_length(lista_recursos_disponibles) == 0){
			string_append_with_format(&lista_recursos_disponibles, "%d", diponibilidad_recurso_n);
		} else {
			string_append_with_format(&lista_recursos_disponibles, ", %d", diponibilidad_recurso_n);
		}
	}

	return lista_recursos_disponibles;
}

void esperar_por(int milisegundos_a_esperar){
	// el * 1000 es para pasarlo a microsegundos
	usleep(milisegundos_a_esperar*1000);
}



