TARGET := main
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)

CFLAGS := -Wall -g


.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ) date_loop.sh
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean: 
	rm -f $(OBJ) $(TARGET)
