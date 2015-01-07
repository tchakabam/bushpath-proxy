GCC_FLAGS         =
GCC_INCLUDE_FLAGS =
GCC_LIBRARY_FLAGS = -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls -lcmocka -lcurl -lgstreamer-1.0

LIB_NAME = bushpath

C_FILES = $(shell find src -type f -name "*.c")
H_FILES = $(shell find src -type f -name "*.h")

TEST_C_FILES = $(shell find tests -type f -name "*.c")

ARCHIVE_TARGET = build/lib$(LIB_NAME).a
OBJECT_TARGET = build/$(LIB_NAME).o
EXECUTABLE_TARGET = build/$(LIB_NAME)
HEADERS_TARGET = include/bushpath/api.h

ifeq ($(OSX_GSTREAMER),yes)
	GCC_INCLUDE_FLAGS=-I/Library/Frameworks/GStreamer.framework/Headers/ -L/Library/Frameworks/GStreamer.framework/Libraries/
endif

.PHONY: all build lib cli start clean tests

all: clean build

build: cli

cli: lib $(EXECUTABLE_TARGET)

$(EXECUTABLE_TARGET): $(C_FILES) $(H_FILES)
	mkdir -p build
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) $(GCC_LIBRARY_FLAGS) -L./build -lbushpath -o build/$(LIB_NAME) src/proxy_cli.c

lib: $(ARCHIVE_TARGET) $(HEADERS_TARGET)

$(ARCHIVE_TARGET): $(OBJECT_TARGET)
	mkdir -p build
	ar rcs $(ARCHIVE_TARGET) $(OBJECT_TARGET) build/tcp_client.o build/tcp_service.o build/throttler.o build/http_parser.o

$(OBJECT_TARGET): $(C_FILES) $(H_FILES)
	mkdir -p build
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) -o build/$(LIB_NAME).o -c src/proxy_api.c
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) -o build/tcp_client.o -c src/tcp_client.c
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) -o build/tcp_service.o -c src/tcp_service.c
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) -o build/throttler.o -c src/throttler.c
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) -o build/http_parser.o -c src/http_parser.c

$(HEADERS_TARGET): src/api.h
	rm -Rf include
	mkdir -p include/bushpath
	cp src/api.h $(HEADERS_TARGET)

start: build
	./$(EXECUTABLE_TARGET)

start-valgrind: build
	valgrind ./$(EXECUTABLE_TARGET)

clean:
	rm -f $(ARCHIVE_TARGET)
	rm -f $(OBJECT_TARGET)
	rm -f $(HEADERS_TARGET)

tests: build/api_test

build/connection_test: lib $(TEST_C_FILES)
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) $(GCC_LIBRARY_FLAGS) -L./build -lbushpath -o build/connection_test tests/connection_test.c
	./build/connection_test

build/api_test: lib $(TEST_C_FILES)
	gcc $(GCC_FLAGS) $(GCC_INCLUDE_FLAGS) $(GCC_LIBRARY_FLAGS) -L./build -lbushpath -o build/api_test tests/api_test.c
	./build/api_test
