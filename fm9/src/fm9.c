#include "fm9.h"



int main(int argc, char *argv[]) {

	inicializarConfig();
	inicializarEstructuraAdministrativa();
	printf("Iniciar servidor\n");
	pthread_t hiloConsola = iniciarConsola();
	iniciarServidor();
	finalizarConsola(hiloConsola);
	return EXIT_SUCCESS;
}

void iniciarServidor(){
	int socketFm9 = crearServerSocket(config->ip, config->puerto_fm9, loggerFm9);
	levantarServidorSelect(socketFm9, loggerFm9, &procesarMensajes);
}

void inicializarConfig(){
	loggerFm9 = configure_logger(NOMBRE_PROCESO, LOG_LEVEL_TRACE);
	// Se busca el archivo .cfg tanto en root como en el directorio padre
	t_config* archivoConfig = config_create("fm9.cfg");
	if(archivoConfig == NULL){
		archivoConfig = config_create("../fm9.cfg");
	}
	levantarDatosConfig(archivoConfig);
}

void levantarDatosConfig(t_config* archivoConfig){
	log_trace(loggerFm9,"levantando datos de configuracion");
	 config = malloc(sizeof(t_config_fm9));
	 config->ip=string_duplicate(config_get_string_value(archivoConfig, IP));
	 config->puerto_fm9 = string_duplicate(config_get_string_value(archivoConfig, PUERTO_FM9));
	 char* modoAux = string_duplicate(config_get_string_value(archivoConfig, MODO));
	 config->tamanio_mem = config_get_int_value(archivoConfig, TAMANIO_MEM);
	 config->max_linea  = config_get_int_value(archivoConfig, MAX_LINEA);
	 config->tamanio_pag = config_get_int_value(archivoConfig, TAMANIO_PAG);
	 if(string_equals_ignore_case(modoAux, MODO_SEGMENTACION_PURA)){
		 config->modo=SEGMENTACION_PURA;
	 }else{
		 if(string_equals_ignore_case(modoAux, MODO_PAG_INVERTIDAS)){
			config->modo=PAG_INVERTIDAS;
		 }else{
			 if(string_equals_ignore_case(modoAux, MODO_SEGMENTACION_PAG)){
			 	config->modo=SEGMENTACION_PAG;
			 }else{
				 log_error(loggerFm9,"Error en el archivo de configuracion");
			 }
		 }
	 }
	 config_destroy(archivoConfig);
	 free(modoAux);
	 log_trace(loggerFm9,"Puerto: %s",config->puerto_fm9);
	 log_trace(loggerFm9,"Modo:  %d",config->modo);
	 log_trace(loggerFm9,"Tamanio memoria:%d",config->tamanio_mem);
	 log_trace(loggerFm9,"Max linea:      %d",config->max_linea);
	 log_trace(loggerFm9,"tamanio pagina: %d",config->tamanio_pag);

}

void inicializarEstructuraAdministrativa() {
	log_trace(loggerFm9,"Inicializando Estructura Administrativa");
	ptrAdminMemoria = malloc(sizeof(t_admin_memoria));
	ptrAdminMemoria->storage = malloc(config->tamanio_mem);//	char storage[config->tamanio_mem];
	memset(ptrAdminMemoria->storage, 0, config->tamanio_mem);
	ptrAdminMemoria->lineasDeMem = config->tamanio_mem/config->max_linea;
	ptrAdminMemoria->tablaStorage=calloc(ptrAdminMemoria->lineasDeMem, sizeof(int));
	ptrAdminMemoria->lineasPorPagina=config->tamanio_pag/config->max_linea;
	
	inicializarModoMemoria();
}

void inicializarModoMemoria(){

	switch(config->modo){
		case SEGMENTACION_PURA:
			ptrAdminMemoria->ptrSegmentos =list_create();
			break;
		case PAG_INVERTIDAS:
			ptrAdminMemoria->ptrSegmentos = list_create();
			int cantFrames = config->tamanio_mem / config->tamanio_pag;
			for(int i=0; i<cantFrames; i++){
				list_add(ptrAdminMemoria->ptrSegmentos, crearNodoTPI(i));
			}
			break;
		case SEGMENTACION_PAG:
			ptrProcesos=list_create();
			ptrFrames=list_create();
			int cantidadFrames = config->tamanio_mem/config->tamanio_pag;
			for(int i=0; i<cantidadFrames; i++){
				list_add(ptrFrames, crearNodoFrame(i));
			}
			break;
		default :
			log_error(loggerFm9,"Modo invalido de almacenamiento de la memoria");
		   break;
		}

}

void procesarMensajes(int socket_cliente, Cabecera* cabecera){
	switch(cabecera->idOperacion){
		case DAM_FM9:
		case CPU_FM9: {
		procesarHandshake(socket_cliente,cabecera);
		break;
	}
	case SUBIR_ARCHIVO_FM9: {
		log_trace(loggerFm9,"SUBIR_ARCHIVO_FM9: %d",SUBIR_ARCHIVO_FM9);
		int pid;
		int cantidadLineasAGuardar;
		if(recibirMensajeInt(socket_cliente,&cantidadLineasAGuardar)==-1){
			log_error(loggerFm9,"Error al recibir cantidad de linea que tengo que recibir");
			break;
		}
		if(recibirMensajeInt(socket_cliente, &pid)){
			log_error(loggerFm9,"Error al recibir el pid");
			break;
		}
		log_info(loggerFm9,"Recibiendo archivo:socket_cliente:%d:transfersize:%d:pid:%d:cantidadLineasAGuardar:%d\n",
				socket_cliente, transferSizeDAM, pid, cantidadLineasAGuardar);
		int posicionMemoria=recibirArchivo(socket_cliente, pid,cantidadLineasAGuardar);
		if(posicionMemoria>=0){
			enviarMensajeInt(socket_cliente, posicionMemoria);
		}
		break;
	}
	case EJECUTAR_ASIGNAR :
		log_trace(loggerFm9,"SOLICITUD_ASSIGN:%d",SOLICITUD_ASSIGN);
		int posicionMemAssign = ERROR, pidAssign = ERROR, lineaAModificar = ERROR,
				tamanioMensaje = ERROR;
		if(recibirMensajeInt(socket_cliente,&posicionMemAssign)==-1){
			log_error(loggerFm9,"Error al recibir posicion de memoria al hacer assign");
			break;
		}
		if(recibirMensajeInt(socket_cliente,&pidAssign)==-1){
			log_error(loggerFm9,"Error al recibir el pid al hacer assign");
			break;
		}
		if(recibirMensajeInt(socket_cliente,&lineaAModificar)==-1){
			log_error(loggerFm9,"Error al recibir la linea a modificar al hacer assign");
			break;
		}
//el mensaje me tiene que llegar con el \n? asi se que es fin de linea
		if(recibirMensajeInt(socket_cliente,&tamanioMensaje)==-1){
			log_error(loggerFm9,"Error al recibir la linea a modificar al hacer assign");
			break;
		}
		char* mensaje=malloc(tamanioMensaje+1);
		if(recibirMensaje(socket_cliente,&mensaje, tamanioMensaje)<0){
			free(mensaje);
			log_error(loggerFm9,"Error al recibir la linea a modificar al hacer assign");
			break;
		}
		if(tamanioMensaje+1>config->max_linea){
			if(enviarMensajeInt(socket_cliente,ASIGNAR_ESPACIO_MEMORIA_INSUFICIENTE)==-1){
				free(mensaje);
				log_error(loggerFm9,"error al enviar asignar espacio de memoria insuficiente");
			}
			break;
		}


		mensaje[tamanioMensaje]='\n';
		log_info(loggerFm9,"Modificando linea de archivo:\n socket_cliente:%d \nposicionMem:%d \npid:%d \nlineaAModificar:%d \ntamanioMensaje:%d\n",
				socket_cliente, posicionMemAssign, pidAssign, lineaAModificar, tamanioMensaje);
		modificarLineaDeArchivo(socket_cliente,posicionMemAssign,pidAssign,lineaAModificar,mensaje,tamanioMensaje+1);
		free(mensaje);
		break;
	case SOLICITUD_FLUSH :
		log_trace(loggerFm9,"SOLICITUD_FLUSH: %d",SOLICITUD_FLUSH);
		int posicionMemoria;
		int pidFlush;
		if(recibirMensajeInt(socket_cliente,&posicionMemoria)==-1){
			log_error(loggerFm9,"Error al recibir posicion de memoria al hacer flush");
			break;
		}
		if(recibirMensajeInt(socket_cliente,&pidFlush)==-1){
			log_error(loggerFm9,"Error al recibir el pid al hacer flush");
			break;
		}
		log_info(loggerFm9,"Enviando archivo a DAM:socket_cliente:%d:posicionMem:%d:pid:%d\n",
						socket_cliente, posicionMemoria, pidFlush);
		enviarArchivo(socket_cliente, posicionMemoria,pidFlush);

		break;
	case EJECUTAR_CLOSE :
		log_trace(loggerFm9,"SOLICITUD_CLOSE: %d",SOLICITUD_CLOSE);
		int posicionMem;
		int pid;
//tengo que recibir por mensajeInt  la posiciondememoria y pid
//devuelvo por mensaje Int 0 si se hizo el close o error FALLO_DE_SEGMENTO=30002
		if(recibirMensajeInt(socket_cliente,&posicionMem)==-1){
			log_error(loggerFm9,"Error al recibir posicion de memoria al hacer flush");
			break;
		}
		if(recibirMensajeInt(socket_cliente,&pid)==-1){
			log_error(loggerFm9,"Error al recibir el pid al hacer flush");
			break;
		}
		//borrar
		log_trace(loggerFm9,"Liberando memoria:posicionMem: %d:pid :%d",posicionMem,pid);
		cerrarArchivo(socket_cliente,posicionMem, pid);
		break;
	case PEDIR_ESCRIPTORIO :
		log_trace(loggerFm9,"PEDIR_ESCRIPTORIO: %d",PEDIR_ESCRIPTORIO);
		int posMemoria;
		int pidScrip;
		if(recibirMensajeInt(socket_cliente,&posMemoria)==-1){
			log_error(loggerFm9,"Error al recibir posicion de memoria al pedir escriptorio");
			break;
		}
		if(recibirMensajeInt(socket_cliente,&pidScrip)==-1){
			log_error(loggerFm9,"Error al recibir el pid al pedir escriptorio");
			break;
		}
		//borrar
		log_trace(loggerFm9,"Enviando escriptorio a CPU:posicionMem: %d:pid :%d",posMemoria,pidScrip);
		enviarEscriptorio(socket_cliente, posMemoria,pidScrip);
		break;
	default :
		log_error(loggerFm9,"Error en el tipo de operación recibido:",cabecera->idOperacion);
		liberarRecursos();
		exit_gracefully(1, loggerFm9);
	   	break;
	}
}

