CC = gcc
CFLAGS = -Wall -Wformat=2 -Wshadow -Wconversion -std=gnu11
RM = rm

.PHONY: all clean

all: slow

clean:
	$(RM) slow

slow: slow.c
	$(CC) $(CFLAGS) -o $@ $^
	sudo setcap "CAP_NET_RAW+ep" $@
