TARGET := main
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)
TMP := dane
OUT := wyniki.txt

SHELL := /bin/bash

CC := gcc
CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	@for i in 1 4 512 1024 4096 8192; do \
		echo "Rozmiar bloku: $$i" >> $(OUT); \
		for ratio in 1 2; do \
			num_blocks=$$((ratio * 8192)); \
			echo "  Liczba bloków: $$num_blocks" >> $(OUT); \
			./$(TARGET) generate $(TMP)lib $$num_blocks $$i; \
			cp $(TMP)lib $(TMP)sys; \
			for type in lib sys; do \
				echo "    Typ: $$type" >> $(OUT); \
				echo "      sort: "$$(./$(TARGET) sort $(TMP)$$type $$num_blocks $$i $$type) >> $(OUT); \
				echo "      copy: "$$(./$(TARGET) copy $(TMP)$$type $(TMP)"$$type"new $$num_blocks $$i $$type) >> $(OUT); \
			done; \
			rm $(TMP)*; \
			echo "Done with: block size $$i, $$num_blocks blocks."; \
		done; \
	done; \
	echo "Done."

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $^ -o $@

%.o: CFLAGS += -c
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ 

.PHONY: clean
clean: 
	rm -f *.o $(TARGET) $(TMP)* $(OUT)

# TODO: correct block numbers!
