TARGET := loader trucker loader_runner

SRC := $(wildcard *.c)
BUILD_DIR := ./build
INCL_DIR := ./include
OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o)
LOADER_OBJ := $(subst $(BUILD_DIR)/trucker.o,,$(OBJ))
LOADER_OBJ := $(subst $(BUILD_DIR)/loader_runner.o,,$(LOADER_OBJ))

LOADER_RUNNER_OBJ := $(BUILD_DIR)/loader_runner.o $(BUILD_DIR)/util.o

TRUCKER_OBJ := $(subst $(BUILD_DIR)/loader.o,,$(OBJ))
TRUCKER_OBJ := $(subst $(BUILD_DIR)/loader_runner.o,,$(TRUCKER_OBJ))

DEPS := $(OBJ:.o=.d)

CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

loader: $(LOADER_OBJ)
	$(CC) $(CFLAGS) -lm $^ -o $@

loader_runner: $(LOADER_RUNNER_OBJ)
	$(CC) $(CFLAGS) $^ -o $@

trucker: $(TRUCKER_OBJ)
	$(CC) $(CFLAGS) -lm $^ -o $@

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

.PHONY:
teardown:
	@for i in `ipcs -m | grep 666 | awk '{print $$2}'`; do \
		ipcrm -m $$i; \
	done; \
	echo "Memory removed."; \
	for i in `ipcs -q | tail -n+4 | head -n+1 | awk '{print $$2}'`; do \
		ipcrm -q $$i; \
	done; \
	echo "Queues removed."; \
	for i in `ipcs -s | grep 666 | awk '{print $$2}'`; do \
		ipcrm -s $$i; \
	done; \
	echo "Semaphores removed."

.PHONY:
echo:
	@echo $(LOADER_OBJ)