# Define version and build date strings
VERSION    := 1.0.0
DATESTRING := $(shell date +%Y%m%d)

# Compiler and compiler flags
CC      := gcc
CFLAGS  += -Wall -O2 -pedantic -DPCRE2_STATIC -D__BUILD_DATE="\"$(DATESTRING)\"" -D__BUILD_VERSION="\"$(VERSION)\""
LDFLAGS := -lpthread

# Define the libraries and compilation flags to be used depending on the OS.
ifeq ($(shell uname),Darwin)
    LDFLAGS := -pthread -lpcre
    CFLAGS  += -I/usr/local/include
    EXT     :=
else ifeq ($(OS),Windows_NT)
    LDFLAGS += -lpthread -static -lpcre2-posix -lpcre2-8 -liconv
    EXT     :=.exe
else
    LDFLAGS += -lpthread -static -lpcreposix -lpcre
    EXT     :=
endif

# Source and object file locations
SRC := src
OBJ := build
SOURCES := $(wildcard $(SRC)/*.c)
OBJS    := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

# Executable binary file name
EXE := 816-opt

# Define the default target
all: $(EXE)$(EXT)

# Define the recipe for linking the executable
$(EXE)$(EXT): $(OBJS)
	@echo "Linking $<"
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

# Define the recipe for compiling object files
%.o : %.c
$(OBJ)/%.o: $(SRC)/%.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -I$(SRC) -c $< -o $@

ifneq ($(OS),Windows_NT)
valgrind: all
	@./tests/memcheck.sh

cppcheck:
	cppcheck $(SOURCES)
endif

tests: all
	@./tests/idempotent.sh

doc:
	@rm -rf doc/html
	@doxygen ./Doxyfile

clean:
	rm -rf ${OBJS}
	rm -f $(EXE)$(EXT)

distclean: clean
	rm -f tests/samples/*.log
	rm -rf doc/html

.PHONY: all clean install doc
