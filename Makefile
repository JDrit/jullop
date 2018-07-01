CC=clang
CFLAGS=-c -g -Wall -Wextra -Wconversion -DDEBUG
LIBS=
EXECUTABLE=jullop
LDFLAGS=
SOURCES=main.c request_context.c
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(LIBS) $< -o $@

clean:
	rm *.o
