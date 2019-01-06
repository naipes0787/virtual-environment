#ifndef CPU_SRC_CPU_H_
#define CPU_SRC_CPU_H_

#include <sharedlib.h>
#include "parser.h"
#include <sys/inotify.h>

typedef struct {
	int id;
	char* escriptorio;
    int pc;
    int inicializado;
    char* archivosAbiertos;
    int ubicacion;
}Dtb;


//VARIABLES GLOBALES


Dtb* DTB;

t_log* loggerCpu;

t_config* config;
char *pathConfig;

t_socket socketSAFA;
t_socket socketDAM;
t_socket socketFM9;


//el wait en el caso de no poder ejecutarlo mandar ERROR
int quantum, ejecucionFinalizada, ejecutado, retardoDeEjecucion, buscarPath;
int error, respuestaWait, archivoAbierto, ejecutar, ejecutandoFlush, crearPath;
int codigoError = ERROR;
int borrarArchivo, inotify, configI;

char* ipSafa;
char* puertoSafa;
char* ipDam;
char* puertoDam;
char* ipFm9;
char* puertoFm9;
char** lineas;


#define EVENT_SIZE  ( sizeof (struct inotify_event) + 24 )
#define INOTIFY_BUF_LEN  ( 1024 * EVENT_SIZE )


//Prototipos

void inicializarCPU();
void inicializarLogger();
void inicializarConfig();
void liberarInotify();
void conexionesConSAFA();
void conexionesConDAM();
void conexionesConFM9();
void pruebaMensajeFM9();
void pruebaMensajeMDJ();
void recibirDTB();
void ejecutarDummy();
void avisarDAM();
void avisarDummyFinalizado();
void ejecutarProximaInstruccion();
void realizarInstruccion(t_instruccion instruccion);
bool pathAbierto(char* path);
bool puedeEjecutar();
void notificarSafa();
void retenerRecurso();
void liberarRecurso();
void traerEscriptorio();
void globalesEnCero();
void inicializarInotify();
void liberarTodosLosArchivos();
void* funcionalidadInotify(void* arg);
void enviarInfoBloqueado(int razonBloqueo);
int obtenerUbicacion(char* path);
int cerrarEscriptorioEnFm9(int idDtb, int ubicacion);


#endif /* CPU_SRC_CPU_H_ */
