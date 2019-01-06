

#ifndef SRC_FM9_H_
#define SRC_FM9_H_

#include <sharedlib.h>
#include <readline/readline.h>

#define PUERTO_FM9 "PUERTO_FM9"
#define MODO "MODO"
#define TAMANIO_MEM "TAMANIO_MEM"
#define MAX_LINEA "MAX_LINEA"
#define TAMANIO_PAG "TAMANIO_PAG"
#define IP "IP"

/*********** Protocolo ***********/
#define NOMBRE_PROCESO         "FM9"
#define RESP_A_HANDSHAKE_FM9   "10"
#define MODO_SEGMENTACION_PURA "SEG"
#define MODO_PAG_INVERTIDAS	   "TPI"
#define MODO_SEGMENTACION_PAG  "SPA"
#define SEGMENTACION_PURA 1
#define PAG_INVERTIDAS	  2
#define SEGMENTACION_PAG  3


t_socket socketDAM = ERROR;
int transferSizeDAM = ERROR;
t_log* loggerFm9 = NULL;
int idAAsignar=0;
int codigoError;

typedef struct {
	char* ip;
	char* puerto_fm9;
	int modo;
	int tamanio_mem;
	int max_linea;
	int tamanio_pag;
//	unsigned int intervalo_dump;
} t_config_fm9;

t_config_fm9* config;

typedef struct {
	int limite;
	int base;
	int pid;
} t_nodo;

typedef struct {
	int pid;
	int pagina;
	int frame;
	int id;
} t_nodo_tpi;

typedef struct {
	int pid;
	t_list* tablaSegmentos;
} t_nodo_proceso;

typedef struct {
	int base;
	t_list* tablaPaginas;
} t_nodo_segmento;

typedef struct{
	int numPag;
	int numFrame;
}t_nodo_frame;

int numPagina=0;
t_list* ptrFrames=NULL;
t_list* ptrProcesos=NULL;
t_list* ptrPaginacion=NULL;

typedef struct{
	int lineasDeMem;
	char* storage;
	int* tablaStorage;
	int lineasPorPagina;
	t_list* ptrSegmentos;
}t_admin_memoria;
t_admin_memoria* ptrAdminMemoria;

//consola
typedef void* (*Operation)(void* param);
typedef struct {
	char* name;
	Operation operation;
	bool accepts_empty_param;
} Command;

pthread_t iniciarConsola();
void finalizarConsola(pthread_t hiloConsola);
void* procesoConsola(void*);
int verifyCommand(char*);
void* mostrarDump(void*);
void* salirConsola(void*);
void segmentacionDump(int pid);
void tpiDump(int pid);
void segPagDump(int pid);
char* recuperarArchivo(char* posicionACopiar, int cantidadDeLineasACopiar);
void liberarRecursos();
void liberarEstructurasModoMemoria();
Command commands[] = { { "dump", mostrarDump, false }, { "quit", salirConsola, true },
		{ "exit", salirConsola, true }, { "q", salirConsola, true } };


void iniciarServidor();
void inicializarConfig();
void levantarDatosConfig(t_config* archivoConfig);
void procesarMensajes(int socket_cliente, Cabecera* cabecera);
void procesarAbrirArchivo();
void inicializarModoMemoria();
void inicializarEstructuraAdministrativa();
t_nodo* buscarNodo(int buffer);
t_nodo* sacarNodo(int baseMem);
t_nodo* crearNodo(int lineasAGuardar,int direccionDondeGuardar,int pid);
t_nodo_tpi* crearNodoTPI(int frame);
int obtenerNumeroDePaginaAAsignar(int pid);
t_nodo_tpi* buscarNodoPorPaginaPorPid(int pid,int numPagina);
t_list* obtenerNodosOrdenadosPorPagMismoId(int id);
void procesarHandshake(int socket_cliente, Cabecera* cabecera);
int	recibirLinea(int socket_cliente, char* lineaMemoria);
int entradasLibresContiguas(int entradasNecesarias);
int enviarArchivoDeAPartes(int socket_cliente, char* lineaAEnviar, int tamanioDeLinea);
int enviarArchivoTotal(int socket_cliente, char* posicionACopiar, int cantidadDeLineasACopiar, int tamanioDeLinea);
void enviarArchivo(int socket_cliente, int posicionMem, int pid);
void cerrarArchivo(int socket_cliente,int posicionMemoria,int pid);
int recibirArchivo(int socket_cliente, int numPid, int lineasAGuardar);
void segmentacionEnviarArchivo(int socket_cliente, int baseMem);
int segmentacionRecibirArchivo(int socket_cliente, int pid,int lineasAGuardar);
void segmentacionGuardarArchivo(char* buffer, int posicion, int direccionDondeGuardar, int tamanioDeLinea);
void segmentacionCerrar(int socket_cliente,int baseMem);
int tpiRecibirArchivo(int socket_cliente, int pid, int lineasAGuardar);
void tpiGuardarArchivo(char* buffer, int pid, int pagina,int linea,t_nodo_tpi* nodoFrameLibre,int tamanioDeLinea);
void tpiEnviarArchivo(int socket_cliente, int pid,int posicionMemoria);
void tpiCerrar(int socket_cliente,int pid,int numPagina);
void modificarLineaDeArchivo(int socket_cliente,int posicionMem,int pid,int lineaAModificar,char* mensaje,int tamanioMensaje);
void segmentacionModificarLinea(int socket_cliente,int baseMem,int lineaAModificar,char* mensaje,int tamanioMensaje);
void tpiModificarLinea(int socket_cliente,int numPagina,int pid,int lineaAModificar,char* mensaje,int tamanioLinea);
void enviarEscriptorio(int socket_cliente,int posicionMem,int pid);
void segmentacionEnviarEscriptorio(int socket_cliente,int baseMem);
void segmentacionEnviarEscriptorioTotal(int socket_cliente,t_nodo*  nodoEncontrado);
void tpiEnviarEscriptorio(int socket_cliente,int pid,int numPagina);
t_nodo_frame* crearNodoFrame(int frame);
t_nodo_proceso* encontrarNodoMismoProceso(int pid);
t_nodo_segmento* encontrarNodoMismoProcesoMismaBase(int pid,int base);
int* crearNodoPagina(int numeroPagina);
int spaRecibirArchivo(int socket_cliente, int pid,int lineasAGuardar);
void spaGuardarArchivo(char* buffer,int pid,int numLinea,t_nodo_frame* nodoFrameLibre,int tamanioDeLinea);
void spaEnviarEscriptorio(int socket_cliente,int pid,int numBase);
void spaModificarLinea(int socket_cliente,int base,int pid,int lineaAModificar,char* mensaje,int tamanioLinea);
void spaEnviarArchivo(int socket_cliente, int pid,int base);
t_nodo_frame* obtenerNodoframe(int numPagina);
void spaCerrar(int socket_cliente,int pid,int numBase);
void spaModificarLinea(int socket_cliente,int base,int pid,int lineaAModificar,char* mensaje,int tamanioLinea);
void spaEnviarArchivo(int socket_cliente, int pid,int base);
#endif /* SRC_FM9_H_ */
