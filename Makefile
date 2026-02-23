CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11
LDFLAGS =

TARGET = xsh
SRCDIR = src
SRCS = $(SRCDIR)/xsh.c

PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: $(TARGET)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	@echo "XSH installed to $(BINDIR)/$(TARGET)"
	@echo "To use as login shell: chsh -s $(BINDIR)/$(TARGET)"
	@echo "Add to /etc/shells: echo '$(BINDIR)/$(TARGET)' >> /etc/shells"

uninstall:
	rm -f $(BINDIR)/$(TARGET)

clean:
	rm -f $(TARGET)
