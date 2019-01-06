#ifndef SAFA_H_
#define SAFA_H_

// Includes
#include <sharedlib.h>
#include <stdbool.h>
#include <commons/collections/queue.h>
#include <semaphore.h>
#include <sys/inotify.h>
#include <readline/readline.h>
#include <stdlib.h>

#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define INOTIFY_BUF_LEN  ( 1024 * EVENT_SIZE )
// Enums
enum State {
	Working = 1, Corrupt = 0
};
enum State state;

// Structs
typedef void* (*Operation)(void* param);

typedef struct {
	char* name;
	Operation operation;
	bool accepts_empty_param;
} Command;

typedef struct {
	int id;
	char* escriptorio;
	int ubicacionEscriptorio;
	int pc;
	int inicializado;
	t_list *archivos_abiertos;
	int quantumDisponible;
	int* infoES;
	bool escriptorioCargado;
	int sentenciasEnNew;
	int sentenciasAlDiego;
	time_t tiempoInicial;
	time_t tiempoFinal;
	int cantidadES;
} Dtb;

typedef struct {
	char* escriptorio;
	int ubicacion;
} Archivo;

typedef struct {
	int socket;
	int idDtbActual;
	bool libre;
} Cpu;

typedef struct {
	int contador;
	char* nombre;
	t_queue *procesosBloqueados;
} Recurso;

// Prototipos
bool esCpuLibre(void* cpu);
bool esElDummy(void*);
bool estaEnLista(t_list* lista, int id);
bool closestIOComparator(void* unDtbVoid, void* otroDtbVoid);

void* ejecutar(void*);
void* finalizar(void*);
void* funcionalidadPcp(void*);
void* funcionalidadPlp(void*);
void* funcionalidadInotify(void*);
void* listenToConsole(void*);
void* metricas(void*);
void* status(void*);
void* salirConsola(void*);

void* comunicacionDam(void*);
void conectarDAM();
void crearNuevaCpu(int socketNuevo, fd_set* master);
void decrementarRecurso(Recurso* recurso, t_socket socketCpu, Dtb* dtb);
void ejecutarClose(t_socket socketCpu);
void ejecutarDtbsEnReady();
void ejecutarWait(int socketCpu);
void ejecutarSignal(int socketCpu);
void eliminarCpu(int socketAEliminar);
void enviarDtbAEjecutar(Cpu* cpu, Dtb* dtb);
void freeArchivo(void*);
void freeDtb(void*);
void freeRecurso(void* recursoVoid);
void imprimirArchivosAbiertos(void* archivoVoid);
void imprimirDtb(void* dtbVoid);
void incrementarRecurso(Recurso* recurso, t_socket socketCpu);
void inicializarConfig();
void inicializarIdDtb();
void inicializarListasPlanificacion();
void inicializarLogger();
void inicializarTablaRecursos();
void initialize();
void llenarDummyConDtb(Dtb*);
void manejarBloqueoDtb(t_socket socketCpu);
void manejarFinEjecucionDtb(t_socket socketCpu);
void manejarOperacionCpu(Cabecera* cabecera, int socketCpu);
void manejarErrorCarga(t_socket socketDam);
void manejarReadyDam(t_socket socketDam);
void pasarATerminated(t_list* listaOriginal, int id);
void pasarATerminatedSiFinalizo(t_list* listaOriginal, int id);
void manejarFinQuantum(t_socket socketCpu);
void manejarDummyFinalizado(t_socket socketDam);
void list_remove_by_id(t_list* list, int id);
void marcarCpuLibre(int socketCpu);
void liberarInotify();
void liberarTodo();
void liberarRecursos(Dtb* dtb);
void operacionArchivoCargado(t_socket socketDam);
/**
* @NAME: procesarErrorCode
* @DESC: Procesa los errores enviados por DAM (Actualmente sólo loguea mensajes)
*/
void procesarErrorCode(int errorCode);
/**
* @NAME: procesarBlockCode
* @DESC: Loggea la razon por la cual se bloqueo el dtb
*/
void procesarBlockCode(int idDtb, int errorCode);
void asignarListaDeIOs(Dtb* dtb, char* respuesta);
void inicializarInotify();
void aumentarMetricas(int cantSentencias);

int cantidadProcesosEnMemoria();
int verifyCommand(char*);
int instruccionesHastaIO(Dtb* unDtb);
int obtenerTiempoRespuesta(Dtb* dtb);
int obtenerTiempoRespuestaPromedio();
int obtenerPromedioSentenciasAlDiego();
int obtenerPromedioSentenciasExit();

Dtb* buscarDtbPorId(int id);
Dtb* crearDtbDummy();
Dtb* crearNuevoDtb(char*);
Dtb* obtenerDtbRR();
Dtb* obtenerProximoDtb();

t_list* obtenerCpuLibres();

char* serializarArchivosAbiertos(Dtb* dtb);
char* obtenerUbicacionArchivos(int idDtb);

// Variables globales
enum State state;

t_log* loggerSafa;
t_config* config;
char *pathConfig;

bool damConectado, cpuConectada;
char* ip;
char* port;
char* algoritmo;

int quantum;
int retardo;
int nivelMultiprogramacion;
int proximoIdDtb;
int serverSocket;
int socketDam;
int inotifyFd = ERROR;
int configFd;

t_queue* ColaNew;

t_list* ListaReady;
t_list* ListaReadyAux;
t_list* ListaRunning;
t_list* ListaBlocked;
t_list* ListaTerminated;
t_list* ListaGeneral;
t_list* CpuConectadas;
t_list* TablaRecursos;

sem_t procesosEnNew;
sem_t dummyLibre;
sem_t usoListaReady;
sem_t usoListaBlocked;
sem_t listaCpusEnUso;

Dtb* dtbDummy;

Command commands[] = { { "ejecutar", ejecutar, false },
		{ "status", status, true }, { "finalizar", finalizar, false }, {
				"metricas", metricas, true }, { "quit", salirConsola, true },
				{ "exit", salirConsola, true }, { "q", salirConsola, true } };

// Metricas

int cantSentenciasEjecutadas = 0;
int cantSentenciasAlDiego = 0;
int sumatoriaSentenciaFinal = 0;
int cantSentenciasFinales = 0;
int sumatoriaTiemposFinales = 0;
#endif /* SAFA_H_ */
