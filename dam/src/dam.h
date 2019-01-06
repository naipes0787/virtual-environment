#ifndef DAM_H_
#define DAM_H_

#include <sharedlib.h>
#include <semaphore.h>

#define IP_SAFA "IP_SAFA"
#define PUERTO_SAFA "PUERTO_SAFA"
#define IP_MDJ "IP_MDJ"
#define PUERTO_MDJ "PUERTO_MDJ"
#define IP_FM9 "IP_FM9"
#define PUERTO_FM9 "PUERTO_FM9"
#define PUERTO "PUERTO"
#define TRANSFER_SIZE "TRANSFER_SIZE"

char *separadorLineas = "\n";
char *OPERACION_ABRIR = "abrir";
char *OPERACION_ASIGNAR = "asignar";
char *OPERACION_FLUSH = "flush";
char *OPERACION_CREAR = "crear";
t_log* loggerDam;
t_config* config;
t_socket socketSAFA = ERROR;
t_socket socketMDJ = ERROR;
t_socket socketFM9 = ERROR;
t_socket serverSocket = ERROR;
// Semáforo para evitar que varios pedidos quieran usar MDJ a la vez
sem_t usoMDJ;
// Semáforo para evitar que varios pedidos quieran usar FM9 a la vez
sem_t usoFM9;
// Semáforo para evitar que varios pedidos quieran usar SAFA a la vez
sem_t usoSAFA;
pthread_t hiloCPU;
fd_set master;
int transferSize = ERROR;
int maximoLineasFM9 = ERROR;
char* port;


/**
* @NAME: conexionSAFA
* @DESC: Crea la conexión con el proceso SAFA, seteando el socketSAFA.
*/
void conexionSAFA();


/**
* @NAME: conexionMDJ
* @DESC: Crea la conexión con el proceso MDJ, seteando el socketMDJ.
*/
void conexionMDJ();


/**
* @NAME: conexionFM9
* @DESC: Crea la conexión con el proceso FM9, seteando el socketFM9.
*/
void conexionFM9();


/**
* @NAME: pruebaMensajes
* @DESC: Se utiliza para probar el reenvío de mensajes entre CPU y FM9/MDJ.
*/
void pruebaMensajes(Cabecera* cabecera, t_socket socketActual, int handshake,
		sem_t semaforo, t_socket socketDestino);


/**
* @NAME: pedirArchivoAMDJ
* @DESC: Pide a MDJ el archivo solicitado por CPU
*/
char* pedirArchivoAMDJ(char* path, int *errorCode);


/**
* @NAME: calcularCantidadLineas
* @DESC: Dado un archivo, calcula la cantidad de líneas que posee en base al separador
* 		 de líneas definido en sharedlib.h
*/
int calcularCantidadLineas(char* archivo);



/**
* @NAME: enviarArchivoAFM9
* @DESC: Envía el archivo obtenido de MDJ a FM9 en líneas separadas en partes de
* 		 transferSize tamaño
*/
int enviarArchivoAFM9(char* archivo, int idGdt,	int *errorCode);


/**
* @NAME: pedirArchivoAFM9
* @DESC: Pide a FM9 el archivo para el cual CPU quiere hacer flush
*/
char* pedirArchivoAFM9(int idGdt, int direccionMemoria, int *errorCode);


/**
* @NAME: enviarArchivoAMDJ
* @DESC: Envía el archivo obtenido de FM9 a MDJ separado en partes de
* 		 transferSize tamaño
*/
void enviarArchivoAMDJ(char *archivo, char* path, int *errorCode);


/**
* @NAME: obtenerEntradasSalidas
* @DESC: Se recorre el archivo separándolo en líneas e inspeccionando
* 		 si las líneas poseen un llamado a abrir, asignar, flush o crear,
* 		 retorna un String formado por las líneas donde se encontraron
* 		 estas operaciones (Comenzando en la línea 0) separadas por coma
* 		 y por último el número de la última línea
*/
char* obtenerEntradasSalidas(char *archivo);


/**
* @NAME: notificarResultadoFlushASAFA
* @DESC: Se le notifica a SAFA si el flush fue exitoso o si hubo un error,
* 		 reenviándole el código de error. También se envía el idGdt.
*/
void notificarResultadoFlushASAFA(int errorCode, int idGdt);


/**
* @NAME: notificarResultadoCrearBorrarASAFA
* @DESC: Se le notifica a SAFA si el creado o borrado fue exitoso o si hubo un error,
* 		 reenviándole el código de error. También se envía el idGdt.
*/
void notificarResultadoCrearBorrarASAFA(int errorCode, int idGdt);


/**
* @NAME: notificarResultadoPedirArchivoASAFA
* @DESC: Se le notifica a SAFA si la carga del archivo fue exitosa o si hubo un error,
* 		 reenviándole el código de error. También se envía el idGdt, la dirección
* 		 de memoria en la que se cargó el archivo y el path del archivo.
*/
void notificarResultadoPedirArchivoASAFA(int errorCode, int idGdt, int direccionMemoria, char* path);


/**
* @NAME: notificarDummyASAFA
* @DESC: Se le notifica a SAFA si la operación dummy termino, enviandole la ubicacion del archivo
* y la lista de entradas y salidas del mismo.
*/
void notificarDummyASAFA(int errorCode, int idGdt, int direccionMemoria, char* stringEntradasSalidas);


/**
* @NAME: manejarOperacionCpu
* @DESC: Evalúa qué tipo de operación fue solicitada por la CPU y ejecuta
* 		 lo que corresponda.
*/
void manejarOperacionCpu(int socketActual, Cabecera* cabecera);


/**
* @NAME: conexionCPU
* @DESC: Utiliza un select para manejar las conexiones entre DAM y las n CPUs.
* 		Se setea socketCPU y socketServer.
* 		En caso de haber una petición por parte de una CPU, se crea un hilo
* 		y se llama a manejarOperacionCpu
*/
void* comunicacionCPU(void* arg);


/**
* @NAME: inicializar_config
* @DESC: Asocia el archivo dam.cfg al atributo config del proceso
*/
void inicializar_config();


/**
* @NAME: inicializar
* @DESC: Inicializa las variables que se precisan utilizar: loggerDam y config. Además
* 		 establece las conexiones con SAFA, MDJ, FM9 y CPU.
*/
void inicializar();


/**
* @NAME: finalizarProceso
* @DESC: Cierra todos los sockets y finaliza el proceso DAM.
*/
void finalizarProceso();

/**
 * Reenvia a Fm9 los archivos a cerrar luego de algun error en memoria
 */
void enviarArchivosACerrar(char* aCerrar, int idGdt);

#endif /* DAM_H_ */
