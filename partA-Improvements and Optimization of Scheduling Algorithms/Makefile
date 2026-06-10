CC ?= gcc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -pedantic

TARGET := scheduler
SRC := src/scheduler.c

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: all
	bash scripts/run_all.sh

test: all
	bash scripts/run_tests.sh

clean:
	rm -f $(TARGET)
	rm -f results/*.csv
