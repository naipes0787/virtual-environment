/*
 ============================================================================
 Name        : safa.c
 Author      : inaki
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */
#include "safa.h"

int main(void) {
	// Inicializo variables, logger y estado
 	puts("Inicializando SAFA.");
	initialize();

	 // Levanto hilo para consola
	pthread_t hiloConsola;
	int resultConsola = pthread_create(&hiloConsola, NULL, listenToConsole, (void*) NULL);
	if(resultConsola != 0){
		log_error(loggerSafa, "\ncan't create thread :[%s]\n", strerror(resultConsola));
	} else {
		puts("Hilo consola creado");
	}

	// Creo socket servidor
	serverSocket = crearServerSocket(ip, port, loggerSafa);

	// Levanto hilo Dam
	conectarDAM();
	pthread_t hiloDam;
	pthread_create(&hiloDam, NULL, comunicacionDam, NULL);

	// Levanto hilo plp
	 pthread_t hiloPlp;

	 int resultPlp = pthread_create(&hiloPlp, NULL, funcionalidadPlp, NULL);

	 if(resultPlp != 0){
	 		log_error(loggerSafa, "\ncan't create thread :[%s]\n", strerror(resultPlp));
	 } else {
	 		puts("Hilo PLP creado");
	 }

	// Levanto hilo para CPUs (pcp)
	 pthread_t hiloPcp;
	 int resultPcp = pthread_create(&hiloPcp, NULL, funcionalidadPcp, NULL);

	 if(resultPcp != 0){
			log_error(loggerSafa, "\ncan't create thread :[%s]\n", strerror(resultPcp));
	 } else {
			puts("Hilo PCP creado");
	 }

	// Levanto hilo inotify
	 pthread_t hiloInotify;
	 int resultInotify = pthread_create(&hiloInotify, NULL, funcionalidadInotify, NULL);

	 if(resultInotify != 0){
			log_error(loggerSafa, "\ncan't create thread :[%s]\n", strerror(resultInotify));
	 } else {
			puts("Hilo Inotify creado");
	 }

	// Finalizo
	pthread_join(hiloConsola, NULL);
	pthread_join(hiloPlp, NULL);
	pthread_join(hiloPcp, NULL);
	pthread_join(hiloDam, NULL);
	pthread_join(hiloInotify, NULL);
	liberarTodo();
	liberarInotify();
	exit_gracefully(1, loggerSafa);
	return 0;
}

void liberarInotify(){
	if(inotifyFd != -1){
		inotify_rm_watch(inotifyFd, configFd);
		close(inotifyFd);
	}
}

void liberarTodo(){
	list_destroy_and_destroy_elements(ListaGeneral, freeDtb);
	list_destroy_and_destroy_elements(CpuConectadas, free);
	list_destroy_and_destroy_elements(TablaRecursos, freeRecurso);
	list_destroy(ListaReady);
	list_destroy(ListaBlocked);
	list_destroy(ListaRunning);
	list_destroy(ListaTerminated);
	queue_destroy(ColaNew);
	free(algoritmo);
	free(port);
	free(ip);
}

void conectarDAM(){
	socketDam = servidorConectarComponenteConCabecera(serverSocket, DAM_SAFA, loggerSafa);

	if(socketDam != 0){
		damConectado = true;
	} else {
		log_error(loggerSafa, "Hubo un error conectando Safa con DAM. Abortando Safa.");
		liberarTodo();
		exit_gracefully(1, loggerSafa);
	}
}

void* comunicacionDam(void* arg){
	while(1){
		int resultado;
		Cabecera* header = recibirCabecera(socketDam, &resultado, loggerSafa);
		if(resultado > 0){
			switch (header->idOperacion) {
						case ARCHIVO_CARGADO:
							operacionArchivoCargado(socketDam);
							break;
						case ERROR_CARGA:
							manejarErrorCarga(socketDam);
							break;
						case INICIAR_DUMMY:
							manejarDummyFinalizado(socketDam);
							break;
						case PASAR_A_READY:
							manejarReadyDam(socketDam);
							break;
			}
		} else if(resultado <= 0){
			 log_error(loggerSafa, "DAM se desconecto de Safa");
			 liberarTodo();
			 liberarInotify();
			 exit_gracefully(1, loggerSafa);
		}
		free(header);
	}
	return NULL;
}

void manejarReadyDam(t_socket socketDam){
	int idDtb;
	recibirMensajeInt(socketDam, &idDtb);
	Dtb* dtb = buscarDtbPorId(idDtb);
	sem_wait(&usoListaBlocked);
	list_remove_by_id(ListaBlocked, idDtb);
	sem_post(&usoListaBlocked);
	sem_wait(&usoListaReady);
	list_add(ListaReady, dtb);
	sem_post(&usoListaReady);
	log_info(loggerSafa, "Se paso a Ready al DTB %i, luego de finalizar la operacion de E/S", dtb->id);
}

void operacionArchivoCargado(t_socket socketDam){
	char* path;
	int largoPath, ubicacionFm9;

	// Recibo path del archivo
	recibirMensajeInt(socketDam, &largoPath);
	path = malloc(largoPath + 1);
	recibirMensaje(socketDam, &path, largoPath);

	// Recibo ubicacion en fm9 del archivo
	recibirMensajeInt(socketDam, &ubicacionFm9);

	// Obtengo el id del Dtb
	int idDtb;
	recibirMensajeInt(socketDam, &idDtb);

	// Busco el dtb por id
	Dtb* dtb = buscarDtbPorId(idDtb);

	if(dtb->escriptorioCargado){
		// No es el escriptorio principal, lo agrego a lista de archivos abiertos
		Archivo* archivo = malloc(sizeof(Archivo));
		archivo->escriptorio = path;
		archivo->ubicacion = ubicacionFm9;
		list_add(dtb->archivos_abiertos, archivo);
		log_trace(loggerSafa, "SAFA recibio confirmacion de carga de archivo %s en FM9, solicitado por el DTB %i", path, idDtb);
	} else {
		// Aca no deberia entrar, pero por las dudas lo dejo
		dtb->ubicacionEscriptorio = ubicacionFm9;
		dtb->escriptorioCargado = true;
	}
	sem_wait(&usoListaBlocked);
	list_remove_by_id(ListaBlocked, idDtb);
	sem_post(&usoListaBlocked);
	sem_wait(&usoListaReady);
	list_add(ListaReady, dtb);
	sem_post(&usoListaReady);
}

