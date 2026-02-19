# Cross-platform Makefile for uva-proxy
# Detects OS and selects the correct platform source file.

UNAME := $(shell uname -s 2>/dev/null || echo Windows)

CC       = gcc
CFLAGS   = -Wall -Wextra -std=c99 -O2
BUILDDIR = build
INCDIR   = include
BIN      = uva-proxy

# Platform detection
ifeq ($(UNAME),Linux)
  CFLAGS      += -D_GNU_SOURCE -DPLATFORM_LINUX
  PLATFORM_SRC = src/platform/linux.c
  LDFLAGS      = -lcurl -ljson-c -lssl -lcrypto -lpthread -lz -ldl -lm
  ifeq ($(STATIC),1)
    LDFLAGS = -static -lcurl -ljson-c -lssl -lcrypto -lpthread -lz -ldl -lm
  endif
endif
ifeq ($(UNAME),Darwin)
  CFLAGS      += -D_GNU_SOURCE -DPLATFORM_MACOS
  PLATFORM_SRC = src/platform/macos.c
  LDFLAGS      = -lcurl -ljson-c -lssl -lcrypto -lpthread -lz -lm
  # Homebrew paths
  ifneq ($(wildcard /opt/homebrew/include/.),)
    CFLAGS  += -I/opt/homebrew/include
    LDFLAGS += -L/opt/homebrew/lib
  else ifneq ($(wildcard /usr/local/include/.),)
    CFLAGS  += -I/usr/local/include
    LDFLAGS += -L/usr/local/lib
  endif
endif
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
  CC           = gcc
  CFLAGS      += -DPLATFORM_WINDOWS
  PLATFORM_SRC = src/platform/windows.c
  LDFLAGS      = -static -lcurl -ljson-c -lssl -lcrypto -lws2_32 -lz -lm
  BIN          = uva-proxy.exe
endif
ifeq ($(findstring MSYS,$(UNAME)),MSYS)
  CC           = gcc
  CFLAGS      += -DPLATFORM_WINDOWS
  PLATFORM_SRC = src/platform/windows.c
  LDFLAGS      = -static -lcurl -ljson-c -lssl -lcrypto -lws2_32 -lz -lm
  BIN          = uva-proxy.exe
endif

# Discover all .c files except platform-specific ones, then add common + target
SRC = $(shell find src/ -name '*.c' ! -path 'src/platform/*') \
      src/platform/common.c $(PLATFORM_SRC)

OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -I$(INCDIR) -c -o $@ $<

clean:
	rm -rf $(BUILDDIR) $(BIN) uva-proxy.exe
