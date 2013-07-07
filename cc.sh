#!/bin/bash
CAR_DIR=/home/elias/programming/c_c++/car_win32/car
LIBS="-lwayland-egl -lwayland-server -lwayland-cursor -lwayland-client -lEGL -lGLESv2"

#g++ -c -Wall -I$CAR_DIR/include $CAR_DIR/src/lin_alg.cpp -o objs/lin_alg.o
#g++ -c -Wall -I$CAR_DIR/include $CAR_DIR/src/lzma/* -o objs/lzma.o
#for lzmasrcfile in $CAR_DIR/src/lzma/*.c; do
#	g++ -c -Wall -I$CAR_DIR/include $lzmasrcfile -o objs/lzma/"$(basename $lzmasrcfile)".o
#done
# ld -r objs/lzma/*.o -o objs/lzma.o

g++ -std=c++11 -fpermissive -Wall $LIBS objs/lzma.o objs/lin_alg.o -I$CAR_DIR/include simple-egl.c -o wl 
