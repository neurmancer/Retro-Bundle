CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
RAYLIB_CFLAGS := $(shell $(PKG_CONFIG) --cflags raylib)
RAYLIB_LIBS := $(shell $(PKG_CONFIG) --libs raylib)

GAME_SOURCES := $(wildcard games/*.c games/*/*.c)
GAME_NAMES := $(basename $(notdir $(GAME_SOURCES)))
BINARIES := $(addprefix bins/,$(GAME_NAMES))
SHARED_SOURCES := $(wildcard shared/*.c)

.PHONY: all clean
all: $(BINARIES)

bins:
	mkdir -p $@

vpath %.c games $(sort $(dir $(wildcard games/*/*.c)))

bins/%: %.c $(SHARED_SOURCES) | bins
	$(CC) $(CPPFLAGS) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) $< $(if $(filter galaga,$*),physics/doppler.c) -o $@ $(RAYLIB_LIBS) $(LDLIBS) -lm

bins/galaga: physics/doppler.c physics/doppler.h

clean:
	rm -f $(BINARIES)
