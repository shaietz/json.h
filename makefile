CC = cc
CFLAGS = -Wall -Wextra -O2
TARGET = c-json-parser

# 1. The default target (just compiles)
all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

# 2. The target you asked for (compiles AND runs)
run: $(TARGET)
	./$(TARGET)

# 3. Cleanup target to remove the binary
clean:
	rm -f $(TARGET)