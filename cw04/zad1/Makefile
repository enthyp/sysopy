TARGET := main
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)

CFLAGS := -Wall -g


.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean
clean: 
	rm -f $(OBJ) $(TARGET)