void manejarDummyFinalizado(t_socket socketDam){
	int largoEntradasSalidas;
	char* entradasSalidas;

	// Obtengo el id del Dtb
	int idDtb;
	recibirMensajeInt(socketDam, &idDtb);

	// Busco el dtb por id
	Dtb* dtb = buscarDtbPorId(idDtb);

	// Recibo ubicacion en fm9 del archivo
	recibirMensajeInt(socketDam, &dtb->ubicacionEscriptorio);
	// Recibo lista de Entradas/Salidas
	recibirMensajeInt(socketDam, &largoEntradasSalidas);
	entradasSalidas = malloc(largoEntradasSalidas + 1);
	recibirMensaje(socketDam, &entradasSalidas, largoEntradasSalidas);
	asignarListaDeIOs(dtb, entradasSalidas);
	if(dtb->escriptorioCargado){
		// No es el escriptorio principal
	} else {
		dtb->escriptorioCargado = true;
	}
	sem_wait(&usoListaReady);
	list_add(ListaReady, dtb);
	sem_post(&usoListaReady);
	log_trace(loggerSafa, "SAFA recibio confirmacion de carga de archivo en FM9, tras operacion Dummy del DTB %i", idDtb);
	log_trace(loggerSafa, "Se agrega el DTB a la cola de Ready\n");
}

void procesarErrorCode(int errorCode){
	switch (errorCode){

		case ABRIR_PATH_INEXISTENTE: {
			log_warning(loggerSafa, "ABRIR: El path que se desea abrir es inexistente\n");
			break;
		}

		case ABRIR_ESPACIO_MEMORIA_INSUFICIENTE: {
			log_warning(loggerSafa, "ABRIR: Memoria insuficiente en FM9 para concluir la apertura\n");
			break;
		}

		case ASIGNAR_ARCHIVO_NO_ABIERTO: {
			log_warning(loggerSafa, "ASIGNAR: El archivo no se encuentra abierto\n");
			break;
		}

		case ASIGNAR_FALLO_SEGMENTO_MEMORIA: {
			log_warning(loggerSafa, "ASIGNAR: Fallo de segmento/memoria\n");
			break;
		}

		case ASIGNAR_ESPACIO_MEMORIA_INSUFICIENTE: {
			log_warning(loggerSafa, "ASIGNAR: Espacio insuficiente en FM9\n");
			break;
		}

		case FLUSH_ARCHIVO_NO_ABIERTO: {
			log_warning(loggerSafa, "FLUSH: El archivo no se encuentra abierto\n");
			break;
		}

		case FLUSH_FALLO_SEGMENTO_MEMORIA: {
			log_warning(loggerSafa, "FLUSH: Fallo de segmento/memoria\n");
			break;
		}

		case FLUSH_ESPACIO_MDJ_INSUFICIENTE: {
			log_warning(loggerSafa, "FLUSH: Espacio insuficiente en MDJ\n");
			break;
		}

		case FLUSH_ARCHIVO_INEXISTENTE_MDJ: {
			log_warning(loggerSafa, "FLUSH: El archivo no existe en MDJ\n");
			break;
		}

		case CLOSE_ARCHIVO_NO_ABIERTO: {
			log_warning(loggerSafa, "CLOSE: El archivo no se encuentra abierto\n");
			break;
		}

		case CLOSE_FALLO_SEGMENTO_MEMORIA: {
			log_warning(loggerSafa, "CLOSE: Fallo de segmento/memoria");
			break;
		}

		case CREAR_ARCHIVO_EXISTENTE: {
			log_warning(loggerSafa, "CREAR: Archivo ya existente");
			break;
		}

		case CREAR_ESPACIO_INSUFICIENTE: {
			log_warning(loggerSafa, "CREAR: Espacio insuficiente");
			break;
		}

		case BORRAR_ARCHIVO_INEXISTENTE: {
			log_warning(loggerSafa, "BORRAR: El archivo no existe");
			break;
		}
	}
}

void procesarBlockCode(int idDtb, int blockCode){
	switch (blockCode){

		case BLOCK_BUSCAR_ARCHIVO: {
			log_info(loggerSafa, "Se bloquea el DTB %i al abrir archivo\n", idDtb);
			break;
		}

		case BLOCK_ESPERAR_RECURSO: {
			log_info(loggerSafa, "Se bloquea el DTB %i al realizar wait sobre recurso no disponible\n", idDtb);
			break;
		}

		case BLOCK_FLUSH: {
			log_info(loggerSafa, "Se bloquea el DTB %i al realizar operacion flush\n", idDtb);
			break;
		}

		case BLOCK_CREAR_ARCHIVO: {
			log_info(loggerSafa, "Se bloquea el DTB %i al crear un archivo nuevo\n", idDtb);
			break;
		}

		case BLOCK_BORRAR_ARCHIVO: {
			log_info(loggerSafa, "Se bloquea el DTB %i al borrar un archivo\n", idDtb);
			break;
		}
	}
}

void manejarErrorCarga(t_socket socketDam){
	int idDtb, errorCode;
	recibirMensajeInt(socketDam, &idDtb);
	recibirMensajeInt(socketDam, &errorCode);
	log_warning(loggerSafa, "Hubo un error en la operacion con DAM del DTB %i\n", idDtb);
	procesarErrorCode(errorCode);
	char* ubicacionArchivos = obtenerUbicacionArchivos(idDtb);
	enviarMensajeInt(socketDam, strlen(ubicacionArchivos));
	enviarMensaje(socketDam, ubicacionArchivos, strlen(ubicacionArchivos));
	free(ubicacionArchivos);
	finalizar((void*) string_itoa(idDtb));
}

char* obtenerUbicacionArchivos(int idDtb){
	Dtb* dtb = buscarDtbPorId(idDtb);
	char* resultado = string_new();
	void agregarUbicacionAString(void* archivoVoid){
		Archivo* archivo = (Archivo*) archivoVoid;
		string_append(&resultado, ",");
		char* ubicacionAux = string_itoa(archivo->ubicacion);
		string_append(&resultado, ubicacionAux);
		free(ubicacionAux);
	}
	list_iterate(dtb->archivos_abiertos, agregarUbicacionAString);
	if(dtb->escriptorioCargado){
		char* ubicacionEscriptorioAux = string_itoa(dtb->ubicacionEscriptorio);
		string_append(&resultado, ",");
		string_append(&resultado, ubicacionEscriptorioAux);
		free(ubicacionEscriptorioAux);
	}

	return resultado;
}

void initialize(){
	state = Corrupt;
	damConectado = false;
	cpuConectada = false;
	sem_init(&procesosEnNew, 0, 0);
	sem_init(&listaCpusEnUso, 0, 1);
	sem_init(&usoListaReady, 0, 1);
	sem_init(&usoListaBlocked, 0, 1);
	inicializarLogger();
	inicializarListasPlanificacion();
	inicializarIdDtb();
	inicializarConfig();
	inicializarInotify();
	inicializarTablaRecursos();
}

void inicializarLogger(){
	loggerSafa = log_create("safa.log", SAFA_PROCESS_NAME, true, LOG_LEVEL_TRACE);
	log_trace(loggerSafa, "Logger safa inicializado");
}

