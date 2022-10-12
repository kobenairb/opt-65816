BIN = opt-65816
SRC = $(BIN).c
OBJ = $(SRC:.c=.o)
CFLAGS = -Wall -Wextra -O2 -g
CC = gcc

all: clean $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

docs:
	@doxygen ./Doxyfile

clean: distclean

distclean:
	@rm -rf *.o $(BIN) doc/html

.PHONY: doc