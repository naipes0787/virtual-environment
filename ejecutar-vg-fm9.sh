#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/teklib/Debug
cd fm9/Debug
make clean
make all
valgrind -v --leak-check=full --show-leak-kinds=all --log-file="../../valgrind-fm9.log" ./fm9

