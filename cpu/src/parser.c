#include "parser.h"
t_instruccion parsear(char* lineaAux)
{
	t_instruccion instruccion;

	if (string_is_empty(lineaAux)){
			instruccion.operacion = fin;

			return instruccion;
	} else if(string_starts_with(lineaAux,"#")){

		instruccion.operacion = ignorar;

		return instruccion;
	}

	char** operacion = string_split(lineaAux, " ");

	if(strcmp(operacion[0], "abrir") == 0){

		instruccion.operacion = abrir;
		instruccion.argumentos.abrir.path = operacion[1];

	} else if(strcmp(operacion[0], "asignar") == 0){

		instruccion.operacion = asignar;
		instruccion.argumentos.asignar.path = operacion[1];
		instruccion.argumentos.asignar.linea = atoi(operacion[2]);
		// Sumo el largo deoperacion, + 1 espacio, el path, + 1 espacio, + largo de linea, + 1 espacio
		int posInicial = strlen(operacion[0]) + 1 + strlen(operacion[1]) + 1 + strlen(operacion[2]) + 1;
		// Al largo de la linea, le resto lo que no es el valor a asignar, y le saco el /n
		int largoAAsignar = strlen(lineaAux) - posInicial;
		instruccion.argumentos.asignar.datos = string_substring(lineaAux, posInicial, largoAAsignar);
		printf("VALOR A ASIGNAR: %s\n", instruccion.argumentos.asignar.datos);
		free(operacion[2]);
	} else if(strcmp(operacion[0], "concentrar") == 0){

		instruccion.operacion = concentrar;

	} else if(strcmp(operacion[0], "wait") == 0){

			instruccion.operacion = wait;
			instruccion.argumentos.wait.recurso = operacion[1];

	} else if(strcmp(operacion[0], "signal") == 0){

			instruccion.operacion = signal;
			instruccion.argumentos.signal.recurso = operacion[1];

	} else if(strcmp(operacion[0], "flush") == 0){

			instruccion.operacion = flush;
			instruccion.argumentos.flush.path = operacion[1];

	} else if(strcmp(operacion[0], "close") == 0){

		instruccion.operacion = cerrar;
		instruccion.argumentos.cerrar.path = operacion[1];

	} else if(strcmp(operacion[0], "crear") == 0){

			instruccion.operacion = crear;
			instruccion.argumentos.crear.path = operacion[1];
			instruccion.argumentos.crear.lineas = atoi(operacion[2]);
			free(operacion[2]);

	} else if(strcmp(operacion[0], "borrar") == 0){

			instruccion.operacion = borrar;
			instruccion.argumentos.borrar.path = operacion[1];

	} else {
		instruccion.operacion = nada;
	}
	free(operacion[0]);
	free(operacion);
	return instruccion;
}
