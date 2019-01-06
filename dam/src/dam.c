#include "dam.h"

void conexionSAFA(){
	char* ip = config_get_string_value(config, IP_SAFA);
	char* port = config_get_string_value(config, PUERTO_SAFA);
	log_trace(loggerDam,"Creando socket cliente para SAFA");
	// Socket mediante el cual DAM se conecta a SAFA
	socketSAFA = clienteConectarComponenteConCabecera(DAM_PROCESS_NAME,
			SAFA_PROCESS_NAME, port, ip, DAM_SAFA, loggerDam);
	log_info(loggerDam, "DAM conectado a %s mediante el socket %d", SAFA_PROCESS_NAME, socketSAFA);
}

void conexionMDJ(){
	char* ip = config_get_string_value(config, IP_MDJ);
	char* port = config_get_string_value(config, PUERTO_MDJ);
	log_trace(loggerDam,"Creando socket cliente para MDJ");
	// Socket mediante el cual DAM se conecta a MDJ
	socketMDJ = clienteConectarComponenteConCabecera(DAM_PROCESS_NAME,
			MDJ_PROCESS_NAME, port, ip, DAM_MDJ, loggerDam);
	log_info(loggerDam, "DAM conectado a %s mediante el socket %d", MDJ_PROCESS_NAME, socketMDJ);
	// Envío mi transfer size para que MDJ sepa qué tamaños voy a enviar y recibir
	enviarCabecera(socketMDJ, "", ENVIAR_TRANSFER_SIZE, loggerDam);
	enviarMensajeInt(socketMDJ, transferSize);
}

void conexionFM9(){
	char* ip = config_get_string_value(config, IP_FM9);
	char* port = config_get_string_value(config, PUERTO_FM9);
	log_trace(loggerDam,"Creando socket cliente para FM9");
	// Socket mediante el cual DAM se conecta a FM9
	socketFM9 = clienteConectarComponenteConCabecera(DAM_PROCESS_NAME,
			FM9_PROCESS_NAME, port, ip, DAM_FM9, loggerDam);
	log_info(loggerDam, "DAM conectado a %s mediante el socket %d", FM9_PROCESS_NAME, socketFM9);
	// Envío mi transfer size para que FM9 sepa qué tamaños voy a enviar y recibir
	enviarMensajeInt(socketFM9, transferSize);
	// Recibo el máximo línea de FM9
	recibirMensajeInt(socketFM9, &maximoLineasFM9);
	log_trace(loggerDam, "DAM recibió de FM9 máximo de líneas: %i", maximoLineasFM9);
}

void pruebaMensajes(Cabecera* cabecera, t_socket socketActual, int handshake,
		sem_t semaforo, t_socket socketDestino){
	char *msj = malloc(cabecera->largoPaquete + 1);
	recibirMensaje(socketActual, &msj, (cabecera->largoPaquete));
	log_info(loggerDam, "Mensaje: %s", msj);
	sem_wait(&semaforo);
	enviarCabecera(socketDestino, msj, handshake, loggerDam);
	enviarMensaje(socketDestino, msj, strlen(msj));
	sem_post(&semaforo);
	free(msj);
}

char* pedirArchivoAMDJ(char* path, int *errorCode){
	int tamanioArchivo, codigoRespuesta = ERROR, tamanioArchivoProcesado = 0,
			offset = 0, tamanioParteArchivo;
	char* parteArchivo = malloc((transferSize * sizeof(char))+1);
	char* archivo = string_new();
	sem_wait(&usoMDJ);
	enviarCabecera(socketMDJ, "", OBTENER_DATOS, loggerDam);
	enviarMensajeInt(socketMDJ, strlen(path));
	enviarMensaje(socketMDJ, path, strlen(path));
	enviarMensajeInt(socketMDJ, offset);

	// Recibir un código por parte de MDJ: OK = 0, sino código de error
	recibirMensajeInt(socketMDJ, &codigoRespuesta);
	if(codigoRespuesta == OK){
		log_trace(loggerDam, "MDJ responde OK a obtener datos");
		recibirMensajeInt(socketMDJ, &tamanioArchivo);
		/* Mientras no se haya recibido el archivo completo, recibir
		 * partes de transfersize tamaño */
		while(tamanioArchivoProcesado < tamanioArchivo){
			recibirMensajeInt(socketMDJ, &tamanioParteArchivo);
			recibirMensaje(socketMDJ, &parteArchivo, tamanioParteArchivo);
			string_append(&archivo, parteArchivo);
			log_trace(loggerDam, "Parte de archivo recibida de MDJ: %s", parteArchivo);
			tamanioArchivoProcesado += strlen(parteArchivo);
		}
	} else {
		log_trace(loggerDam, "MDJ responde con código de error: %i", codigoRespuesta);
		*errorCode = codigoRespuesta;
	}
	sem_post(&usoMDJ);
	free(parteArchivo);
	return archivo;
}

