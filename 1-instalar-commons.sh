#!/bin/bash
# Ejecutar desde dentro del directorio raíz del TP
cd ..
git clone https://github.com/sisoputnfrba/so-commons-library.git
cd so-commons-library
make
sudo make install
ls -l /usr/include/commons
