#ifndef SHAREDLIB_H_
#define SHAREDLIB_H_
/*
 * Header file utilizado para encapsular aquellas funcionalidades
 * que pueden ser utilizadas por más de un módulo, y así poder
 * mantenernos DRY (https://en.wikipedia.org/wiki/Don%27t_repeat_yourself)
 */

/*********** INCLUDES ***********/
#include <stdio.h>
#include <string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <errno.h>
#include <stdlib.h> // Para malloc
#include <sys/socket.h> // Para crear sockets, enviar, recibir, etc
#include <netdb.h> // Para getaddrinfo
#include <unistd.h> // Para close
#include <pthread.h> // Para aplicaciones multithread
#include <commons/config.h>//Para archivos de configuracion
#include <commons/string.h>//Para strings


/*********** CONSTANTES ***********/
#define DEFAULT_IP "127.0.0.1"
#define DEFAULT_PORT "8080"
#define SAFA_PROCESS_NAME "SAFA"
#define CPU_PROCESS_NAME "CPU"
#define DAM_PROCESS_NAME "DAM"
#define FM9_PROCESS_NAME "FM9"
#define MDJ_PROCESS_NAME "MDJ"
#define ERROR -1
#define OK 0
#define MAX_CONNECTIONS 100
#define TAMANIO_HANDSHAKE 2*sizeof(char)

/*********** PROTOCOLO ***********/
enum PROTOCOLO {
	// HANDSHAKE INICIAL
	DAM_SAFA = 0,
	DAM_MDJ = 1,
	DAM_FM9 = 2,
	CPU_DAM = 3,
	CPU_SAFA = 4,
	CPU_FM9 = 6,

	// ENTENDIDOS POR SAFA
	ARCHIVO_CARGADO = 11,
	ERROR_CARGA = 12,
	PASAR_A_READY = 13,
	EJECUTAR = 14,
	PASAR_A_EXIT = 15,
	BLOQUEAR_DUMMY = 16,
	PASAR_A_BLOCKED = 17,
	FIN_QUANTUM = 18,
	FIN_QUANTUM_SOBRANTE = 25,
	OBTENERIOS = 40,

	// ENTENDIDOS POR CPU
	//INICIAR_DUMMY = 5,
	PEDIR_ARCHIVO = 8,
	//ERROR_CARGA = 12,
	//EJECUTAR = 14,
	//PASAR_A_EXIT = 15,
	//BLOQUEAR_DUMMY = 16,
	//PASAR_A_BLOCKED = 17,
	//FIN_QUANTUM = 18,
	//FIN_QUANTUM_SOBRANTE = 25,
	BLOQUEAR_ABRIR_ARCHIVO = 26,
	EJECUTAR_WAIT = 27,
	EJECUTAR_SIGNAL = 28,
	PEDIR_ESCRIPTORIO = 29,
	ARCHIVO_NO_ABIERTO = 31,
	EJECUTAR_ASIGNAR = 32,
	EJECUTAR_FLUSH = 33,
	BLOQUEAR_FLUSH = 34,
	EJECUTAR_CLOSE = 35,
	EJECUTAR_CREAR = 36,
	BLOQUEAR_CREAR = 37,
	EJECUTAR_BORRAR = 38,
	BLOQUEAR_BORRAR = 39,
	LIBERAR = 42,

	// ENTENDIDOS POR DAM
	PRUEBA_MENSAJE_FM9 = 99,
	PRUEBA_MENSAJE_MDJ = 98,
	INICIAR_DUMMY = 5,
	/*  Los siguientes son entendidos por DAM pero ya están definidos
	 *	  PEDIR_ARCHIVO = 8,
	 *	  EJECUTAR_FLUSH = 33,
	 *	  EJECUTAR_CREAR ) 36,
	 *    EJECUTAR_BORRAR = 38,
	 *    OBTENER_DATOS = 38,
	*/
	// ENTENDIDOS POR MDJ
	ENVIAR_TRANSFER_SIZE = 30,
	VALIDAR_ARCHIVO = 20,
	CREAR_ARCHIVO = 21,
	OBTENER_DATOS = 22,
	GUARDAR_DATOS = 23,
	BORRAR_ARCHIVO = 24,


