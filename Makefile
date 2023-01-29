all: server client
server: server.c
	gcc -pthread server.c -o server
client: client.c 
	gcc -pthread client.c -o client
clean:
	rm -f server client
