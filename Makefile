GCC_FLAGS=
GCC_LIBRARY_FLAGS = -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lgnutls

LIB_NAME = bushpath

C_FILES = $(shell find src -type f -name "*.c")
H_FILES = $(shell find src -type f -name "*.h")

ARCHIVE_TARGET = build/lib$(LIB_NAME).a
OBJECT_TARGET = build/$(LIB_NAME).o
EXECUTABLE_TARGET = build/$(LIB_NAME)
HEADERS_TARGET = include/bushpath/api.h

ifeq ($(OSX_GSTREAMER),yes)
	GCC_FLAGS=-I/Library/Frameworks/GStreamer.framework/Headers/ -L/Library/Frameworks/GStreamer.framework/Libraries/
endif

.PHONY: all build lib cli start clean tests

all: clean build

build: cli

cli: $(EXECUTABLE_TARGET) library

$(EXECUTABLE_TARGET): $(C_FILES) $(H_FILES)
	mkdir -p build
	gcc $(GCC_FLAGS) $(GCC_LIBRARY_FLAGS) -L./build -lbushpath -o build/$(LIB_NAME) src/proxy_cli.c

lib: $(ARCHIVE_TARGET) $(HEADERS_TARGET)

$(ARCHIVE_TARGET): $(OBJECT_TARGET)
	mkdir -p build
	ar rcs $(ARCHIVE_TARGET) $(OBJECT_TARGET)

$(OBJECT_TARGET): $(C_FILES) $(H_FILES)
	mkdir -p build
	gcc $(GCC_FLAGS) -o build/$(LIB_NAME).o -c src/proxy_api.c

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

tests: