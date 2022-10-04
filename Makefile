BIN = wdc65816
SRC = $(BIN).c
OBJ = $(SRC:.c=.o)
CFLAGS = -Wall -O2 -g
CC = gcc

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *~

distclean: clean
	rm -f *.o $(BIN)