void inicializarListasPlanificacion(){
	ColaNew = queue_create();
	ListaReady = list_create();
	ListaReadyAux = list_create(); // Lista auxiliar para VRR
	ListaRunning = list_create();
	ListaBlocked = list_create();
	ListaTerminated = list_create();
	ListaGeneral = list_create();
	CpuConectadas = list_create();
	dtbDummy = crearDtbDummy();
	list_add(ListaBlocked, dtbDummy);
	log_info(loggerSafa, "DTB Dummy agregado a lista de bloqueados");
	sem_init(&dummyLibre, 0, 1);
	puts("Colas de planificacion inicializadas");
}


void inicializarIdDtb(){
	proximoIdDtb = 1;
	puts("ID DTBs inicializados");
}

void inicializarConfig(){
	/* Configuración para encontrar el archivo .cfg tanto al buildear
	 * desde root como desde src */
	config = config_create("safa.cfg");
	if(config == NULL){
		pathConfig = "../safa.cfg";
		config = config_create(pathConfig);
	} else {
		pathConfig = "safa.cfg";
	}
	if(config == NULL){
		log_error(loggerSafa, "No se encontro el archivo de configuracion");
	}
	char* portAux = config_get_string_value(config, "PUERTO");
	port = malloc(strlen(portAux) + 1);
	strcpy(port, portAux);
	char* ipAux = config_get_string_value(config, "IP");
	ip = malloc(strlen(ipAux) + 1);
	strcpy(ip, ipAux);
	quantum = config_get_int_value(config, "QUANTUM");
	char* algoritmoAux = config_get_string_value(config, "ALGORITMO");
	algoritmo = malloc(strlen(algoritmoAux) + 1);
	strcpy(algoritmo, algoritmoAux);
	nivelMultiprogramacion = config_get_int_value(config, "MULTIPROGRAMACION");
	retardo = config_get_int_value(config, "RETARDO");
	log_info(loggerSafa, "SAFA configurado. Puerto: %s | IP: %s | Quantum: %i | Algoritmo: %s | Multiprog: %i | Retardo: %i\n",
			port, ip, quantum, algoritmo, nivelMultiprogramacion, retardo);
	portAux = algoritmoAux = ipAux = NULL;
	config_destroy(config);
}

void inicializarInotify(){
	inotifyFd = inotify_init();
	if(inotifyFd < 0){
		perror("inotify_init");
		log_error(loggerSafa, "No se pudo inicializar Inotify");
	}
}

void* funcionalidadInotify(void* arg){
	while(1){
		char buffer[INOTIFY_BUF_LEN];
		configFd = inotify_add_watch(inotifyFd, pathConfig, IN_MODIFY);
		int length = read(inotifyFd, buffer, INOTIFY_BUF_LEN);
		if (length <= 0) {
			perror("read");
		}

		int offset = 0;

		while (offset < length) {
			struct inotify_event *event = (struct inotify_event *) &buffer[offset];
			if (event->mask & IN_MODIFY) {
				log_info(loggerSafa, "Se modifico el archivo de config. Recargando configuracion");
				inicializarConfig();
			}
			offset += sizeof (struct inotify_event) + event->len;
		}
	}
	return NULL;
}

void inicializarTablaRecursos(){
	TablaRecursos = list_create();
}

int obtenerProximoId(){
	int id_actual = proximoIdDtb;
	proximoIdDtb += 1;
	return id_actual;
}

Dtb* crearNuevoDtb(char* rutaEscriptorio){
	Dtb* dtbNuevo = malloc(sizeof(Dtb));
	dtbNuevo->id = obtenerProximoId();
	dtbNuevo->escriptorio = malloc(strlen(rutaEscriptorio)+1);
	strcpy(dtbNuevo->escriptorio, rutaEscriptorio);
	dtbNuevo->escriptorio[strlen(rutaEscriptorio)] = '\0';
	dtbNuevo->inicializado = 1;
	dtbNuevo->pc = 0;
	dtbNuevo->archivos_abiertos = list_create();
	dtbNuevo->quantumDisponible = quantum;
	dtbNuevo->escriptorioCargado = false;
	dtbNuevo->sentenciasEnNew = 0;
	dtbNuevo->sentenciasAlDiego = 0;
	dtbNuevo->tiempoInicial = time(0);
	queue_push(ColaNew, (void*) dtbNuevo);
	sem_post(&procesosEnNew);
	list_add(ListaGeneral, (void*) dtbNuevo); // Lista que guarda todos los Dtb que se ejecutaron
	return dtbNuevo;
}

Dtb* crearDtbDummy(){
	Dtb* dtb = malloc(sizeof(Dtb));
	dtb->id = 0;
	dtb->inicializado = 0;
	dtb->ubicacionEscriptorio = -1;
	dtb->escriptorio = malloc(strlen("DUMMY") + 1);
	strcpy(dtb->escriptorio, "DUMMY");
	dtb->pc = 0;
	dtb->archivos_abiertos = list_create();
	dtb->quantumDisponible = quantum;
	dtb->escriptorioCargado = false;
	log_info(loggerSafa, "DTB Dummy creado");
	return dtb;
}

void llenarDummyConDtb(Dtb* dtbAEjecutar){
	int scriptLength = strlen(dtbAEjecutar->escriptorio);
	dtbDummy->escriptorio = realloc((void*)dtbDummy->escriptorio, scriptLength+1);
	strcpy(dtbDummy->escriptorio, dtbAEjecutar->escriptorio);
	dtbDummy->escriptorio[scriptLength] = '\0';
	dtbDummy->id = dtbAEjecutar->id;
	log_info(loggerSafa, "Se asigna el DTB N°: %i, al DTB Dummy\n", dtbAEjecutar->id);
}

void freeDtb(void* dtbVoid){
	Dtb* dtb = (Dtb*) dtbVoid;
	free(dtb->escriptorio);
	free(dtb->infoES);
	list_destroy_and_destroy_elements(dtb->archivos_abiertos, (void*) freeArchivo);
	free(dtb);
}

void freeRecurso(void* recursoVoid){
	Recurso* recurso = (Recurso*) recursoVoid;
	free(recurso->nombre);
	queue_destroy(recurso->procesosBloqueados);
	free(recurso);
}

