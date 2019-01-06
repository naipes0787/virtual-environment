#include "mdj.h"

int main(void) {
	// creo logger y cargo configuracion desde el .cfg
	puts("Inicializando MDJ.");
	inicializar();

	// Compruebo la existencia de Archivos Metadata y Bitmap
	comprobarFIFA();

	//cargo el bitmap en "Memoria"
	puts("Cargando Bitmap...");
	cargarBitmap();


	// Levanto hilo para consola
	pthread_t hiloConsola;
	int result = pthread_create(&hiloConsola, NULL, consolaMDJ, (void*) NULL);
	if(result != 0){
		printf("\ncan't create thread :[%s]", strerror(result));
	} else {
		puts("Hilo consola creado");
	}
	//Creo el socket server de MDJ
	serverSocket = crearServerSocket(ip, port, loggerMDJ);

	// me conecto con DAM
	comunicacionConDAM();

	// Levanto hilo para trabajar con DAM
	pthread_t hiloDAM;
	int resultado = pthread_create(&hiloDAM, NULL, gestionarPedidosDam, (void*) &socketDam);
	if(resultado != 0){
			printf("\ncan't create thread :[%s]", strerror(resultado));
		} else {
			puts("Hilo DAM creado");
		}

	// finalizo
	pthread_join(hiloConsola, NULL);
	pthread_join(hiloDAM,NULL);
	log_info(loggerMDJ, "Hilos de ejecución cerrados");
	liberarRecursos();
	log_info(loggerMDJ, "Recursos liberados. Finalizando MDJ!");
	exit_gracefully(1, loggerMDJ);
	return 0;
}


/*
 * comprobarFIFA : Comprueba la existencia de los archivos Metadata y Bitmap *********************************************************************
 */

void comprobarFIFA(){
	/* El directorio mnt debe estar ubicado al mismo nivel que el directorio del proyecto (tp-2018-2c-Tek).
	 * Para que mnt sea encontrado, el proceso mdj debe ejecutarse desde el directorio tp-2018-2c-Tek/mdj
	 * de la siguiente forma: ./Debug/mdj */
	// Abro la metadata
	char* ubicacionMeta = ubicacionMetadata();
	char* ubicacionBitm = ubicacionBitmap();
	FILE* fdMetadata = fopen(ubicacionMeta,"r");
	FILE* fdBitmap = fopen(ubicacionBitm,"r");
  
	// Si ambos File Decryptor dan vacios, o alguno, no existen los archivos en el path indicado
	if(fdBitmap == NULL || fdBitmap == NULL){
		puts("No se pudo comprobar la existencia de archivos Metadata o Bitmap de FIFA, finalizando proceso...");
		if(fdMetadata != NULL){
			fclose(fdMetadata);
		}
		if(fdBitmap != NULL){
			fclose(fdBitmap);
		}
		config_destroy(config); // no pongo liberar recursos, porque involucra al bitmap, y en esta etapa no se creo todavia, pero si debo liberar al config
		exit_gracefully(1, loggerMDJ);
	}
	else{
		fclose(fdMetadata);
		fclose(fdBitmap);
		puts("Se pudo comprobar la existencia de archivos Metadata o Bitmap de FIFA, iniciando proceso...");
	}
	free(ubicacionMeta);
	free(ubicacionBitm);
}

char* ubicacionMetadata(){
	char* ubicacionMeta = string_new();
	char* base = "../../";
	string_append(&ubicacionMeta, base);
	string_append(&ubicacionMeta, puntoMontaje);
	string_append(&ubicacionMeta, "/Metadata/Metadata.bin");
	return ubicacionMeta;
}

char* ubicacionBitmap(){
	char* ubicacionBitm = string_new();
	char* base = "../../";
	string_append(&ubicacionBitm, base);
	string_append(&ubicacionBitm, puntoMontaje);
	string_append(&ubicacionBitm, "/Metadata/Bitmap.bin");
	return ubicacionBitm;

}

/*
 * CargarBitmap :  crea el Bitarray para manejar el bitmap ****************************************************************************************************
 */

void cargarBitmap(){
	// me traigo la información del archivo metadata para manipularla
	char* ubicacionMeta = ubicacionMetadata();
	t_config* metadataCfg = config_create(ubicacionMeta);
	metadata.cantidadBloques = config_get_int_value(metadataCfg, "CANTIDAD_BLOQUES");
	metadata.tamanioBloques = config_get_int_value(metadataCfg, "TAMANIO_BLOQUES");
	metadata.magicNumber = config_get_string_value(metadataCfg,"MAGIC_NUMBER");
	// Creo y cargo el bitArray para manejar el Bitmap
	int bitsBitArray = (metadata.cantidadBloques/8) + 1; //cada bloque es un bit en el array, cada 8 tengo un byte..
	char* ubicacionBitm = ubicacionBitmap();
	int fdBitmap = open(ubicacionBitm,O_RDWR,S_IRUSR | S_IWUSR);
	struct stat fdStat;
	if(fstat(fdBitmap, &fdStat) == -1){
		perror("No se pudo leer el Bitmap.\n");
		close(fdBitmap);
		free(ubicacionMeta);
		free(ubicacionBitm);
		config_destroy(metadataCfg);
		exit_gracefully(1, loggerMDJ);
	}
	char* bitArray = mmap(NULL,fdStat.st_size +1,PROT_READ | PROT_WRITE , MAP_SHARED, fdBitmap,0);
	bitmap = bitarray_create_with_mode(bitArray, bitsBitArray, MSB_FIRST);
	puts("Se ha cargado el Bitmap!");
	free(ubicacionMeta);
	free(ubicacionBitm);
	config_destroy(metadataCfg);
	close(fdBitmap);
}

/*
 * Inicializar: Cargo el logger y la configuracion del mdj.cfg **********************************************************************************************
 */

void inicializar() {
	initializeLogger();
	crearConfiguracion();

}

void initializeLogger() {
	loggerMDJ = configure_logger("MDJ", LOG_LEVEL_TRACE);
}

void inicializar_config(){
	// Se busca el archivo .cfg tanto en root como en el directorio padre
	config = config_create("mdj.cfg");
	if(config == NULL){
		config = config_create("../mdj.cfg");
	}
}

