CC=clang
CFLAGS=-c -g -Wall -Wextra -DDEBUG
LIBS=
EXECUTABLE=jullop
LDFLAGS=
SOURCES=main.c
OBJECTS=$(SOURCES:.c=.o)

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(LIBS) $(OBJECTS) -o $@

.c.o:
	$(CC) $(CFLAGS) $(LIBS) $< -o $@

clean:
	rm *.o
