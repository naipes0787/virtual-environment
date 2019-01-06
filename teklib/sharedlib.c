#include "sharedlib.h"


t_log* configure_logger(char* process_name, t_log_level log_level) {
	// Reservo para el fin de línea y el ".log"
	char log_name[(strlen(process_name)+5)];
	strcpy(log_name, process_name);
	strcat(log_name, ".log");
	t_log* logger = log_create(log_name, process_name, true, log_level);
	// Reservo para "Logger [process_name] inicializado.\0"
	char mensaje[(strlen(process_name)+22)];
	strcpy(mensaje, "Logger ");
	strcat(mensaje, process_name);
	strcat(mensaje, " inicializado.");
	log_trace(logger, mensaje);
	return logger;
}



t_socket crearServerSocket(char* ip, char* port, t_log* logger){
	struct addrinfo hints, *res;
	t_socket socketServer;
	if(ip == NULL){
		ip = DEFAULT_IP;
	}
	if(port == NULL){
		port = DEFAULT_PORT;
	}
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	getaddrinfo(ip, port, &hints, &res);
	socketServer = socket(res -> ai_family, res -> ai_socktype, res -> ai_protocol);
	int active = 1;
	if (setsockopt(socketServer,SOL_SOCKET,SO_REUSEADDR,&active,sizeof(active)) != 0){
		_exit_with_error(socketServer, "Error en setsockopt", NULL, logger);
	}
	int result = bind(socketServer, res-> ai_addr, res->ai_addrlen);
	if(result == ERROR){
		_exit_with_error(socketServer, "Error en bind", NULL, logger);
	}

	// Escuchar
	int resultado = listen(socketServer , MAX_CONNECTIONS);
	if(resultado == ERROR){
		_exit_with_error(socketServer, "Error en el listen", NULL, logger);
	   }
	log_trace(logger, "Server socket escuchando");
	freeaddrinfo(res);
	return socketServer;
}

void levantarServidorSelect(int serverSocket, t_log* logger, void (*procesarMensaje)(int,Cabecera*)){
	int nuevaConexion;
	fd_set master;
	fd_set read_fds;
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	struct timeval tv;
	int selectResult;
	// Agrego el socket de escucha al fdset master
	FD_SET(serverSocket, &master);
	// Registrar el máximo file descriptor, hasta el momento es éste
	int fdmax = serverSocket;
	while(1){
		read_fds = master;
		tv.tv_sec = 5;
		tv.tv_usec = 50000;
		selectResult = select(fdmax+1, &read_fds, NULL, NULL, &tv);
		if (selectResult == -1){
			perror("SELECT: ");
			log_warning(logger, "Error al realizar un select");
		} else if(selectResult == 0){
			continue;
		} else {
			int socketActual;
			for(socketActual = 0; socketActual <= fdmax; socketActual++) {
				if (FD_ISSET(socketActual, &read_fds)) {
					if (socketActual == serverSocket) {
						// LLegó una nueva conexión
						nuevaConexion = aceptarConexion(socketActual, logger);
						if (nuevaConexion == -1) {
							// Error al realizar el accept
							perror("ACCEPT: ");
							log_warning(logger, "Error al aceptar una conexión");
						} else {
							// Agregar Cpu al fdset master
							FD_SET(nuevaConexion, &master);
							if (nuevaConexion > fdmax) {
								fdmax = nuevaConexion;
							}
						}
					} else {
						int respuestaRecv = ERROR;
						Cabecera* cabecera = recibirCabecera(socketActual, &respuestaRecv, logger);
						if (respuestaRecv <= 0) { // Hubo error en el recv o se desconecto el cliente
							if (respuestaRecv ==0) { // Hubo error en el recv o se desconecto el cliente
								log_trace(logger,"Conexión cerrada por el cliente");
							}else{
								log_warning(logger, "Error en el recv");
							}
							close(socketActual);
							FD_CLR(socketActual, &master); // eliminar del conjunto maestro
						} else {
						   log_trace(logger, "Cabecera idOperacion: %d", cabecera->idOperacion);
						   log_trace(logger, "Cabecera largo del paquete: %d", cabecera->largoPaquete);
						   procesarMensaje(socketActual,cabecera);
						}
						free(cabecera);
					}
				} //END of FD_ISSET
			} //END of for socketActual <= fdmax
		}
	} //END of While(1)
}