void crearConfiguracion() {
	inicializar_config();
	ip = config_get_string_value(config, "IP");
	port = config_get_string_value(config, "PUERTO");
	retardo = config_get_int_value(config, "RETARDO");
	puntoMontaje = config_get_string_value(config, "PUNTO_MONTAJE");
	log_trace(loggerMDJ, "Configuracion inicializada");
	log_info(loggerMDJ, "La IP es: %s", ip);
	log_info(loggerMDJ, "El PUERTO es: %s", port);
	log_info(loggerMDJ, "El RETARDO es: %d", retardo);
	log_info(loggerMDJ, "El PUNTO DE MONTAJE es: %s", puntoMontaje);
}

/*
 * Comunicacion con DAM : establece la comunicacion con el DAM **********************************************************************************************
 */

void comunicacionConDAM(){
	socketDam = servidorConectarComponenteConCabecera(serverSocket, DAM_MDJ, loggerMDJ);
	if(socketDam != 0){
		log_info(loggerMDJ, "Se conecto DAM en el socket %i", socketDam);
	}
}

/*
 * GestionarPedidosDAM: funcion central, manejo los pedidos que recibo de DAM *******************************************************************************
 */

void* gestionarPedidosDam(void* socketVoid){
	int socketActual = *(int*) socketVoid;
	while(true){
		int resultado;
		Cabecera* cabecera = recibirCabecera(socketActual, &resultado, loggerMDJ);
		if(resultado > 0){
			switch (cabecera->idOperacion) {
				case DAM_MDJ: {
					// Es sólo un handshake entre DAM y MDJ
					log_info(loggerMDJ, "DAM se conectó en socket: %d", socketActual);
					enviarCabecera(socketActual, "", DAM_MDJ, loggerMDJ);
					break;
				}
				case PRUEBA_MENSAJE_MDJ: {
					char *msj = malloc(cabecera->largoPaquete +1);
					recibirMensaje(socketActual, &msj, (cabecera->largoPaquete));
					log_info(loggerMDJ, "Llegó el mensaje reenviado de CPU: %s", msj);
					free(msj);
					break;
				}
				case ENVIAR_TRANSFER_SIZE: {
					recibirMensajeInt(socketActual,&transferSizeDAM);
					log_info(loggerMDJ, "Transfer size: %d", transferSizeDAM);
					break;
				}
				case VALIDAR_ARCHIVO : ;//Validar Archivo
					log_trace(loggerMDJ, "Se recibio Validar Archivo!");
					//Recibo los parametros de Validar Archivo
					int largoPathVA;
					recibirMensajeInt(socketActual,&largoPathVA);
					char *pathVA = malloc(largoPathVA+1);
					recibirMensaje(socketActual,&pathVA,largoPathVA);
					log_info(loggerMDJ, "Se recibio el path: %s",pathVA);
					//Aplico el retardo de la operacion
					usleep(retardo*1000);
					//Ejecuto la funcion Validar Archivo
					log_info(loggerMDJ, "Ejecutando Validar Archivo con path: %s",pathVA);
					int resultadoValidar;
					resultadoValidar = validarArchivo(pathVA);
					//Envio el resultado
					log_info(loggerMDJ, "Informando al DAM el Resultado: %d", resultadoValidar);
					enviarMensajeInt(socketActual, resultadoValidar);
					// Libero memoria
					log_info(loggerMDJ, "Ya informe al DAM. A la espera de la siguiente orden!");
					free(pathVA);
					break;
				case CREAR_ARCHIVO: ;//Crear Archivo
					log_trace(loggerMDJ, "Se recibio Crear Archivo!");
					// Recibo los parametros de Crear Archivo
					int largoPathCA;
					recibirMensajeInt(socketActual,&largoPathCA);
					char *pathCA = malloc(largoPathCA+1);
					recibirMensaje(socketActual,&pathCA,largoPathCA);
					int cantidadLineas;
					recibirMensajeInt(socketActual,&cantidadLineas);
					log_info(loggerMDJ, "Se recibio el path: %s",pathCA);
					log_info(loggerMDJ, "Se recibio cantidad de lineas: %d",cantidadLineas);
					//Aplico el retardo de la operacion
					usleep(retardo*1000);
					// ejecuto la funcion Crear Archivo
					log_info(loggerMDJ, "Ejecutando Crear Archivo con path: %s y cantidad lineas: %d", pathCA, cantidadLineas);
					int resultadoCrear;
					resultadoCrear = crearArchivo(pathCA, cantidadLineas);
					// envio resultado
					log_info(loggerMDJ, "Informando al DAM el Resultado: %d", resultadoCrear);
					enviarMensajeInt(socketActual,resultadoCrear);
					//Libero Memoria
					log_info(loggerMDJ, "Ya informe al DAM. A la espera de la siguiente orden!");
					free(pathCA);
					break;
				case OBTENER_DATOS: //Obtener Datos
					log_trace(loggerMDJ, "Se recibio Obtener Datos!");
					//Recibo los parametros de Obtener Datos
					int largoPathOD;
					recibirMensajeInt(socketActual, &largoPathOD);
					char *pathOD = malloc(largoPathOD+1);
					recibirMensaje(socketActual,&pathOD,largoPathOD);
					int offsetOD;
					recibirMensajeInt(socketActual, &offsetOD);
//					int sizeOD;
//					recibirMensajeInt(socketActual, &sizeOD);
					log_info(loggerMDJ, "Se recibio el path: %s",pathOD);
					log_info(loggerMDJ, "Se recibio el offset: %d",offsetOD);
					//Aplico el retardo de la operacion
					usleep(retardo*1000);
					log_info(loggerMDJ, "Validando la existencia de archivo en path: %s",pathOD);
					int resultadoObtener = validarArchivo(pathOD);
					//envio el resultado tomando en cuenta el transfer size en caso de no haber error
					if (resultadoObtener == ABRIR_PATH_INEXISTENTE){
						log_info(loggerMDJ, "No se encontro el archivo solicitado");
						enviarMensajeInt(socketActual,resultadoObtener);
					}else{
						enviarMensajeInt(socketActual,OK); // se envia el OK para avisar a DAM que puede continar
						log_info(loggerMDJ, "Ejecutando Obtener Datos en path: %s con offset:  %d",pathOD,offsetOD);
						char* datosObtenidos = obtenerDatos(pathOD,offsetOD,0);
						int tamanioMensaje = strlen(datosObtenidos);
						// Se envía el tamaño total del archivo
						log_info(loggerMDJ, "Se enviara a DAM un archivo de tamaño: %d",tamanioMensaje);
						enviarMensajeInt(socketActual,tamanioMensaje);
						while (tamanioMensaje>0){

							// si el tamaño del mensaje es menor al transfer size, lo mando entero
							if(tamanioMensaje <= transferSizeDAM){
								enviarMensajeInt(socketActual,tamanioMensaje);
								enviarMensaje(socketActual,datosObtenidos,tamanioMensaje);
								tamanioMensaje = 0;
							}else{
								char* mensajeParcial = string_substring_until(datosObtenidos,transferSizeDAM);
								enviarMensajeInt(socketActual,transferSizeDAM);
								enviarMensaje(socketActual,mensajeParcial,transferSizeDAM);
								char *datosObtenidosTemporal = string_substring_from(datosObtenidos, transferSizeDAM);
								free(datosObtenidos);
								datosObtenidos = datosObtenidosTemporal;
								datosObtenidosTemporal = NULL;
								tamanioMensaje = tamanioMensaje - transferSizeDAM;
								free(mensajeParcial);
							}
						}
						log_info(loggerMDJ, "Se enviaron los datos pedidos!");
						free(datosObtenidos);
					}
					log_info(loggerMDJ, "Ya informe al DAM. A la espera de la siguiente orden!");
					free(pathOD);
					break;
				case GUARDAR_DATOS: //Guardar Datos
					log_trace(loggerMDJ, "Se recibio Guardar Datos!");
					//Recibo los parametros de Guardar Datos
					int largoPathGD;
					recibirMensajeInt(socketActual,&largoPathGD);
					char* pathGD = malloc(largoPathGD+1);
					recibirMensaje(socketActual,&pathGD,largoPathGD);
					int offsetGD;
					recibirMensajeInt(socketActual,&offsetGD);
					int sizeGD;
					recibirMensajeInt(socketActual,&sizeGD);
					log_info(loggerMDJ, "Se recibio el path: %s",pathGD);
					log_info(loggerMDJ, "Se recibio el offset: %d",offsetGD);
					log_info(loggerMDJ, "Se recibio el size: %d",sizeGD);
					// La memoria de bufferGD se libera en otra función al guardarDatos
					char* bufferGD = string_new();
					// el DAM me va a mandar de a cachos  debido al transferSize
					int tamanioDelBuffer = sizeGD;

					while(tamanioDelBuffer > 0){
						if(tamanioDelBuffer < transferSizeDAM){ // por aca entra cuando la ultima porcion del buffer a escribir es menor al transfer size
							int tamanioFinalRecibido;
							recibirMensajeInt(socketActual,&tamanioFinalRecibido);
							char* auxFinal = malloc(tamanioFinalRecibido + 1);
							recibirMensaje(socketActual,&auxFinal,tamanioFinalRecibido);
							string_append(&bufferGD, auxFinal);
							free(auxFinal);
							tamanioDelBuffer = 0;
						}else{
							int tamanioRecibido;
							recibirMensajeInt(socketActual,&tamanioRecibido);
							char* aux = malloc(tamanioRecibido +1);
							recibirMensaje(socketActual,&aux,tamanioRecibido);
							string_append(&bufferGD, aux);
							free(aux);
							tamanioDelBuffer = tamanioDelBuffer - tamanioRecibido;
						}
					}
					//Aplico el retardo de la operacion
					usleep(retardo*1000);
					printf("RECIBI PARA GUARDAR: %s", bufferGD);
					log_info(loggerMDJ, "Ejecutando Guardar Datos en path: %s con offset: %d ,size: %d",pathGD,offsetGD,sizeGD);
					// ejecuto guardar datos
					int resultadoGD;
					resultadoGD = guardarDatos(pathGD,offsetGD,sizeGD,bufferGD);
					if(resultadoGD == 0){
						log_info(loggerMDJ, "Se guardaron correctamente los datos");
					}
					//Envio el resultado
					log_info(loggerMDJ, "Informando al DAM el Resultado: %d", resultadoGD);
					enviarMensajeInt(socketActual,resultadoGD);
					//libero memoria
					log_info(loggerMDJ, "Ya informe al DAM. A la espera de la siguiente orden!");
					free(pathGD);
					break;
				case BORRAR_ARCHIVO: //Borrar Archivo
					log_trace(loggerMDJ, "Se recibio Borrar Archivo!");
					//Recibo los parametros de Borrar Archivo
					int largoPathBA;
					recibirMensajeInt(socketActual,&largoPathBA);
					char *pathBA = malloc(largoPathBA+1);
					recibirMensaje(socketActual,&pathBA,largoPathBA);
					log_info(loggerMDJ, "Se recibio el path: %s",pathBA);
					//Aplico el retardo de la operacion
					usleep(retardo*1000);
					//Ejecuto la funcion Borrar Archivo
					log_info(loggerMDJ, "Ejecutando Borrar Archivo con path: %s",pathBA);
					int resultadoBorrar;
					resultadoBorrar = borrarArchivo(pathBA);
					//Envio el resultado
					log_info(loggerMDJ, "Informando al DAM el Resultado: %d", resultadoBorrar);
					enviarMensajeInt(socketActual,resultadoBorrar);
					// Libero memoria
					log_info(loggerMDJ, "Ya informe al DAM. A la espera de la siguiente orden!");
					free(pathBA);
					break;
				default: //No se selecciono una accion valida
					perror("No se selecciono una accion valida!!!!!!");
					int resultadoError = -1;
					enviarMensajeInt(socketActual,resultadoError);
			} //END SWITCH
		} else {
			log_error(loggerMDJ, "DAM se desconecto de MDJ. Finalizando");
			free(cabecera); // aca tambien debo liberar esta cabecera antes de cerrar
			liberarRecursos(); // se agrega para liberar recursos ante un
			log_info(loggerMDJ, "Recursos liberados. Finalizando MDJ!");
			exit_gracefully(1, loggerMDJ);
		}
		free(cabecera);
	}
}

