CC=clang
IGNORE_WARNINGS=-Wno-unused-parameter -Wno-unused-function
CFLAGS=-std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion \
	-fno-builtin-malloc -fno-builtin-calloc -fno-builtin-realloc -fno-builtin-free \
$(IGNORE_WARNINGS)
LIBS=-lpthread -lprofiler -ltcmalloc
EXECUTABLE=jullop
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
