#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP

### Compilación TEKLIB ###
mkdir -p teklib/Debug/src
cp teklib/make/* teklib/Debug
gcc -Ipthread -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"teklib/Debug/src/sharedlib.d" -MT"teklib/Debug/src/sharedlib.o" -o "teklib/Debug/sharedlib.o" "teklib/sharedlib.c"
gcc -shared -o "teklib/Debug/libteklib.so"  ./teklib/Debug/sharedlib.o   -lcommons -lpthread
### Fin Compilación TEKLIB ###


### Compilación SAFA ###
mkdir -p safa/Debug/src
cp safa/make/* safa/Debug
mv safa/Debug/subdir.mk safa/Debug/src
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"safa/Debug/src/safa.d" -MT"safa/Debug/src/safa.o" -o "safa/Debug/src/safa.o" "safa/src/safa.c"
gcc -L"teklib/Debug" -o "safa/Debug/safa" ./safa/Debug/src/safa.o -lteklib -lpthread -lreadline -lcommons
### Fin Compilación SAFA ###

### Compilación MDJ ###
mkdir -p mdj/Debug/src
cp mdj/make/* mdj/Debug
mv mdj/Debug/subdir.mk mdj/Debug/src
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"mdj/Debug/src/mdj.d" -MT"mdj/Debug/src/mdj.o" -o "mdj/Debug/src/mdj.o" "mdj/src/mdj.c"
gcc -L"teklib/Debug" -o "mdj/Debug/mdj" ./mdj/Debug/src/mdj.o -lteklib -lrt -lcrypto -lpthread -lssl -lreadline -lcommons
### Fin Compilación MDJ ###


### Compilación FM9 ###
mkdir -p fm9/Debug/src
cp fm9/make/* fm9/Debug
mv fm9/Debug/subdir.mk fm9/Debug/src
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -std=c99 -MMD -MP -MF"fm9/Debug/src/fm9.d" -MT"fm9/Debug/src/fm9.o" -o "fm9/Debug/src/fm9.o" "fm9/src/fm9.c"
gcc -L"teklib/Debug" -o "fm9/Debug/fm9" ./fm9/Debug/src/fm9.o -lteklib -lcommons -lpthread -lreadline
### Fin Compilación FM9 ###

### Compilación DAM ###
mkdir -p dam/Debug/src
cp dam/make/* dam/Debug
mv dam/Debug/subdir.mk dam/Debug/src
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"dam/Debug/src/dam.d" -MT"dam/Debug/src/dam.o" -o "dam/Debug/src/dam.o" "dam/src/dam.c"
gcc -L"teklib/Debug" -o "dam/Debug/dam" ./dam/Debug/src/dam.o -lteklib -lpthread -lcommons
### Fin Compilación DAM ###

### Compilación CPU ###
mkdir -p cpu/Debug/src
cp cpu/make/* cpu/Debug
mv cpu/Debug/subdir.mk cpu/Debug/src
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"cpu/Debug/src/cpu.d" -MT"cpu/Debug/src/cpu.o" -o "cpu/Debug/src/cpu.o" "cpu/src/cpu.c"
gcc -I"teklib" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"cpu/Debug/src/parser.d" -MT"cpu/Debug/src/parser.o" -o "cpu/Debug/src/parser.o" "cpu/src/parser.c"
gcc -L"teklib/Debug" -o "cpu/Debug/cpu" ./cpu/Debug/src/cpu.o ./cpu/Debug/src/parser.o -lteklib -lpthread -lcommons
### Fin Compilación CPU ###

echo "Los proyectos fueron compilados correctamente!"
