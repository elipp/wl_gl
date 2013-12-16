#!/bin/bash

set -e

LIBDIR="-L/usr/local/lib"
LIBS="-lwayland-egl -lwayland-server -lwayland-cursor -lwayland-client -lEGL -lGLESv2 -ljpeg -lrt -lpng -llinalg -lpthread"
INCLUDE="-I/usr/local/include -Iinclude"
SOURCES="src/main.cpp src/wl_stuff.cpp src/texturewl.cpp src/timer.cpp src/mesh.cpp"

install -d objs/lzma

#g++ -c -Wall -I$CAR_DIR/include $CAR_DIR/src/lin_alg.cpp -o objs/lin_alg.o

if [ ! -f objs/lzma.o ]; then
	for lzmasrcfile in $CAR_DIR/src/lzma/*.cpp; do
		g++ -c -Wall -I$CAR_DIR/include $lzmasrcfile -o objs/lzma/"$(basename $lzmasrcfile)".o
	done
	ld -r objs/lzma/*.o -o objs/lzma.o
fi

#g++ -std=c++11 -g -Wall objs/lzma.o $INCLUDE $SOURCES -o kar $LIBS $LIBDIR 

g++ -std=c++11 -g -Wall $INCLUDE \
src/net/socket.cpp src/common.cpp src/net/taskthread.cpp src/timer.cpp src/net/server.cpp \
src/net/protocol.cpp src/net/dedicated_server.cpp -lpthread -o dedicated_server 

g++ -std=c++11 -g -Wall $INCLUDE \
src/net/socket.cpp src/common.cpp src/net/taskthread.cpp src/timer.cpp src/net/server.cpp \
src/net/protocol.cpp src/net/client.cpp src/net/client_funcs.cpp src/net/dedicated_client.cpp \
-lpthread -llinalg -o dedicated_client
