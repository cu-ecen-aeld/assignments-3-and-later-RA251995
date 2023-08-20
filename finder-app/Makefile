CC ?= $(CROSS_COMPILE)gcc 
CCFLAGS ?= -Wall

all:
	$(CC) $(CCFLAGS) -c -o writer.o writer.c
	$(CC) $(LDFLAGS) writer.o -o writer

clean:
	rm -f *.o writer *.elf *.map
