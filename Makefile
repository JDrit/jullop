CC=clang
CFLAGS=-std=gnu11 -c -g -O0 -Wall -Wextra -Wconversion -Wno-unused-parameter -DDEBUG
LIBS=-lpthread
EXECUTABLE=jullop
LDFLAGS=
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o
