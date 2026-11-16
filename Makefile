CC = gcc
CFLAGS = -Wall

myshell: shell.c
	$(CC) $(CFLAGS) -o myshell shell.c

clean:
	rm -f myshell
