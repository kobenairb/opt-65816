msys_version := $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)
$(info The version of MSYS you are running is $(msys_version) (0 meaning not MSYS at all))

VERSION := 1.0.0
DATESTRING := $(shell date +%Y%m%d)

CC      := gcc
CFLAGS  := -Wall -Wextra -O2 -pedantic -D__BUILD_DATE="\"$(DATESTRING)\"" -D__BUILD_VERSION="\"$(VERSION)\""
LDFLAGS := -pthread

ifeq ($(shell uname),Darwin)
	LDFLAGS += -lpcre
	EXT :=
else ifeq ($(OS),Windows_NT)
	CFLAGS  += -I/ucrt64/include
	LDFLAGS += -L/ucrt64/lib -Wl,-Bstatic -lpcre -lpcreposix -Wl,-Bdynamic
	EXT := .exe
else
	LDFLAGS += -static -lpcre -lpcreposix
	EXT :=
endif

EXE = 816-opt

SRC = src
OBJ = build

SOURCES := $(wildcard $(SRC)/*.c)
OBJECTS := $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SOURCES))

all: $(EXE)$(EXT)

$(EXE)$(EXT): $(OBJECTS) build/libpcre.a build/libpcreposix.a
	@echo "Linking $<"
	@cp /ucrt64/lib/libpcre.a build/
	@cp /ucrt64/lib/libpcreposix.a build/
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

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
	rm -rf ${OBJECTS}
	rm -f $(EXE)$(EXT)

distclean: clean
	rm -f tests/samples/*.log
	rm -rf doc/html

.PHONY: doc
