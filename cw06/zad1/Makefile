TARGET := client server
INCL := protocol.h

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
		done \
	else \
		echo "No user queues found."; \
	fi
