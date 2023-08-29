CC ?= $(CROSS_COMPILE)gcc 
CCFLAGS ?= -Wall -Werror -g
LDFLAGS ?= -pthread -lrt

all:
	$(CC) $(CCFLAGS) -c -o aesdsocket.o aesdsocket.c
	$(CC) aesdsocket.o -o aesdsocket $(LDFLAGS)

clean:
	rm -f *.o aesdsocket
