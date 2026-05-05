CC = gcc
CFLAGS = -std=c11 -Wall -Wextra

all: main client

main: main.c
	$(CC) $(CFLAGS) main.c -o main

client: client.c
	$(CC) $(CFLAGS) client.c -o client

test: all
	bash test.sh
	
run: all
	bash run.sh

clean:
	rm main client