CC=clang
CFLAGS=-c -g -O0 -Wall -Wextra -Wconversion -Wno-unused-parameter -DDEBUG
LIBS=
EXECUTABLE=jullop
LDFLAGS=
SOURCES=main.c request_context.c event_loop.c
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(LIBS) $< -o $@

clean:
	rm *.o