/*
 * Funcionalidades auxiliares del MDJ **********************************************************************************************************************
 */

int tamanioPathArchivo(char* pathArchivo){
	return (strlen("../..")+strlen(puntoMontaje)+strlen("/Archivos/")+strlen(pathArchivo)+1);
}

int tamanioPathBloque(char* bloque){
	return (strlen("../..")+strlen(puntoMontaje)+strlen("/Bloques/")+strlen(bloque)+strlen(".bin")+1);
}

char* armarPathArchivo(char* direccion, char* pathArchivo){

	//La estructura del FS no va a cambiar, por lo tanto...
	//char* pathDelBitmap = string_new();

	//realpath("/fifa-examples/fifa-entrega/Metadata/Bitmap.bin",pathDelBitmap);

	string_append(&direccion,"../..");
	string_append(&direccion, puntoMontaje);
	string_append(&direccion, "/Archivos/");
	string_append(&direccion, pathArchivo);
	return direccion;
}

char* armarStringBloques(int bloques[], int cantidadBloques){
	char *arrayBloques = string_new();
	string_append(&arrayBloques, "BLOQUES=[");
	int i;
	int bloque;
	char *bloqueChar;
	for(i=0; i< cantidadBloques; i++){
		bloque = bloques[i];
		bloqueChar = string_itoa(bloque);
		string_append(&arrayBloques, bloqueChar);
		free(bloqueChar);
		if((i+1) < cantidadBloques){
			string_append(&arrayBloques, ",");
		}
	}
	string_append(&arrayBloques, "]\n");
	return arrayBloques;
}

