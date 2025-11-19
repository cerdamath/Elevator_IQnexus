CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = metro

$(TARGET): metro.c
	$(CC) $(CFLAGS) -o $(TARGET) metro.c -lm

clean:
	rm -f $(TARGET)

.PHONY: clean