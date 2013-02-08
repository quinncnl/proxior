CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=-levent -g
SOURCES=config.c http.c logging.c match.c util.c main.c rpc.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=proxyrouter

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o proxyrouter