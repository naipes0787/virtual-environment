#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/teklib/Debug
cd dam/Debug
make clean
make all
./dam

