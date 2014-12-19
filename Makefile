
GLIB_HEADERS_DIR = /Library/Frameworks/GStreamer.framework/Headers/
GCC_LIBRARY_FLAGS = -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls
PROXY_EXECUTABLE_BINARY_NAME = proxy

.PHONY: all build executable start clean tests

all: clean build

build: executable

executable:
	gcc -I$(GLIB_HEADERS_DIR) $(GCC_LIBRARY_FLAGS) -o $(PROXY_EXECUTABLE_BINARY_NAME) src/proxy.c

library:

start: build
	./proxy

clean:
	rm -f proxy

tests: