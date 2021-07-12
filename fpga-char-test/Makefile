CC ?= gcc
CFLAGS += -Wall -g

RM=rm

all: test-addrs

test-addrs: test-addrs.c
	$(CC) $(CFLAGS) $< -o $@

install: test-addrs
	$(INSTALL) -D -m 0755 test-addrs $(DESTDIR)/usr/bin/test-addrs

clean:
	$(RM) test-addrs