int calcularCantidadLineas(char* archivo){
	int cantidadLineas = 0;
	const char *archivoRestante = archivo;
	while((archivoRestante = strstr(archivoRestante, separadorLineas))) {
	   cantidadLineas++;
	   archivoRestante++;
	}
	return cantidadLineas;
}

int enviarArchivoAFM9(char* archivo, int idGdt,	int *errorCode){
	char* lineaArchivo;
	char *textoArchivo;
	int cantidadCharLineaActual = 0, direccionMemoria = ERROR, codigoRespuesta = ERROR;
	char strArchivo[strlen(archivo)];
	strcpy(strArchivo, archivo);
	sem_wait(&usoFM9);
	enviarCabecera(socketFM9, "", SUBIR_ARCHIVO_FM9, loggerDam);
	int cantidadLineasTotales = calcularCantidadLineas(archivo);
	enviarMensajeInt(socketFM9, cantidadLineasTotales);
	enviarMensajeInt(socketFM9, idGdt);

	// Recibir un código por parte de FM9: OK = 0, sino código de error
	recibirMensajeInt(socketFM9, &codigoRespuesta);
	if(codigoRespuesta == OK){
		log_trace(loggerDam, "FM9 responde OK a subir archivo");
		char *parteLineaArchivo;
		textoArchivo = strtok(strArchivo, separadorLineas);
		int contadorLineas = 0;
		while(contadorLineas < cantidadLineasTotales){
			lineaArchivo = string_new();
			// Puede ser que sean líneas vacías
			if(textoArchivo != NULL){
				string_append(&lineaArchivo, textoArchivo);
			}
			string_append(&lineaArchivo, separadorLineas);
			cantidadCharLineaActual = strlen(lineaArchivo);
			// Si la cantidad de caracteres supera el máximo por línea, hay un error
			if(cantidadCharLineaActual > maximoLineasFM9){
				*errorCode = ERROR_CHARS_SUPERAN_MAXIMO;
				enviarMensajeInt(socketFM9, *errorCode);
				free(lineaArchivo);
				break;
			} else {
				enviarMensajeInt(socketFM9, OK);
				int cantidadCharProcesados = 0;
				while(cantidadCharProcesados < cantidadCharLineaActual){
					// Mientras no se haya enviado la línea completa, mandar partes de transferSize tamaño
					parteLineaArchivo = string_substring_until(lineaArchivo, transferSize);
					if(strlen(lineaArchivo) > transferSize){
						char* lineaArchivoTemporal = string_substring_from(lineaArchivo, transferSize);
						free(lineaArchivo);
						lineaArchivo = lineaArchivoTemporal;
						lineaArchivoTemporal = NULL;
					}
					enviarMensajeInt(socketFM9, strlen(parteLineaArchivo));
					log_trace(loggerDam, "Parte de archivo a enviar a FM9: %s", parteLineaArchivo);
					enviarMensaje(socketFM9, parteLineaArchivo, strlen(parteLineaArchivo));
					cantidadCharProcesados += strlen(parteLineaArchivo);
					// string_substring_until termina haciendo un malloc
					free(parteLineaArchivo);
				}
				textoArchivo = strtok(NULL, separadorLineas);
				free(lineaArchivo);
				contadorLineas++;
			}
		}
		if(*errorCode == OK){
			recibirMensajeInt(socketFM9, &direccionMemoria);
		}
	} else {
		log_trace(loggerDam, "FM9 responde con código de error: %i", codigoRespuesta);
		*errorCode = codigoRespuesta;
	}
	sem_post(&usoFM9);
	return direccionMemoria;
}

