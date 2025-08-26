LUA_ABI=5.4

CC?=gcc
CFLAGS?=-O2 -W -Wall -Wpointer-arith -Wno-unused-parameter -Werror -Isrc -I/usr/include/lua$(LUA_ABI) -D_GNU_SOURCE
LDFLAGS?=
LIBS?=-lcurl -lyyjson -llua$(LUA_ABI) -lm
SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
BIN=bootstrap
all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJ) $(BIN)

TEST_BIN=test_codec

$(TEST_BIN): test/test_codec.c src/lws_codec.c src/lws_codec.h
	$(CC) $(CFLAGS) -o $@ test/test_codec.c src/lws_codec.c

test: $(TEST_BIN)
	./$(TEST_BIN)

test_clean:
	rm -f $(TEST_BIN)

.PHONY: all clean test
