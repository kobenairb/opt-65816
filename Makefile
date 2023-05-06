# Define version and build date strings
VERSION    := 1.0.0
DATESTRING := $(shell date +%Y%m%d)

# Compiler and linker flags
CC      = gcc
CFLAGS  += -Wall -O2 -pedantic -D__BUILD_DATE="\"$(DATESTRING)\"" -D__BUILD_VERSION="\"$(VERSION)\""
LDFLAGS := -lpthread

# Define the libraries and compilation flags to be used depending on the OS.
ifeq ($(shell uname),Darwin)
	EXT     :=
else ifeq ($(OS),Windows_NT)
# 	ifeq ($(MSYSTEM),MINGW64)
# 		LIB_PATH = -L/mingw64/lib
# 	else ifeq ($(MSYSTEM),MINGW32)
# 		LIB_PATH = -L/mingw32/lib
# 	else ifeq ($(MSYSTEM),UCRT64)
# 		LIB_PATH = -L/ucrt64/lib
# 	else ifeq ($(MSYSTEM),UCRT32)
# 		LIB_PATH = -L/ucrt32/lib
# 	else
# 		$(error PLATFORM is supported or not tested, please choose one the following compilation toolchain: mingw32, mingw64, ucrt32 or ucrt64)
# 	endif
# LDFLAGS += -static -L$(LIB_PATH) -lregex -ltre -lintl -liconv
	LDFLAGS += -static -lregex -ltre -lintl -liconv
	EXT     := .exe
else
	LDFLAGS += -static
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