int hayEspacioDisponible(int size){
	int bloquesNecesarios = (size/metadata.tamanioBloques)+1;
	int i= 0;
	int bloquesLibres = 0;

	size_t tamanioBitmap = bitarray_get_max_bit(bitmap);

	while(i<tamanioBitmap){
		bool valorBit;
		valorBit = bitarray_test_bit(bitmap,i);
		if (valorBit == true){
			i ++;
		}else{
			bloquesLibres ++;
			i++;
		}
	}
	if(bloquesLibres>=bloquesNecesarios){
		// hay la suficiente cantidad de bloques libres
		log_info(loggerMDJ, "Hay espacio disponible para guardar el archivo");
		return 1;
	}else{
		// no hay la suficiente cantidad de bloques libres
		log_info(loggerMDJ, "No hay espacio disponible para guardar el archivo");
		return -1;
	}
}

void asignarBloques(int arrayBloques[], int posArray, int cantBloquesAAsignar){

	unsigned int k = 0;
	unsigned int j;

	while (k<cantBloquesAAsignar){
		for (j=0; j< metadata.cantidadBloques;j++){
			if (bitarray_test_bit(bitmap,j) == false){ //si el bit es 0, significa que el bloque en la posicion j esta libre
				bitarray_set_bit(bitmap,j); //seteo el bit a 1
				arrayBloques[posArray]=j; // guardo el bloque en el array de bloques, a partir de la posicion indicada
				printf("se asigno al archivo el bloque nro: %d \n",j);
				posArray++;
				break;
			}
		}
		k++; //avanzo el while para buscar el siguiente bloque
	}//END OF WHILE
}

void escribirBloques(int arrayBloques[], char* buffer){
	int tamanioAEscribir;
	tamanioAEscribir = strlen(buffer);
	int posBloque = 0;

	while (tamanioAEscribir>0){
		char* bufferParcial = string_substring_until(buffer,metadata.tamanioBloques);
		int tamanioEscrito = strlen(bufferParcial);

		int nroBloque = arrayBloques[posBloque];
		char* nroBloqueChar = string_itoa(nroBloque);
		char* direccionBloque = string_new();

		string_append(&direccionBloque, "../..");
		string_append(&direccionBloque, puntoMontaje);
		string_append(&direccionBloque, "/Bloques/");
		string_append(&direccionBloque, nroBloqueChar);
		string_append(&direccionBloque, ".bin");
//		direccionBloque = armarPathBloque(direccionBloque, nroBloqueChar);

		free(nroBloqueChar);
		// Abro el archivo del bloque
		FILE* fdBloque = fopen(direccionBloque,"r+");
		if (fdBloque == NULL) {
			log_trace(loggerMDJ, "Error al abrir bloque para escribir!");
			free(direccionBloque);
			free(bufferParcial);
			exit_gracefully(1, loggerMDJ);
		}
		// escribo en el bloque abierto
		fwrite(bufferParcial,strlen(bufferParcial),1,fdBloque);
		// cierro el archivo
		fclose(fdBloque);
		// elimino del buffer lo que ya escribi
		char *bufferTemporal = string_substring_from(buffer, tamanioEscrito);
		free(buffer);
		buffer = bufferTemporal;
		bufferTemporal = NULL;
		// actualizo lo que falta por escribir
		tamanioAEscribir = tamanioAEscribir - tamanioEscrito;
		//limpio el buffer parcial
		memset(bufferParcial,'\0',strlen(bufferParcial));
		free(bufferParcial);
		// elimino estructuras
		free(direccionBloque);
		//paso al siguiente bloque
		posBloque ++;
	}
	free(buffer);
}

int verificarDirectorio(char* path){
	char* direccionVD = string_new();
	direccionVD = armarPathArchivo(direccionVD,path);
	char *indexUltimaBarra=strrchr(direccionVD,'/');
	char *directorioSinFileName = string_substring_until(direccionVD, indexUltimaBarra-direccionVD+1);

	// valido que el directorio exista; si no existe, lo creo
	DIR *dirAValidar;
	dirAValidar = opendir(directorioSinFileName);
	if(dirAValidar == NULL ) {
	  log_trace(loggerMDJ,"El directorio donde se intenta guardar el archivo no existe, creando directorio...");
	  int resultadoMK;
	  resultadoMK = mkdir(directorioSinFileName,S_IRWXU); // permisos r,w,x para el usuario
	  t_list* listaAuxVD = list_create();
	  if(resultadoMK == -1){
		  log_trace(loggerMDJ,"No se pudo crear el directorio solicitado");
		  free(dirAValidar);
		  list_destroy_and_destroy_elements(listaAuxVD,free);
		  free(directorioSinFileName);
		  free(direccionVD);
		  return ERROR_CREAR_DIRECTORIO;
	  }
	  log_trace(loggerMDJ,"El directorio donde se intenta guardar el archivo se ha creado.");
	  free(dirAValidar);
	  list_destroy_and_destroy_elements(listaAuxVD,free);
	  free(directorioSinFileName);
	  free(direccionVD);
	  return OK;
	  }
	else{
		// Si el directorio existe, no es necesario hacer nada, se puede guardar el file
		log_trace(loggerMDJ,"El directorio donde se intenta guardar el archivo se ha validado.");
		free(dirAValidar);
		free(directorioSinFileName);
		free(direccionVD);
		return OK;
	}
}


