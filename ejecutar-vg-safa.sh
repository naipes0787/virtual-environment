#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/teklib/Debug
cd safa/Debug
make clean
make all
valgrind -v --leak-check=full --show-leak-kinds=all --log-file="../../valgrind-safa.log"  ./safa
