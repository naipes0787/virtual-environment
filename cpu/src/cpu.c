#include "cpu.h"

int main(void) {
	puts("Inicializando CPU.");

	inicializarCPU();

	pthread_t hiloInotify;
	int resultInotify = pthread_create(&hiloInotify, NULL,
			funcionalidadInotify, NULL);

	if(resultInotify != 0){
		log_error(loggerCpu,"\nNo se pudo crear el hilo Inotify:[%s]\n", strerror(resultInotify));
	} else {
		 log_trace(loggerCpu,"Hilo Inotify creado");
	}

	/*
	*Descomentar si se quiere probar alguna funcionalidad desde CPU:
	* pruebaMensajeFM9();
	* pruebaMensajeMDJ();
	* pruebaCrearArchivo();
	* pruebaBorrarArchivo();
	*/

	while(1){
		globalesEnCero();

		recibirDTB();

		if (ejecutar == 1) {
			if (DTB->inicializado == 0){
				ejecutarDummy();
			}else{
				traerEscriptorio();
				log_info(loggerCpu,"Llegó OK el escriptorio");
				while (puedeEjecutar())
				{
					ejecutarProximaInstruccion();
					ejecutado++;
					log_info(loggerCpu,"Ejecuté una instruccion");
				}
				notificarSafa();
				free(DTB->archivosAbiertos);
				free(DTB->escriptorio);
				free(DTB);
				DTB = NULL;
				int i = 0;
				while(lineas[i] != NULL){
					free(lineas[i]);
					i++;
				}
				free(lineas);
			}
		}
	}
	pthread_join(hiloInotify, NULL);
	liberarInotify();
	exit_gracefully(1, loggerCpu);
}

void liberarInotify(){
	inotify_rm_watch(inotify, configI);
	close(inotify);
	config_destroy(config);
}

void inicializarConfig(){
	// Se busca el archivo .cfg tanto en root como en el directorio padre
	config = config_create("cpu.cfg");
	if(config == NULL){
		pathConfig = "../cpu.cfg";
		config = config_create(pathConfig);
	} else {
		pathConfig = "cpu.cfg";
	}
	ipSafa = config_get_string_value(config, "IP_SAFA");
	puertoSafa =config_get_string_value(config, "PUERTO_SAFA");
	ipDam = config_get_string_value(config, "IP_DAM");
	puertoDam = config_get_string_value(config, "PUERTO_DAM");
	ipFm9 = config_get_string_value(config, "IP_FM9");
	puertoFm9 =config_get_string_value(config, "PUERTO_FM9");

	retardoDeEjecucion= config_get_int_value(config, "RETARDO");

	log_info(loggerCpu,"CPU configurado. Retardo: %i \n", retardoDeEjecucion);
}

void inicializarCPU(){
	inicializarLogger();
	inicializarConfig();
	conexionesConSAFA();
	conexionesConDAM();
	conexionesConFM9();
	inicializarInotify();
}

void conexionesConSAFA(){
	log_trace(loggerCpu, "Creando socket cliente para SAFA");
	socketSAFA = clienteConectarComponenteConCabecera(CPU_PROCESS_NAME,
			SAFA_PROCESS_NAME, puertoSafa, ipSafa, CPU_SAFA, loggerCpu);
	log_info(loggerCpu, "Conectado con SAFA!");
}

void conexionesConDAM(){
	log_trace(loggerCpu, "Creando socket cliente para DAM");

	socketDAM = clienteConectarComponenteConCabecera(CPU_PROCESS_NAME,
			DAM_PROCESS_NAME, puertoDam, ipDam, CPU_DAM, loggerCpu);
	log_info(loggerCpu, "Conectado con DAM!");
}

void pruebaMensajeFM9(){
	char* msjFM9 = "Hola FM9 desde CPU";
	log_trace(loggerCpu, "Mandando msj a FM9!");
	enviarCabecera(socketDAM, msjFM9, PRUEBA_MENSAJE_FM9, loggerCpu);
	enviarMensaje(socketDAM, msjFM9, string_length(msjFM9)+1);
}