/*
 * Funcionalidad del MDJ *********************************************************************************************************************************
 */

// Validar archivo: comprueba la existencia de un archivo pasado por path

int validarArchivo(char* pathArchivo){
	char* direccionVA = string_new();
	direccionVA = armarPathArchivo(direccionVA, pathArchivo);
	int fd = open(direccionVA,O_RDWR,S_IRUSR | S_IWUSR);
	struct stat fdStat;
	if(fstat(fd,&fdStat) == -1){
		// El archivo no existe
		log_trace(loggerMDJ, "El archivo no existe");
		close(fd);
		if(direccionVA != NULL){
			free(direccionVA);
		}
		return ABRIR_PATH_INEXISTENTE;
	} else{
		log_trace(loggerMDJ, "El archivo fue validado correctamente!");
		close(fd);
		free(direccionVA);
		return OK;
	}
}

//Crear archivo: dado un path, crea un archivo y asigna N cantidad de bloques necesarios para guardar n bytes--- tamaño bloque actual: 64b

int crearArchivo(char* path, int cantidadLineas){
	// Primero valido que el archivo no exista ya
	int resultadoValidar = validarArchivo(path);
	int cantidadBytes = cantidadLineas*sizeof(char);
	if (resultadoValidar == OK){
		log_trace(loggerMDJ, "Error 50001: el archivo que se intenta crear ya existe");
		return CREAR_ARCHIVO_EXISTENTE;
	}

	// segundo valido que haya suficiente espacio
	int resultadoEspacio = hayEspacioDisponible(cantidadBytes);
	if (resultadoEspacio == -1){
		log_trace(loggerMDJ, "Error 50002: no hay espacio disponible para guardar la información");
		return CREAR_ESPACIO_INSUFICIENTE;
	}

	// tercero valido que el directorio donde quiero crear el archivo exista, si no existe debo crearlo
	// esto es por ejemplo si que quiere crear un equipo, se pasara equipos/racing.txt, el directorio equipos no existe en el FS de la catedra

	int resultadoVerificarDirCA = 0;
	resultadoVerificarDirCA = verificarDirectorio(path);
	if (resultadoVerificarDirCA == ERROR_CREAR_DIRECTORIO){
			log_trace(loggerMDJ, "Error 80001: no se pudo crear el directorio necesario para almacenar el archivo");
			return ERROR_CREAR_DIRECTORIO;
	}

	//Calculo cantidad de bloques
	int cantBloquesCA = (cantidadBytes/(metadata.tamanioBloques))+1;
	// creo el Array que va a almacenar los numeros de bloques //
	int bloquesCA[cantBloquesCA];
	//asigno bloques
	int posArrayCA = 0;
	asignarBloques(bloquesCA,posArrayCA,cantBloquesCA);

	// guardo los \n en los bloques.
	int bytesAGuardarCA = cantidadBytes;
	int nroBloqueCA = 0;
	//mientras tenga bytes y no haya recorrido el total de los bloques, guardo en bloques
	while( bytesAGuardarCA >0 && nroBloqueCA <= cantBloquesCA){
		int bytesAEscribirCA = 0;

		if (bytesAGuardarCA - metadata.tamanioBloques > 0 ){ //Voy a guardar una cantidad de bytes igual al tamaño MAX del bloque (por enunciado, 64)
			//armo el path del bloque para escribir
			char* bloqueCA;
			bloqueCA = string_new();
			sprintf(bloqueCA,"%d",bloquesCA[nroBloqueCA]);
			char* direccionCA = string_new();

			string_append(&direccionCA, "../..");
			string_append(&direccionCA, puntoMontaje);
			string_append(&direccionCA, "/Bloques/");
			string_append(&direccionCA, bloqueCA);
			string_append(&direccionCA, ".bin");

			bytesAEscribirCA = metadata.tamanioBloques; // indico que voy a guardar los 64 bytes

			// Abro el archivo del bloque
			FILE* fdBloqueCA = fopen(direccionCA,"r+");
			if (fdBloqueCA == NULL) {
				log_trace(loggerMDJ, "Error al abrir bloque para crear!");
				exit_gracefully(1, loggerMDJ);
			}
			//escribo los caracteres "\n"
			unsigned int k;
			for(k=0;k<bytesAEscribirCA;k++){
				fwrite("\n", 1, 1, fdBloqueCA);
			}
			// actualizo la cantidad de bytes restantes a guardar
			bytesAGuardarCA = bytesAGuardarCA - bytesAEscribirCA;
			//Cierro el archivo y libero las estructuras
			fclose(fdBloqueCA);
			free(direccionCA);
			free(bloqueCA);

		}else{ // escribo los bytes restantes en el ultimo bloque, o escribo los bytes en el unico bloque
			//armo el path del bloque para escribir
			bytesAEscribirCA = bytesAGuardarCA;
			char* bloqueCA = string_itoa(bloquesCA[nroBloqueCA]);
			char* direccionCA = string_new();

			string_append(&direccionCA, "../..");
			string_append(&direccionCA, puntoMontaje);
			string_append(&direccionCA, "/Bloques/");
			string_append(&direccionCA, bloqueCA);
			string_append(&direccionCA, ".bin");

			// Abro el archivo del bloque
			FILE* fdBloqueCA = fopen(direccionCA,"r+");
			if (fdBloqueCA == NULL) {
				log_trace(loggerMDJ, "Error al abrir bloque para crear!");
				exit_gracefully(1, loggerMDJ);
			}
			//escribo los caracteres "\n"
			unsigned int k;
			for(k=0;k<bytesAGuardarCA;k++){
				fwrite("\n", 1, 1, fdBloqueCA);
			}
			bytesAGuardarCA = 0;
			//Cierro el archivo y libero las estructuras
			fclose(fdBloqueCA);
			free(direccionCA);
			free(bloqueCA);
		}
		// paso al siguiente bloque
		nroBloqueCA ++;
	} //END OF WHILE
	// Finalmente creo el archivo y escribo la información
	// armo el full path
	char* direccion = string_new();
	direccion = armarPathArchivo(direccion, path);
	// creo el archivo y guardo la informacion
	FILE* file = fopen(direccion,"wb+");
	if (file == NULL) {
		log_trace(loggerMDJ, "ERROR AL CREAR ARCHIVO!");
		exit_gracefully(1, loggerMDJ);
	}
	char* lineaTamanio = string_new();
	char* cantidadBytesChar = string_itoa(cantidadBytes);
	string_append_with_format(&lineaTamanio, "%s=%s\n", "TAMANIO", cantidadBytesChar);
	free(cantidadBytesChar);
	fwrite(lineaTamanio, strlen(lineaTamanio), 1, file);
	free(lineaTamanio);
	char* lineaBloques = armarStringBloques(bloquesCA, cantBloquesCA);
	fwrite(lineaBloques, strlen(lineaBloques), 1, file);
	free(lineaBloques);
	fclose(file);
	//elimino estructuras usadas
	free(direccion);
	// Informo que se creo el archivo correctamente y salgo
	log_trace(loggerMDJ, "El archivo se creo correctamente!");

	return OK;
}

