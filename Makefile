CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

BIN = opt-65816
SRC = $(BIN).c
OBJ = $(SRC:.c=.o)

all: clean $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

docs:
	@rm -rf doc/html
	@doxygen ./Doxyfile

clean: distclean

distclean:
	@rm -rf *.o $(BIN) doc/html

.PHONY: doc