void pruebaMensajeMDJ(){
	char* msjMDJ = "Hola MDJ desde CPU";
	log_trace(loggerCpu, "Mandando msj a MDJ!");
	enviarCabecera(socketDAM, msjMDJ, PRUEBA_MENSAJE_MDJ, loggerCpu);
	enviarMensaje(socketDAM, msjMDJ, string_length(msjMDJ)+1);
}

void pruebaCrearArchivo(){
	log_trace(loggerCpu, "Probando crear archivo!");
	char* path = "/equipos/equipo1.txt";
	int cantidadLineas = 5;
	int idGdtTest = 1;
	enviarCabecera(socketDAM, "", EJECUTAR_CREAR, loggerCpu);
	enviarMensajeInt(socketDAM, idGdtTest);
	enviarMensajeInt(socketDAM, strlen(path));
	enviarMensaje(socketDAM, path, strlen(path));
	enviarMensajeInt(socketDAM, cantidadLineas);
}

void pruebaBorrarArchivo(){
	log_trace(loggerCpu, "Probando borrar archivo!");
	char* path = "/equipos/pruebaborrar.txt";
	int idGdtTest = 1;
	enviarCabecera(socketDAM, "", EJECUTAR_BORRAR, loggerCpu);
	enviarMensajeInt(socketDAM, idGdtTest);
	enviarMensajeInt(socketDAM, strlen(path));
	enviarMensaje(socketDAM, path, strlen(path));
}

void conexionesConFM9(){
	log_trace(loggerCpu,"Creando socket cliente para FM9");

	socketFM9 = clienteConectarComponenteConCabecera(CPU_PROCESS_NAME,
				FM9_PROCESS_NAME, puertoFm9, ipFm9, CPU_FM9, loggerCpu);
	log_info(loggerCpu, "Conectado con FM9!");
}

void inicializarLogger(){
	loggerCpu = log_create("CPU.log", "CPU", true, LOG_LEVEL_TRACE);
	log_trace(loggerCpu, "Logger cpu inicializado");
}

void inicializarInotify(){
	inotify = inotify_init();
	if(inotify < 0){
		perror("inotify_init");
		log_error(loggerCpu, "No se pudo inicializar Inotify");
	}
}

void recibirDTB()
{
	int respuestaRecv;
	log_info(loggerCpu, "Recibiendo un nuevo DTB");
	Cabecera* header = recibirCabecera(socketSAFA, &respuestaRecv, loggerCpu);
	if (respuestaRecv < 0){
		log_error(loggerCpu,"Error al recibir cabecera.");
	} else if(respuestaRecv == 0){
		log_error(loggerCpu, "SAFA se desconectó");
		liberarInotify();
		exit_gracefully(1, loggerCpu);
	} else {
		if (header->idOperacion == EJECUTAR){

			Dtb* dtbActual = malloc(sizeof(Dtb));
			int tamanioRecibir, tamanioArchivos;

			recibirMensajeInt(socketSAFA, &dtbActual->id);
			log_info(loggerCpu,"DTB id %i",dtbActual->id);

			recibirMensajeInt(socketSAFA, &tamanioRecibir);

			dtbActual->escriptorio = malloc(tamanioRecibir + 1);

			recibirMensaje(socketSAFA, &dtbActual->escriptorio, tamanioRecibir);
			log_info(loggerCpu,"Escriptorio: %s",dtbActual->escriptorio);

			recibirMensajeInt(socketSAFA, &dtbActual->pc);
			log_info(loggerCpu,"Program counter: %i",dtbActual->pc);

			recibirMensajeInt(socketSAFA, &dtbActual->inicializado);
			log_info(loggerCpu,"Estado de inicialización: %i",dtbActual->inicializado);

			recibirMensajeInt(socketSAFA, &quantum);
			log_info(loggerCpu,"Quantum: %i",quantum);

			recibirMensajeInt(socketSAFA, &tamanioArchivos);

			dtbActual->archivosAbiertos = malloc(tamanioArchivos + 1);

			recibirMensaje(socketSAFA, &dtbActual->archivosAbiertos, tamanioArchivos);

			recibirMensajeInt(socketSAFA, &dtbActual->ubicacion);
			log_info(loggerCpu,"Ubicación en memoria: %i", &dtbActual->ubicacion);

			log_info(loggerCpu, "Se recibio el DTB %i para ejecutar", dtbActual->id);
			DTB = dtbActual;
			dtbActual = NULL;
			ejecutar = 1;

		} else if(header->idOperacion == LIBERAR){
			int idDtb, ubicacion;
			recibirMensajeInt(socketSAFA, &idDtb);
			recibirMensajeInt(socketSAFA, &ubicacion);
			int resultado = cerrarEscriptorioEnFm9(idDtb, ubicacion);
			enviarMensajeInt(socketSAFA, resultado);
			ejecutar = 0;
		} else{
			log_trace(loggerCpu,"Protocolo incorrecto. ID operacion = %i", header->idOperacion);
			ejecutar = 0;
		}
	}
	free(header);
}

