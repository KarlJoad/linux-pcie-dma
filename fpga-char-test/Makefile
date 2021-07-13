CC ?= gcc
CFLAGS += -Wall -g

RM=rm

all: test-addrs test-ioctls

test-ioctls: test-ioctls.c ioctls.h
	$(CC) $(CFLAGS) $< -o $@

test-addrs: test-addrs.c
	$(CC) $(CFLAGS) $< -o $@

install: test-addrs test-ioctls
	$(INSTALL) -D -m 0755 test-addrs $(DESTDIR)/usr/bin/test-addrs
	$(INSTALL) -D -m 0755 test-ioctls $(DESTDIR)/usr/bin/test-ioctls

clean:
	$(RM) test-addrs
