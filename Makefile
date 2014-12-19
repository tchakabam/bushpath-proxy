GCC_FLAGS=
GCC_LIBRARY_FLAGS = -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls

PROXY_EXECUTABLE_NAME = proxy

ifeq ($(OSX_GSTREAMER),yes)
	GCC_FLAGS=-I/Library/Frameworks/GStreamer.framework/Headers/ -L/Library/Frameworks/GStreamer.framework/Libraries/
endif

.PHONY: all build executable start clean tests

all: clean build

build: executable

executable:
	gcc $(GCC_FLAGS) $(GCC_LIBRARY_FLAGS) -o $(PROXY_EXECUTABLE_NAME) src/proxy.c

library:

start: build
	./proxy

clean:
	rm -f proxy

tests: