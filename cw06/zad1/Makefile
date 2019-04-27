TARGET := client server
INCL := commons.h

CFLAGS := -Wall -g

.PHONY: all
all: $(TARGET)

$(TARGET): %: %.o $(INCL)
	$(CC) $(CFLAGS) $< -o $@

.PHONY:
clean: teardown
	@rm -f *.o $(TARGET)

.PHONY:
teardown: # remove all user queues
	@q_id=`ipcs -q | awk '{if ($$3=="jlanecki") print $$2}'`; \
	if [[ ! -z "$$q_id" ]]; then \
		for id in $$q_id; do \
			ipcrm msg $$id; \
		done; \
		echo "All user queues removed."; \
	else \
		echo "Queue removal: no user queues found."; \
	fi