void ejecutarDummy()
{
	avisarDAM();
	avisarDummyFinalizado();
	log_info(loggerCpu,"Se mandó a ejecutar DUMMY a DAM y se le notifica a SAFA");
	log_trace(loggerCpu,"DUMMY para el GDT %d", DTB->id);
}

void avisarDAM()
{
	enviarCabecera(socketDAM, "", INICIAR_DUMMY, loggerCpu);

	enviarMensajeInt(socketDAM, DTB->id);

	enviarMensajeInt(socketDAM, strlen(DTB->escriptorio));

	enviarMensaje(socketDAM, DTB->escriptorio, strlen(DTB->escriptorio));


}

void avisarDummyFinalizado()
{
	//aviso que ya ejecute dummy
	enviarCabeceraCpuSafa(socketSAFA, ejecutado, BLOQUEAR_DUMMY, loggerCpu);
}


void ejecutarProximaInstruccion(){
	t_instruccion instruccion;
	if(lineas[DTB->pc] == NULL){
		instruccion.operacion = fin;
	} else {
		instruccion = parsear(lineas[DTB->pc]);
	}

	realizarInstruccion(instruccion);

	DTB->pc ++;
}

void realizarInstruccion (t_instruccion instruccion){
	usleep(retardoDeEjecucion * 1000);
	switch(instruccion.operacion){

	case ignorar:
		break;

	case fin:
		ejecucionFinalizada = 1;
		break;

	case abrir:

		if(pathAbierto(instruccion.argumentos.abrir.path)){
			log_info(loggerCpu,"El archivo ya se encuentra abierto");
		} else {
			enviarCabecera(socketDAM, "", PEDIR_ARCHIVO, loggerCpu);
			enviarMensajeInt(socketDAM, DTB->id);

			int largoPath = strlen(instruccion.argumentos.abrir.path);
			enviarMensajeInt(socketDAM, largoPath);

			enviarMensaje(socketDAM, instruccion.argumentos.abrir.path, largoPath);

			buscarPath = 1;
			log_trace(loggerCpu, "El DTB %i solicitó abrir el archivo %s",
					DTB->id, instruccion.argumentos.abrir.path);
			free(instruccion.argumentos.abrir.path);

		}
		break;

	case concentrar:

		log_trace(loggerCpu, "El DTB %i esta concentrando", DTB->id);
		break;

	case asignar:

			if(pathAbierto(instruccion.argumentos.asignar.path)){
				//log_info(loggerCpu,"asignando");

				enviarCabecera(socketFM9, "", EJECUTAR_ASIGNAR,
						loggerCpu);

				int ubicacionArchivo = obtenerUbicacion(instruccion.argumentos.asignar.path);
				enviarMensajeInt(socketFM9, ubicacionArchivo);
				enviarMensajeInt(socketFM9, DTB->id);
				enviarMensajeInt(socketFM9, instruccion.argumentos.asignar.linea);

				int tamanioDatos = strlen(instruccion.argumentos.asignar.datos);
				enviarMensajeInt(socketFM9, tamanioDatos);
				enviarMensaje(socketFM9, instruccion.argumentos.asignar.datos,
						tamanioDatos);

				log_trace(loggerCpu, "El DTB %i solicitó asignar sobre el archivo %s",
						DTB->id, instruccion.argumentos.asignar.path);

				int resultado;
				recibirMensajeInt(socketFM9, &resultado);
				if(resultado != OK){
					codigoError = resultado;
					archivoAbierto = ERROR;
					log_trace(loggerCpu, "ERROR %i: No se puede asignar en path %s", resultado,
							instruccion.argumentos.asignar.path);
				}
			}
			else{
				archivoAbierto = ERROR;
				codigoError = ASIGNAR_ARCHIVO_NO_ABIERTO;
				log_trace(loggerCpu, "ERROR %i: No se encuentra abierto el path %s",
						codigoError, instruccion.argumentos.asignar.path);
			}
			free(instruccion.argumentos.asignar.datos);
			free(instruccion.argumentos.asignar.path);
		break;

	case wait:
		retenerRecurso(instruccion.argumentos.wait.recurso);
		free(instruccion.argumentos.wait.recurso);
		break;

	case signal:
		liberarRecurso(instruccion.argumentos.signal.recurso);
		log_trace(loggerCpu,"Se liberó el recurso: %s",
				instruccion.argumentos.signal.recurso);
		free(instruccion.argumentos.signal.recurso);
		break;

	case flush:
		//log_info(loggerCpu,"ejecutando flush");
		if(pathAbierto(instruccion.argumentos.flush.path)){

			enviarCabecera(socketDAM, "", EJECUTAR_FLUSH, loggerCpu);

			enviarMensajeInt(socketDAM,DTB->id);

			int tamanioPath = strlen(instruccion.argumentos.flush.path);
			enviarMensajeInt(socketDAM, tamanioPath);

			enviarMensaje(socketDAM, instruccion.argumentos.flush.path, tamanioPath);

			int ubicacionPath = obtenerUbicacion(instruccion.argumentos.flush.path);
			enviarMensajeInt(socketDAM, ubicacionPath);

			ejecutandoFlush = 1;
			log_trace(loggerCpu, "El DTB %i solicitó flush", DTB->id);

		}
		else{
			archivoAbierto = ERROR;
			codigoError = ASIGNAR_ARCHIVO_NO_ABIERTO;
			log_trace(loggerCpu, "ERROR %i: No se encuentra abierto el path %s",
					codigoError, instruccion.argumentos.flush.path);
			archivoAbierto = ERROR;
		}
		free(instruccion.argumentos.flush.path);
		break;

	case cerrar:
		//log_info(loggerCpu,"ejecutando close");
		if(pathAbierto(instruccion.argumentos.cerrar.path)){

			int tamanioPath = strlen(instruccion.argumentos.cerrar.path);

			// Avisa a Fm9
			enviarCabecera(socketFM9, "", EJECUTAR_CLOSE, loggerCpu);
			int ubicacion = obtenerUbicacion(instruccion.argumentos.cerrar.path);
			enviarMensajeInt(socketFM9, ubicacion);
			enviarMensajeInt(socketFM9, DTB->id);
			int resultadoClose;
			recibirMensajeInt(socketFM9, &resultadoClose);
			if(resultadoClose != OK){
				codigoError = resultadoClose;
				archivoAbierto = ERROR;
				log_trace(loggerCpu, "ERROR %i: No se puede cerrar el path %s", resultadoClose,
						instruccion.argumentos.cerrar.path);
			} else {
				// Avisa a SAFA
				enviarCabeceraCpuSafa(socketSAFA, ejecutado, EJECUTAR_CLOSE, loggerCpu);
				enviarMensajeInt(socketSAFA, DTB->id);
				enviarMensajeInt(socketSAFA, tamanioPath);
				enviarMensaje(socketSAFA, instruccion.argumentos.cerrar.path,
						tamanioPath);
				log_trace(loggerCpu, "El DTB %i solicitó cerrar el archivo %s",
						DTB->id, instruccion.argumentos.cerrar.path);
				// Recibir nueva lista de archivos abiertos
				int tamanioArchivos;
				free(DTB->archivosAbiertos);
				char * archivosAuxiliar;
				recibirMensajeInt(socketSAFA, &tamanioArchivos);
				archivosAuxiliar = malloc(tamanioArchivos + 1);
				recibirMensaje(socketSAFA, &archivosAuxiliar, tamanioArchivos);
				DTB->archivosAbiertos = archivosAuxiliar;
				archivosAuxiliar = NULL;
			}

		} else{
			archivoAbierto = ERROR;
			codigoError = ASIGNAR_ARCHIVO_NO_ABIERTO;
			log_trace(loggerCpu, "ERROR %i: No se encuentra abierto el path %s",
					codigoError, instruccion.argumentos.cerrar.path);
		}
		free(instruccion.argumentos.cerrar.path);
		break;

	case crear: {
		//log_info(loggerCpu,"ejecutando crear");
			enviarCabecera(socketDAM, "", EJECUTAR_CREAR, loggerCpu);

			enviarMensajeInt(socketDAM, DTB->id);

			int tamanioPath = strlen(instruccion.argumentos.crear.path);
			enviarMensajeInt(socketDAM, tamanioPath);
			enviarMensaje(socketDAM, instruccion.argumentos.crear.path,
					tamanioPath);

			enviarMensajeInt(socketDAM, instruccion.argumentos.crear.lineas);

			log_trace(loggerCpu, "El DTB %i solicitó crear el archivo %s",
					DTB->id, instruccion.argumentos.crear.path);
			crearPath = 1;
			free(instruccion.argumentos.crear.path);
		break;
	}

	case borrar: {
		//log_info(loggerCpu,"ejecutando borrar");
		enviarCabecera(socketDAM, "", EJECUTAR_BORRAR, loggerCpu);

		enviarMensajeInt(socketDAM, DTB->id);

		int tamanioPath = strlen(instruccion.argumentos.borrar.path);
		enviarMensajeInt(socketDAM, tamanioPath);
		enviarMensaje(socketDAM, instruccion.argumentos.borrar.path,
				tamanioPath);

		log_trace(loggerCpu, "El DTB %i solicitó borrar el archivo %s",
				DTB->id, instruccion.argumentos.borrar.path);
		borrarArchivo = 1;
		free(instruccion.argumentos.borrar.path);
		break;
	}

	case nada:
		error = 1;
		break;
	}
}

