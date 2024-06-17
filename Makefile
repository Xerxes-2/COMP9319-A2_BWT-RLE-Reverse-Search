# Detect the OS
UNAME_S := $(shell uname -s)

# Choose the compiler based on the OS
ifeq ($(UNAME_S),Linux)
    CC=gcc
endif
ifeq ($(UNAME_S),Darwin)  # Darwin is for macOS
    CC=clang
endif

# Define the target name
TARGET=bwtsearch

# Define where the source files are.
SRC_DIR=./
LIB_DIR=./lib/

# Define the flags for the compiler.
# -I. adds current directory in the include path.
RELEASE_CFLAGS=-g -O3 -march=native
DEBUG_CFLAGS=-g -O0 -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-label -Wno-unused-result -Wno-unused-const-variable -Wno-unused-local-typedefs

# List of source files
SRC=$(SRC_DIR)main.c $(LIB_DIR)index.c $(LIB_DIR)search.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(RELEASE_CFLAGS) -o $(TARGET) $(SRC)

debug: $(SRC)
	$(CC) $(DEBUG_CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