	// ENTENDIDOS POR FM9
	SUBIR_ARCHIVO_FM9 = 10,
	SOLICITUD_FLUSH  = 41,
	SOLICITUD_ASSIGN = 15,
	SOLICITUD_CLOSE = 17,
	MEMORIA_SIN_ESPACIO = 18,
	OK_CARGA=19,
	LIBERAR_MEMORIA = 101
};

/*********** CODIGOS DE ERROR ***********/
enum CODIGO_ERROR {
	ABRIR_PATH_INEXISTENTE = 10001,
	ABRIR_ESPACIO_MEMORIA_INSUFICIENTE = 10002,
	ASIGNAR_ARCHIVO_NO_ABIERTO = 20001,
	ASIGNAR_FALLO_SEGMENTO_MEMORIA = 20002,
	ASIGNAR_ESPACIO_MEMORIA_INSUFICIENTE = 20003,
	FLUSH_ARCHIVO_NO_ABIERTO = 30001,
	FLUSH_FALLO_SEGMENTO_MEMORIA = 30002,
	FLUSH_ESPACIO_MDJ_INSUFICIENTE = 30003,
	FLUSH_ARCHIVO_INEXISTENTE_MDJ = 30004,
	CLOSE_ARCHIVO_NO_ABIERTO = 40001,
	CLOSE_FALLO_SEGMENTO_MEMORIA = 40002,
	CREAR_ARCHIVO_EXISTENTE = 50001,
	CREAR_ESPACIO_INSUFICIENTE = 50002,
	BORRAR_ARCHIVO_INEXISTENTE = 60001,
	ERROR_CHARS_SUPERAN_MAXIMO = 99998,
	ERROR_ARCHIVO_SIN_LINEAS = 99999,
	ERROR_CREAR_DIRECTORIO = 80001
};

enum OPERACION_BLOQUEANTE {
	BLOCK_BUSCAR_ARCHIVO = 420,
	BLOCK_ESPERAR_RECURSO = 421,
	BLOCK_FLUSH = 422,
	BLOCK_CREAR_ARCHIVO = 423,
	BLOCK_BORRAR_ARCHIVO = 424
};

/*********** TIPOS DE DATOS ***********/
typedef int t_socket;

typedef struct {
  int idOperacion;
  int largoPaquete;
} __attribute__((packed)) Cabecera;

/*********** FUNCIONES ***********/

/**
* @NAME: configure_logger
* @DESC: Crea y devuelve una instancia de logger, tomando por parametro el nombre del proceso
* para el que se crea y el nivel de logueo que se precisa. El log se almacena en el directorio
* del proceso con el nombre [process_name].log
* También se loguea que el logger fue inicializado.
*/
t_log* configure_logger(char* process_name, t_log_level log_level);


/*
 * @NAME: crearServerSocket
 * @DESC: Crear un socket servidor
 *   - Si no se envía ip, por defecto se utiliza localhost
 *   - Si no se envía puerto, por defecto se utiliza 8080
 *   Ante un error se loguea, se cierra el socket y se termina el proceso
 */
t_socket crearServerSocket(char* ip, char* port, t_log* logger);


/*
 * @NAME: levantarServidorSelect
 * @DESC: Se comienza a escuchar mediante un socket y se queda a la espera de conexiones
 * Ante un error importante se loguea, se cierra el socket y se termina el proceso
 */
void levantarServidorSelect(int serverSocket, t_log* logger, void (*procesarMensaje)(int,Cabecera*));


/*
 * @NAME: aceptarConexion
 * @DESC: aceptar una conexión.
 * Ante un error se loguea, se cierra el socket y se termina el proceso
 */
int aceptarConexion(int connection_socket, t_log* logger);


/*
 * @NAME: conectarAServer
 * @DESC: Conectar al servidor, creando un socket cliente.
 *   - Si no se envía ip, por defecto se utiliza localhost
 *   - Si no se envía puerto, por defecto se utiliza 8080
 *  Ante un error se cierra el socket y se termina el proceso
 */
