#ifndef MDJ_H_
#define MDJ_H_

#include <stdlib.h>
#include <sharedlib.h>
#include <unistd.h>
#include <pthread.h>
#include <readline/readline.h>
#include <commons/string.h>
#include <string.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <commons/bitarray.h>
#include <commons/collections/list.h>
#include <commons/txt.h>
#include <openssl/md5.h> // Para calcular el MD5
#include <sys/stat.h> // para el mkdir


typedef struct{
	int tamanioBloques;
	int cantidadBloques;
	char* magicNumber;
}Metadata;
Metadata metadata;

typedef void* (*Operation)(void* param);
typedef struct {
	char* name;
	Operation operation;
	bool accepts_empty_param;
} Command;

char* ip;
char* port;
int retardo;
char* puntoMontaje;

FILE* fd;
t_log* loggerMDJ;
t_config* config;
t_bitarray* bitmap;
int transferSizeDAM = ERROR;
int serverSocket;
int socketDam;

void comprobarFIFA();
void cargarBitmap();
void inicializar();
void initializeLogger();
void crearConfiguracion();
void inicializar_config();
void* gestionarPedidosDam(void*);
//void gestionarPedidosDam(int socketActual, Cabecera* cabecera); Borrar antes de entregar
void comunicacionConDAM();
void procesarConexiones(int socket_cliente);
void enviarMensajePrefijado(char *msg);

int tamanioPathArchivo(char* pathArchivo);
int verificarDirectorio(char* path);
int tamanioPathBloque(char* bloque);
char* armarPathArchivo(char* direccion, char* pathArchivo);
char* armarStringBloques(int bloques[], int cantBloques);
int hayEspacioDisponible(int bytes);
void asignarBloques(int arrayBloques[], int posArray, int cantBloquesAAsignar);
void escribirBloques(int arrayBloques[], char* buffer);

int validarArchivo(char* path);
int crearArchivo(char* path, int cantBytes);
char* obtenerDatos(char* path, int offset, int size);
void obtenerDatosSHORT_VERSION(char* path, int offset, int size);
int guardarDatos(char* path, int offset, int size, char* buffer);
int borrarArchivo(char* path);

int verifyCommand(char*);
void* consolaMDJ(void*);
void* comandoLS(void*);
void* comandoCD(void*);
void* comandoMD5(void*);
void* comandoCat(void*);
void* salirConsola(void*);
bool esSalirConsola(char* nombreComando);
Command commands[] = { { "ls", comandoLS, true }, { "cd", comandoCD, false },
		{ "md5", comandoMD5 , false }, {"cat", comandoCat, false }, { "quit", salirConsola, true },
		{ "exit", salirConsola, true }, { "q", salirConsola, true } };

void liberarRecursos();

char* ubicacionMetadata();
char* ubicacionBitmap();

#endif /* MDJ_H_ */
