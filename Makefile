SHELL = /bin/sh
CC    = gcc
PKGCONFIG = pkg-config

CFLAGS      += -Wall
CFLAGS      += $(shell $(PKGCONFIG) --cflags dbus-1)
LDFLAGS      = $(shell $(PKGCONFIG) --libs dbus-1)
 
TARGET  = monitor_state
OBJECTS = monitor_state.o
 
PREFIX = $(DESTDIR)/usr
BINDIR = $(PREFIX)/bin
 
 
all: $(TARGET)
 
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS)

install: $(TARGET)
	install -D $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	-rm $(BINDIR)/$(TARGET)
 
clean:
	-rm -f $(OBJECTS)
 
distclean: clean
	-rm -f $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY : all install uninstall clean distclean
