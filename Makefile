CC=clang
IGNORE_WARNINGS=-Wno-unused-parameter -Wno-unused-function
CFLAGS=-std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion -DDEBUG $(IGNORE_WARNINGS)
LIBS=-lpthread
EXECUTABLE=jullop
LDFLAGS=
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) -g $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
