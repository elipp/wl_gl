#!/bin/bash

set -e

CC="g++ -std=c++11 -Wall"
LIBS="-lwayland-egl -lwayland-server -lwayland-cursor -lwayland-client -lEGL -lGLESv2 -ljpeg -lrt -lpng -llinalg -lpthread"
INCLUDE="-I/usr/local/include -Iinclude"
SOURCES="src/main.cpp src/wl_stuff.cpp src/texturewl.cpp src/timer.cpp src/mesh.cpp"
NET_SOURCES="$(find src/net -type f | grep -v 'dedicated')"

install -d objs/lzma

if [ ! -f objs/lzma.o ]; then
	for lzmasrcfile in $CAR_DIR/src/lzma/*.cpp; do
		g++ -c -Wall -I$CAR_DIR/include $lzmasrcfile -o objs/lzma/"$(basename $lzmasrcfile)".o
	done
	ld -r objs/lzma/*.o -o objs/lzma.o
fi

CLIENT_SOURCES="src/wl_stuff.cpp src/texturewl.cpp src/timer.cpp src/mesh.cpp src/shader.cpp src/common.cpp src/keyboard.cpp $NET_SOURCES src/main.cpp"
DEDICATED_SERVER_SOURCES="src/common.cpp src/timer.cpp src/keyboard.cpp $NET_SOURCES src/net/dedicated_server.cpp"
DEDICATED_CLIENT_SOURCES="src/common.cpp src/timer.cpp src/keyboard.cpp $NET_SOURCES src/net/dedicated_client.cpp"

echo "Building main client (kar)..."
$CC objs/lzma.o $INCLUDE $CLIENT_SOURCES -o kar $LIBS 
echo "done."

echo "Building dedicated server..."
$CC $INCLUDE $DEDICATED_SERVER_SOURCES -o dedicated_server -lpthread -llinalg
echo "done."

#echo "Building dedicated client..."
#$CC $INCLUDE $DEDICATED_CLIENT_SOURCES -o dedicated_client -lpthread -llinalg 
#echo "done."
