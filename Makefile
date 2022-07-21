CC = gcc
CFLAGS = -Wall -Wformat=2 -Wshadow -Wconversion -std=gnu11 -pthread -Ofast

basedir = $(shell pwd)/

build_path = $(basedir)/build/
include_path = $(basedir)/include/
source_path = $(basedir)/src/

elf_name = slow

sources = $(shell ls $(source_path) 2>/dev/null)
modules = $(sources:%.c=%.c.o)

# -----------------------------------------------------------------------

MAKEFLAGS += --jobs=$(shell nproc)
MAKEFLAGS += --output-sync=target

vpath %.c   $(source_path)
vpath %.h   $(include_path)

vpath %.o   $(build_path)

.PHONY: all cap clean

# -----------------------------------------------------------------------

all: $(elf_name)

clean:
	rm -f $(elf_name)
	rm -f $(build_path)/*.o

debug: CFLAGS += -g
debug: CXXFLAGS += -g
debug: all

# -----------------------------------------------------------------------

cap: $(elf_name)
	sudo setcap "CAP_NET_RAW+ep" $(elf_name)

# -----------------------------------------------------------------------

semaphore.c.o: semaphore.c semaphore.h
bounded_buffer.c.o: bounded_buffer.c bounded_buffer.h semaphore.h

# -----------------------------------------------------------------------

$(elf_name): $(modules)
	cd $(build_path) \
	&& $(CC) $(CFLAGS) -o $(basedir)/$@ $^ $(libraries)

%.c.o: %.c
	$(CC) $(CFLAGS) -c -I$(include_path) $< -o $(build_path)/$@
