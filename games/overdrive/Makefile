CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
RAYLIB_LIBS := $(shell $(PKG_CONFIG) --libs raylib)

.PHONY: all clean test
all: bin/overdrive

bin:
	mkdir -p $@

bin/overdrive: overdrive.c $(wildcard code/*.c) ../../physics/doppler.c ../../physics/doppler.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) $< ../../physics/doppler.c -o $@ $(RAYLIB_LIBS) $(LDLIBS) -lm

bin/handling_test: tests/handling_test.c overdrive.c $(wildcard code/*.c) ../../physics/doppler.c ../../physics/doppler.h | bin
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) $< ../../physics/doppler.c -o $@ $(RAYLIB_LIBS) $(LDLIBS) -lm

test: bin/handling_test
	./bin/handling_test

clean:
	rm -f bin/overdrive bin/handling_test
