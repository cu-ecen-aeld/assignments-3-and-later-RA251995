CC ?= $(CROSS_COMPILE)gcc 
CCFLAGS += -Wall -Werror

all:
	$(CC) $(CCFLAGS) -c -o aesdsocket.o aesdsocket.c
	$(CC) $(LDFLAGS) aesdsocket.o -o aesdsocket

clean:
	rm -f *.o aesdsocket