// obtenerDatos: Dado un archivo pasado por path, leo la cantidad de bytes pedidas desde un offset **********************************************************
char* obtenerDatos(char* path, int offset, int size){
	// armo el full path
	char* direccion = string_new();
	direccion = armarPathArchivo(direccion, path);
	// lo abro como un config para aprovechar funcionalidad config_get_array_value: me traigo el listado de bloques directamente como un array.
	t_config* archivoALeer = config_create(direccion);
	int tamanioArchivo = config_get_int_value(archivoALeer,"TAMANIO");
	char** bloquesArchivoALeer = config_get_array_value(archivoALeer,"BLOQUES");
	config_destroy(archivoALeer);
	int bytesALeer = tamanioArchivo;
	char* datosAObtener = string_new();

	// me fijo que el offset y el size no sea mayor al tamaño del archivo
	if (offset > tamanioArchivo){
		perror("No se puede leer lo solicitado del archivo");
	}

	// Me fijo cual es el bloque de inicio (Posicion del array, no numero de bloque)
	int bloqueInicio = offset/metadata.tamanioBloques;

	// leo lo que puedo del primer bloque: en un archivo de 64b, mi offset es 60, voy a leer 4 bytes del primer bloque

	// mientras me queden bytes por leer , leo de los proximos bloques
	while (bytesALeer > 0){
		char* bloqueActual = string_new();
		char* direccionBloque = string_new();
		char* bufferLectura = malloc(metadata.tamanioBloques + 1);
		memset(bufferLectura, '\0', metadata.tamanioBloques);
		string_append(&bloqueActual,bloquesArchivoALeer[bloqueInicio]);

		string_append(&direccionBloque, "../..");
		string_append(&direccionBloque, puntoMontaje);
		string_append(&direccionBloque, "/Bloques/");
		string_append(&direccionBloque, bloqueActual);
		string_append(&direccionBloque, ".bin");

		//abro el archivo del bloque
		FILE* fdBloque = fopen(direccionBloque,"r");
		if (fdBloque == NULL) {
			log_trace(loggerMDJ, "Error al abrir bloque para obtener!");
			exit_gracefully(1, loggerMDJ);
		}
		// a los siguientes bloques debo leerlos desde el principio
		// si lo bytes restantes por leer son menor al tamaño de un bloque, leo eso entero
		if(bytesALeer < metadata.tamanioBloques){
			fseek(fdBloque, offset, SEEK_SET);
			fread(bufferLectura,bytesALeer,1,fdBloque);
			bufferLectura[bytesALeer] = '\0';
			string_append(&datosAObtener, bufferLectura);
			bytesALeer = 0;
			free(bloqueActual);
			free(direccionBloque);
			free(bufferLectura);
			fclose(fdBloque);
		}else{ // leo el tamaño de un bloque y disminuyo la cantidad de bytes leidos
			fseek(fdBloque, offset,SEEK_SET);
			fread(bufferLectura,metadata.tamanioBloques,1,fdBloque);
			bufferLectura[metadata.tamanioBloques] = '\0';
			if(strlen(bufferLectura) > metadata.tamanioBloques){
				char* aux = string_substring_until(bufferLectura, metadata.tamanioBloques);
				string_append(&datosAObtener, aux);
				free(aux);
			} else {
				string_append(&datosAObtener, bufferLectura);
			}
			bytesALeer = bytesALeer - metadata.tamanioBloques;
			free(bloqueActual);
			free(direccionBloque);
			free(bufferLectura);
			fclose(fdBloque);
		}
		bloqueInicio ++;
	} // END OF WHILE
	//Ya obtuve los datos
	log_trace(loggerMDJ, "Los datos fueron obtenidos.\n");
	//limpio estructuras y salgo
	int i = 0;
	// bloqueInicio
	while(bloquesArchivoALeer[i] != NULL){
		free(bloquesArchivoALeer[i]);
		i++;
	}
	free(bloquesArchivoALeer);
	free(direccion);
	return datosAObtener;
}

// guardar datos: guardo en el archivo indicado por path, la cantidad de bytes que vienen en el buffer

