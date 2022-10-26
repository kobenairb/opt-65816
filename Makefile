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

valgrind: all
	@for file in samples/*.ps; do \
	    echo -n "Optimizing $$file "; \
	    valgrind --quiet \
		    --leak-check=full \
		    --track-origins=yes \
		    --exit-on-first-error=yes \
		    --error-exitcode=1 \
		    ./opt-65816 $${file} >/dev/null; \
		if [[ $$? -eq 0 ]]; then \
		    echo "[OK]"; \
		else \
		    echo "[KO]"; \
		    exit $$?; \
		fi \
	done

doc:
	@rm -rf doc/html
	@doxygen ./Doxyfile

clean: distclean

distclean:
	@rm -rf *.o $(BIN) doc/html

.PHONY: doc