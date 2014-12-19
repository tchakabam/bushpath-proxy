GCC_FLAGS=
GCC_LIBRARY_FLAGS = -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls

LIB_NAME = bushpath

ifeq ($(OSX_GSTREAMER),yes)
	GCC_FLAGS=-I/Library/Frameworks/GStreamer.framework/Headers/ -L/Library/Frameworks/GStreamer.framework/Libraries/
endif

.PHONY: all build executable start clean tests

all: clean build

build: library executable

executable:
	gcc $(GCC_FLAGS) $(GCC_LIBRARY_FLAGS) -L. -lbushpath -o $(LIB_NAME) src/proxy_cli.c

library:
	gcc $(GCC_FLAGS) -o $(LIB_NAME).o -c src/proxy_api.c
	ar rcs lib$(LIB_NAME).a $(LIB_NAME).o
	rm -Rf include
	mkdir -p include/bushpath
	cp src/api.h include/bushpath/api.h

start: build
	./$(LIB_NAME)

start-valgrind: build
	valgrind ./$(LIB_NAME)

clean:
	rm -f $(LIB_NAME).o
	rm -f lib$(LIB_NAME).a
	rm -f $(LIB_NAME)

tests: