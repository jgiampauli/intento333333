#include "memoria_de_instrucciones.h"

void leer_pseudo(int cliente_fd){

	//recibe el path de la instruccion enviado por consola.c
	char* archivo_path;
	int pid;

	recibir_path_y_pid(cliente_fd, &archivo_path, &pid);

	char* path = string_new();
	string_append(&path, path_instrucciones);
	string_append(&path, "/");
	string_append(&path, archivo_path);

	if(string_contains(path, "./")){
		char* buffer = malloc(100*sizeof(char));
		getcwd(buffer, 100); // --> obtiene el path absoluto de "./" y lo guarda en buffer
		string_append(&buffer, "/"); // como no incluye el / se lo agrego aca
		path = string_replace(path, "./", buffer);
	}else if(string_contains(path, "~/")){
		path = string_replace(path, "~/", "/home/utnso/");
	}

	FILE* archivo = fopen(path,"r");


	//comprobar si el archivo existe
	if(archivo == NULL){
		log_error(logger, "Error en la apertura del archivo: Error: %d (%s)", errno, strerror(errno));
		free(path);
		return;
	}

	//declaro variables

	char* cadena;
	t_list* lista_instrucciones = list_create();

	//leo el pseudocodigo, pongo en la lista
	while(feof(archivo) == 0)
	{

		cadena = malloc(300);
		char *resultado_cadena = fgets(cadena, 300, archivo);



		//si el char esta vacio, hace break.
		if(resultado_cadena == NULL)
		{
			log_error(logger, "No encontre el archivo o esta vacio");
			break;
		}

		// se borra los '\n'
		if(string_contains(cadena, "\n")){
			char** array_de_cadenas = string_split(cadena, "\n");

			cadena = string_array_pop(array_de_cadenas);


			while(strcmp(cadena, "") == 0){
				cadena = string_array_pop(array_de_cadenas);
			}

			string_array_destroy(array_de_cadenas);
		}

		/*ya tengo la linea del codigo, ahora tengo que separarlos en opcode
		y los parametros. Esta dividido por un espacio*/

		//creo t_instruccion
		t_instruccion *ptr_inst = malloc(sizeof(t_instruccion));

		//leo el opcode
		char* token = strtok(cadena," ");
		ptr_inst->opcode = token;

		ptr_inst->opcode_lenght = strlen(ptr_inst->opcode) + 1;


		ptr_inst->parametros[0] = NULL;
		ptr_inst->parametros[1] = NULL;
		ptr_inst->parametros[2] = NULL;

		//leo parametros
		token = strtok(NULL, " ");
		int n = 0;
		while(token != NULL)
		{
			ptr_inst->parametros[n] = token;
			token = strtok(NULL, " ");
			n++;
		}

		if(ptr_inst->parametros[0] != NULL){
			ptr_inst->parametro1_lenght = strlen(ptr_inst->parametros[0])+1;
		} else {
			ptr_inst->parametro1_lenght = 0;
		}
		if(ptr_inst->parametros[1] != NULL){
			ptr_inst->parametro2_lenght = strlen(ptr_inst->parametros[1])+1;
		} else {
			ptr_inst->parametro2_lenght = 0;
		}
		if(ptr_inst->parametros[2] != NULL){
			ptr_inst->parametro3_lenght = strlen(ptr_inst->parametros[2])+1;
		} else {
			ptr_inst->parametro3_lenght = 0;
		}

		list_add(lista_instrucciones,ptr_inst);

	}

	dictionary_put(lista_instrucciones_porPID, string_itoa(pid),lista_instrucciones);

	enviar_mensaje("OK", cliente_fd, INICIAR_PROCESO);

	free(path);
	fclose(archivo);
}

void enviar_instruccion_a_cpu(int cliente_cpu,int retardo_respuesta)
{

	int program_counter ;
	int pid ;

	recibir_programcounter(cliente_cpu, &pid, &program_counter);


	t_list* lista_instrucciones = dictionary_get(lista_instrucciones_porPID,string_itoa(pid));

	if(lista_instrucciones == NULL){
		log_error(logger, "No encontre lista de instrucciones despues de leer el pseudocodigo");
	}

	//consigo la instruccion
	t_instruccion *instruccion = list_get(lista_instrucciones,program_counter-1);

	//Una vez obtenida la instruccion, creo el paquete y serializo
	t_paquete *paquete_instruccion = crear_paquete(INSTRUCCION);

	agregar_a_paquete(paquete_instruccion,instruccion->opcode,instruccion->opcode_lenght);
	agregar_a_paquete(paquete_instruccion,instruccion->parametros[0],instruccion->parametro1_lenght);
	agregar_a_paquete(paquete_instruccion,instruccion->parametros[1],instruccion->parametro2_lenght);
	agregar_a_paquete(paquete_instruccion,instruccion->parametros[2],instruccion->parametro3_lenght);

	esperar_por(retardo_respuesta);

	//envio paquete
	enviar_paquete(paquete_instruccion,cliente_cpu);
	// libero memoria
	eliminar_paquete(paquete_instruccion);
}
