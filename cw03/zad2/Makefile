TARGET := monitor

SRC := $(wildcard *.c)
BUILD_DIR := ./build
INCL_DIR := ./include
OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o)
DEPS := $(wildcard $(INCL_DIR)/*.h)

CFLAGS := -Wall -g 


.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	@./$(TARGET) .

$(TARGET): $(OBJ) 
	$(CC) $(CFLAGS) $^ -o $@

$(OBJ): CFLAGS += -I$(INCL_DIR) 
$(OBJ): CFLAGS += -c
$(OBJ): $(BUILD_DIR)/%.o: %.c $(DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR):
	@mkdir -p $@


.PHONY: clean
clean: 
	@rm -rf $(BUILD_DIR) $(TARGET) archive

