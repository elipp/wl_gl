#!/bin/bash

set -e

CAR_DIR=/home/elias/prog/kar
LIBS="-lwayland-egl -lwayland-server -lwayland-cursor -lwayland-client -lEGL -lGLESv2 -ljpeg -lrt -lpng"
INCLUDE="-I$CAR_DIR/include -Iinclude"
SOURCES="$(find src -type f)"

mkdir -p objs
mkdir -p objs/lzma

#g++ -c -Wall -I$CAR_DIR/include $CAR_DIR/src/lin_alg.cpp -o objs/lin_alg.o

#for lzmasrcfile in $CAR_DIR/src/lzma/*.cpp; do
#	g++ -c -Wall -I$CAR_DIR/include $lzmasrcfile -o objs/lzma/"$(basename $lzmasrcfile)".o
#done

#ld -r objs/lzma/*.o -o objs/lzma.o

g++ -std=c++11 -g -Wall $LIBS objs/lzma.o objs/lin_alg.o $INCLUDE $SOURCES -o kar
