CC=clang
IGNORE_WARNINGS=-Wno-unused-parameter -Wno-unused-function
CFLAGS=-std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion \
	-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free \
	$(IGNORE_WARNINGS)
LIBS=-lpthread -lprofiler -ltcmalloc

EXECUTABLE=jullop
SOURCES=$(filter-out $(wildcard *test.c), $(wildcard *.c))
OBJECTS=$(SOURCES:.c=.o)

MAILBOX_TEST=mailbox_test
TEST_SOURCES=$(filter-out main.c, $(wildcard *.c))
TEST_OBJECTS=$(TEST_SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE) $(MAILBOX_TEST)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

$(MAILBOX_TEST): $(TEST_OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(TEST_OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
