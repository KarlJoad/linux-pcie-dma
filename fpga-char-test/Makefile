CC ?= gcc
CFLAGS += -Wall -g

RM=rm

all: test-addrs test-ioctls test-give-virtine

test-ioctls: test-ioctls.c ioctls.h
	$(CC) $(CFLAGS) $< -o $@

test-addrs: test-addrs.c
	$(CC) $(CFLAGS) $< -o $@

test-give-virtine: test-give-virtine.c
	$(CC) $(CFLAGS) $< -o $@

install: test-addrs test-ioctls test-give-virtine
	$(INSTALL) -D -m 0755 test-addrs $(DESTDIR)/usr/bin/test-addrs
	$(INSTALL) -D -m 0755 test-ioctls $(DESTDIR)/usr/bin/test-ioctls
	$(INSTALL) -D -m 0755 test-give-virtine $(DESTDIR)/usr/bin/test-give-virtine

clean:
	$(RM) test-addrs
