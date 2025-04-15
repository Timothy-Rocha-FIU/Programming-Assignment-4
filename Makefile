# Makefile for Assignment 4 - COP4610

CC = gcc
CFLAGS = -Wall -static
TARGET = memory_allocator
SRC = PA4.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o