int aceptarConexion(int connection_socket, t_log* logger){
	// Aceptar conexión - Bloqueado hasta que haya una conexión
	struct sockaddr_in clientAddress;
	unsigned int addressSize = sizeof(struct sockaddr_in);
	int socketCliente = accept(connection_socket, (struct sockaddr *)&clientAddress,
			(socklen_t *)&addressSize);
	if(socketCliente == ERROR){
		log_warning(logger,"Error en el accept");
		return socketCliente;
	}
	log_trace(logger, "Recibí una conexión");
	return socketCliente;
}


int conectarAServer(char* ip, char* port, t_log* logger) {
  struct addrinfo hints;
  struct addrinfo *server_info;
  if(ip == NULL){
	ip = DEFAULT_IP;
  }
  if(port == NULL){
	port = DEFAULT_PORT;
  }
  memset(&hints, 0, sizeof(hints));
  // La maquina se encarga de verificar si usamos IPv4 o IPv6
  hints.ai_family = AF_UNSPEC;
  // Indica que usaremos el protocolo TCP
  hints.ai_socktype = SOCK_STREAM;

  // Carga en server_info los datos de la conexion
  getaddrinfo(ip, port, &hints, &server_info);
  t_socket client_socket = socket(server_info->ai_family, server_info->ai_socktype,
		  server_info->ai_protocol);
  if (client_socket == ERROR) {
	  freeaddrinfo(server_info);
	  _exit_with_error(client_socket, "No pudo crearse el socket", NULL, logger);
  }
  int result = connect(client_socket, server_info->ai_addr, server_info->ai_addrlen);
  freeaddrinfo(server_info); // No lo necesitamos mas

  // Si hay error, se loguea y se termina el programa
  if (result == ERROR) {
    _exit_with_error(client_socket, "No pudo establecerse la conexión al servidor", NULL, logger);
  }
  log_trace(logger, "Conectado!");
  return client_socket;
}


int enviarCabecera(int socketReceptor, char* mensaje, int id, t_log* logger) {
	int tamanioMensaje = strlen(mensaje);
	int tamanioCabecera;
	Cabecera * cabecera = (Cabecera*) malloc(sizeof(Cabecera));
	cabecera->largoPaquete = tamanioMensaje;
	cabecera->idOperacion = id;
	tamanioCabecera = sizeof(Cabecera);
	if(enviarMensaje(socketReceptor, cabecera, tamanioCabecera) < 0){
		log_warning(logger,"Error al enviar cabecera");
		free(cabecera);
		return -1;
	}else{
		free(cabecera);
		return 1;
	}
}

// EnviarCabecera modificado para enviar la cantida de sentencias ejecutadas el largoPaquete
int enviarCabeceraCpuSafa(int socketReceptor, int cantSentencias, int id, t_log* logger) {
	int tamanioCabecera;
	Cabecera * cabecera = (Cabecera*) malloc(sizeof(Cabecera));
	cabecera->largoPaquete = cantSentencias;
	cabecera->idOperacion = id;
	tamanioCabecera = sizeof(Cabecera);
	if(enviarMensaje(socketReceptor, cabecera, tamanioCabecera) < 0){
		log_warning(logger,"Error al enviar cabecera");
		free(cabecera);
		return -1;
	}else{
		free(cabecera);
		return 1;
	}
}


Cabecera* recibirCabecera(int socketEmisor, int* resultado, t_log* logger){
	Cabecera* cabecera = (Cabecera*) malloc(sizeof(Cabecera));
	int resultadoRecv;
	resultadoRecv = recv(socketEmisor, cabecera, sizeof(Cabecera), 0);
	if (resultadoRecv < 0) {
		log_warning(logger,"Error en recibir cabecera");
//		exit(1);
	} else if (resultadoRecv == 0) {
		log_warning(logger, "El socket emisor se encuentra desconectado");
		close(socketEmisor);
//		exit(1);
	}
	// Guardo en la variable pasada por referencia el resultado del recv por si se necesita verificar
	if(resultado != NULL) { (*resultado) = resultadoRecv; }

	return cabecera;
}