bool pathAbierto(char* path){
	return string_contains(DTB->archivosAbiertos, path);
}

void notificarSafa(){

	if (ejecucionFinalizada == 1){

		enviarCabeceraCpuSafa(socketSAFA, ejecutado, PASAR_A_EXIT, loggerCpu);

		enviarMensajeInt(socketSAFA, DTB->id);
		enviarMensajeInt(socketSAFA, DTB->pc);

		// Libero la memoria utilizada por el escriptorios
		liberarTodosLosArchivos();

	}else if(buscarPath == 1){ //Aca entra cuando tiene que ir a abrir un archivo

		enviarInfoBloqueado(BLOCK_BUSCAR_ARCHIVO);

	} else if(respuestaWait == ERROR){
		// Si no pudo hacer el wait (Porque el semaforo ya estaba en 0) se bloquea.
		enviarInfoBloqueado(BLOCK_ESPERAR_RECURSO);

	}else if(ejecutandoFlush == 1){

		enviarInfoBloqueado(BLOCK_FLUSH);

	}else if(crearPath == 1){

		enviarInfoBloqueado(BLOCK_CREAR_ARCHIVO);

	} else if(borrarArchivo == 1){

		enviarInfoBloqueado(BLOCK_BORRAR_ARCHIVO);

	}else if(archivoAbierto == ERROR){

		enviarCabeceraCpuSafa(socketSAFA, ejecutado, ARCHIVO_NO_ABIERTO, loggerCpu);
		enviarMensajeInt(socketSAFA, codigoError);
		codigoError = ERROR;
		enviarMensajeInt(socketSAFA, DTB->id);
		enviarMensajeInt(socketSAFA, DTB->pc);

		// Libero la memoria utilizada por el escriptorios
		liberarTodosLosArchivos();

	}else if(quantum == ejecutado){

		enviarCabeceraCpuSafa(socketSAFA, ejecutado, FIN_QUANTUM, loggerCpu);

		enviarMensajeInt(socketSAFA,DTB->id);

		enviarMensajeInt(socketSAFA,DTB->pc);

	} else {

		enviarCabeceraCpuSafa(socketSAFA, ejecutado, ERROR_CARGA, loggerCpu);

		enviarMensajeInt(socketSAFA,DTB->id);
	}
}

