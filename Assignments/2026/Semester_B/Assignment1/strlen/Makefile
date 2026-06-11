CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = test_strlen

all: $(TARGET)

strlen_sse42.o: strlen_sse42.s
	$(CC) -c strlen_sse42.s -o strlen_sse42.o

$(TARGET): test_strlen.c strlen_sse42.o
	$(CC) $(CFLAGS) test_strlen.c strlen_sse42.o -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f *.o $(TARGET)
