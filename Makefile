CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -O2
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE -Iinclude
LDFLAGS ?=
LDLIBS ?= -lcurl -lm

TARGET := flight
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
LIB_OBJ := $(filter-out src/main.o,$(OBJ))
TEST_TARGET := tests/test_core
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
INSTALL ?= install

.PHONY: all clean run test install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

src/airport_reference.o: data/airports_generated.inc

run: $(TARGET)
	./$(TARGET) QF9

$(TEST_TARGET): tests/test_core.c $(LIB_OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ tests/test_core.c $(LIB_OBJ) $(LDLIBS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

install: $(TARGET)
	$(INSTALL) -d "$(DESTDIR)$(BINDIR)"
	$(INSTALL) -m 755 $(TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"

clean:
	rm -f $(OBJ) $(TARGET) $(TEST_TARGET)
