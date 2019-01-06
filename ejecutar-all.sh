#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/teklib/Debug

lxterminal -e "./ejecutar-safa.sh & sleep 5" & lxterminal -e "./ejecutar-mdj.sh" & lxterminal -e "./ejecutar-fm9.sh"
sleep 2
lxterminal -e "./ejecutar-dam.sh" 
sleep 2
lxterminal -e "./ejecutar-cpu.sh"