void retenerRecurso(char *recurso){

	enviarCabeceraCpuSafa(socketSAFA, ejecutado, EJECUTAR_WAIT, loggerCpu);

	enviarMensajeInt(socketSAFA,DTB->id);

	int largoRecurso = strlen(recurso);
	enviarMensajeInt(socketSAFA,largoRecurso);
	enviarMensaje(socketSAFA, recurso, largoRecurso);

	// Se recibe un 0 si se pudo retener, o -1 si hubo error
	recibirMensajeInt(socketSAFA, &respuestaWait);
	if(respuestaWait == 0){
		log_trace(loggerCpu, "El DTB %i ejecuto Wait exitosamente sobre %s", DTB->id, recurso);
	} else {
		respuestaWait = ERROR;
		log_trace(loggerCpu, "Se ejecutó un Wait sobre %s, el cual no pudo asignarse. Bloqueando el DTB %i", recurso, DTB->id);
	}
}

void liberarRecurso(char* recurso){

	enviarCabeceraCpuSafa(socketSAFA, ejecutado, EJECUTAR_SIGNAL, loggerCpu);

	enviarMensajeInt(socketSAFA, DTB->id);
	int largoRecurso = strlen(recurso);
	enviarMensajeInt(socketSAFA,largoRecurso);
	enviarMensaje(socketSAFA, recurso, largoRecurso);

	int respuestaSignal;
	recibirMensajeInt(socketSAFA, &respuestaSignal);
	log_trace(loggerCpu, "El DTB %i ejecutó un Signal sobre el recurso %s", DTB->id, recurso);
}

