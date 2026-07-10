CC = gcc
CFLAGS = -Wall -Wextra

TARGETS = server client

all: server client

server: server.o packet.o common.o transfer.o
	$(CC) $^ -o $@

client: client.o packet.o common.o
	$(CC) $^ -o $@

server.o: server.c
	$(CC) $(CFLAGS) -c $<

client.o: client.c
	$(CC) $(CFLAGS) -c $<

packet.o: packet.c packet.h
	$(CC) $(CFLAGS) -c $<

common.o: common.c common.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(TARGETS)