int recibirArchivo(int socket_cliente, int numPid, int lineasAGuardar){
	switch(config->modo){
		case SEGMENTACION_PURA:
			return segmentacionRecibirArchivo(socket_cliente,numPid, lineasAGuardar);

			break;
		case PAG_INVERTIDAS:
			return tpiRecibirArchivo(socket_cliente, numPid,lineasAGuardar);
			break;
		case SEGMENTACION_PAG:
			return spaRecibirArchivo(socket_cliente, numPid,lineasAGuardar);
			break;
		default :
			log_error(loggerFm9,"Error al tomar el modo de almacenamiento.\n");
			return -1;
			break;
	}
}

void segmentacionGuardarArchivo(char* buffer, int posicion,int direccionDondeGuardar,int tamanioDeLinea) {

	ptrAdminMemoria->tablaStorage[direccionDondeGuardar+posicion]=tamanioDeLinea;
	memcpy(ptrAdminMemoria->storage+(direccionDondeGuardar*config->max_linea)+(posicion*config->max_linea),buffer,config->max_linea);

/*
Primer parametro de memcpy: a la direccion de storage le sumo la direccion que representa desde que linea voy a comenzar a guardar lo que me
envían (posición base del archivo en memoria) y la tercer suma representa la posición donde voy a ir guardando cada linea del archivo.
 */
}

t_nodo* crearNodo(int lineasAGuardar,int direccionDondeGuardar,int pid){
	t_nodo* ptrNodoTabla=calloc(1,sizeof(t_nodo));
	ptrNodoTabla->base=direccionDondeGuardar*config->max_linea;
	ptrNodoTabla->limite=lineasAGuardar;
	ptrNodoTabla->pid=pid;
	return ptrNodoTabla;
}

t_nodo_tpi* crearNodoTPI(int frame){
//x	log_trace(loggerFm9,"Creo nodo tpi:%d\n",frame);
	t_nodo_tpi* ptrNodoTabla=calloc(1,sizeof(t_nodo_tpi));
	ptrNodoTabla->pid=-1;
	ptrNodoTabla->frame=frame;
	ptrNodoTabla->pagina=-1;
	ptrNodoTabla->id=0;
	return ptrNodoTabla;
}
t_nodo_frame* crearNodoFrame(int frame){
//x	log_trace(loggerFm9,"Creo nodo segmentacion paginada:%d\n",frame);
	t_nodo_frame* ptrNodoFrame=malloc(sizeof(t_nodo_frame));
	ptrNodoFrame->numFrame=frame;
	ptrNodoFrame->numPag=-1;
	return ptrNodoFrame;
}

t_nodo_proceso* crearNodoProceso(int pid){
//x	log_trace(loggerFm9,"Creo nodo proceso:%d\n",pid);
	t_nodo_proceso* ptrNodoProceso=malloc(sizeof(t_nodo_proceso));
	ptrNodoProceso->pid=pid;
	ptrNodoProceso->tablaSegmentos=list_create();
	return ptrNodoProceso;
}

t_nodo_segmento* crearNodoSegmento(int base){
//x	log_trace(loggerFm9,"Creo nodo segmento de segmentacion paginada:%d\n",base);
	t_nodo_segmento* ptrNodoSegmento=malloc(sizeof(t_nodo_segmento));
	ptrNodoSegmento->base=base;
	ptrNodoSegmento->tablaPaginas=list_create();
	return ptrNodoSegmento;
}

int entradasLibresContiguas(int entradasNecesarias){

	for(int i=0;i<ptrAdminMemoria->lineasDeMem;i++){
		if(ptrAdminMemoria->tablaStorage[i]==0){
			int j;
			for(j=0;j+i<ptrAdminMemoria->lineasDeMem && ptrAdminMemoria->tablaStorage[i+j]==0 && j<entradasNecesarias;j++);
			if(j>=entradasNecesarias){
				return i;
			}
			i=j+i;
		}
	}
	return -1;
}

int segmentacionRecibirArchivo(int socket_cliente, int pid,int lineasAGuardar){
	int lineaDondeGuardar=entradasLibresContiguas(lineasAGuardar);
	codigoError = OK;
	int tamanioDeLinea;
//borrar
//	log_trace(loggerFm9,"Num linea inicial donde guardar el archivo: %d",lineaDondeGuardar);
	if(lineaDondeGuardar>=0){
		if(enviarMensajeInt(socket_cliente, OK)==-1){
			log_error(loggerFm9,"Error al confirmar espacio disponible para guardar archivo:%d",enviarMensajeInt);
			return -1;
		}
		for(int i=0;i<lineasAGuardar;i++){
			char* lineaMemoria=malloc(config->max_linea);
			memset(lineaMemoria, 0, config->max_linea);
			recibirMensajeInt(socket_cliente, &codigoError);
			// DAM notifica si hubo un error, si está OK se recibe la línea
			if(codigoError == OK){
				tamanioDeLinea=recibirLinea(socket_cliente, lineaMemoria);
				if(tamanioDeLinea==-1){
					log_error(loggerFm9,"Error al recibir linea numero:%d",i);
					free(lineaMemoria);
					return -1;
				}
				//borrar
//				log_trace(loggerFm9,"Linea recibida desde Dam(%d caracteres): %s",tamanioDeLinea,lineaMemoria);
				segmentacionGuardarArchivo(lineaMemoria, i,lineaDondeGuardar,tamanioDeLinea);
			} else {
				free(lineaMemoria);
				return -1;
			}
			free(lineaMemoria);
		}
		list_add(ptrAdminMemoria->ptrSegmentos, crearNodo(lineasAGuardar,lineaDondeGuardar,pid));

	}else{
		log_error(loggerFm9,"No hay espacio suficiente en memoria %d",ABRIR_ESPACIO_MEMORIA_INSUFICIENTE);
		enviarMensajeInt(socket_cliente,ABRIR_ESPACIO_MEMORIA_INSUFICIENTE);
		return -1;
	}
	log_info(loggerFm9,"Direccion de memoria enviada a Dam: %d",lineaDondeGuardar*config->max_linea);
	return lineaDondeGuardar*config->max_linea;
}

int	recibirLinea(int socket_cliente, char* lineaMemoria){
//	log_trace(loggerFm9,"dentro del proceso recibirLinea:socket_cliente:%d:transfersize: %d", socket_cliente, transferSizeDAM);
	int tamanioDeLinea;
	int sizeLinea=0;
	for(int j=0;j<config->max_linea;j=j+transferSizeDAM){
		if(recibirMensajeInt(socket_cliente,&tamanioDeLinea)==-1){
			log_error(loggerFm9,"Error al recibir el tamanio de linea:%d",tamanioDeLinea);
			return -1;
		}

		char* lineaRecibida=malloc(tamanioDeLinea+1);
		memset(lineaRecibida, 0, tamanioDeLinea);
		if(recibirMensaje(socket_cliente,&lineaRecibida, tamanioDeLinea)<0){
			log_error(loggerFm9,"Error al recibir el mensaje:%s",lineaRecibida);
			free(lineaRecibida);
			return -1;
		}
		memcpy(lineaMemoria+j,lineaRecibida,tamanioDeLinea);
		if(lineaRecibida[tamanioDeLinea-1]=='\n'){
			j=config->max_linea;
		}
		sizeLinea=sizeLinea+tamanioDeLinea;
//borrar
		log_trace(loggerFm9,"linea recibida:%s",lineaRecibida);
		free(lineaRecibida);
	}
	return sizeLinea;
}

t_nodo* buscarNodo(int buffer){

	bool mismoNodo(void * nodo) {
			return ((t_nodo*)nodo)->base == buffer;
		}

	t_nodo* ptrSegmentosAux = list_find(ptrAdminMemoria->ptrSegmentos, mismoNodo);
	return ptrSegmentosAux;
}

