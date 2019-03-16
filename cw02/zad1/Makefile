TARGET := main
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)
TMP := dane

CC := gcc
CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	./$(TARGET) generate $(TMP) 100 512; \
	./$(TARGET) sort $(TMP) 1000 1 lib; \
	./$(TARGET) copy dane $(TMP)1 1 51200 lib

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: CFLAGS += -c
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ 

.PHONY: clean
clean: 
	rm -f *.o $(TARGET) $(TMP)*