char* pedirArchivoAFM9(int idGdt, int direccionMemoria, int *errorCode){
	char *archivo = string_new();
	char* parteArchivo = malloc((transferSize * sizeof(char))+1);
	int codigoRespuesta, cantidadLineasTotales, tamanioParteArchivo;
	sem_wait(&usoFM9);
	enviarCabecera(socketFM9, "", SOLICITUD_FLUSH, loggerDam);
	enviarMensajeInt(socketFM9, direccionMemoria);
	enviarMensajeInt(socketFM9, idGdt);
	// Recibir un código por parte de FM9: OK = 0, sino código de error
	recibirMensajeInt(socketFM9, &codigoRespuesta);
	if(codigoRespuesta == OK){
		log_trace(loggerDam, "FM9 responde OK a flush");
		recibirMensajeInt(socketFM9, &cantidadLineasTotales);
		int cantidadLineasProcesadas = 0;
		while(cantidadLineasProcesadas < cantidadLineasTotales){
			recibirMensajeInt(socketFM9, &tamanioParteArchivo);
			recibirMensaje(socketFM9, &parteArchivo, tamanioParteArchivo);
			log_trace(loggerDam, "Parte de archivo recibida de FM9: %s", parteArchivo);
			// Si en la parte del archivo procesada hay un separador, incrementar líneas
			if(string_contains(parteArchivo, separadorLineas)){
				cantidadLineasProcesadas++;
			}
			string_append(&archivo, parteArchivo);
		}
	} else {
		log_trace(loggerDam, "FM9 responde con código de error: %i", codigoRespuesta);
		*errorCode = codigoRespuesta;
	}
	sem_post(&usoFM9);
	free(parteArchivo);
	return archivo;
}

void enviarArchivoAMDJ(char *archivo, char* path, int *errorCode){
	int tamanioArchivo = strlen(archivo), tamanioArchivoProcesado = 0, offset = 0;
	char *archivoRestante = archivo;
	sem_wait(&usoMDJ);
	enviarCabecera(socketMDJ, "", GUARDAR_DATOS, loggerDam);
	enviarMensajeInt(socketMDJ, strlen(path));
	enviarMensaje(socketMDJ, path, strlen(path));
	enviarMensajeInt(socketMDJ, offset);
	enviarMensajeInt(socketMDJ, tamanioArchivo);
	/* Mientras no haya enviado el archivo completo, sigo enviando
	 * partes de transfersize tamaño */
	while(tamanioArchivoProcesado < tamanioArchivo){
		char *parteArchivo = string_substring_until(archivoRestante, transferSize);
		if(strlen(archivoRestante) >= transferSize){
			char* archivoRestanteTemporal = string_substring_from(archivoRestante, transferSize);
			free(archivoRestante);
			archivoRestante = archivoRestanteTemporal;
			archivoRestanteTemporal = NULL;

		}
		enviarMensajeInt(socketMDJ, strlen(parteArchivo));
		log_trace(loggerDam, "Parte de archivo a enviar a MDJ: %s", parteArchivo);
		enviarMensaje(socketMDJ, parteArchivo, strlen(parteArchivo));
		tamanioArchivoProcesado += strlen(parteArchivo);
		free(parteArchivo);
	}
	free(archivoRestante);
	// Recibir un código por parte de MDJ: OK = 0, sino código de error
	recibirMensajeInt(socketMDJ, errorCode);
	sem_post(&usoMDJ);
}

