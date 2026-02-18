CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -D_GNU_SOURCE -O2
LDFLAGS = -lcurl -ljson-c -lpthread -lssl -lcrypto /lib64/libsqlite3.so.0

SRCDIR  = src
INCDIR  = include
BUILDDIR = build

SRC = $(shell find $(SRCDIR) -name '*.c')
OBJ = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRC))
BIN = uva-proxy

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR) $(BIN)
