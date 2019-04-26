TARGET := sender catcher
SRC := $(TARGET).c

CFLAGS := -Wall -g


.PHONY: all
all: $(TARGET)

$(TARGET): %: %.o
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean: 
	rm -f *.o $(TARGET)