int enviarMensaje(int socketReceptor, void *mensaje, int largoMensaje) {
	// largoMensaje va por parametro para reutilizar la funcion en envio de headers y de mensajes
	// (el tamanio del header se calcula con sizeof y el del mensaje con strlen)
	int bytesEnviados = 0;
	int bytesRestantes = largoMensaje;
	int resultadoSend;
	while (bytesEnviados < bytesRestantes) {
		resultadoSend = send(socketReceptor, mensaje + bytesEnviados, bytesRestantes, 0);
		if (resultadoSend == -1) {
			break;
		}
		bytesEnviados += resultadoSend;
		bytesRestantes -= resultadoSend;
	}
	if(bytesEnviados == largoMensaje) {
		return 0;
	}
	else {
		return -1; // Cada vez que se usa enviarMensaje se debe chequear su valor de retorno
	}
}

int enviarMensajeInt(int socketReceptor, int mensaje) {
	int mensajeFormateado = htonl(mensaje);
	int bytesEnviados = 0;
	int bytesRestantes = sizeof(mensaje);
	int resultadoSend;
	while (bytesEnviados < bytesRestantes) {
		resultadoSend = send(socketReceptor, &mensajeFormateado, bytesRestantes, 0);
		if (resultadoSend == -1) {
			break;
		}
		bytesEnviados += resultadoSend;
		bytesRestantes -= resultadoSend;
	}
	if(bytesEnviados == sizeof(mensaje)) {
		return 0;
	}
	else {
		return -1; // Cada vez que se usa enviarMensaje se debe chequear su valor de retorno
	}
}

int recibirMensajeInt(int socketEmisor, int* buffer){
	int resultadoRecv;
	int bufferNetwork;
	resultadoRecv = recv(socketEmisor, &bufferNetwork, sizeof(*buffer), 0);
	if(resultadoRecv < 0){
		perror("Recv: ");
		return  -1;
	} else if (resultadoRecv == 0){
		close(socketEmisor);
		return -1; // TODO ver como deberia romper
	}
	*buffer = ntohl(bufferNetwork);
	return 0;
}


int recibirMensaje(int socketEmisor, char** buffer, int largoMensaje){
	int resultadoRecv;

	resultadoRecv = recv(socketEmisor, *buffer, largoMensaje, 0);
	if(resultadoRecv < 0){
		perror("Recv: ");
		return  -1;
	} else if (resultadoRecv == 0){
		//close(socketEmisor);
		free(*buffer);
		return -1; // TODO ver como deberia romper
	}
	(*buffer)[largoMensaje] = '\0';
	return 0;
}


int checkComponentes(int mensajeRecibido, t_log* logger){
	if (mensajeRecibido == DAM_SAFA){
		log_trace(logger, "Hola DAM desde SAFA!");
	} else if (mensajeRecibido == DAM_MDJ){
		log_trace(logger, "Hola DAM desde MDJ!");
	} else if (mensajeRecibido == DAM_FM9){
		log_trace(logger, "Hola DAM desde FM9!");
	} else if (mensajeRecibido == CPU_DAM){
		log_trace(logger, "Hola CPU desde DAM!");
	} else if (mensajeRecibido == CPU_SAFA){
		log_trace(logger, "Hola CPU desde SAFA!");
	} else if (mensajeRecibido == CPU_FM9){
		log_trace(logger, "Hola CPU desde FM9!");
	}else {
		return -1;
	}
	return 0;
}