void freeArchivo(void* archivoVoid){
	Archivo* archivo = (Archivo*) archivoVoid;
	free(archivo->escriptorio);
	free(archivo);
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

bool esSalirConsola(char* nombreComando){
	return (string_equals_ignore_case(nombreComando, "exit") ||
			string_equals_ignore_case(nombreComando, "quit") ||
			string_equals_ignore_case(nombreComando, "q"));
}

void * listenToConsole(void* arg){
	while(1){
		char* entered_command = (char*) readline("\n Ingrese un comando: ");
		char** command_data = string_split(entered_command, " ");
		char* command = command_data[0];
		char* parameter = command_data[1];
		log_info(loggerSafa, "Procesando el comando '%s'\n", command);
		int command_index = verifyCommand(command);
		if(command_index == -1){
			puts("El comando ingresado no es soportado por SAFA");
		}
		else{
			Command command = commands[command_index];
			if(parameter == NULL){
				if(command.accepts_empty_param){
					if(esSalirConsola(command.name)){
						free(command_data[0]);
						free(command_data[1]);
						free(entered_command);
						free(command_data);
					}
					command.operation(NULL);
				}
				else{
					puts("ERROR. El comando ingresado necesita un parametro");
				}
			}
			else{
				command.operation((void*)parameter);
			}
		}
		free(command_data[0]);
		free(entered_command);
		free(command_data);
	}
	return NULL;
}

void * funcionalidadPlp(void* arg){
	while(1){
		sem_wait(&procesosEnNew);
		if(cantidadProcesosEnMemoria() < nivelMultiprogramacion){
			sem_wait(&dummyLibre);
			Dtb * dtbAEjecutar = queue_pop(ColaNew);
			llenarDummyConDtb(dtbAEjecutar);
			sem_wait(&usoListaBlocked);
			list_remove_by_condition(ListaBlocked, esElDummy);
			sem_post(&usoListaBlocked);
			sem_wait(&usoListaReady);
			list_add(ListaReady, dtbDummy);
			sem_post(&usoListaReady);
			log_info(loggerSafa, "Se agrega el DTB Dummy a Ready");
		} else {
			// Si no puede ejecutar porque alcanzo el nivel de multiprogramacion, se mantiene en new
			sem_post(&procesosEnNew);
		}
	}
	return NULL;
}

void crearNuevaCpu(int socketNuevo, fd_set* master){
	FD_SET(socketNuevo, master); // Agrego nueva Cpu al master fd
	Cpu* nuevoCpu = malloc(sizeof(Cpu));
	nuevoCpu->socket = socketNuevo;
	nuevoCpu->libre = true;
	list_add(CpuConectadas, nuevoCpu);
	log_info(loggerSafa, "Se conecto una nueva Cpu. Hay %i conectadas\n", CpuConectadas->elements_count);
}

void ejecutarDtbsEnReady(){
	t_list* cpuLibres = obtenerCpuLibres();
	if(cpuLibres->elements_count > 0){
		int cpuLibreActual;
		for(cpuLibreActual=0; cpuLibreActual < cpuLibres->elements_count; cpuLibreActual++) {
			Dtb* dtb = obtenerProximoDtb();
			if(dtb == NULL){
				// log_info(loggerSafa, "No hay dtbs en ready al momento de planificar");
				break;
			}
			usleep(retardo * 1000);
			Cpu* cpu = list_remove(cpuLibres, cpuLibreActual);
			enviarDtbAEjecutar(cpu, dtb);
			cpu = NULL;
		}
	}
	list_destroy(cpuLibres);
}

void * funcionalidadPcp(void* arg){
	// Inicializo variables necesarias para select

	fd_set master;
	fd_set read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	int fdmax;
	int nuevaConexion;
	int selectResult;
	struct timeval tv;

	// Agrego el socket de escucha de Safa al fdset master

	FD_SET(serverSocket, &master);
	fdmax = serverSocket;

	while(1){
		tv.tv_sec = 15;
		tv.tv_usec = 0;

		ejecutarDtbsEnReady();

		read_fds = master;

		selectResult = select(fdmax+1, &read_fds, NULL, NULL, &tv);

		if (selectResult == -1) {
			// TODO: A los hilos que hacen while(1) cambiarlo por while(!murio), y si hay error setear murio en true
			perror("select");
			liberarTodo();
			liberarInotify();
			_exit_with_error(serverSocket, "Error en select", NULL, loggerSafa);
		} else if(selectResult == 0){
			// Hubo timeout en el select, continuo con el while
			continue;
		}

		int socketActual;

		for(socketActual = 0; socketActual <= fdmax; socketActual++) {
			if (FD_ISSET(socketActual, &read_fds)) {
				if (socketActual == serverSocket) {
					// LLego una nueva conexion

					nuevaConexion = aceptarConexion(socketActual, loggerSafa);

					if (nuevaConexion == -1) {
						// Error al realizar el accept
						perror("accept");

					} else {
						// Agrega Cpu a lista y al fdset master
						if(!cpuConectada){
							// Marco que se conecto la primer CPU
							cpuConectada = true;
							if(damConectado && cpuConectada){
								// Safa ya puede comenzar a ejecutar
								state = Working;
							}
						}
						crearNuevaCpu(nuevaConexion, &master);
						if (nuevaConexion > fdmax) { fdmax = nuevaConexion; }

					}
				} else {

					// Manejo mensaje enviado por CPU
					int respuestaRecv;
					Cabecera* cabecera = recibirCabecera(socketActual, &respuestaRecv, loggerSafa);
					if (respuestaRecv <= 0) { // Hubo error en el recv o se desconecto el cliente
						eliminarCpu(socketActual);
						FD_CLR(socketActual, &master); // Eliminar Cpu del master set
					} else {
						manejarOperacionCpu(cabecera, socketActual);
					}
					free(cabecera);
				}
			}
		}
	}
	return NULL;
}

void* ejecutar(void* rutaVoid){
	if(state == Corrupt){
		puts("SAFA se encuentra en estado corrupto. No se puede ejecutar");
		return NULL;
	}
	char* ruta = (char*)rutaVoid;
	log_trace(loggerSafa, "\nEjecutando la ruta %s\n", ruta);
    Dtb* dtb = crearNuevoDtb(ruta);
    log_trace(loggerSafa, "DTB creado con id:%i\n", dtb->id);
    free(ruta);
	return NULL;
}

void* status(void* idVoid){
	if (idVoid == NULL){
		if(state == Corrupt){
			printf("SAFA se encuentra en estado corrupto.\n");
		} else {
			printf("SAFA se encuentra en estado operativo.\n");
		}
		printf("Lista NEW: %i elementos\n", ColaNew->elements->elements_count);
			list_iterate(ColaNew->elements, imprimirDtb);
		printf("Lista READY: %i elementos\n", ListaReady->elements_count);
			list_iterate(ListaReady, imprimirDtb);
		printf("Lista EXECUTING: %i elementos\n", ListaRunning->elements_count);
			list_iterate(ListaRunning, imprimirDtb);
		printf("Lista BLOCK: %i elementos\n", ListaBlocked->elements_count);
			list_iterate(ListaBlocked, imprimirDtb);
		printf("Lista FINISHED: %i elementos\n", ListaTerminated->elements_count);
			list_iterate(ListaTerminated, imprimirDtb);
	} else {
		char* idStr = (char*) idVoid;
		int id = atoi(idStr);
		Dtb* dtb = buscarDtbPorId(id);
		if(dtb == NULL){
			puts("El id ingresado no corresponde a un DTB en ejecucion\n");
		} else {
			printf("ID: %i\n SCRIPT: %s\n PC: %i\n INICIALIZADO: %i\n",
							dtb->id, dtb->escriptorio, dtb->pc, dtb->inicializado);
			if(dtb->archivos_abiertos->elements_count > 0){
				puts("Archivos abiertos: \n");
				list_iterate(dtb->archivos_abiertos, imprimirArchivosAbiertos);
			} else {
				puts("No tiene archivos abiertos");
			}
		}
	}
    return NULL;
}

void* finalizar(void* idVoid){
	char* idStr = (char*) idVoid;
	int id = atoi(idStr);
	free(idStr);
	if(estaEnLista(ListaRunning, id)){
		// Cuando esta en running lo flaggeo como eliminado (inicializado = 2) 
		// para eliminarlo cuando vuelva de la cpu
		log_info(loggerSafa, "El DTB se encuentra ejecutando. Se lo finalizara en diferido");
		Dtb* dtb = buscarDtbPorId(id);
		dtb->inicializado = 2;
	} else if (estaEnLista(ListaReady, id)) {
		pasarATerminated(ListaReady, id);
	} else if (estaEnLista(ListaBlocked, id)) {
		pasarATerminated(ListaBlocked, id);
	} else if (estaEnLista(ColaNew->elements, id)){
		pasarATerminated(ColaNew->elements, id);
	} else if (estaEnLista(ListaGeneral, id)){
		// Para los que estan ejecutando el Dummy
		pasarATerminated(ListaGeneral, id);
	} else {
		puts("El id ingresado no corresponde a un DTB en ejecucion\n");
	}
	return NULL;
}

void* salirConsola(void* idVoid){
	puts("Cerrando el proceso SAFA\n");
	liberarInotify();
	liberarTodo();
	exit_gracefully(1, loggerSafa);
	return NULL;
}

void pasarATerminated(t_list* listaOriginal, int id){
	bool compararPorId(void* unDtb){
		return  id == ((Dtb*) unDtb)->id && ((Dtb*) unDtb)->inicializado == 1;
	}
	Dtb* dtb = (Dtb*) list_find(listaOriginal, compararPorId);
	list_remove_by_condition(listaOriginal, compararPorId);
	list_add(ListaTerminated, dtb);
	dtb->tiempoFinal = time(0);
	sumatoriaTiemposFinales += obtenerTiempoRespuesta(dtb);
	sumatoriaSentenciaFinal += dtb->pc; // Agrego en que sentencia se fue a exit
	cantSentenciasFinales ++;
	liberarRecursos(dtb);
	log_info(loggerSafa, "Se abortó el DTB %i", id);
}

void pasarATerminatedSiFinalizo(t_list* listaOriginal, int id){
	bool compararPorIdYTerminated(void* unDtb){
		Dtb* dtb = (Dtb*) unDtb;
		return  (id == dtb->id) && (dtb->inicializado == 2);
	}
	Dtb* dtb = (Dtb*) list_find(listaOriginal, compararPorIdYTerminated);
	if(dtb){
		list_remove_by_id(listaOriginal, id);
		list_add(ListaTerminated, dtb);
		liberarRecursos(dtb);
	}
}

void liberarRecursos(Dtb* dtb){
	/*
	 * Genero funcion que, para cada recurso del sistema, chequea si el dtb recibido por parametro
	 * esta en la lista de bloqueados por ese recurso, y lo elimina de esa lista.
	 * */
	void liberarPorId(void* unRecurso){
		Recurso* recurso = (Recurso*) unRecurso;
		bool compararPorId(void* unDtb){
			return (dtb->id == ((Dtb*) unDtb)->id);
		}
		int countPrevio = recurso->procesosBloqueados->elements->elements_count;
		list_remove_by_condition(recurso->procesosBloqueados->elements, compararPorId);
		int countActual = recurso->procesosBloqueados->elements->elements_count;
		recurso->contador += (countPrevio - countActual); // aumento el contador solo en la cant que se libero.
	}
	// Aplico la funcion para cada elemento de la tabla de recursos
	list_iterate(TablaRecursos, liberarPorId);
}

void* metricas(void* idVoid){
	if (idVoid == NULL){
		printf("METRICAS GENERALES ----------------\n");
		printf("1) Cantidad de sentencias ejecutadas: %i\n", cantSentenciasEjecutadas);
		printf("2) Cantidad de sentencias ejecutadas al Diego: %i\n", cantSentenciasAlDiego);
		printf("3) Promedio de sentencias ejecutadas al Diego: %i%s\n", obtenerPromedioSentenciasAlDiego(), "%");
		printf("4) Promedio de sentencias ejecutadas hasta llegar a Exit: %i Sentencias\n", obtenerPromedioSentenciasExit());
		printf("5) Tiempo de respuesta promedio: %i segundos\n", obtenerTiempoRespuestaPromedio());
	} else {
		char* idStr = (char*) idVoid;
		int id = atoi(idStr);
		Dtb* dtb = buscarDtbPorId(id);
		if(dtb != NULL && dtb->inicializado != 0){
			printf("METRICAS DTB %i ----------------\n", id);
			printf("1) Cantidad de sentencias esperadas en New: %i\n", dtb->sentenciasEnNew);
			printf("2) Cantidad de sentencias que fueron al Diego: %i\n", dtb->sentenciasAlDiego);
			if(estaEnLista(ListaTerminated, id)){
				printf("3) Cantidad de sentencias hasta llegar a Exit: %i\n", dtb->pc);
				printf("4) Tiempo de respuesta del proceso: %i segundos\n", obtenerTiempoRespuesta(dtb));
			}
		} else {
			printf("El DTB ingresado no existe, o es el Dummy");
		}
		free(idStr);
	}
	return NULL;
}

int obtenerTiempoRespuesta(Dtb* dtb){
	return difftime(dtb->tiempoFinal, dtb->tiempoInicial);
}

int obtenerTiempoRespuestaPromedio(Dtb* dtb){
	if(ListaTerminated->elements_count > 0){
		return ((float)sumatoriaTiemposFinales / (float)ListaTerminated->elements_count);
	} else {
		return 0;
	}
}

int obtenerPromedioSentenciasAlDiego(){
	if(cantSentenciasFinales > 0 && cantSentenciasEjecutadas > 0){
		return (((float) cantSentenciasAlDiego)/((float) cantSentenciasEjecutadas))*100;
	} else {
		return 0;
	}
}

int obtenerPromedioSentenciasExit(){
	if(cantSentenciasFinales > 0){
	return (sumatoriaSentenciaFinal/cantSentenciasFinales);
	} else {
		return 0;
	}
}

int cantidadProcesosEnMemoria(){
	bool noEsElDummy(void* dtbVoid){
		return !esElDummy(dtbVoid);
	}
	t_list* listaReadyAux = list_filter(ListaReady, noEsElDummy);
	t_list* listaRunningAux = list_filter(ListaRunning, noEsElDummy);
	t_list* listaBlockedAux = list_filter(ListaBlocked, noEsElDummy);
	int count = listaReadyAux->elements_count + listaRunningAux->elements_count + listaBlockedAux->elements_count;
	list_destroy(listaReadyAux);
	list_destroy(listaRunningAux);
	list_destroy(listaBlockedAux);
	return count;
}

bool esElDummy(void* dtbVoid){
	Dtb* dtb = (Dtb*) dtbVoid;
	return dtb->inicializado == 0;
}

bool estaEnLista(t_list* lista, int id){
	bool compararPorId(void* unDtb)
	{
		return  id == ((Dtb*) unDtb)->id && ((Dtb*) unDtb)->inicializado == 1;
	}
	return list_any_satisfy(lista, compararPorId);
}

Dtb* buscarDtbPorId(int id){
	bool compararPorId(void* unDtb){
		return  (id == ((Dtb*) unDtb)->id) && ((((Dtb*) unDtb)->inicializado) == 1);
	}
	return (Dtb*) list_find(ListaGeneral, compararPorId);
}

void imprimirArchivosAbiertos(void* archivoVoid){
	Archivo* archivo = (Archivo*) archivoVoid;
	printf("Nombre: %s, Ubicacion: %i\n", archivo->escriptorio, archivo->ubicacion);
}

void imprimirDtb(void* dtbVoid){
	Dtb* dtb = (Dtb*) dtbVoid;
	if(dtb->inicializado == 0){
		printf("   Id: %i, Script: %s, Ubicacion: %i inicializado: %i (DUMMY)\n", dtb->id, dtb->escriptorio, dtb->ubicacionEscriptorio, dtb->inicializado);
	} else {
		printf("   Id: %i, Script: %s, Ubicacion: %i, Inicializado: %i\n", dtb->id, dtb->escriptorio, dtb->ubicacionEscriptorio,  dtb->inicializado);
	}
}

t_list* obtenerCpuLibres(){
	if(CpuConectadas->elements_count > 0) {
		return list_filter(CpuConectadas, esCpuLibre); // TODO: Ver si se debe sincronizar acceso a listas;
	} else {
	   return list_duplicate(CpuConectadas);
	}
}

bool esCpuLibre(void* cpuVoid){
	Cpu* cpu = (Cpu*) cpuVoid;
	return cpu->libre;
}

Dtb* obtenerProximoDtb(){
	if(strcmp(algoritmo, "RR") == 0){
		return obtenerDtbRR();
	} else if(strcmp(algoritmo, "VRR") == 0){
		if(ListaReadyAux->elements_count > 0){
			return list_remove(ListaReadyAux, 0);
		} else {
			return obtenerDtbRR();
		}
	} else if(strcmp(algoritmo, "PROPIO") == 0){
		// Ordeno la lista segun algoritmo
		list_sort(ListaReady, closestIOComparator);
		sem_wait(&usoListaReady);
		Dtb* dtb = list_remove(ListaReady, 0);
		sem_post(&usoListaReady);
		return dtb;
	} else {
		log_warning(loggerSafa, "El algoritmo ingresado no es valido");
	}
	return NULL;
}

Dtb* obtenerDtbRR(){
	sem_wait(&usoListaReady);
	Dtb* dtb = list_remove(ListaReady, 0);
	sem_post(&usoListaReady);
	return dtb; // Saco el primer proceso de la cola de Ready
}

void eliminarCpu(int socketAEliminar){
	bool compararPorSocket(void* cpuVoid){
			return  ((Cpu*) cpuVoid)->socket == socketAEliminar;
	}
	list_remove_and_destroy_by_condition(CpuConectadas, compararPorSocket, free);
	log_info(loggerSafa, "Se desconecto una Cpu. Quedan %i conectadas\n", CpuConectadas->elements_count);
}

void manejarOperacionCpu(Cabecera* cabecera, int socketCpu){
	if(cabecera->idOperacion != EJECUTAR_WAIT || cabecera->idOperacion != EJECUTAR_SIGNAL || cabecera->idOperacion != BLOQUEAR_DUMMY){
		// Aprovecho para meter en largoPaquete la cantidad de sentencias ejecutadas
		aumentarMetricas(cabecera->largoPaquete);
	}

	switch(cabecera->idOperacion){
		case BLOQUEAR_DUMMY :
			log_info(loggerSafa, "Se bloquea el DTB Dummy");
			list_remove_by_condition(ListaRunning, esElDummy);
			sem_wait(&usoListaBlocked);
			list_add(ListaBlocked, dtbDummy);
			sem_post(&usoListaBlocked);
			sem_post(&dummyLibre);
			break;
		case PASAR_A_BLOCKED:
			cantSentenciasAlDiego++; // Siempre que va al diego se bloquea
			manejarBloqueoDtb(socketCpu);
			break;
		case FIN_QUANTUM:
			manejarFinQuantum(socketCpu);
			break;
		case EJECUTAR_WAIT:
			ejecutarWait(socketCpu);
			break;
		case EJECUTAR_SIGNAL:
			ejecutarSignal(socketCpu);
			break;
		case EJECUTAR_CLOSE:
			ejecutarClose(socketCpu);
			break;
		case PASAR_A_EXIT:
			manejarFinEjecucionDtb(socketCpu);
			break;
		case ARCHIVO_NO_ABIERTO: {
			int codigoError;
			recibirMensajeInt(socketCpu, &codigoError);
			procesarErrorCode(codigoError);
			manejarFinEjecucionDtb(socketCpu);
			break;
		}
		case ERROR_CARGA:
			manejarFinEjecucionDtb(socketCpu); // Si hubo un error lo paso a exit
			break;
		case CPU_SAFA: {
			log_trace(loggerSafa, "Realizando handshake con CPU en socket: %d\n", socketCpu);
		    enviarCabecera(socketCpu, "", CPU_SAFA, loggerSafa);
		    break;
		}

	}
	if(cabecera->idOperacion != EJECUTAR_WAIT && cabecera->idOperacion != EJECUTAR_SIGNAL && cabecera->idOperacion != EJECUTAR_CLOSE){
		marcarCpuLibre(socketCpu);
	}
}

void ejecutarWait(int socketCpu){
	int idDtb, largoRecurso;
	char* recurso;
	recibirMensajeInt(socketCpu, &idDtb);
	recibirMensajeInt(socketCpu, &largoRecurso);
	recurso = malloc(largoRecurso + 1);
	recibirMensaje(socketCpu, &recurso, largoRecurso);

	bool compararRecursoPorNombre(void* recursoVoid){
				Recurso* unRecurso = (Recurso*) recursoVoid;
				return  (strcmp(unRecurso->nombre, recurso) == 0);
	}

	Dtb* dtb = buscarDtbPorId(idDtb);
	if(list_any_satisfy(TablaRecursos, compararRecursoPorNombre)){
		Recurso* recursoEncontrado = list_find(TablaRecursos, compararRecursoPorNombre);
		decrementarRecurso(recursoEncontrado, socketCpu, dtb);
	} else {
		Recurso* recursoNuevo = malloc(sizeof(Recurso));
		recursoNuevo->contador = 1;
		recursoNuevo->procesosBloqueados = queue_create();
		recursoNuevo->nombre = malloc(largoRecurso + 1);
		strcpy(recursoNuevo->nombre, recurso);
		list_add(TablaRecursos, recursoNuevo);
		log_trace(loggerSafa, "El DTB %i creo el recurso  %s", idDtb, recurso);
		decrementarRecurso(recursoNuevo, socketCpu, dtb);
	}
	free(recurso);
}

void decrementarRecurso(Recurso* recurso, t_socket socketCpu, Dtb* dtb){
	if(recurso->contador <= 0){
		// Lo meto en la cola y aviso a Cpu que no pude hacer wait
		queue_push(recurso->procesosBloqueados, dtb);
		enviarMensajeInt(socketCpu, -1);
		recurso->contador--;
	} else {
		recurso->contador--;
		enviarMensajeInt(socketCpu, 0);
	}
	log_trace(loggerSafa, "El DTB %i decremento el contador del recurso %s, quedando en %i", dtb->id, recurso->nombre, recurso->contador);
}

void ejecutarSignal(int socketCpu){
	int idDtb, largoRecurso;
	char* recurso;
	recibirMensajeInt(socketCpu, &idDtb);
	recibirMensajeInt(socketCpu, &largoRecurso);
	recurso = malloc(largoRecurso + 1);
	recibirMensaje(socketCpu, &recurso, largoRecurso);

	bool compararRecursoPorNombre(void* recursoVoid){
				Recurso* unRecurso = (Recurso*) recursoVoid;
				return  (strcmp(unRecurso->nombre, recurso) == 0);
	}

	if(list_any_satisfy(TablaRecursos, compararRecursoPorNombre)){
		Recurso* recursoEncontrado = list_find(TablaRecursos, compararRecursoPorNombre);
		incrementarRecurso(recursoEncontrado, socketCpu);
	} else {
		Recurso* recursoNuevo = malloc(sizeof(Recurso));
		recursoNuevo->contador = 1;
		recursoNuevo->procesosBloqueados = queue_create();
		recursoNuevo->nombre = malloc(largoRecurso + 1);
		strcpy(recursoNuevo->nombre, recurso);
		list_add(TablaRecursos, (void*) recursoNuevo);
		enviarMensajeInt(socketCpu, 0);
	}
	free(recurso);
}

void incrementarRecurso(Recurso* recurso, t_socket socketCpu){
	recurso->contador++;
	if(recurso->procesosBloqueados->elements->elements_count > 0){
		// Saco un proceso bloqueado por ese recurso y lo mando a Ready
		Dtb* unDtb = (Dtb*) queue_pop(recurso->procesosBloqueados);
		sem_wait(&usoListaBlocked);
		list_remove_by_id(ListaBlocked, unDtb->id);
		sem_post(&usoListaBlocked);
		sem_wait(&usoListaReady);
		list_add(ListaReady, unDtb);
		sem_post(&usoListaReady);
		log_trace(loggerSafa, "Se paso a Ready al proceso %i luego de signal", unDtb->id);
	}
	enviarMensajeInt(socketCpu, 0);
	log_trace(loggerSafa, "Se aumento el contador del recurso %s, quedando en %i", recurso->nombre, recurso->contador);

}

void ejecutarClose(t_socket socketCpu){
	int idDtb, largoNombreArchivo;
	char* nombreArchivo;

	// Recibo datos del archivo a cerrar
	recibirMensajeInt(socketCpu, &idDtb);

	recibirMensajeInt(socketCpu, &largoNombreArchivo);

	nombreArchivo = malloc(largoNombreArchivo + 1);

	recibirMensaje(socketCpu, &nombreArchivo, largoNombreArchivo);

	Dtb* dtb = buscarDtbPorId(idDtb);

	// Defino funcion helper para comparar nombre de archivo
	bool compararArchivoPorNombre(void* archivoVoid){
					Archivo* unArchivo = (Archivo*) archivoVoid;
					return  (strcmp(unArchivo->escriptorio, nombreArchivo) == 0);
	}
	// Lo remuevo de la lista y le hago free
	list_remove_and_destroy_by_condition(dtb->archivos_abiertos, compararArchivoPorNombre, freeArchivo);
	free(nombreArchivo);

	// Envio archivos abiertos actualizado
	char* archivosAbiertos = serializarArchivosAbiertos(dtb);
	int largoArchivos = strlen(archivosAbiertos);
	enviarMensajeInt(socketCpu, largoArchivos);
	enviarMensaje(socketCpu, archivosAbiertos, largoArchivos);
	free(archivosAbiertos);
}

void manejarFinEjecucionDtb(t_socket socketCpu){
	int idDtb;
	recibirMensajeInt(socketCpu, &idDtb);
	Dtb* dtb = buscarDtbPorId(idDtb);
	recibirMensajeInt(socketCpu, &dtb->pc);
	list_remove_by_id(ListaRunning, idDtb);
	list_add(ListaTerminated, dtb);
	sumatoriaSentenciaFinal += dtb->pc; // Agrego en que sentencia se fue a exit
	cantSentenciasFinales ++;
	dtb->tiempoFinal = time(0);
	sumatoriaTiemposFinales += obtenerTiempoRespuesta(dtb);
	liberarRecursos(dtb);
	log_info(loggerSafa, "El DTB %i finalizo su ejecucion", idDtb);
}

void manejarBloqueoDtb(t_socket socketCpu){
	// Obtengo el id del Dtb
	int idDtb;
	recibirMensajeInt(socketCpu, &idDtb);
	// Busco el dtb por id
	Dtb* dtb = buscarDtbPorId(idDtb);
	// Actualizo el PC
	recibirMensajeInt(socketCpu, &dtb->pc);
	// Recibo cuanto quantum le sobro
	int quantumRestante;
	recibirMensajeInt(socketCpu, &quantumRestante);
	if(strcmp(algoritmo, "VRR") == 0){
		dtb->quantumDisponible = quantumRestante;
		log_info(loggerSafa, "Se desalojo el DTB %i con quantum restante de %i", dtb->id, dtb->quantumDisponible);
	} else {
		// No hay prioridad por utilizar menos quantum, manejo normal.
		dtb->quantumDisponible = quantum;
	}

	// Recibo el motivo del bloqueo, para loggear
	int razonBloqueo;
	recibirMensajeInt(socketCpu, &razonBloqueo);
	procesarBlockCode(idDtb, razonBloqueo);
	// Lo saco de running
	list_remove_by_id(ListaRunning, idDtb);
	// Lo paso a Blocked
	sem_wait(&usoListaBlocked);
	list_add(ListaBlocked, dtb);
	sem_post(&usoListaBlocked);
	dtb->sentenciasAlDiego++;
}

void manejarFinQuantum(t_socket socketCpu){
	// Obtengo el id del Dtb
	int idDtb;
	recibirMensajeInt(socketCpu, &idDtb);
	// Busco el dtb por id
	Dtb* dtb = buscarDtbPorId(idDtb);
    // Actualizo el PC
	recibirMensajeInt(socketCpu, &dtb->pc);
	// Lo saco de running
	list_remove_by_id(ListaRunning, idDtb);
	//Seteo quantum restante al quantum establecido (Para mantener consistencia si hay cambio de algoritmo)
	dtb->quantumDisponible = quantum;
	// Lo paso a ready
	sem_wait(&usoListaReady);
	list_add(ListaReady, dtb);
	sem_post(&usoListaReady);
}

void enviarDtbAEjecutar(Cpu* cpu, Dtb* dtb){
	// Envio Header
	enviarCabecera(cpu->socket, "EJECUTAR", EJECUTAR, loggerSafa);

	// Envio Id
	enviarMensajeInt(cpu->socket, dtb->id);

	// Envio nombre script
	int largoNombreEscriptorio = strlen(dtb->escriptorio);
	enviarMensajeInt(cpu->socket, largoNombreEscriptorio);
	enviarMensaje(cpu->socket, dtb->escriptorio, largoNombreEscriptorio);

	// Envio Program counter
	enviarMensajeInt(cpu->socket, dtb->pc);

	// Envio flag inicializado
	enviarMensajeInt(cpu->socket, dtb->inicializado);

	//Envio Quantum, si es VRR envio el restante, sino envio el establecido por config
	int quantumEnviado;
	if(strcmp(algoritmo, "VRR") == 0){
		enviarMensajeInt(cpu->socket, dtb->quantumDisponible);
		quantumEnviado = dtb->quantumDisponible;
	} else {
		enviarMensajeInt(cpu->socket, quantum);
		quantumEnviado = quantum;
	}

	// Envio archivos abiertos por el dtb
	char* archivosAbiertos = serializarArchivosAbiertos(dtb);
	int largoArchivos = strlen(archivosAbiertos);
	enviarMensajeInt(cpu->socket, largoArchivos);
	enviarMensaje(cpu->socket, archivosAbiertos, largoArchivos);

	// Envio ubicacion script
	enviarMensajeInt(cpu->socket, dtb->ubicacionEscriptorio);

	log_info(loggerSafa, "Se envio a ejecutar el DTB %i a la cpu en socket %i con quantum de %i\n", dtb->id, cpu->socket, quantumEnviado);
	// Lo agrego a running
	list_add(ListaRunning, dtb);

	// Actualizo la info de la cpu
	cpu->idDtbActual = dtb->id;
	cpu->libre = false;
	free(archivosAbiertos);

}

char* serializarArchivosAbiertos(Dtb* dtb){
	int cantidadArchivos = dtb->archivos_abiertos->elements_count;

	if(cantidadArchivos <= 0){
		// Se asigna una cadena fija, porque hay confictos al hacer un recv de tamanio 0 en cpu
		char* sinArchivos = malloc(5);
		strcpy(sinArchivos,"VOID");
		return sinArchivos;
	}

	char* resultado = string_new();
	int i;
	for(i = 0; i < cantidadArchivos; i++){
		Archivo* archivo = (Archivo*) list_get(dtb->archivos_abiertos, i);
		char* ubicacionChar = string_itoa(archivo->ubicacion);
		string_append_with_format(&resultado, "@%s$%s", archivo->escriptorio, ubicacionChar);
		free(ubicacionChar);
	}

	return resultado;
}

void list_remove_by_id(t_list* list, int id){
	bool compararPorId(void* unDtb){
			return  id == ((Dtb*) unDtb)->id && 1 == ((Dtb*) unDtb)->inicializado;
		}
	list_remove_by_condition(list, compararPorId);
}

void marcarCpuLibre(int socketCpu){
	bool compararPorSocket(void* cpuVoid){
		return socketCpu == ((Cpu*) cpuVoid)->socket;
	}
	Cpu* cpu = (Cpu*) list_find(CpuConectadas, compararPorSocket);
	bool compararPorIdYTerminated(void* unDtb){
			Dtb* dtb = (Dtb*) unDtb;
			return  (cpu->idDtbActual == dtb->id) && (dtb->inicializado == 2);
	}
	Dtb* dtb = (Dtb*) list_find(ListaRunning, compararPorIdYTerminated);
	if(dtb){
		enviarCabecera(socketCpu, "", LIBERAR, loggerSafa);
		enviarMensajeInt(socketCpu, dtb->id);
		enviarMensajeInt(socketCpu, dtb->ubicacionEscriptorio);
		int resultado;
		recibirMensajeInt(socketCpu, &resultado);
		if(resultado == 0){
			log_info(loggerSafa, "Se libero la memoria del DTB %i, que habia sido finalizado mientras ejecutaba", dtb->id);
		}
	}
	cpu->libre = true;
	// Si mientras se estaba ejecutando se finalizo el proceso desde consola (inicializado = 2)
	// se lo pasa a terminated y se liberan sus recursos
}

bool closestIOComparator(void* unDtbVoid, void* otroDtbVoid){
	Dtb* unDtb = (Dtb*) unDtbVoid;
	Dtb* otroDtb = (Dtb*) otroDtbVoid;
	int cant1, cant2;
	cant1 = instruccionesHastaIO(unDtb);
	cant2 = instruccionesHastaIO(otroDtb);
	return cant1 < cant2;
}

void asignarListaDeIOs(Dtb* dtb, char* respuesta){
	// Separo y cuento cantidad de entradas
	char** arrayEntradas = string_split(respuesta, ",");
	int count;
	for(count = 0; arrayEntradas[count] != NULL; count ++);

	// Guardo memoria para array de ints
	dtb->infoES = malloc(count * sizeof(int));

	// Asigno los datos
	int i;
	for(i = 0; i < count; i++){
		dtb->infoES[i] = atoi(arrayEntradas[i]);
	}
	free(respuesta);
	for(i = 0; i < count; i++){
			free(arrayEntradas[i]);
	}
	dtb->cantidadES = count;
	free(arrayEntradas);
}

int instruccionesHastaIO(Dtb* unDtb){
	if(unDtb->inicializado == 0){
		return 1;
	}
	int i;
	int proximaES = -1;
	for(i=0;i<unDtb->cantidadES; i++){
		if(unDtb->infoES[i] >= unDtb->pc){
			proximaES = unDtb->infoES[i];
			break;
		}
	}
	if(proximaES != -1){
		return (proximaES - unDtb->pc);
	}
	return 1000000; //Si no me quedan E/S devuelvo valor muy grande, TODO: filtrar antes dtbs con ios restantes;
}

void aumentarMetricas(int cantSentencias){
	void aumentarCantidadEsperada(void* unDtb){
		Dtb* dtb = (Dtb*) unDtb;
		dtb->sentenciasEnNew += cantSentencias;
	}
	list_iterate(ColaNew->elements, aumentarCantidadEsperada);
	cantSentenciasEjecutadas += cantSentencias;
}
