# Define compiler
CC=gcc

# Define the target name
TARGET=bwtsearch

# Define where the source files are.
SRC_DIR=./
LIB_DIR=./lib/

# Define the flags for the compiler. 
# -I. adds current directory in the include path.
CFLAGS=-g -O3

# List of source files
SRC=$(SRC_DIR)main.c $(LIB_DIR)index.c $(LIB_DIR)search.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
