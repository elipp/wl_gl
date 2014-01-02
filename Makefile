CXX := g++ -Wall -g -march=native -std=c++11 
CFLAGS := -c
SRCDIR := src
OBJDIR := objs
INCLUDES := -Iinclude -I/usr/local/include
COMMON_LIBS := -llinalg -lpthread
WAYLAND_LIBS := -lwayland-egl -lwayland-server -lwayland-cursor -lwayland-client -lEGL -lGLESv2 -ljpeg -lrt -lpng 

COMMON_SOURCES := $(SRCDIR)/keyboard.cpp \
	$(SRCDIR)/common.cpp \
	$(SRCDIR)/timer.cpp \
	$(SRCDIR)/mesh.cpp \
	$(SRCDIR)/wl_stuff.cpp \
	$(SRCDIR)/shader.cpp \
	$(SRCDIR)/texturewl.cpp \
	$(SRCDIR)/car.cpp \
	$(SRCDIR)/text.cpp \
	$(SRCDIR)/linux_input_h_keychar_map.cpp

LZMA_SOURCES := $(SRCDIR)/lzma/7zStream.cpp \
	$(SRCDIR)/lzma/Alloc.cpp \
	$(SRCDIR)/lzma/7zFile.cpp \
	$(SRCDIR)/lzma/LzmaDec.cpp \
	$(SRCDIR)/lzma/LzmaUtil.cpp 

NET_SOURCES := $(SRCDIR)/net/client.cpp \
	      $(SRCDIR)/net/protocol.cpp \
	      $(SRCDIR)/net/socket.cpp \
	      $(SRCDIR)/net/taskthread.cpp \
	      $(SRCDIR)/net/server.cpp \
	      $(SRCDIR)/net/client_funcs.cpp 

ALL_SOURCES := $(COMMON_SOURCES) $(LZMA_SOURCES) $(NET_SOURCES)

OBJECTS=$(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(COMMON_SOURCES) $(LZMA_SOURCES))
NETOBJS=$(patsubst $(SRCDIR)/net/%.cpp, $(OBJDIR)/net/%.o, $(NET_SOURCES))

DEDICATED_SERVER_DEPS := $(NETOBJS) $(OBJDIR)/keyboard.o $(OBJDIR)/common.o $(OBJDIR)/timer.o

all: kar dedicated_server 

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CFLAGS) $(INCLUDES) $(COMMON_LIBS) $< -o $@ 	

kar: $(OBJECTS) $(NETOBJS) $(ALL_SOURCES) $(SRCDIR)/main.cpp
	$(CXX) $(OBJECTS) $(NETOBJS) $(SRCDIR)/main.cpp -o $@ $(INCLUDES) $(COMMON_LIBS) $(WAYLAND_LIBS)

dedicated_server: $(DEDICATED_SERVER_DEPS) $(ALL_SOURCES) $(SRCDIR)/net/dedicated_server.cpp
	$(CXX) $(DEDICATED_SERVER_DEPS) $(SRCDIR)/net/dedicated_server.cpp -o $@ $(INCLUDES) $(COMMON_LIBS)
	

clean:
	rm -fR objs 
	rm -f kar dedicated_server