int guardarDatos(char* path, int offset, int size, char* buffer){
	// Primero valido que el archivo exista ya
	int resultadoValidar = validarArchivo(path);
	if (resultadoValidar == 10001){
		log_trace(loggerMDJ, "Error 30004: el archivo donde se intenta guardar datos no existe");
		return FLUSH_ARCHIVO_INEXISTENTE_MDJ;
	}

	// armo el full path
	char* direccion = string_new();
	direccion = armarPathArchivo(direccion, path);
	// lo abro como un config para aprovechar funcionalidad config_get_array_value: me traigo el listado de bloques directamente como un array.
	t_config* archivoAActualizar = config_create(direccion);
	int tamanioArchivo = config_get_int_value(archivoAActualizar, "TAMANIO");
	char** bloquesArchivo = config_get_array_value(archivoAActualizar,"BLOQUES");
	// Si el tamaño a guardar es mayor que el tamaño del archivo, valido que haya suficiente espacio

	if(tamanioArchivo < size && size > metadata.tamanioBloques){
		int tamanioExtra = size - tamanioArchivo;
		int resultadoEspacio = hayEspacioDisponible(tamanioExtra);

		if (resultadoEspacio == -1){
			log_trace(loggerMDJ, "Error 30003: no hay espacio disponible para guardar la información");
			return FLUSH_ESPACIO_MDJ_INSUFICIENTE;
		}
		// cuento con el espacio necesario, debo saber cuantos nuevos bloques voy a necesitar
		int bloquesNecesarios = (size/metadata.tamanioBloques)+1;
		int bloquesActuales = (tamanioArchivo/metadata.tamanioBloques)+1;
		int tamanioNuevoArray = bloquesActuales+bloquesNecesarios;
		// creo el Array que va a almacenar los nuevos bloques
		int bloquesActualizado[tamanioNuevoArray];

		// copio los bloques actuales al nuevo array
		int i = 0;
		while(bloquesArchivo[i] != NULL){
			bloquesActualizado[i] = atoi(bloquesArchivo[i]);
			i++;
		}
		// agrego nuevos bloques al array
		int posArray = bloquesActuales;
		//itero el bitarray bitmap, asignando los n primeros bloques que encuentro en 0 (libres)
		asignarBloques(bloquesActualizado,posArray,bloquesNecesarios);
		// abro los bloques y sobreescribo en cada uno
		escribirBloques(bloquesActualizado, buffer);
		config_set_value(archivoAActualizar, "BLOQUES", armarStringBloques(bloquesActualizado, i));
		config_set_value(archivoAActualizar, "TAMANIO", string_itoa(size));
	}else{ // lo que voy a guardar entra en los bloques actuales y no es necesario agregar nuevos bloques
		//
		int bloquesNecesarios = (size/metadata.tamanioBloques)+1;
		// paso el nro de bloques de char* a int
		int bloquesInt[bloquesNecesarios];
		// copio los bloques actuales al nuevo array
		int i = 0;
		while(bloquesArchivo[i] != NULL){
			bloquesInt[i] = atoi(bloquesArchivo[i]);
			i++;
		}
		// abro los bloques y sobreescribo en cada uno
		char *sizeChar = string_itoa(size);
		escribirBloques(bloquesInt, buffer);
		config_set_value(archivoAActualizar, "TAMANIO", sizeChar);
		free(sizeChar);
		}
	//libero memoria y termino
	int j = 0;
	while(bloquesArchivo[j]!= NULL){
		free(bloquesArchivo[j]);
		j++;
	}
	config_save(archivoAActualizar);
	config_destroy(archivoAActualizar);
	free(bloquesArchivo);
	free(direccion);
	return OK;
}

// borrar Archivo: dado un path, desasigno los bloques y borro el archivo ****************************************************************************************

int borrarArchivo(char* path){
		// Primero valido que el archivo no exista ya
		int resultadoValidar = validarArchivo(path);
		if (resultadoValidar == 10001){
			log_trace(loggerMDJ, "Error 60001: el archivo que se intenta borrar no existe");
			return BORRAR_ARCHIVO_INEXISTENTE;
		}else{
			// armo el full path
			char* direccion = string_new();
			direccion = armarPathArchivo(direccion, path);
			// lo abro como un config para aprovechar funcionalidad config_get_array_value: me traigo el listado de bloques directamente como un array.
			t_config* archivoABorrar = config_create(direccion);
			char** bloquesALiberar = config_get_array_value(archivoABorrar,"BLOQUES");
			config_destroy(archivoABorrar);
			// itero el bitarray borrando los bloques indicados
			unsigned int i = 0;
			while(bloquesALiberar[i] != NULL){
				bitarray_clean_bit(bitmap,atoi(bloquesALiberar[i]));
				i++;
			}
			// con los bloques liberados, elimino el archivo
			remove(direccion);
			//libero memoria
			int j=0;
			while(bloquesALiberar[j]!=NULL){
				free(bloquesALiberar[j]);
				j++;
			}
			free(bloquesALiberar);
			free(direccion);
			log_trace(loggerMDJ, "El archivo fue eliminado correctamente!");
			return OK;
		}
}

/*
 * consolaMDJ : FUNCIONES DE CONSOLA ************************************************************************************************************************
 */

int verifyCommand(char* command) {
	int i;
	for (i = 0; commands[i].name; i++) {
		if (strcmp(command, commands[i].name) == 0) {
			return i;
		}
	}
	return -1;
}

bool esSalirConsola(char* nombreComando){
	return (string_equals_ignore_case(nombreComando, "exit") ||
			string_equals_ignore_case(nombreComando, "quit") ||
			string_equals_ignore_case(nombreComando, "q"));
}