char* obtenerEntradasSalidas(char *archivo){
	char *stringEntradasSalidas = string_new();
	char strArchivo[strlen(archivo)];
	strcpy(strArchivo, archivo);
	char* lineaArchivo = strtok(strArchivo, separadorLineas);
	int contadorLineas = -1;
	while(lineaArchivo != NULL){
		contadorLineas++;
		if(string_contains(lineaArchivo, OPERACION_ABRIR) ||
				string_contains(lineaArchivo, OPERACION_FLUSH) ||
				string_contains(lineaArchivo, OPERACION_CREAR)){
			// Si hay una operación de entrada/salida, se agrega el nro de línea
			char* contadorLineasChar = string_itoa(contadorLineas);
			string_append(&stringEntradasSalidas, contadorLineasChar);
			free(contadorLineasChar);
			string_append(&stringEntradasSalidas, ",");
		}
		lineaArchivo = strtok(NULL, separadorLineas);
	}
	/* Por último, se agrega la cantidad totales de líneas (Sin importar si hubo
	 *  o no entrada/salida en el archivo) */
	char *contadorLineasChar = string_itoa(contadorLineas);
	string_append(&stringEntradasSalidas, contadorLineasChar);
	free(contadorLineasChar);
	return stringEntradasSalidas;
}

