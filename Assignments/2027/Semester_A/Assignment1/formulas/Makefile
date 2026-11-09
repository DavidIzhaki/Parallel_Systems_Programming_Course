CC = gcc
CFLAGS = -Wall -O2 -msse4.1
TARGET = test_formula1

all: $(TARGET)

$(TARGET): main.c formula1.c formulas.h
	$(CC) $(CFLAGS) main.c formula1.c -o $(TARGET) -lm

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
