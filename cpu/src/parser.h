#ifndef PARSER_H_
#define PARSER_H_

#include <sharedlib.h>
#include <commons/string.h>


/*
 * @NAME: t_instruccion
 * @DESC: el struct tiene una operacion, el enum lo hago para un futuro switch.
 * la union me permite "unir" a esto un struct atributos que va a ir variando dependiendo
 * de la operacion leida
 * lo que tiene dentro el struct t_instrucion es un operacion y un atributos.
 */
typedef struct {
		enum {
			ignorar,
			fin,
			abrir,
			concentrar,
			asignar,
			wait,
			signal,
			flush,
			cerrar,
			crear,
			borrar,
			nada
		} operacion;
		union {
			struct {
				char* path;
			} abrir;
			struct {
				char* path;
				int linea;
				char* datos;
			} asignar;
			struct {
				char* recurso;
			} wait;
			struct {
				char* recurso;
			} signal;
			struct {
				char* path;
			} flush;
			struct {
				char* path;
			} cerrar;
			struct {
				char* path;
				int lineas;
			} crear;
			struct {
				char* path;
			} borrar;
		} argumentos;
	} t_instruccion;


t_instruccion parsear(char* linea);


#endif /* PARSER_H_ */
