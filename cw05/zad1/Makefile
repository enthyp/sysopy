TARGET := main
OBJ := $(TARGET).o

CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean: 
	rm -f *.o $(TARGET)
