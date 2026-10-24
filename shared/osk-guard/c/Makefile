# osk-guard, C-Fassung. Ziele: all, test, clean.
#
# Gebaut wird mit gcc (auf Windows MinGW, sonst der des Systems). Ohne
# Fremdcode: nur die Standardbibliothek und die Sockets des Systems.
#
#     mingw32-make            baut Beispiel und Prueflauf
#     mingw32-make test       laesst den Prueflauf laufen
#     mingw32-make clean      raeumt Erzeugtes weg

CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O2
VECTORS  ?= ../tests/vectors.json
LICENSES ?= ../tests/license-vectors.json

# Auf Windows liegen die Sockets in ws2_32 und der Zufall in bcrypt.
ifeq ($(OS),Windows_NT)
  LDLIBS = -lws2_32 -lbcrypt
  EXE    = .exe
else
  LDLIBS =
  EXE    =
endif

LIB_OBJ  = osk_guard.o sha2.o ed25519_verify.o
TEST_OBJ = test_guard.o osk_guard.o sha2.o ed25519_verify.o

TARGETS = example$(EXE) test_guard$(EXE)

.PHONY: all test clean

all: $(TARGETS)

example$(EXE): example.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

test_guard$(EXE): $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

osk_guard.o: osk_guard.c osk_guard.h sha2.h ed25519_verify.h
sha2.o: sha2.c sha2.h
ed25519_verify.o: ed25519_verify.c ed25519_verify.h sha2.h
example.o: example.c osk_guard.h
test_guard.o: test_guard.c osk_guard.h sha2.h ed25519_verify.h

test: test_guard$(EXE)
	./test_guard$(EXE) $(VECTORS) $(LICENSES)

clean:
	-rm -f *.o example example.exe test_guard test_guard.exe
