CC=gcc
CFLAGS=-I.
LDFLAGS=-pthread

all: server client

server : server.c libcommon.a
	$(CC) -o server server.c -L. -lcommon $(CFLAGS) $(LDFLAGS)

client : client.c libcommon.a
	$(CC) -o client client.c -L. -lcommon $(CFLAGS) $(LDFLAGS)

# Правило для создания статической библиотеки
libcommon.a : common.o
	ar rcs libcommon.a common.o

# Правило компиляции объектного файла библиотеки
common.o : common.c common.h
	$(CC) -c common.c -o common.o $(CFLAGS)

clean :
	rm -f server client *.o *.a 