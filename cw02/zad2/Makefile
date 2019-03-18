TARGET := main
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)

CC := gcc
CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	./$(TARGET) . "<" 15-03-2019_10:00 dir; \
	#./$(TARGET) . "<" 15-03-2019_10:00 dir 

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: CFLAGS += -c
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ 

.PHONY: clean
clean: 
	rm -f *.o $(TARGET)
