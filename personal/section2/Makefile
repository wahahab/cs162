CC=gcc
CFLAG=-g

PROGRAMS=hello heap_alloc simple_wait_pid exec_fork

all: $(PROGRAMS)

clean:
	rm $(PROGRAMS)

.c:
	$(CC) $(CFLAG) $? -o $@
