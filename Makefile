CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

BIN = 816-tcc-opt

SRC = src
OBJ = build

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

all: clean $(BIN)

$(BIN): $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -I$(SRC) -c $< -o $@

valgrind: all
	@./tests/memcheck.sh

tests: all
	@./tests/idempotent.sh

cppcheck:
	cppcheck $(SOURCES)

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