void* consolaMDJ(void* arg){
	char currDirectory[PATH_MAX];
	getcwd(currDirectory,sizeof(currDirectory));
	char* readlineString=string_new();

	while(1){
		string_append(&readlineString,"\nConsolaMDJ - ");
		string_append(&readlineString,currDirectory);
		string_append(&readlineString,">>");

		char* entered_command = (char*) readline(readlineString);
		char** command_data = (char**)string_split(entered_command, " ");

		if(command_data[0] != NULL){
			printf("Procesando el comando '%s'\n", command_data[0]);
			int command_index = verifyCommand(command_data[0]);

			if(command_index == -1){
					puts("El comando ingresado no es soportado por la consola de MDJ...");
					puts("La consola MDJ soporta los siguientes comandos: ls, cd, MD5 y cat.");
			}else{
					Command command = commands[command_index];
					if(command_data[1] == NULL){
						if(command.accepts_empty_param){
							if(esSalirConsola(command.name)){
								free(command_data[0]);
								free(command_data[1]);
								free(entered_command);
								free(command_data);
							}
							command.operation(NULL);
						}else{
							puts("ERROR. El comando ingresado necesita un parametro");
						}
					}else{
						command.operation((void*) command_data[1]);
					}
			}
			free(command_data[0]);
			free(command_data[1]);
			free(entered_command);
			free(command_data);
			getcwd(currDirectory,sizeof(currDirectory));
			strcpy(readlineString,"");
		}else{
			puts("ERROR. No ha ingresado un comando");
			free(command_data[0]);
			free(entered_command);
			free(command_data);
			getcwd(currDirectory,sizeof(currDirectory));
			strcpy(readlineString,"");
		}
	}
	free(readlineString);
	return NULL;
}

// ComandoLS: Lista los directorios y archivos. ****************************************************************************************************************

void* comandoLS(void* path){
	struct dirent **namelist;
	int n;

	if (path == NULL) { //No se paso un path parametro, se usa el path actual
		char currPath[PATH_MAX];
		getcwd(currPath,sizeof(currPath));
		printf("Current working dir: %s\n", currPath);
		n = scandir(currPath, &namelist, NULL, alphasort);
	}else{
		n = scandir(path, &namelist, NULL, alphasort);
	}

	if(n < 0){
		perror("scandir");
	}else{
		while (n--){
			printf("%s\n",namelist[n]->d_name);
			free(namelist[n]);
		}
		free(namelist);
	}
	return NULL;
}

// comandoCD: Se cambia el directorio actual al pasado por parametro ******************************************************************************************

void* comandoCD(void* path){
	if(strcmp(path,".")== 0){ // "." indica el directorio actual

		char currPath[PATH_MAX];
		getcwd(currPath,sizeof(currPath));
		printf("Current working dir: %s\n", currPath);
		int chdirOK = chdir(currPath);
		if (chdirOK == 0){
			printf("Se ha cambiado el directorio a %s\n",currPath);
		}else{
			puts("ERROR. No se pudo cambiar el directorio");
		}

	} else if (strcmp(path,"..")== 0){ 	// ".." indica el directorio superior
		char currPath[PATH_MAX];
		getcwd(currPath,sizeof(currPath));
		printf("Current working dir: %s\n", currPath);
		char* supDir = string_new();
		char** dirSuperior = string_split(currPath,"/");
		t_list* listaAux = list_create();
		int i = 0;
		while(dirSuperior[i] != NULL){
			list_add(listaAux,dirSuperior[i]);
			i++;
		}
		int sizeListaAux = list_size(listaAux);
		// Elimino el ultimo elemento del path paramentro
		list_remove(listaAux,sizeListaAux -1);
		// Rearmo el path
		int nuevoSize = list_size(listaAux);
		unsigned int j;
		for(j = 0;j<nuevoSize;j++){
			string_append(&supDir,"/");
			string_append(&supDir,list_get(listaAux,j));
		}
		//Cambio al directorio superior
		int chdirOK = chdir(supDir);
		if (chdirOK == 0){
			printf("Se ha cambiado el directorio a %s\n",supDir);
		}else{
			puts("ERROR. No se pudo cambiar el directorio");
		}
		int k=0;
		while(dirSuperior[k]!= NULL){
			free(dirSuperior[k]);
			k++;
		}
		list_destroy(listaAux);
		free(dirSuperior);
		free(supDir);
	}else{
		//Cambio al directorio parametro
		int chdirOK = chdir(path);
		if (chdirOK == 0){
			printf("Se ha cambiado el directorio a %s\n",(char*) path);
		}else{
			puts("ERROR. No se pudo cambiar el directorio");
		}
	}
	return NULL;
}

//Comando Md5: Genera el md5 del archivo y lo muestra por pantalla *********************************************************************************************

void* comandoMD5(void* path){
	// Primero valido que el archivo  exista ya
		int resultadoValidar = validarArchivo(path);
		if (resultadoValidar == 10001){
			puts( "Error : el archivo del que se intenta obtener datos no existe");
			return NULL;
		}else{
			// armo el full path
			char* direccion = string_new();
			direccion = armarPathArchivo(direccion, path);
			// llamo a obtenerDatos para que me traiga el contenido de los bloques del archivo a leer
			char* datosObtenidos = obtenerDatos(path,0,0);

			ssize_t bytesFile;
			bytesFile = strlen(datosObtenidos);

			//Se realiza segun TP0
			unsigned char digest [MD5_DIGEST_LENGTH];
			MD5_CTX context;
			MD5_Init(&context);
			MD5_Update(&context,datosObtenidos,bytesFile);
			MD5_Final(digest, &context);
			//imprimo el contenido del MD5
			int n;
			for (n=0;n<MD5_DIGEST_LENGTH;n++){
				printf("%02x", digest[n]);
			}
			printf("\n");
			free(datosObtenidos);
			free(direccion);
			return NULL;
		}
}

//comando Cat: Muestra el contenido de un archivo por pantalla ************************************************************************************************

void* comandoCat(void* path){
	// Primero valido que el archivo  exista ya
	int resultadoValidar = validarArchivo(path);
	if (resultadoValidar == 10001){
		puts( "Error : el archivo del que se intenta obtener datos no existe");
	}else{
		// armo el full path
		char* direccion = string_new();
		direccion = armarPathArchivo(direccion, path);
		// llamo a obtenerDatos para que me traiga el contenido de los bloques del archivo a leer
		char* datosObtenidos = obtenerDatos(path,0,0);
		//imprimo y libero recursos
		printf("%s\n",datosObtenidos);
		free(direccion);
		free(datosObtenidos);
	}
	return NULL;
}

void* salirConsola(void* idVoid){
	puts("Cerrando el proceso MDJ\n");
	config_destroy(config);
	free(bitmap);
	exit_gracefully(1, loggerMDJ);
	return NULL;
}

/*
 * Liberar Recursos: elimina la estructura que almacena el config y el loggerMDJ ***********************************************************************************
 */

void liberarRecursos(){
	config_destroy(config);
	free(bitmap);
}