t_socket servidorConectarComponente(int server_socket, char* servidor,char* componente, char* codigoHandshake, t_log* logger){
	int client_socket;
	char *bufferMensaje;
	bufferMensaje = malloc(2);
	log_trace(logger,"Conectando %s con %s\n", servidor, componente);
	client_socket = aceptarConexion(server_socket,logger);
	if(client_socket == ERROR) {
		return -1;
	}
	int salidaRecibeMensaje =recibirMensaje(client_socket, &bufferMensaje,  sizeof(char));
	if(salidaRecibeMensaje < 0){
		log_error(logger,"Error al recibir handshake");
		return -1;
	}else{
		if(checkComponentes(atoi(bufferMensaje), logger) == -1){
			log_error(logger,"Handshake invalido");
			close(client_socket);
			free(bufferMensaje);
			//free(codigoHandshake);
			exit_gracefully(1, logger);
		}
		if (enviarMensaje(client_socket,
				codigoHandshake, strlen(codigoHandshake)) == ERROR){
			log_error(logger,"Error al responder handshake");
			close(client_socket);
			free(bufferMensaje);
			//free(codigoHandshake);
			exit_gracefully(1, logger);
		}
	}
	log_trace(logger,"Conectado correctamente");
	free(bufferMensaje);
	return client_socket;
}

t_socket servidorConectarComponenteConCabecera(int serverSocket, int codigoHandshake, t_log* logger){
	int resultadoRecv;
	int socketCliente = aceptarConexion(serverSocket, logger);
	Cabecera* cabecera = recibirCabecera(socketCliente, &resultadoRecv,  logger);
	if(resultadoRecv > 0 && (checkComponentes(cabecera->idOperacion, logger) != -1) && (codigoHandshake == cabecera->idOperacion)){
		enviarCabecera(socketCliente, "", cabecera->idOperacion, logger);
		free(cabecera);
		return socketCliente;
	} else {
		log_error(logger,"Handshake invalido");
		free(cabecera);
		close(socketCliente);
		exit_gracefully(1, logger);
		return -1;
	}
}


t_socket clienteConectarComponenteConCabecera(char* cliente, char* componente,
		char* puerto, char* ip, int codigoHandshake, t_log* logger) {
	int socketServ;
	char *bufferMensaje;
	bufferMensaje = malloc(TAMANIO_HANDSHAKE);
	socketServ = conectarAServer(ip, puerto, logger);
	if(enviarCabecera(socketServ, "", codigoHandshake, logger) == ERROR){
		log_error(logger,"Error al conectar %s con %s\n", cliente, componente);
		close(socketServ);
		free(bufferMensaje);
		exit_gracefully(1,logger);
	} else {
		int respuestaRecv;
		// TODO recibirCabecera puede tirar error
		Cabecera* cabecera = recibirCabecera(socketServ, &respuestaRecv, logger);
		if(checkComponentes(cabecera->idOperacion, logger) == -1){
			log_error(logger,"Error al conectar %s con %s\n", cliente, componente);
			close(socketServ);
			free(bufferMensaje);
			free(cabecera);
			exit_gracefully(1,logger);
		}
		free(cabecera);
	}
	free(bufferMensaje);
	return socketServ;
}


t_socket clienteConectarComponente(char* cliente, char* componente,
		char* puerto, char* ip, char* codigoHandshake, t_log* logger) {
	int socketServ;
	char *bufferMensaje;
	bufferMensaje = malloc(2 * sizeof(char));
	socketServ = conectarAServer(ip, puerto, logger);
	if (enviarMensaje(socketServ, codigoHandshake, sizeof(codigoHandshake)) == ERROR) {
		log_error(logger,"Error al conectar %s con %s\n", cliente, componente);
		close(socketServ);
		free(bufferMensaje);
		exit_gracefully(1,logger);
	} else {
		recibirMensaje(socketServ, &bufferMensaje,  sizeof(char));
		if(checkComponentes(atoi(bufferMensaje), logger) == -1){
			log_error(logger,"Error al conectar %s con %s\n", cliente, componente);
			close(socketServ);
			free(bufferMensaje);
			exit_gracefully(1,logger);
		}
	}
	free(bufferMensaje);
	return socketServ;
}


void _exit_with_error(int socket, char* error_msg, void * buffer, t_log* logger) {
  if (buffer != NULL) {
    free(buffer);
  }
  log_error(logger, error_msg);
  close(socket);
  exit_gracefully(1, logger);
}


void exit_gracefully(int return_nr, t_log* logger) {
  log_destroy(logger);
  exit(return_nr);
}
