CC = gcc
CFLAGS = -Wall -Wextra -O2 -msse4.1
TARGET = test_hamming

all: $(TARGET)

$(TARGET): test_hamming.c hamming_intrinsics.c
	$(CC) $(CFLAGS) test_hamming.c hamming_intrinsics.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
