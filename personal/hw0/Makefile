CFLAG = -g
CC = gcc
PROGRAMS = main wc map

.PHONY: all clean

all: $(PROGRAMS)

clean:
	rm main wc map

.c:
	$(CC) $(CFLAG) $@.c -o $@