void notificarResultadoFlushASAFA(int errorCode, int idGdt){
	char* archivosACerrar;
	int largoArchivosACerrar;
	sem_wait(&usoSAFA);
	if(errorCode == OK){
		log_info(loggerDam, "Se guardo exitosamente el archivo en MDJ");
		enviarCabecera(socketSAFA, "", PASAR_A_READY, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
	} else {
		// Informar el error que reportó DAM, MDJ o FM9
		log_info(loggerDam, "Hubo un error al guardar el archivo en MDJ");
		enviarCabecera(socketSAFA, "", ERROR_CARGA, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
		enviarMensajeInt(socketSAFA, errorCode);

		// Recibo de SAFA los archivos que tengo que limpiar de FM9
		recibirMensajeInt(socketSAFA, &largoArchivosACerrar);
		archivosACerrar = malloc(largoArchivosACerrar + 1);
		recibirMensaje(socketSAFA, &archivosACerrar, largoArchivosACerrar);
	}
	sem_post(&usoSAFA);

	if(errorCode != OK){
		enviarArchivosACerrar(archivosACerrar, idGdt);
	}
}

void notificarResultadoCrearBorrarASAFA(int errorCode, int idGdt){
	char* archivosACerrar;
	int largoArchivosACerrar;
	sem_wait(&usoSAFA);
	if(errorCode == OK){
		log_info(loggerDam, "La operación impactó exitosamente en MDJ");
		enviarCabecera(socketSAFA, "", PASAR_A_READY, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
	} else {
		// Informar el error que reportó DAM, MDJ o FM9
		enviarCabecera(socketSAFA, "", ERROR_CARGA, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
		enviarMensajeInt(socketSAFA, errorCode);

		// Recibo de SAFA los archivos que tengo que limpiar de FM9
		recibirMensajeInt(socketSAFA, &largoArchivosACerrar);
		archivosACerrar = malloc(largoArchivosACerrar + 1);
		recibirMensaje(socketSAFA, &archivosACerrar, largoArchivosACerrar);
	}
	sem_post(&usoSAFA);

	if(errorCode != OK){
		enviarArchivosACerrar(archivosACerrar, idGdt);
	}

}

void notificarResultadoPedirArchivoASAFA(int errorCode, int idGdt, int direccionMemoria, char* path){
	char* archivosACerrar;
	int largoArchivosACerrar;
	sem_wait(&usoSAFA);
	if(errorCode == OK){
		log_info(loggerDam, "El archivo se cargó exitosamente en FM9");
		enviarCabecera(socketSAFA, "", ARCHIVO_CARGADO, loggerDam);
		enviarMensajeInt(socketSAFA, strlen(path));
		enviarMensaje(socketSAFA, path, strlen(path));
		enviarMensajeInt(socketSAFA, direccionMemoria);
		enviarMensajeInt(socketSAFA, idGdt);
		sem_post(&usoSAFA);
	} else {
		// Informar el error que reportó DAM, MDJ o FM9
		enviarCabecera(socketSAFA, "", ERROR_CARGA, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
		enviarMensajeInt(socketSAFA, errorCode);

		// Recibo de SAFA los archivos que tengo que limpiar de FM9
		recibirMensajeInt(socketSAFA, &largoArchivosACerrar);
		archivosACerrar = malloc(largoArchivosACerrar + 1);
		recibirMensaje(socketSAFA, &archivosACerrar, largoArchivosACerrar);
	}
	sem_post(&usoSAFA);

	if(errorCode != OK){
		enviarArchivosACerrar(archivosACerrar, idGdt);
	}
}

void enviarArchivosACerrar(char* aCerrar, int idGdt){
	char** archivos = string_split(aCerrar, ",");
	int i = 0;
	sem_wait(&usoFM9);
	log_info(loggerDam, "Eliminando los archivos abiertos por el DTB abortado");
	while(archivos[i] != NULL){
		enviarCabecera(socketFM9, "", EJECUTAR_CLOSE, loggerDam);
		enviarMensajeInt(socketFM9, atoi(archivos[i]));
		enviarMensajeInt(socketFM9, idGdt);
		int resultadoClose;
		recibirMensajeInt(socketFM9, &resultadoClose);
		if(resultadoClose != OK){
			log_error(loggerDam, "Error liberando memoria en FM9 luego de finalizacion de DTB %i", idGdt);
		}
		free(archivos[i]);
		i++;
	}
	free(archivos);
	free(aCerrar);
	sem_post(&usoFM9);
}

void notificarDummyASAFA(int errorCode, int idGdt, int direccionMemoria, char* stringEntradasSalidas){
	sem_wait(&usoSAFA);
	if(errorCode == OK){
		log_info(loggerDam, "El DUMMY se cargó exitosamente en FM9");
		enviarCabecera(socketSAFA, "", INICIAR_DUMMY, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
		enviarMensajeInt(socketSAFA, direccionMemoria);
		enviarMensajeInt(socketSAFA, strlen(stringEntradasSalidas));
		enviarMensaje(socketSAFA, stringEntradasSalidas, strlen(stringEntradasSalidas));
		free(stringEntradasSalidas);
	} else {
		// Informar el error que reportó DAM, MDJ o FM9
		enviarCabecera(socketSAFA, "", ERROR_CARGA, loggerDam);
		enviarMensajeInt(socketSAFA, idGdt);
		enviarMensajeInt(socketSAFA, errorCode);
	}
	log_trace(loggerDam, "Notifique a Safa la finalizacion de la operacion Dummy");
	sem_post(&usoSAFA);
}

void manejarOperacionCpu(int socketActual, Cabecera* cabecera){
	switch (cabecera->idOperacion) {

		case CPU_DAM: {
			log_trace(loggerDam, "Una nueva CPU en socket: %d", socketActual);
			enviarCabecera(socketActual, "", CPU_DAM, loggerDam);
			break;
		}

		case PRUEBA_MENSAJE_FM9: {
			pruebaMensajes(cabecera, socketActual, PRUEBA_MENSAJE_FM9, usoFM9, socketFM9);
			break;
		}

		case PRUEBA_MENSAJE_MDJ: {
			pruebaMensajes(cabecera, socketActual, PRUEBA_MENSAJE_MDJ, usoMDJ, socketMDJ);
			break;
		}

		case PEDIR_ARCHIVO: {
			// Subir archivo a FM9 desde MDJ
			log_info(loggerDam, "Llegó operación de CPU para abrir un archivo");
			int errorCode = OK; // En errorCode se almacena el código de error
			int tamanioPath, direccionMemoria = 0, idGdt;
			recibirMensajeInt(socketActual, &idGdt);
			recibirMensajeInt(socketActual, &tamanioPath);
			char *path = malloc(tamanioPath + 1);
			recibirMensaje(socketActual, &path, tamanioPath);
			log_info(loggerDam, "Ehhh, voy a buscar %s para %i", path, idGdt);

			// Pedir archivo a MDJ
			log_info(loggerDam, "Recibiendo archivo de MDJ");
			char* archivo = pedirArchivoAMDJ(path, &errorCode);
			if(errorCode == OK){
				// Si el archivo tiene líneas, enviar a FM9
				log_trace(loggerDam, "Contenido del archivo: %s\n", archivo);
				if(string_contains(archivo, "\n")){
					log_info(loggerDam, "Enviando archivo a FM9");
					direccionMemoria = enviarArchivoAFM9(archivo, idGdt, &errorCode);
				} else {
					errorCode = ERROR_ARCHIVO_SIN_LINEAS;
				}
				free(archivo);
			}

			// Avisar a SAFA que el archivo fue subido o que hubo un error
			log_info(loggerDam, "Notificando a SAFA");
			notificarResultadoPedirArchivoASAFA(errorCode, idGdt, direccionMemoria, path);
			free(path);
			break;
		}

		case INICIAR_DUMMY: {
			// Subir escriptorio base a FM9 desde MDJ, siendo la operación DUMMY
			log_info(loggerDam, "Llegó operación INICIAR_DUMMY de CPU");
			int errorCode = OK; // En errorCode se almacena el código de error
			int tamanioPath, direccionMemoria = 0, idGdt;
			char *stringEntradasSalidas;

			recibirMensajeInt(socketActual, &idGdt);
			recibirMensajeInt(socketActual, &tamanioPath);
			char *path = malloc(tamanioPath + 1);
			recibirMensaje(socketActual, &path, tamanioPath);
			log_info(loggerDam, "Ehhh, voy a buscar %s para %i", path, idGdt);

			// Pedir archivo a MDJ
			log_info(loggerDam, "Recibiendo archivo de MDJ");
			char* archivo = pedirArchivoAMDJ(path, &errorCode);
			if(errorCode == OK){
				// Si el archivo tiene líneas, enviar a FM9. Sino, hay un error
				log_trace(loggerDam, "Contenido del archivo: %s\n", archivo);
				if(string_contains(archivo, "\n")){
					log_info(loggerDam, "Enviando archivo a FM9");
					direccionMemoria = enviarArchivoAFM9(archivo, idGdt, &errorCode);
					if(errorCode == OK){
						stringEntradasSalidas = obtenerEntradasSalidas(archivo);
						log_trace(loggerDam, "Lista de entradas salidas: %s\n", stringEntradasSalidas);
					}
				} else {
					errorCode = ERROR_ARCHIVO_SIN_LINEAS;
				}
				free(archivo);
			}
			// Avisar a SAFA que el archivo fue subido o que hubo un error
			log_info(loggerDam, "Notificando a SAFA");
			notificarDummyASAFA(errorCode, idGdt, direccionMemoria, stringEntradasSalidas);
			free(path);
			break;
		}

		case EJECUTAR_FLUSH: {
			// Subir archivo a MDJ desde FM9
			log_info(loggerDam, "Llegó operación EJECUTAR_FLUSH de CPU");
			int errorCode = OK; // En errorCode se almacena el código de error
			int tamanioPath, direccionMemoria, idGdt;
			recibirMensajeInt(socketActual, &idGdt);
			recibirMensajeInt(socketActual, &tamanioPath);
			char *path = malloc(tamanioPath + 1);
			recibirMensaje(socketActual, &path, tamanioPath);
			recibirMensajeInt(socketActual, &direccionMemoria);

			log_info(loggerDam, "Recibiendo archivo de FM9");
			char* archivo = pedirArchivoAFM9(idGdt, direccionMemoria, &errorCode);

			// Si no hubo errores por parte de FM9
			if(errorCode == OK){
				log_info(loggerDam, "Enviando archivo a MDJ");
				enviarArchivoAMDJ(archivo, path, &errorCode);
			}

			log_info(loggerDam, "Notificando a SAFA");
			notificarResultadoFlushASAFA(errorCode, idGdt);
			free(path);
			break;
		}

		case EJECUTAR_CREAR: {
			log_info(loggerDam, "Llegó operación EJECUTAR_CREAR de CPU");
			int errorCode = OK; // En errorCode se almacena el código de error
			int tamanioPath, idGdt, codigoRespuesta, cantidadLineas;
			recibirMensajeInt(socketActual, &idGdt);
			recibirMensajeInt(socketActual, &tamanioPath);
			char *path = malloc(tamanioPath + 1);
			recibirMensaje(socketActual, &path, tamanioPath);
			recibirMensajeInt(socketActual, &cantidadLineas);
			sem_wait(&usoMDJ);
			log_info(loggerDam, "Mandando a crear archivo a MDJ");
			enviarCabecera(socketMDJ, "", CREAR_ARCHIVO, loggerDam);
			enviarMensajeInt(socketMDJ, tamanioPath);
			enviarMensaje(socketMDJ, path, tamanioPath);
			// Enviar a MDJ la cantidad de bytes máximos que tendrá el archivo
			enviarMensajeInt(socketMDJ, cantidadLineas);
			// Recibir un código por parte de MDJ: OK = 0, sino código de error
			recibirMensajeInt(socketMDJ, &codigoRespuesta);
			sem_post(&usoMDJ);
			if(codigoRespuesta != OK){
				log_trace(loggerDam, "Hubo un error de código: %i", codigoRespuesta);
				errorCode = codigoRespuesta;
			}

			log_info(loggerDam, "Notificando a SAFA");
			notificarResultadoCrearBorrarASAFA(errorCode, idGdt);
			free(path);
			break;
		}

		case EJECUTAR_BORRAR: {
			log_info(loggerDam, "Llegó operación EJECUTAR_BORRAR de CPU");
			// En errorCode se almacena el código de error
			int errorCode = OK;
			int tamanioPath, idGdt, codigoRespuesta;
			recibirMensajeInt(socketActual, &idGdt);
			recibirMensajeInt(socketActual, &tamanioPath);
			char *path = malloc(tamanioPath + 1);
			recibirMensaje(socketActual, &path, tamanioPath);
			sem_wait(&usoMDJ);
			log_info(loggerDam, "Mandando a borrar archivo a MDJ");
			enviarCabecera(socketMDJ, "", BORRAR_ARCHIVO, loggerDam);
			enviarMensajeInt(socketMDJ, tamanioPath);
			enviarMensaje(socketMDJ, path, strlen(path));
			// Recibor un código por parte de MDJ: OK = 0, sino código de error
			recibirMensajeInt(socketMDJ, &codigoRespuesta);
			sem_post(&usoMDJ);
			if(codigoRespuesta != OK){
				log_trace(loggerDam, "Hubo un error de código: %i", codigoRespuesta);
				errorCode = codigoRespuesta;
			}

			log_info(loggerDam, "Notificando a SAFA");
			notificarResultadoCrearBorrarASAFA(errorCode, idGdt);
			free(path);
			break;
		}

		default: {
			// Operación inválida
			log_warning(loggerDam, "Código de operación no válido");
		}
	}
}

void* comunicacionCPU(void* arg){
	// TODO: Mejora: Estoy levantando un select común, pero debería procesar con hilos los pedidos de CPU
	levantarServidorSelect(serverSocket, loggerDam, &manejarOperacionCpu);
	return NULL;
}

void inicializar_config(){
	// Se busca el archivo .cfg tanto en root como en el directorio padre
	config = config_create("dam.cfg");
	if(config == NULL){
		config = config_create("../dam.cfg");
	}
}

void inicializar(){
	sem_init(&usoMDJ, 0, 1);
	sem_init(&usoFM9, 0, 1);
	sem_init(&usoSAFA, 0, 1);
	loggerDam = configure_logger(DAM_PROCESS_NAME, LOG_LEVEL_TRACE);
	inicializar_config();
	transferSize = atoi(config_get_string_value(config, TRANSFER_SIZE));
	char* ip = config_get_string_value(config, "IP");
	// Handshakes
	conexionSAFA();
	conexionMDJ();
	conexionFM9();
	port = config_get_string_value(config, PUERTO);
	serverSocket = crearServerSocket(ip, port, loggerDam);
	pthread_t hiloConexionCpu;
	int resultHiloConexion = pthread_create(&hiloConexionCpu, NULL, comunicacionCPU, NULL);
	if(resultHiloConexion != OK){
		log_error(loggerDam,"No se pudo realizar la comunicación con CPU\n");
	} else {
		log_trace(loggerDam,"Comunicación con CPU OK!\n");
	}
	config_destroy(config);
	log_trace(loggerDam,"Proceso %s inicializado", DAM_PROCESS_NAME);
	pthread_join(hiloConexionCpu, NULL);
}

void finalizarProceso(){
	close(socketSAFA);
	close(socketFM9);
	close(serverSocket);
	exit_gracefully(1, loggerDam);
}

int main(void) {
	inicializar();
	finalizarProceso();
	return 0;
}
