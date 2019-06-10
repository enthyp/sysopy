TARGET := server client

SRC := $(wildcard *.c)
BUILD_DIR := ./build
INCL_DIR := ./include
OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o)
SERVER_OBJ := $(subst $(BUILD_DIR)/client.o,,$(OBJ))
CLIENT_OBJ := $(subst $(BUILD_DIR)/server.o,,$(OBJ))

DEPS := $(OBJ:.o=.d)

CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -lpthread $^ -o $@

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -lpthread $^ -o $@

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
clean:
	@rm -rf $(TARGET) $(BUILD_DIR)
