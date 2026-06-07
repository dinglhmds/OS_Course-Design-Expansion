# ============================================================================
# Real-Time Scheduling Research - Makefile
# One-click compile, run, and visualize
# ============================================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c99 -I./include -D_GNU_SOURCE
LDFLAGS = -lm

SRCDIR  = src
SCHDIR  = src/schedulers
INCDIR  = include
OBJDIR  = build
BINDIR  = bin
RESDIR  = results
VISDIR  = visualization

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/task.c \
          $(SRCDIR)/simulator.c \
          $(SRCDIR)/analysis.c \
          $(SRCDIR)/utils.c \
          $(SCHDIR)/rms.c \
          $(SCHDIR)/edf.c \
          $(SCHDIR)/llf.c \
          $(SCHDIR)/dms.c \
          $(SCHDIR)/fcfs.c

OBJECTS = $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(SOURCES)))

TARGET  = $(BINDIR)/rt-scheduler
TEST    = $(BINDIR)/test-basic

.PHONY: all run test clean plot help

all: $(TARGET) $(TEST)

run: $(TARGET)
	@echo "========================================"
	@echo "  Running real-time scheduling experiments..."
	@echo "========================================"
	@mkdir -p $(RESDIR)
	./$(TARGET)
	@echo ""
	@echo "========================================"
	@echo "  Experiments done, generating plots..."
	@echo "========================================"
	@$(MAKE) plot
	@echo ""
	@echo "========================================"
	@echo "  All done!"
	@echo "  Executable: $(TARGET)"
	@echo "  Results:    $(RESDIR)/"
	@echo "  Plots:      $(RESDIR)/*.png"
	@echo "========================================"

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "[LINK] $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]   $@"

$(OBJDIR)/%.o: $(SCHDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]   $@"

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

TEST_SOURCES = test/test_basic.c \
               $(SRCDIR)/task.c \
               $(SRCDIR)/simulator.c \
               $(SRCDIR)/analysis.c \
               $(SRCDIR)/utils.c \
               $(SCHDIR)/rms.c \
               $(SCHDIR)/edf.c \
               $(SCHDIR)/llf.c \
               $(SCHDIR)/dms.c \
               $(SCHDIR)/fcfs.c

TEST_OBJECTS = $(patsubst %.c,$(OBJDIR)/%.o,$(notdir $(TEST_SOURCES)))

$(TEST): $(TEST_OBJECTS) | $(BINDIR)
	$(CC) $(TEST_OBJECTS) -o $@ $(LDFLAGS)
	@echo "[LINK] $@"

$(OBJDIR)/test_basic.o: test/test_basic.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "[CC]   $@"

test: $(TEST)
	@echo "========================================"
	@echo "  Running unit tests..."
	@echo "========================================"
	./$(TEST)

plot:
	@mkdir -p $(RESDIR)
	@if command -v python3 >/dev/null 2>&1; then \
		python3 $(VISDIR)/plot.py $(RESDIR); \
	else \
		echo "Warning: python3 not found, skipping plot generation"; \
		echo "Install matplotlib: pip3 install matplotlib"; \
	fi

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(RESDIR)
	@echo "[CLEAN] Build files and results removed"

help:
	@echo "Real-Time Scheduling Research - Available commands:"
	@echo ""
	@echo "  make        - Compile project"
	@echo "  make run    - Compile and run all experiments"
	@echo "  make plot   - Generate visualization plots"
	@echo "  make clean  - Clean all generated files"
	@echo "  make help   - Show this help"
	@echo ""
	@echo "Dependencies: gcc, make, python3, matplotlib"
