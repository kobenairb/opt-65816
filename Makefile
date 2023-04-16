VERSION := 1.0.0
DATESTRING := $(shell date +%Y%m%d)

CC := gcc
CFLAGS += -Wall -Wextra -O2 -g -pedantic -D__BUILD_DATE="\"$(DATESTRING)\"" -D__BUILD_VERSION="\"$(VERSION)\""
LDFLAGS :=

ifeq ($(shell uname),Darwin)
	LDFLAGS += -lpcre
else
	LDFLAGS += -lpcreposix -lpcre
endif

EXE = 816-tcc-opt

SRC = src
OBJ = build

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

ifeq ($(OS),Windows_NT)
	EXT=.exe
else
	EXT=
endif

all: $(EXE)$(EXT)

$(EXE)$(EXT): $(OBJECTS)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -I$(SRC) -c $< -o $@ $(LDFLAGS)

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
	rm -rf ${OBJECTS}
	rm -f $(BIN)

distclean: clean
	rm -f tests/samples/*.log
	rm -rf doc/html

.PHONY: doc
