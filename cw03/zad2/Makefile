TARGET := monitor
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)

CC := gcc
CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	@./$(TARGET) .

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: CFLAGS += -c
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ 

.PHONY: clean
clean: 
	rm -f $(OBJ) $(TARGET)
