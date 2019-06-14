TARGET := main make_filter

SRC := $(wildcard *.c)
BUILD_DIR := ./build
INCL_DIR := ./include
OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o)
MAIN_OBJ := $(subst $(BUILD_DIR)/make_filter.o,,$(OBJ))
FILTER_OBJ := $(subst $(BUILD_DIR)/main.o,,$(OBJ))

DEPS := $(OBJ:.o=.d)

CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

main: $(MAIN_OBJ)
	$(CC) $(CFLAGS) -lpthread -lm $^ -o $@

make_filter: $(FILTER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

-include $(DEPS)

$(BUILD_DIR)/%.d: CFLAGS += -I$(INCL_DIR)
$(BUILD_DIR)/%.d: %.c | $(BUILD_DIR)
	@set -e; rm -f $@; \
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$;

$(OBJ): CFLAGS += -I$(INCL_DIR)
$(OBJ): CFLAGS += -c
$(OBJ): | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

.PHONY:
test: $(TARGET)
	@touch Times.txt; \
	for filter in img/filter*; do \
		for i in 1 2 4 8; do \
			for mode in block interleaved; do \
				header="Filter: $$filter Threads: $$i Mode: $$mode"; \
				echo $$header >> Times.txt; \
				./main $$i $$mode img/mountain.ascii.pgm $$filter "img/conv-$${filter:4}-$$i-$$mode.ascii.pgm" >> \
				Times.txt; \
				echo -e "\n" >> Times.txt; \
				echo $$header DONE!; \
			done; \
		done; \
	done

.PHONY:
clean:
	@rm -rf $(TARGET) $(BUILD_DIR) img/conv*
