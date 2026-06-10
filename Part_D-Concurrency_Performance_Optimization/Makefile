CC ?= gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -pthread

TARGET := bench_counter
SRC := src/bench_counter.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)