void traerEscriptorio(){

	enviarCabecera(socketFM9, "", PEDIR_ESCRIPTORIO,
			loggerCpu);

	enviarMensajeInt(socketFM9, DTB->ubicacion);

	enviarMensajeInt(socketFM9, DTB->id);

	int tamanioARecibir;
	recibirMensajeInt(socketFM9, &tamanioARecibir);
	char* escriptorioEnMemoria = malloc(tamanioARecibir + 1);
	recibirMensaje(socketFM9, &escriptorioEnMemoria, tamanioARecibir);
	lineas = string_split(escriptorioEnMemoria, "\n");
	free(escriptorioEnMemoria);
}

void globalesEnCero(){

	ejecucionFinalizada = ejecutado = buscarPath = error = borrarArchivo = 0;
	respuestaWait = archivoAbierto = ejecutandoFlush = crearPath = 0;
	ejecutar = 0;
	if(DTB != NULL){
		free(DTB->archivosAbiertos);
		free(DTB->escriptorio);
		free(DTB);
		DTB = NULL;
	}
	log_info(loggerCpu, "Se reinician registros de ejecución\n\n"); // Doble /n para separar entre los distintos dtb en el log
}

bool puedeEjecutar(){

	return ((ejecutado < quantum) && (ejecucionFinalizada == 0) &&
			(buscarPath == 0) && (error == 0) && (respuestaWait != ERROR)
			&& (archivoAbierto == 0) && (ejecutandoFlush == 0) &&
			(crearPath == 0) && (borrarArchivo == 0));
}

