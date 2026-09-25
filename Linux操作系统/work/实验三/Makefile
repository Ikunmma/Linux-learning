CC = gcc
CFLAGS = -Wall -Wextra -std=c11
all: server client
server: server.c
	$(CC) $(CFLAGS) server.c -o server
client: client.c
	$(CC) $(CFLAGS) client.c -o client
clean:
	rm -f server client
	rm -rf server_files downloads