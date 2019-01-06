# TEK - Simulador de sistema multiprocesador

## Introducción
Simulador (Entorno virtualizado) de algunos aspectos de un sistema multiprocesador, con la capacidad de interpretar la ejecución de scripts escritos en un lenguaje detallado en el [enunciado](https://docs.google.com/document/d/1_mX1Qnzw3lO01dMwurFitsfnSV3tbx1jFo88c9JpILQ). El sistema planificará y ejecutará estos scripts controlando sus solicitudes de I/O, administrando los accesos a recursos tanto propios como compartidos.
Las partes del sistema son **SAFA** (Proceso central, planificador), **DAM** (DMA entre File System y Memoria), **MDJ** (File System), **FM9** (Memoria) y la/s **CPU/s**.

## Bash Scripts
En el proyecto se encuentran algunos bash scripts para facilitar su uso. Los mismos deben ser ejecutados en el orden mostrado:
1. **1-instalar-commons.sh:** Instala una biblioteca en el sistema, necesaria para la correcta ejecución del simulador
2. **2-compilar-proyectos.sh:** Compila cada una de las partes del sistema, junto con la biblioteca compartida

Luego, se pueden encontrar los bash scripts necesarios para iniciar cada una de las partes del sistema (Siempre y cuando antes además se haya configurado ya un file system), las cuales se deben iniciar en el siguiente orden:
1. **ejecutar-fm9.sh, ejecutar-mdj.sh, ejecutar-safa:** Estos tres en cualquier orden
2. Luego, **ejecutar-dam.sh**
3. Por último, **ejecutar-cpu.c** (Se pueden iniciar tantos procesos CPU como se requiera, para simular el multiprocesamiento)

## File System de prueba
Además, en el proyecto también se puede encontrar el directorio fifa-tek, el cual contiene un file system inicial para poder utilizar el sistema.