int conectarAServer(char* ip, char* port, t_log* logger);


/*
 * @NAME: enviarCabecera
 * @DESC: Funcion para enviar la cabecera con el detalle de la informacion
 */
int enviarCabecera(int socketReceptor, char* mensaje, int, t_log* logger);


/*
 * @NAME: recibirCabecera
 * @DESC: Funcion para recibir la cabecera con el detalle de la informacion
 */
Cabecera* recibirCabecera(int socketEmisor, int* resultado, t_log* logger);


/*
 * @NAME: enviarMensaje
 * @DESC: Envia un mensaje al socketReceptor. Devuelve 0 si el envio fue exitoso,
 * o -1 si no se enviaron todos los bytes.
 */
int enviarMensaje(int socketReceptor, void *mensaje, int largoMensaje);

/*
 * @NAME: enviarMensajeInt
 * @DESC: Envia un int al socketReceptor, transformando de bytes del host a bytes de la red
 * Devuelve 0 si el envio fue exitoso,
 * o -1 si no se enviaron todos los bytes.
 */
int enviarMensajeInt(int socketReceptor, int mensaje);

/*
 * @NAME: recibirMensaje
 * @DESC: Permite recibir un mensaje y guardarlo en el buffer especificado en el
 * 		  segundo parametro. Devuelve -1 si hubo un error al escuchar, o 0 si el
 * 		  mensaje se recibio correctamente.
 */
int recibirMensaje(int socketEmisor, char** buffer, int largoMensaje);

/*
 * @NAME: recibirMensajeInt
 * @DESC: Permite recibir un int haciendo la conversion de bytes de la red a bytes del host
 * y simplifica la operatoria.
 */
int recibirMensajeInt(int socketEmisor, int* buffer);


/*
 * @NAME: checkComponentes
 * @DESC: Verifica que el mensaje recibido para el handshake sea uno de los permitidos.
 * Devuelve 0 si el resultado está bien o -1 en caso contrario.
 */
int checkComponentes(int mensajeRecibido, t_log* logger);


/*
 * @NAME: servidorConectarComponente
 * @DESC: Permite aceptar la conexión al servidor y logea los resultados
 */
t_socket servidorConectarComponente(int socketEscucha, char* servidor,char* componente,
		char* codigoHandshake, t_log* logger);

/*
 * @NAME: servidorConectarComponenteConCabecera
 * @DESC: Permite enviar una conexión a un servidor y logea los resultados
 * 			Utiliza enviarCabecera.
 */
t_socket servidorConectarComponenteConCabecera(int serverSocket, int codigoHandshake, t_log* logger);

/*
 * @NAME: clienteConectarComponente
 * @DESC: Permite enviar una conexión a un servidor y logea los resultados
 * 			Utiliza enviarMensaje.
 */
t_socket clienteConectarComponente(char* cliente, char* componente, char* puerto, char* ip,
		char* codigoHandshake, t_log* logger);

/*
 * @NAME: clienteConectarComponente
 * @DESC: Permite enviar una conexión a un servidor y logea los resultados
 * 			Utiliza enviarCabecera.
 */
t_socket clienteConectarComponenteConCabecera(char* cliente, char* componente,
		char* puerto, char* ip, int codigoHandshake, t_log* logger);

/*
 * @NAME: _exit_with_error
 * @DESC: Utilizado para finalizar el proceso con error. Se loguea el error,
 * 		  se cierra el socket y se termina el proceso.
 */
void _exit_with_error(int socket, char* error_msg, void* buffer, t_log* logger);


/*
 * @NAME: exit_gracefully
 * @DESC: Se destruye el logger y se termina el proceso.
 */
void exit_gracefully(int return_nr, t_log* logger);

// Cabecera utilizada entre cpu y safa para calcular metricas
int enviarCabeceraCpuSafa(int socketReceptor, int cantSentencias, int id, t_log* logger);


#endif // SHAREDLIB_H_
