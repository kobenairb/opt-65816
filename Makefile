BIN = opt-65816
SRC = $(BIN).c
OBJ = $(SRC:.c=.o)
CFLAGS = -Wall -O2 -g
CC = gcc

all: clean $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean: distclean

distclean:
	rm -f *.o $(BIN)