void segmentacionEnviarArchivo(int socket_cliente, int baseMem){
	t_nodo* nodoEncontrado=buscarNodo(baseMem);
	//borrar
	log_trace(loggerFm9,"Num. base %d cantidad de lineas del archivo: %d",nodoEncontrado->base,nodoEncontrado->limite);
	if(nodoEncontrado==NULL){
		log_error(loggerFm9,"Error FALLO DE SEGMENTO 30002");
		if(enviarMensajeInt(socket_cliente,FLUSH_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 30002");
			return;
		}
	}else{
		if(enviarMensajeInt(socket_cliente,OK)==-1){
			log_error(loggerFm9,"Error al enviar que se encontro elsegmento para poder enviar el archivo a dma");
			return;
		}

		if(enviarMensajeInt(socket_cliente,nodoEncontrado->limite)){
			log_error(loggerFm9,"Error al enviar la cantidad de lineas que envio a dma");
			return;
		}

		enviarArchivoTotal(socket_cliente, ptrAdminMemoria->storage+baseMem,nodoEncontrado->limite,config->max_linea);
		log_info(loggerFm9,"Se envio el archivo completo a dam\n");
	}
	return;
}

int enviarArchivoDeAPartes(int socket_cliente, char* lineaAEnviar, int tamanioDeLinea){
//*	log_trace(loggerFm9,"enviarArchivoDeAPartes:socket_cliente:%d:transfersize:%d:lineaAEnviar:%s:tamanioDeLinea:%d",
//			socket_cliente, transferSizeDAM, lineaAEnviar, tamanioDeLinea);
	char* parteDeLinea=malloc(transferSizeDAM+1);
	int tamanioAEnviar = transferSizeDAM;
	for(int i=0;i<tamanioDeLinea;i=i+transferSizeDAM){
		memset(parteDeLinea,0,transferSizeDAM);
		memcpy(parteDeLinea,lineaAEnviar+i, transferSizeDAM);
		if(tamanioDeLinea<i+transferSizeDAM){
			tamanioAEnviar=tamanioDeLinea%transferSizeDAM;
		}
	// Borrar - Valgrind: Error por no estar inicializada parteDeLinea
//*		log_trace(loggerFm9,"enviarArchivoDeAPartes parteDeLinea: %s",parteDeLinea);
//		log_trace(loggerFm9,"llineapor parte ParaEnviar tamanioAEnviar: %d",tamanioAEnviar);
		if(enviarMensajeInt(socket_cliente,tamanioAEnviar)==-1){
			log_error(loggerFm9,"Error al enviar tamanio de parte de linea a enviar");
			free(parteDeLinea);
			return -1;
		}

		if(enviarMensaje(socket_cliente,parteDeLinea, tamanioAEnviar)==-1){
			log_error(loggerFm9,"Error al enviar tamanio de parte de linea a enviar");
			free(parteDeLinea);
			return -1;
		}

	}
	free(parteDeLinea);
	return 0;
}

int enviarArchivoTotal(int socket_cliente, char* posicionACopiar,
		int cantidadDeLineasACopiar, int tamanioDeLineaMax){
	//borrar
//*	log_trace(loggerFm9,"enviarArchivoTotal:socket_cliente:%d:transferSizeDAM:%d:posicionACopiar:%s:cantidadDeLineasACopiar:%d:tamanioDeLinea:%d"
//			, socket_cliente, transferSizeDAM,posicionACopiar,cantidadDeLineasACopiar,tamanioDeLineaMax);
	for(int i=0; i<cantidadDeLineasACopiar;i++){
		char* lineaARecuperar=malloc(tamanioDeLineaMax);
		memset(lineaARecuperar,0,tamanioDeLineaMax);
		memcpy(lineaARecuperar, posicionACopiar+(i*tamanioDeLineaMax), tamanioDeLineaMax);
//*		log_trace(loggerFm9,"enviando linea a dam:%s",lineaARecuperar);
		if((int)strlen(lineaARecuperar) <= transferSizeDAM){
			if(enviarMensajeInt(socket_cliente, strlen(lineaARecuperar))==-1){
				log_error(loggerFm9,"Error al enviar tamanio de linea a enviar");
				free(lineaARecuperar);
				return -1;
			}
			enviarMensaje(socket_cliente, lineaARecuperar, strlen(lineaARecuperar));

			free(lineaARecuperar);
		}else{
			int respuesta= enviarArchivoDeAPartes(socket_cliente, lineaARecuperar, strlen(lineaARecuperar));
			free(lineaARecuperar);
			if(respuesta==-1){
				log_error(loggerFm9,"valor de retorno erroneo de enviar arch. de a partes %d",respuesta);
				return -1;
			}
		}

	}

	return 0;
}

void enviarArchivo(int socket_cliente, int posicionMem, int pid){
	switch(config->modo){
		case SEGMENTACION_PURA:
			segmentacionEnviarArchivo(socket_cliente, posicionMem);
			break;
		case PAG_INVERTIDAS:
			tpiEnviarArchivo(socket_cliente, pid, posicionMem);
			break;
		case SEGMENTACION_PAG:
			spaEnviarArchivo(socket_cliente, pid, posicionMem);
			break;
		default :
			log_error(loggerFm9,"Error al enviar arch.");
			break;
	}
}

void cerrarArchivo(int socket_cliente,int posicionMemoria,int pid){
	switch(config->modo){
		case SEGMENTACION_PURA:
			segmentacionCerrar(socket_cliente,posicionMemoria);
			break;
		case PAG_INVERTIDAS:
			tpiCerrar(socket_cliente,pid,posicionMemoria);

			break;
		case SEGMENTACION_PAG:
			spaCerrar(socket_cliente,pid,posicionMemoria);

			break;
		default :
			log_error(loggerFm9,"Error al enviar arch.");
			break;
	}
}

t_nodo* sacarNodo(int baseMem){
	bool mismoNodo(void* nodo) {
		return ((t_nodo*)nodo)->base == baseMem;
	}

	t_nodo* nodoEncontrado = list_remove_by_condition(ptrAdminMemoria->ptrSegmentos, mismoNodo);
	return nodoEncontrado;
}

void segmentacionCerrar(int socket_cliente,int baseMemoria){
	t_nodo* nodoSacado=sacarNodo(baseMemoria);
	if(nodoSacado==NULL){
		log_error(loggerFm9,"Error FALLO DE SEGMENTO 40002");
		if(enviarMensajeInt(socket_cliente,CLOSE_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar FALLO DE SEGMENTO 30002 al cerrar el arch.");
			return;
		}
	}else{
		for(int i=0;i<nodoSacado->limite;i++){
//otra forma de liberar memoria
//memset(ptrAdminMemoria->tablaStorage[baseMem/config->max_linea], 0, nodoEncontrado->limite);
			int lineaBase = nodoSacado->base/config->max_linea;
			ptrAdminMemoria->tablaStorage[lineaBase + i]=0;
			memset(ptrAdminMemoria->storage+(nodoSacado->base)+(i*config->max_linea), 0, config->max_linea);
		}
			free(nodoSacado);
	}
	if(enviarMensajeInt(socket_cliente,OK)==-1){
		log_error(loggerFm9,"Error al enviar FALLO DE SEGMENTO 30002 al cerrar el arch.");
	}
	log_info(loggerFm9,"se libero espacio de memoria.");
	return;
}

bool tpiNodoFrameLibre(void* nodoTpi){
	t_nodo_tpi* nodo= (t_nodo_tpi*) nodoTpi;
    return nodo->pagina == -1 && nodo->pid == -1;
}
bool spaNodoFrameLibre(void* nodoSpa){
	t_nodo_frame* nodo= (t_nodo_frame*) nodoSpa;
    return nodo->numPag == -1;
}

int tpiRecibirArchivo(int socket_cliente, int pid, int lineasAGuardar){
    idAAsignar++;
    int cantidadPaginasAGuardar = (lineasAGuardar*config->max_linea) / config->tamanio_pag;
    codigoError = OK;
    if(((lineasAGuardar*config->max_linea) % config->tamanio_pag)!=0){
    	cantidadPaginasAGuardar=cantidadPaginasAGuardar+1;
    }
    //borrar
//*    log_trace(loggerFm9,"cantidadPaginasAGuardar: %d",cantidadPaginasAGuardar);
    int framesLibres = list_count_satisfying(ptrAdminMemoria->ptrSegmentos, tpiNodoFrameLibre);
//borrar
//*    log_trace(loggerFm9,"framesLibres: %d",framesLibres);

    int numPaginaInicial=-1;
    if(framesLibres >= cantidadPaginasAGuardar){
    	if(enviarMensajeInt(socket_cliente, OK)==-1){
    		log_error(loggerFm9,"Error al confirmar espacio disponible para guardar archivo:%d",enviarMensajeInt);
    		return -1;
    	}
    	char* buffer=malloc(config->max_linea);
    	int numPagina=0;
    	int tamanioDeLinea;
        for(int i=0;i<lineasAGuardar;i=i+ptrAdminMemoria->lineasPorPagina){
        	t_nodo_tpi* nodoFrameLibre = list_find(ptrAdminMemoria->ptrSegmentos, &tpiNodoFrameLibre);
//identificar arch. de un mismo pid
        	numPagina=obtenerNumeroDePaginaAAsignar(pid);
        	if(numPaginaInicial==-1){
 //       	    numFrameInicial=nodoFrameLibre->frame*ptrAdminMemoria->lineasPorPagina*config->max_linea;
        		numPaginaInicial=numPagina;
        	}
			for(int j=0;j<ptrAdminMemoria->lineasPorPagina && i+j<lineasAGuardar;j++){
				memset(buffer, 0, config->max_linea);
				recibirMensajeInt(socket_cliente, &codigoError);
				// DAM notifica si hubo un error, si está OK se recibe la línea
				if(codigoError != OK){
					free(buffer);
					return -1;
				}
				tamanioDeLinea=recibirLinea(socket_cliente, buffer);
				log_trace(loggerFm9,"numero de pagina:%d  y tamanio de la linea a guardar: %d",numPagina,tamanioDeLinea);
				if(tamanioDeLinea==-1){
					log_error(loggerFm9,"Error al recibir linea numero:%d",i);
					free(buffer);
					return -1;
				}else{
					tpiGuardarArchivo(buffer, pid,numPagina,j,nodoFrameLibre,tamanioDeLinea);
			//borrar
//*					log_trace(loggerFm9," guardado en buffer: %s",buffer);
				}
			}
        }
        free(buffer);
    } else {
        enviarMensajeInt(socket_cliente,ABRIR_ESPACIO_MEMORIA_INSUFICIENTE);
        return -1;
    }
    log_info(loggerFm9,"Direccion de memoria enviada a Dam: %d",numPaginaInicial);
    return numPaginaInicial;
}

int obtenerNumeroDePaginaAAsignar(int pid){
	int cantFrames =config->tamanio_mem/config->tamanio_pag;
	t_nodo_tpi* nodoEncontrado;
	int i;
	bool encontrado=false;
	for(i=0;i<cantFrames && !encontrado;i++){
		nodoEncontrado=buscarNodoPorPaginaPorPid(pid, i);
		encontrado=nodoEncontrado==NULL;
	}
//*	log_trace(loggerFm9,"obtenerNumeroDePaginaAAsignar i:%d",i);
	return i-1;
}

void tpiGuardarArchivo(char* buffer, int pid, int pagina,int linea,t_nodo_tpi* nodoFrameLibre,int tamanioDeLinea){
	ptrAdminMemoria->tablaStorage[(nodoFrameLibre->frame*ptrAdminMemoria->lineasPorPagina) + linea]=tamanioDeLinea;
    memset(ptrAdminMemoria->storage + (nodoFrameLibre->frame*config->tamanio_pag)+(linea*config->max_linea), 0, config->max_linea);
    memcpy(ptrAdminMemoria->storage + (nodoFrameLibre->frame*config->tamanio_pag)+(linea*config->max_linea), buffer, config->max_linea);
    nodoFrameLibre->pid = pid;
    nodoFrameLibre->pagina = pagina;
    nodoFrameLibre->id = idAAsignar;
}

void tpiEnviarArchivo(int socket_cliente, int pid,int numPagina){
	t_nodo_tpi* nodoEncontrado= buscarNodoPorPaginaPorPid(pid,numPagina);
/*
	log_trace(loggerFm9,"tpiEnviarArchivo nodoEncontrado->id: %d",nodoEncontrado->id);
	log_trace(loggerFm9,"tpiEnviarArchivo nodoEncontrado->pid: %d",nodoEncontrado->pid);
	log_trace(loggerFm9,"tpiEnviarArchivo nodoEncontrado->frame: %d",nodoEncontrado->frame);
	log_trace(loggerFm9,"tpiEnviarArchivo nodoEncontrado->pagina: %d",nodoEncontrado->pagina);
*/
	if(nodoEncontrado==NULL){
		log_error(loggerFm9,"Error al buscar nodo por pagina y por pid");
	    return;
	}
	 t_list* nodosId=obtenerNodosOrdenadosPorPagMismoId(nodoEncontrado->id);
	 if(list_is_empty(nodosId)){
		 log_error(loggerFm9,"Error FALLO DE SEGMENTO 30002");
	     if(enviarMensajeInt(socket_cliente,FLUSH_FALLO_SEGMENTO_MEMORIA)==-1){
	    	 log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 30002");
	     }
	     list_destroy(nodosId);
	     return;
	}
    if(enviarMensajeInt(socket_cliente,OK)==-1){
    	log_error(loggerFm9,"Error al enviar que se encontro el pid en tpi para poder enviar el archivo a dma");
    	return;
    }
    int cantidadPaginas=list_size(nodosId);
    t_nodo_tpi* ultimoNodo=list_get(nodosId, cantidadPaginas-1);
    int cantLineasUltimaPagina;
//posicion de la primer linea del frame
    int lineaBase=ultimoNodo->frame*ptrAdminMemoria->lineasPorPagina;
//se calcula la cantidad de lineas de la ultima pagina
    for(cantLineasUltimaPagina=0;ptrAdminMemoria->tablaStorage[lineaBase+cantLineasUltimaPagina]>0 &&
    	cantLineasUltimaPagina<ptrAdminMemoria->lineasPorPagina;cantLineasUltimaPagina++);
//se calcula el total de lineas sumando las lineas por paginas sin tener en cuenta la ultima pagina + las lineas calculadas de la ultima pagina
    int cantidadDeLineasAEnviar=((cantidadPaginas-1)*ptrAdminMemoria->lineasPorPagina)+cantLineasUltimaPagina;
    if(enviarMensajeInt(socket_cliente,cantidadDeLineasAEnviar)==-1){
    	log_error(loggerFm9,"Error al enviar la cantidad de lineas que envio a dma");
    	return;
    }
    void tpiEnviarPagina(void* nodoTpi){
        int frame = ((t_nodo_tpi*)nodoTpi)->frame;
        int pagina=((t_nodo_tpi*)nodoTpi)->pagina;
        int cantLineasPorPagina;
        if(ultimoNodo->pagina==pagina){
        	cantLineasPorPagina=cantLineasUltimaPagina;
        }else{
        	cantLineasPorPagina = ptrAdminMemoria->lineasPorPagina;
        }
        enviarArchivoTotal(socket_cliente, ptrAdminMemoria->storage + (frame*config->tamanio_pag),cantLineasPorPagina,config->max_linea);

/*
    // int cantNodos = list_size(nodosPid);
    // int cantLineas = (cantNodos*config->tamanio_pag) / config->max_linea;
   char* bufferAux=malloc(config->max_linea);
       for(int i=0; i<cantLineasPorPagina; i++) {
            // TODO enviar de a transfer_size
            memcpy(bufferAux, ptrAdminMemoria->storage + (frame*config->tamanio_pag) + (i*config->max_linea), config->max_linea);
            // bufferAux[config->max_linea+1]='\0';
            enviarMensaje(socket_cliente, bufferAux, config->max_linea+1);
        }
        free(bufferAux);
*/
    }

    list_iterate(nodosId, tpiEnviarPagina);

    log_info(loggerFm9,"Se envio el archivo completo a dam\n");
    list_destroy(nodosId);
}

void tpiCerrar(int socket_cliente,int pid,int numPagina) {
    void liberarFrameYTablaStorage(void* nodo) {
        int frame = ((t_nodo_tpi*)nodo)->frame;
        int tamanioPag = config->tamanio_pag;
        memset(ptrAdminMemoria->storage + (frame*tamanioPag), 0, tamanioPag);
        for(int i=0;i<ptrAdminMemoria->lineasPorPagina;i++){
        	ptrAdminMemoria->tablaStorage[ (frame*ptrAdminMemoria->lineasPorPagina) + i]=0;
        }
        ((t_nodo_tpi*)nodo)->pagina = -1;
        ((t_nodo_tpi*)nodo)->pid = -1;
    }
    t_nodo_tpi* nodoEncontrado= buscarNodoPorPaginaPorPid(pid,numPagina);
    log_trace(loggerFm9,"Frame %d, pagina %d del archivo %d a liberar del proceso %d",nodoEncontrado->frame,
    		nodoEncontrado->pagina,nodoEncontrado->id,nodoEncontrado->pid);

    if(nodoEncontrado==NULL){
        log_error(loggerFm9,"Error al buscar nodo por pagina y por pid");
        return;
    }
    t_list* nodosId=obtenerNodosOrdenadosPorPagMismoId(nodoEncontrado->id);
    if(list_is_empty(nodosId)){
        log_error(loggerFm9,"Error FALLO DE SEGMENTO 30002");
        if(enviarMensajeInt(socket_cliente,CLOSE_FALLO_SEGMENTO_MEMORIA)==-1){
        	log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 30002");
        }
        list_destroy(nodosId);
        return;
    }

    list_iterate(nodosId, liberarFrameYTablaStorage);
    if(enviarMensajeInt(socket_cliente,OK)==-1){
    	log_error(loggerFm9,"Error al enviar FALLO DE SEGMENTO 30002 al cerrar el arch.");
    }
    list_destroy(nodosId);
    log_info(loggerFm9,"se libero espacio de memoria.");
    return;
}

void modificarLineaDeArchivo(int socket_cliente,int posicionMem,int pid,int lineaAModificar,char* mensaje,int tamanioMensaje){
	switch(config->modo){
		case SEGMENTACION_PURA:
			segmentacionModificarLinea(socket_cliente,posicionMem,lineaAModificar,mensaje,tamanioMensaje);
			break;
		case PAG_INVERTIDAS:
			tpiModificarLinea(socket_cliente,posicionMem,pid,lineaAModificar,mensaje,tamanioMensaje);
			break;
		case SEGMENTACION_PAG:
			spaModificarLinea(socket_cliente,posicionMem,pid,lineaAModificar,mensaje,tamanioMensaje);
			break;
		default :
			log_error(loggerFm9,"Error en el tipo de almacenamiento de la memoria");
			break;
	}
}

void segmentacionModificarLinea(int socket_cliente,int baseMem,int lineaAModificar,char* mensaje,int tamanioMensaje){
	t_nodo* nodoEncontrado=buscarNodo(baseMem);
	//borrar
	log_trace(loggerFm9,"Base %d,limite %d",nodoEncontrado->base,nodoEncontrado->limite);
	log_trace(loggerFm9,"Linea antes de modificarla:%s",ptrAdminMemoria->storage+baseMem+((lineaAModificar-1)*config->max_linea));
	if(nodoEncontrado==NULL || nodoEncontrado->limite<lineaAModificar){
		log_error(loggerFm9,"No se encontro la base en la tabla de segmentos:%d",ASIGNAR_FALLO_SEGMENTO_MEMORIA);
		if(enviarMensajeInt(socket_cliente,ASIGNAR_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 20002");
		}
		return;
	}
	int numLineaBase=baseMem/config->max_linea;
	ptrAdminMemoria->tablaStorage[numLineaBase+lineaAModificar-1]=tamanioMensaje;
	memset(ptrAdminMemoria->storage+baseMem+((lineaAModificar-1)*config->max_linea),0,config->max_linea);
//la linea a modificar arranca desde 0, ver si cpu me manda el menaje con \n
	memcpy(ptrAdminMemoria->storage+baseMem+((lineaAModificar-1)*config->max_linea),mensaje,tamanioMensaje);
	log_trace(loggerFm9,"Linea despues de modificarla:%s",ptrAdminMemoria->storage+baseMem+((lineaAModificar-1)*config->max_linea));
	if(enviarMensajeInt(socket_cliente,OK)==-1){
		log_error(loggerFm9,"Error al enviar que se encontro elsegmento para poder enviar el archivo a dma");
		return;
	}
}

void tpiModificarLinea(int socket_cliente,int numPagina,int pid,int lineaAModificar,char* mensaje,int tamanioLinea){
	t_nodo_tpi* nodoEncontrado= buscarNodoPorPaginaPorPid(pid,numPagina);
	if(nodoEncontrado==NULL){
		log_error(loggerFm9,"No se encontro nodo por pagina:%d y por pid:%d",numPagina,pid);
		if(enviarMensajeInt(socket_cliente,ASIGNAR_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 20002");
		}
		return;
	}
	t_list* nodosId=obtenerNodosOrdenadosPorPagMismoId(nodoEncontrado->id);
	if(list_is_empty(nodosId)){
		log_error(loggerFm9,"No se encontraron las paginas del archivo a modificar");
		list_destroy(nodosId);
		return;
	}

	int paginasOcupadas=lineaAModificar/ptrAdminMemoria->lineasPorPagina;
	int restoPaginasOcupadas=lineaAModificar%ptrAdminMemoria->lineasPorPagina;
	if(restoPaginasOcupadas==0){
		paginasOcupadas=paginasOcupadas-1;
	}
	int lineaDePaginaAModificar=lineaAModificar-(paginasOcupadas*ptrAdminMemoria->lineasPorPagina);
	if(lineaDePaginaAModificar<=0){
		log_error(loggerFm9,"linea sobrepasa la cant de paginas del archivo, pagina:%d y por pid:%d",numPagina,pid);
		if(enviarMensajeInt(socket_cliente,ASIGNAR_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 20002");
		}
		return;
	}
/*pruebaXXX
	void mostrarPagina(void* nodoTPI){
    	t_nodo_tpi* nodo = (t_nodo_tpi*)nodoTPI;
        int dirFisicaIni = nodo->frame * ptrAdminMemoria->lineasPorPagina;
        log_info(loggerFm9, "Pagina: %d - Frame: %d - Dir.Fisica: %d - %d",
        		nodo->pagina, nodo->frame, dirFisicaIni,
				dirFisicaIni + ptrAdminMemoria->lineasPorPagina);
        int i;
        for(i=0; i < ptrAdminMemoria->lineasPorPagina; i++){
        	char *linea=malloc(config->max_linea + 1);
            memcpy(linea, ptrAdminMemoria->storage + (dirFisicaIni * config->max_linea) +
            		(i * config->max_linea), config->max_linea);
            linea[config->max_linea]='\0';
            log_info(loggerFm9, "Linea %d: %s", i, linea);
            free(linea);
        }
    }
    list_iterate(nodosId, mostrarPagina);
*/
//	log_trace(loggerFm9,"posicion del nodo en la que se encuentra la linea :%d\n",paginasOcupadas);
//	log_trace(loggerFm9,"lineas restantes de una pagina :%d\n",lineaDePaginaAModificar);
	t_nodo_tpi* nodoBuscado=list_get(nodosId, paginasOcupadas);
	log_trace(loggerFm9,"Frame %d,Pagina %d y id del archivo %d a modificar.",nodoBuscado->frame,nodoBuscado->pagina,nodoBuscado->id);
//**	log_trace(loggerFm9,"Linea antes de modificarla: %s",ptrAdminMemoria->storage+(nodoBuscado->frame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea));
//	log_trace(loggerFm9,"numero de proceso:%d\n",nodoBuscado->pid);
	if(nodoBuscado==NULL){
		log_error(loggerFm9,"No se encontro la pagina a modificar");
		list_destroy(nodosId);
		return;
	}

	ptrAdminMemoria->tablaStorage[(nodoBuscado->frame*ptrAdminMemoria->lineasPorPagina)+(lineaAModificar-1)]=tamanioLinea;
	memset(ptrAdminMemoria->storage+(nodoBuscado->frame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea),0,config->max_linea);
	memcpy(ptrAdminMemoria->storage+(nodoBuscado->frame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea),mensaje,tamanioLinea);
//**s	log_trace(loggerFm9,"Linea despues de modificarla: %s",*ptrAdminMemoria->storage+(nodoBuscado->frame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea));
	if(enviarMensajeInt(socket_cliente,OK)==-1){
		log_error(loggerFm9,"Error al enviar que se encontro el pid en tpi para poder enviar el archivo a dma");
		list_destroy(nodosId);
		return;
	}
//pruebaXXX
//	list_iterate(nodosId, mostrarPagina);
//
	list_destroy(nodosId);
}

void enviarEscriptorio(int socket_cliente,int posicionMem,int pid){
	switch(config->modo){
		case SEGMENTACION_PURA:
			segmentacionEnviarEscriptorio(socket_cliente,posicionMem);
			break;
		case PAG_INVERTIDAS:
			tpiEnviarEscriptorio(socket_cliente,pid,posicionMem);
			break;
		case SEGMENTACION_PAG:
			spaEnviarEscriptorio(socket_cliente,pid,posicionMem);

			break;
		default :
			log_error(loggerFm9,"Error al enviar arch.");
			break;
	}
}
void segmentacionEnviarEscriptorio(int socket_cliente,int baseMem){
	t_nodo* nodoEncontrado=buscarNodo(baseMem);
	//borrar
//	log_trace(loggerFm9,"nodoEncontrado->base: %d",nodoEncontrado->base);
//	log_trace(loggerFm9,"nodoEncontrado->limite: %d",nodoEncontrado->limite);
	if(nodoEncontrado==NULL){
		log_error(loggerFm9,"Error FALLO DE SEGMENTO 70002");
		return;
	}else{
		segmentacionEnviarEscriptorioTotal(socket_cliente,nodoEncontrado);
		log_info(loggerFm9,"Se envio el archivo completo a cpu\n");
	}
	return;
}

void segmentacionEnviarEscriptorioTotal(int socket_cliente,t_nodo*  nodoEncontrado){
	int lineaBase=nodoEncontrado->base/config->max_linea;
	int tamanioArchivo=0;
	char* archivo=malloc(nodoEncontrado->limite*config->max_linea);
	for(int i=0; i<nodoEncontrado->limite;i++){
		memcpy(archivo+tamanioArchivo,ptrAdminMemoria->storage+nodoEncontrado->base+(i*config->max_linea),ptrAdminMemoria->tablaStorage[lineaBase+i]);
		tamanioArchivo=tamanioArchivo+ptrAdminMemoria->tablaStorage[lineaBase+i];
	}
	archivo[tamanioArchivo]='\0';
	int tamanioArchivoAEnviar=strlen(archivo);
//borrar
	log_trace(loggerFm9,"TamanioArchivoAEnviar:%d",tamanioArchivoAEnviar);
	log_trace(loggerFm9,"ArchivoAEnviar:%s",archivo);
	if(enviarMensajeInt(socket_cliente,tamanioArchivoAEnviar)==-1){
		log_error(loggerFm9,"Error al enviar tamanio de linea a enviar");
		free(archivo);
		return;
	}
	enviarMensaje(socket_cliente, archivo,tamanioArchivoAEnviar);
	free(archivo);

}
t_nodo_tpi* buscarNodoPorPaginaPorPid(int pid,int numPagina){
	//borrar
//		log_trace(loggerFm9,"buscarNodoPorPaginaPorPid");
	bool mismoNodo(void* nodo) {
			return  ((t_nodo_tpi*)nodo)->pid== pid && ((t_nodo_tpi*)nodo)->pagina== numPagina;
		}

	t_nodo_tpi* ptrSegmentosAux = list_find(ptrAdminMemoria->ptrSegmentos, mismoNodo);
	return ptrSegmentosAux;
}
t_list* obtenerNodosOrdenadosPorPagMismoId(int id){
	//borrar
//	log_trace(loggerFm9,"obtenerNodosOrdenadosPorPagMismoId");
	bool mismoId(void* nodoTPI) {
		t_nodo_tpi* nodo = (t_nodo_tpi*) nodoTPI;
	    return nodo->id == id;
	}

	bool ordenaPorPagina(void* nodo1, void* nodo2) {
		if(((t_nodo_tpi*)nodo1)->pagina < ((t_nodo_tpi*)nodo2)->pagina){
			return true;
		}else{
			return false;
		}
	}
	t_list* nodosId = list_filter(ptrAdminMemoria->ptrSegmentos, mismoId);
	if(nodosId==NULL || list_is_empty(nodosId)){
		return list_create();
	}

	list_sort(nodosId,ordenaPorPagina);
	return nodosId;
}

void tpiEnviarEscriptorio(int socket_cliente,int pid,int numPagina){
	//borrar
//*	log_trace(loggerFm9,"tpiEnviarEscriptorio");
	t_nodo_tpi* nodoEncontrado= buscarNodoPorPaginaPorPid(pid,numPagina);
	if(nodoEncontrado==NULL){
		log_error(loggerFm9,"Error al buscar nodo por pagina y por pid");
		return;
	}
	t_list* nodosId=obtenerNodosOrdenadosPorPagMismoId(nodoEncontrado->id);
	if(list_is_empty(nodosId)){
		list_destroy(nodosId);
		return;
	}
	int cantNodosId=list_size(nodosId);
	int tamanioMaxArchivo=cantNodosId*ptrAdminMemoria->lineasPorPagina*config->max_linea;
	char* archivoMax=malloc(tamanioMaxArchivo);
	memset(archivoMax,0,tamanioMaxArchivo);
	int indice=0;
	int tamanioArchivoEnLineas=cantNodosId*ptrAdminMemoria->lineasPorPagina;
	int tamanioActualArchivo=0;

	for(int i=0;i<tamanioArchivoEnLineas;i+=ptrAdminMemoria->lineasPorPagina){
		t_nodo_tpi* nodo=list_get(nodosId, indice);
		bool finDeLineaDePagina=false;
		for(int j=0;j<ptrAdminMemoria->lineasPorPagina && !finDeLineaDePagina;j++){
			int desplazamientoFrame = nodo->frame * config->tamanio_pag;
			int desplazamientoLinea = j*config->max_linea;
			void* posicionDelStorage = ptrAdminMemoria->storage+desplazamientoFrame+desplazamientoLinea;
			int tamanioLineaACopiar = ptrAdminMemoria->tablaStorage[nodo->frame*ptrAdminMemoria->lineasPorPagina+j];
			if(tamanioLineaACopiar<=0){
				finDeLineaDePagina=true;
			}
			memcpy(archivoMax+tamanioActualArchivo,posicionDelStorage,tamanioLineaACopiar);
			tamanioActualArchivo += tamanioLineaACopiar;
		}
		indice++;
	}
	archivoMax[tamanioActualArchivo]='\0';
	int tamanioArchivoAEnviar=strlen(archivoMax);
	//borrar
	log_trace(loggerFm9,"tamanioArchivoAEnviar:%d",tamanioArchivoAEnviar);
	log_trace(loggerFm9,"tamanioArchivoAEnviar:%s",archivoMax);
	if(enviarMensajeInt(socket_cliente,tamanioArchivoAEnviar)==-1){
		log_error(loggerFm9,"Error al enviar tamanio de linea a enviar");
		free(archivoMax);
		return;
	}
	enviarMensaje(socket_cliente, archivoMax,tamanioArchivoAEnviar);
	log_info(loggerFm9,"Se envio el archivo completo a cpu\n");
	list_destroy(nodosId);
	free(archivoMax);
}
t_nodo_proceso* encontrarNodoMismoProceso(int pid){

	bool mismoProceso(void* nodoSpa){
		return ((t_nodo_proceso*)nodoSpa)->pid==pid;
	}
	t_nodo_proceso* nodoProceso=list_find(ptrProcesos, mismoProceso);
	return nodoProceso;
}

t_nodo_segmento* encontrarNodoMismoProcesoMismaBase(int pid,int base){
	t_nodo_proceso* nodoProceso=encontrarNodoMismoProceso(pid);

	bool mismaBase(void* nodo){
		return ((t_nodo_segmento*)nodo)->base==base;
	}
	t_nodo_segmento* nodoEncontrado=list_find(nodoProceso->tablaSegmentos, mismaBase);
	return nodoEncontrado;
}

int spaRecibirArchivo(int socket_cliente, int pid,int lineasAGuardar){
	int cantidadPaginasAGuardar = (lineasAGuardar*config->max_linea)/config->tamanio_pag;
	codigoError = OK;
	if(((lineasAGuardar*config->max_linea) % config->tamanio_pag)!=0){
	    cantidadPaginasAGuardar=cantidadPaginasAGuardar+1;
	}
//borrar
//	log_trace(loggerFm9,"cantidadPaginasAGuardar: %d",cantidadPaginasAGuardar);
	int framesLibres = list_count_satisfying(ptrFrames, spaNodoFrameLibre);
	//borrar
//	log_trace(loggerFm9,"framesLibres >= cantidadPaginasAGuardar: %d:%d",framesLibres,cantidadPaginasAGuardar);
	int baseInicial=-1;
	if(framesLibres >= cantidadPaginasAGuardar){
	    if(enviarMensajeInt(socket_cliente, OK)==-1){
	    	log_error(loggerFm9,"Error al confirmar espacio disponible para guardar archivo:%d",enviarMensajeInt);
	    	return -1;
	   	}
	    char* buffer=malloc(config->max_linea);
	    int tamanioDeLinea;
	    t_nodo_proceso* nodoProceso=encontrarNodoMismoProceso(pid);
	    if(nodoProceso==NULL){
	    	list_add(ptrProcesos, crearNodoProceso(pid));
	    }
	    for(int i=0;i<lineasAGuardar;i=i+ptrAdminMemoria->lineasPorPagina){
	    	idAAsignar++;
	    	t_nodo_frame* nodoFrameLibre = list_find(ptrFrames, &spaNodoFrameLibre);
	    	nodoFrameLibre->numPag=idAAsignar;
	        if(baseInicial==-1){
	        	baseInicial=nodoFrameLibre->numFrame*ptrAdminMemoria->lineasPorPagina;
	        	t_nodo_proceso* nodoProceso=encontrarNodoMismoProceso(pid);
	        	list_add(nodoProceso->tablaSegmentos, crearNodoSegmento(baseInicial));
	        }
	        t_nodo_segmento* nodoSegmento=encontrarNodoMismoProcesoMismaBase(pid,baseInicial);
	        list_add(nodoSegmento->tablaPaginas, crearNodoPagina(idAAsignar));
	    	for(int j=0;j<ptrAdminMemoria->lineasPorPagina && i+j<lineasAGuardar;j++){
	    		recibirMensajeInt(socket_cliente, &codigoError);
	    		// DAM notifica si hubo un error, si está OK se recibe la línea
	    		if(codigoError != OK){
	    			free(buffer);
	    			return -1;
	    		}
	    		memset(buffer, 0, config->max_linea);
	    		tamanioDeLinea=recibirLinea(socket_cliente, buffer);
	    		log_trace(loggerFm9,"Base:%d,numero de pagina:%d  y tamanio de la linea a guardar: %d",nodoSegmento->base,idAAsignar,tamanioDeLinea);
	    		if(tamanioDeLinea==-1){
	    			log_error(loggerFm9,"Error al recibir linea numero:%d",i);
	    			free(buffer);
	    			return -1;
	    		}else{
	    			spaGuardarArchivo(buffer, pid,j,nodoFrameLibre,tamanioDeLinea);
	    			//borrar
//	    			log_trace(loggerFm9," guardado en buffer: %s",buffer);
	    		}
	    	}
	    }
	    free(buffer);
	} else {
		enviarMensajeInt(socket_cliente,ABRIR_ESPACIO_MEMORIA_INSUFICIENTE);
	    return -1;
	}
	log_info(loggerFm9,"Direccion de memoria enviada a Dam: %d",baseInicial);
	return baseInicial;
}

int* crearNodoPagina(int numeroPagina){
	int* nodo = malloc(sizeof(int));
	*nodo = numeroPagina;
	return nodo;
}

void spaGuardarArchivo(char* buffer,int pid,int numLinea,t_nodo_frame* nodoFrameLibre,int tamanioDeLinea){
	ptrAdminMemoria->tablaStorage[(nodoFrameLibre->numFrame*ptrAdminMemoria->lineasPorPagina) + numLinea]=tamanioDeLinea;
	memset(ptrAdminMemoria->storage + (nodoFrameLibre->numFrame*config->tamanio_pag)+(numLinea*config->max_linea), 0, config->max_linea);
	memcpy(ptrAdminMemoria->storage + (nodoFrameLibre->numFrame*config->tamanio_pag)+(numLinea*config->max_linea), buffer, config->max_linea);
}

void spaEnviarEscriptorio(int socket_cliente,int pid,int numBase){
	t_nodo_segmento* nodoSegmento=encontrarNodoMismoProcesoMismaBase(pid, numBase);
	if(nodoSegmento==NULL){
		log_error(loggerFm9,"No se encuentra en proceso");
		return;
	}

	int cantPaginas=list_size(nodoSegmento->tablaPaginas);
	int tamanioMaxArchivo=cantPaginas*ptrAdminMemoria->lineasPorPagina*config->max_linea;
	char* archivoMax=malloc(tamanioMaxArchivo);
	memset(archivoMax,0,tamanioMaxArchivo);
	int tamanioActualArchivo=0;
	for(int i=0;i<cantPaginas;i++){
		int* pagina=list_get(nodoSegmento->tablaPaginas, i);
		t_nodo_frame* nodoFrame=obtenerNodoframe(*pagina);
		bool finDeLineaDePagina=false;
		for(int j=0;j<ptrAdminMemoria->lineasPorPagina && !finDeLineaDePagina;j++){
			int desplazamientoFrame =nodoFrame->numFrame *config->tamanio_pag;
			int desplazamientoLinea = j*config->max_linea;
			void* posicionDelStorage = ptrAdminMemoria->storage+desplazamientoFrame+desplazamientoLinea;
			int tamanioLineaACopiar = ptrAdminMemoria->tablaStorage[nodoFrame->numFrame*ptrAdminMemoria->lineasPorPagina+j];
			if(tamanioLineaACopiar<=0){
				finDeLineaDePagina=true;
			}
			memcpy(archivoMax+tamanioActualArchivo,posicionDelStorage,tamanioLineaACopiar);
			tamanioActualArchivo += tamanioLineaACopiar;
		}
	}
	archivoMax[tamanioActualArchivo]='\0';
	int tamanioArchivoAEnviar=strlen(archivoMax);
	//borrar
	log_trace(loggerFm9,"TamanioArchivoAEnviar:%d",tamanioArchivoAEnviar);
	log_trace(loggerFm9,"ArchivoAEnviar:%s",archivoMax);
	if(enviarMensajeInt(socket_cliente,tamanioArchivoAEnviar)==-1){
		log_error(loggerFm9,"Error al enviar tamanio de linea a enviar");
		free(archivoMax);
		return;
	}
//se envio escriptorio
	enviarMensaje(socket_cliente, archivoMax,tamanioArchivoAEnviar);
	log_info(loggerFm9,"Se envio el archivo completo a cpu\n");
	free(archivoMax);

}

t_nodo_frame* obtenerNodoframe(int numPagina){
	bool mismaPagina(void* nodoFrame){
		return ((t_nodo_frame*)nodoFrame)->numPag ==numPagina;
	}
	t_nodo_frame* nodoEncontrado=list_find(ptrFrames, mismaPagina);
	return nodoEncontrado;
}

void spaCerrar(int socket_cliente,int pid,int numBase) {
    void liberarFrameYTablaStorage(void* pagina) {
        int* numPagina = (int*)pagina;
        int tamanioPag = config->tamanio_pag;
        t_nodo_frame* nodoFrame=obtenerNodoframe(*numPagina);
        log_trace(loggerFm9,"Frame %d, pagina %d, base %d del archivo a liberar del proceso %d",nodoFrame->numFrame,nodoFrame->numPag,numBase,pid);
        memset(ptrAdminMemoria->storage +(nodoFrame->numFrame*tamanioPag), 0, tamanioPag);
        for(int i=0;i<ptrAdminMemoria->lineasPorPagina;i++){
        	ptrAdminMemoria->tablaStorage[(nodoFrame->numFrame*ptrAdminMemoria->lineasPorPagina) + i]=0;
        }
        nodoFrame->numPag = -1;

    }
    t_nodo_segmento* nodoSegmento=encontrarNodoMismoProcesoMismaBase(pid, numBase);
    list_iterate(nodoSegmento->tablaPaginas, liberarFrameYTablaStorage);
    t_nodo_proceso* nodoProceso=encontrarNodoMismoProceso(pid);
    bool mismaBase(void* nodo){
        return ((t_nodo_segmento*)nodo)->base==nodoSegmento->base;
    }

    void liberarNodoSegmento(void* nodo){
    	void liberarPagina(void* pagina) {
    		free(pagina);
    	}
//valgrind-ver si hay que agregarle null
    	t_nodo_segmento* nodoSegmento=(t_nodo_segmento*)nodo;
    	list_destroy_and_destroy_elements(nodoSegmento->tablaPaginas, liberarPagina);
        free(nodo);
    }

    list_remove_and_destroy_by_condition(nodoProceso->tablaSegmentos, mismaBase, liberarNodoSegmento);

    void liberarNodoProceso(void* nodoProceso){
    	list_destroy(((t_nodo_proceso*)nodoProceso)->tablaSegmentos);
    	free(nodoProceso);
    }

    bool mismoPid(void* nodo){
    	int pid=((t_nodo_proceso*)nodo)->pid;
    	return pid==nodoProceso->pid;
    }

    if(list_is_empty(nodoProceso->tablaSegmentos)){
    	list_remove_and_destroy_by_condition(ptrProcesos, mismoPid, liberarNodoProceso);
    }
    if(enviarMensajeInt(socket_cliente,OK)==-1){
    	log_error(loggerFm9,"Error al enviar FALLO DE SEGMENTO 30002 al cerrar el arch.");
    }
    log_info(loggerFm9,"se libero espacio de memoria.");
    return;
}

void spaModificarLinea(int socket_cliente,int base,int pid,int lineaAModificar,char* mensaje,int tamanioLinea){
	t_nodo_segmento* nodoSegmento=encontrarNodoMismoProcesoMismaBase(pid, base);
	if(nodoSegmento==NULL){
		// TODO retornar error "fallo de segmento"
		log_error(loggerFm9,"No se encontro nodo por pid:%d y por base:%d",pid,base);
		if(enviarMensajeInt(socket_cliente,ASIGNAR_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 20002");
		}
		return;
	}
//paginas ocupadas:indice de la tabla de paginas con el cual voy a acceder para encontrar el frame.
	int paginasOcupadas=lineaAModificar/ptrAdminMemoria->lineasPorPagina;
//restoCantPaginas:si es igual a cero se resta en  ya que el indice al que se quiere acceder arranca en 0
	int restoCantPaginas=lineaAModificar%ptrAdminMemoria->lineasPorPagina;
	if(restoCantPaginas==0){
		paginasOcupadas=paginasOcupadas-1;
	}
//indica la linea de la pagina a modificar sin tener en cuenta que el indice para acceder a memoria arranca en 0.
//Para acceder a la memoria se tiene que restar en 1.
	int lineaDePaginaAModificar=lineaAModificar-(paginasOcupadas*ptrAdminMemoria->lineasPorPagina);
	if(lineaDePaginaAModificar<=0){
		// TODO retornar error "fallo de segmento"
		log_error(loggerFm9,"linea sobrepasa la cant de paginas del archivo. pid:%d y por base:%d",pid,base);
		if(enviarMensajeInt(socket_cliente,ASIGNAR_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 20002");
		}
		return;
	}
	log_trace(loggerFm9,"posicion del nodo de pagina en la que se encuentra la linea :%d\n",paginasOcupadas);
	log_trace(loggerFm9,"lineas restantes de una pagina :%d\n",restoCantPaginas);
	int* pagina=list_get(nodoSegmento->tablaPaginas, paginasOcupadas);
	if(pagina==NULL){
		log_error(loggerFm9,"No se encontro la pagina a modificar");
		return;
	}
	t_nodo_frame* nodoFrame=obtenerNodoframe(*pagina);
	log_trace(loggerFm9,"Frame %d,Pagina %d y base %d del archivo a modificar.",nodoFrame->numFrame,*pagina,base);
//	log_trace(loggerFm9,"Frame donde se va a modificar:%d\n",nodoFrame->numFrame);
//	log_trace(loggerFm9,"numero de pagina que se va a modificar :%d\n",*pagina);

	ptrAdminMemoria->tablaStorage[(nodoFrame->numFrame*ptrAdminMemoria->lineasPorPagina)+(lineaAModificar-1)]=tamanioLinea;
	memset(ptrAdminMemoria->storage+(nodoFrame->numFrame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea),0,config->max_linea);
	memcpy(ptrAdminMemoria->storage+(nodoFrame->numFrame*config->tamanio_pag)+((lineaDePaginaAModificar-1)*config->max_linea),mensaje,tamanioLinea);
	if(enviarMensajeInt(socket_cliente,OK)==-1){
		log_error(loggerFm9,"Error al enviar que se encontro el pid en spa para poder enviar el archivo a dma");
		return;
	}
}

void spaEnviarArchivo(int socket_cliente, int pid,int base){
	t_nodo_segmento* nodoSegmento=encontrarNodoMismoProcesoMismaBase(pid, base);
	if(nodoSegmento==NULL){
		log_error(loggerFm9,"Error FALLO DE SEGMENTO 30002");
		if(enviarMensajeInt(socket_cliente,FLUSH_FALLO_SEGMENTO_MEMORIA)==-1){
			log_error(loggerFm9,"Error al enviar el FALLO DE SEGMENTO 30002");
		}
	    return;
	}
	if(enviarMensajeInt(socket_cliente,OK)==-1){
    	log_error(loggerFm9,"Error al informar que se encontro la direccion de memoria para poder enviar el archivo a dma");
    	return;
    }
    int cantidadPaginas=list_size(nodoSegmento->tablaPaginas);
    int* ultimaPagina=list_get(nodoSegmento->tablaPaginas, cantidadPaginas-1);
    int cantLineasUltimaPagina;
//posicion de la primer linea del frame
    t_nodo_frame* nodoFrame=obtenerNodoframe(*ultimaPagina);
    int lineaBase=nodoFrame->numFrame*ptrAdminMemoria->lineasPorPagina;
//se calcula la cantidad de lineas de la ultima pagina
    for(cantLineasUltimaPagina=0;ptrAdminMemoria->tablaStorage[lineaBase+cantLineasUltimaPagina]>0 &&
    	cantLineasUltimaPagina<ptrAdminMemoria->lineasPorPagina;cantLineasUltimaPagina++);
//se calcula el total de lineas sumando las lineas por paginas sin tener en cuenta la ultima pagina + las lineas calculadas de la ultima pagina
    int cantidadDeLineasAEnviar=((cantidadPaginas-1)*ptrAdminMemoria->lineasPorPagina)+cantLineasUltimaPagina;
    if(enviarMensajeInt(socket_cliente,cantidadDeLineasAEnviar)==-1){
    	log_error(loggerFm9,"Error al enviar la cantidad de lineas que envio a dma");
    	return;
    }
    void spaEnviarPagina(void* numPagina){
        int pagina=*((int*)numPagina);
        t_nodo_frame* nodoFrame=obtenerNodoframe(pagina);
        int frame=nodoFrame->numFrame;
        int cantLineasPorPagina;
        if((*ultimaPagina)==pagina){
        	cantLineasPorPagina=cantLineasUltimaPagina;
        }else{
        	cantLineasPorPagina = ptrAdminMemoria->lineasPorPagina;
        }
        enviarArchivoTotal(socket_cliente, ptrAdminMemoria->storage + (frame*config->tamanio_pag),cantLineasPorPagina,config->max_linea);
    }

    list_iterate(nodoSegmento->tablaPaginas, spaEnviarPagina);
    log_info(loggerFm9,"Se envio el archivo completo a dam\n");
}

void procesarHandshake(int socket_cliente, Cabecera* cabecera){
	if(cabecera->idOperacion==DAM_FM9){
		log_info(loggerFm9, "DAM se conectó en socket: %d", socket_cliente);
		if(enviarCabecera(socket_cliente, "", DAM_FM9, loggerFm9)==ERROR){
			log_error(loggerFm9,"Error al enviar el handshake");
		} else {
			recibirMensajeInt(socket_cliente, &transferSizeDAM);
			enviarMensajeInt(socket_cliente, config->max_linea);
			log_trace(loggerFm9, "FM9 recibió de DAM transfer size: %i", transferSizeDAM);
		}
	}else{
		log_info(loggerFm9, "CPU se conectó en socket: %d", socket_cliente);
		enviarCabecera(socket_cliente, "", CPU_FM9, loggerFm9);
	}
}

// Funciones para CONSOLA
void finalizarConsola(pthread_t hiloConsola){
    pthread_join(hiloConsola, NULL);
}

pthread_t iniciarConsola(){
    // Levanto hilo para consola
	pthread_t hiloConsola;
	int resultConsola = pthread_create(&hiloConsola, NULL, procesoConsola, (void*) NULL);
	if(resultConsola != 0){
	    log_error(loggerFm9,"No se pudo crear el hilo para la consola: %s", strerror(resultConsola));
	} else {
	    log_info(loggerFm9, "Consola inicializada correctamente");
	}

	return hiloConsola;
}

bool esSalirConsola(char* nombreComando){
	return (string_equals_ignore_case(nombreComando, "exit") ||
			string_equals_ignore_case(nombreComando, "quit") ||
			string_equals_ignore_case(nombreComando, "q"));
}

void * procesoConsola(void* arg){

    while(1){
        char* entered_command = (char*) readline("\n Ingrese un comando: ");
        char** command_data = (char**)string_split(entered_command, " ");
        char* commandName = command_data[0];
        char* parameter = command_data[1];
        if(commandName != NULL){
			printf("Procesando el comando '%s'\n", commandName);
			int command_index = verifyCommand(commandName);
			if(command_index == -1){
				puts("El comando ingresado no es soportado por FM9");
			}
			else{
				Command command = commands[command_index];
				if(parameter == NULL){
					if(command.accepts_empty_param){
						if(esSalirConsola(command.name)){
							free(command_data[0]);
							free(command_data[1]);
							free(command_data);
						}
						command.operation(NULL);
					} else {
						puts("ERROR. El comando ingresado necesita un parametro");
					}
				} else {
					command.operation((void*)parameter);
					free(command_data[1]);
				}
			}
			free(command_data);
			free(commandName);
        } else {
        	puts("Por favor, introduzca un comando válido");
        }
		free(entered_command);
    }
    return NULL;
}

int verifyCommand(char* command) {
	int i;
	for (i = 0; commands[i].name; i++) {
		if (strcmp(command, commands[i].name) == 0) {
			return i;
		}
	}
	return -1;
}

void* mostrarDump(void* pidString){
	int pid = atoi(pidString);
	log_info(loggerFm9, "Realizando Dump pid: %d", pid);
	switch(config->modo){
		case SEGMENTACION_PURA:
			segmentacionDump(pid);
			break;
		case PAG_INVERTIDAS:
			tpiDump(pid);
			break;
		case SEGMENTACION_PAG:
			segPagDump(pid);
			break;
		default :
			log_error(loggerFm9,"Modo de almacenamiento de la memoria invalido");
			break;
	}
	return NULL;
}

void segmentacionDump(int pid) {
    int idSegmento = 0; //coincide con indice en la lista
    bool seLogueo = false;
    void mostrarSegmento(void* nodoSegmento){
    	t_nodo* nodo = (t_nodo*)nodoSegmento;
        if(nodo->pid == pid){
            log_info(loggerFm9, "Segmento: %d\t Dir.Logica: %d %d\t Dir.Fisica: %d - %d\n",
            		idSegmento,nodo->base/config->max_linea, nodo->limite, nodo->base/config->max_linea,
					((nodo->base/config->max_linea) + nodo->limite));
            int i;
            for(i=0; i<nodo->limite; i++){
            	char*linea = malloc(config->max_linea + 1);
            	memcpy(linea, ptrAdminMemoria->storage + nodo->base + (i * config->max_linea), config->max_linea);
            	linea[config->max_linea]='\0';
            	log_info(loggerFm9, "Linea %d: %s", i, linea);
            	free(linea);
            }
            seLogueo=true;
        }
        idSegmento++;
    }

    list_iterate(ptrAdminMemoria->ptrSegmentos, mostrarSegmento);
    if(!seLogueo) {
    	printf("El proceso %d no se encuentra en memoria", pid);
    }
}

void tpiDump(int pid) {
    bool ordenaPorPagina(t_nodo_tpi* nodo1, t_nodo_tpi* nodo2) {
    	return (nodo1->pagina < nodo2->pagina);
    }

    bool mismoPid(void* nodo) {
    	t_nodo_tpi* nodoTpi = (t_nodo_tpi*)nodo;
        return nodoTpi->pid == pid;
    }

    t_list* nodosPid = list_filter(ptrAdminMemoria->ptrSegmentos, mismoPid);
    if(list_is_empty(nodosPid)){
    	printf("El proceso %d no se encuentra en memoria", pid);
    	return;
    }
    list_sort(nodosPid, (void*) ordenaPorPagina);

    void mostrarPagina(void* nodoTPI){
    	t_nodo_tpi* nodo = (t_nodo_tpi*)nodoTPI;
        int dirFisicaIni = nodo->frame * ptrAdminMemoria->lineasPorPagina;
        log_info(loggerFm9, "Pagina: %d - Frame: %d - Dir.Fisica: %d - %d",
        		nodo->pagina, nodo->frame, dirFisicaIni,
				dirFisicaIni + ptrAdminMemoria->lineasPorPagina);
        int i;
        for(i=0; i < ptrAdminMemoria->lineasPorPagina; i++){
        	char *linea=malloc(config->max_linea + 1);
            memcpy(linea, ptrAdminMemoria->storage + (dirFisicaIni * config->max_linea) +
            		(i * config->max_linea), config->max_linea);
            linea[config->max_linea]='\0';
            log_info(loggerFm9, "Linea %d: %s", i, linea);
            free(linea);
        }
    }
    list_iterate(nodosPid, mostrarPagina);

    list_destroy(nodosPid);
}

void segPagDump(int pid) {
	t_nodo_proceso* nodoProceso = encontrarNodoMismoProceso(pid);
	if(nodoProceso==NULL){
	    printf("El proceso %d no se encuentra en memoria", pid);
	    return;
	}
	void mostrarPagina(void* pagina){
		int* numPagina = (int*)pagina;
		int tamanioPag = config->tamanio_pag;
		t_nodo_frame* nodoFrame = obtenerNodoframe(*numPagina);
    	log_info(loggerFm9, "Página: %d - Frame: %d", *numPagina, nodoFrame->numFrame);
    	int i;
        for(i=0; i < ptrAdminMemoria->lineasPorPagina; i++){
        	char *linea=malloc(config->max_linea + 1);
        	memcpy(linea, ptrAdminMemoria->storage + (nodoFrame->numFrame * tamanioPag) +
        	            		(i * config->max_linea), config->max_linea);
        	linea[config->max_linea]='\0';
        	log_info(loggerFm9, "Linea %d: %s", i, linea);
        	free(linea);
        }
	}

	void mostrarSegmento(void* nodoSegmento){
		t_nodo_segmento* nodo = (t_nodo_segmento*)nodoSegmento;
        log_info(loggerFm9, "Base: %d - Cantidad de páginas: %d",
        		nodo->base, list_size(nodo->tablaPaginas));
		list_iterate(nodo->tablaPaginas, mostrarPagina);
	}
	list_iterate(nodoProceso->tablaSegmentos, mostrarSegmento);
}

void* salirConsola(void* idVoid){
	puts("Cerrando el proceso FM9\n");
	liberarRecursos();
	puts("se libero la memoria");
	exit_gracefully(1, loggerFm9);
	return NULL;
}
void liberarRecursos(){
	free(ptrAdminMemoria->storage);
	liberarEstructurasModoMemoria();
	free(ptrAdminMemoria->tablaStorage);
	free(config->puerto_fm9);
	free(config->ip);
	free(config);
}
void liberarEstructurasModoMemoria(){
	void liberaFrame(void* nodo){
		free(nodo);
	}
	switch(config->modo){
		case SEGMENTACION_PURA:

			list_destroy_and_destroy_elements(ptrAdminMemoria->ptrSegmentos,liberaFrame);
			break;
		case PAG_INVERTIDAS:
			list_destroy_and_destroy_elements(ptrAdminMemoria->ptrSegmentos,liberaFrame);
			break;
		case SEGMENTACION_PAG:

			list_destroy_and_destroy_elements(ptrFrames,liberaFrame);

			void eliminarProceso(void* nodo){
				t_nodo_proceso* nodoProceso=(t_nodo_proceso*)nodo;
				if(!list_is_empty(nodoProceso->tablaSegmentos)){
					void eliminarSegmento(void* nodo){
						void liberaPagina(void* pagina){
							free(pagina);
						}
						t_nodo_segmento* nodoSegmento=(t_nodo_segmento*) nodo;
						list_clean_and_destroy_elements(nodoSegmento->tablaPaginas, liberaPagina);
					}
					list_clean_and_destroy_elements(nodoProceso->tablaSegmentos, eliminarSegmento);
				}

			}


			list_destroy_and_destroy_elements(ptrProcesos, eliminarProceso);

			break;
		default :
			printf("Error al liberar estructuras de la memoria");
		   break;
		}

}
