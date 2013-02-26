PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

G=-g
CC=gcc
WALL=-Wall
CFLAGS=-c $(WALL) $(G)
LDFLAGS=-levent $(G)
SOURCES=config.c http.c logging.c match.c util.c main.c rpc.c hashmap.c socks.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=proxior

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) $< -o $@

install: all
	mkdir -p $(TARGET)$(BINDIR)
	rm -f $(TARGET)$(BINDIR)/proxior	
	cp -f proxior $(TARGET)$(BINDIR)/
	mkdir /etc/proxior
	cp -f gaelist sshlist proxior.conf /etc/proxior

clean:
	rm *.o proxior

uninstall:
	rm -f $(TARGET)$(BINDIR)/proxior
	rm -f /etc/proxior/*
	rmdir /etc/proxior/