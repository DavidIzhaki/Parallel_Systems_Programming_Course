# File: Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99

# Object files
OBJS = main.o decode_c.o decode_asm.o

# Executable name
TARGET = test_all

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

decode_c.o: decode.c
	$(CC) $(CFLAGS) -c decode.c -o decode_c.o

decode_asm.o: decode.s
	$(CC) -c decode.s -o decode_asm.o

run: all
	./$(TARGET)

clean:
	rm -f *.o $(TARGET)