void enviarInfoBloqueado(int razonBloqueo){
	enviarCabeceraCpuSafa(socketSAFA, ejecutado, PASAR_A_BLOCKED, loggerCpu);
	enviarMensajeInt(socketSAFA,DTB->id);
	enviarMensajeInt(socketSAFA,DTB->pc);
	enviarMensajeInt(socketSAFA, (quantum - ejecutado));
	enviarMensajeInt(socketSAFA, razonBloqueo);
	log_info(loggerCpu, "Enviando a bloquear el DTB %i", DTB->id);
}


void* funcionalidadInotify(void* arg){
	while(1){
		char buffer[INOTIFY_BUF_LEN];
		configI = inotify_add_watch(inotify, pathConfig, IN_MODIFY);
		int length = read(inotify, buffer, INOTIFY_BUF_LEN);
		if (length <= 0) {
			perror("read");
		}

		int offset = 0;

		while (offset < length) {
			struct inotify_event *event = (struct inotify_event *) &buffer[offset];
			if (event->mask & IN_MODIFY) {
				log_info(loggerCpu, "Se modificó el archivo de config. "
						"Recargando configuración");
				inicializarConfig();


			}
			offset += sizeof (struct inotify_event) + event->len;
		}
	}
	return NULL;
}

int obtenerUbicacion(char* path){
	char** archivos = string_split(DTB->archivosAbiertos, "@");
	bool finalizar = false;
	int i=0;
	int ubicacion;
	while(!finalizar){
		if(string_contains(archivos[i], path)){
			char** archivo = string_split(archivos[i], "$");
			ubicacion = atoi(archivo[1]);
			finalizar = true;
			int j = 0;
			while(archivo[j] != NULL){
				free(archivo[j]);
				j++;
			}
			free(archivo);
		} else {
			i++;
		}
	}
	i=0;
	while(archivos[i] != NULL){
		free(archivos[i]);
		i++;
	}
	free(archivos);
	return ubicacion;
}

void liberarTodosLosArchivos(){
	// Obtengo un array con cada archivo
	if(strcmp(DTB->archivosAbiertos, "VOID") != 0){
		char** archivos = string_split(DTB->archivosAbiertos, "@");
		int i=0;
		int ubicacion;
		// Itero los archivos
		while(archivos[i] != NULL){
				// Separo entre nombre y ubicacion de un archivo
				char** archivo = string_split(archivos[i], "$");
				ubicacion = atoi(archivo[1]);

				// Envio a FM9 el archivo a liberar
				cerrarEscriptorioEnFm9(DTB->id, ubicacion);
				int j= 0;

				// Libero la memoria alocada
				while(archivo[j] != NULL){
					free(archivo[j]);
					j++;
				}
				free(archivo);
				i++;
		}

		// Libero el array
		i=0;
		while(archivos[i] != NULL){
			free(archivos[i]);
			i++;
		}
		free(archivos);
	}
	// Finalmente, envio a cerrar el escriptorio base
	cerrarEscriptorioEnFm9(DTB->id, DTB->ubicacion);
}

int cerrarEscriptorioEnFm9(int id, int ubicacion){
	enviarCabecera(socketFM9, "", EJECUTAR_CLOSE, loggerCpu);
	enviarMensajeInt(socketFM9, ubicacion);
	enviarMensajeInt(socketFM9, id);
	int resultadoClose = ERROR;
	recibirMensajeInt(socketFM9, &resultadoClose);
	if(resultadoClose == CLOSE_FALLO_SEGMENTO_MEMORIA){
		log_trace(loggerCpu, "ERROR %i: No se puede cerrar el Path: %s", DTB->id, DTB->escriptorio);
	} else if(resultadoClose == 0) {
		log_trace(loggerCpu, "Se liberó correctamente la memoria utilizada por el escriptorio del DTB %i", DTB->id);
	}
	return resultadoClose;
}

