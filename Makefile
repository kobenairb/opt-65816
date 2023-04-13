CC = gcc
CFLAGS := -Wall -Wextra -O2 -g -pedantic
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
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c
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
	rm -rf ${OBJECTS}
	rm -f $(BIN)

distclean: clean
	rm -f tests/samples/*.log
	rm -rf doc/html

.PHONY: doc
