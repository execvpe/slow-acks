CC = gcc
CFLAGS = -Wall -Wformat=2 -Wshadow -Wconversion -std=gnu11 -Ofast
RM = rm

.PHONY: all cap clean nocap

elf_name = slow

all: cap

clean:
	$(RM) $(elf_name)

cap: $(elf_name)
	sudo setcap "CAP_NET_RAW+ep" $(elf_name)

nocap: $(elf_name)

$(elf_name): $(elf_name).c
	$(CC) $(CFLAGS) -o $@ $^
