TARGET := monitor
SRC := $(TARGET).c
OBJ := $(SRC:%.c=%.o)

LDIR := reader
LIB := $(LDIR)/lib$(LDIR).so

CC := gcc
CFLAGS := -Wall -g

AR := ar
ARFLAGS := rcs

LDLIBS := -l$(LDIR)
LDFLAGS := -L$(LDIR)


.PHONY: all
all: $(TARGET)

.PHONY: test
test: $(TARGET)
	@./$(TARGET) .

$(TARGET): $(OBJ) $(LIB)
	$(CC) $(CFLAGS) $< $(LDFLAGS) $(LDLIBS) -o $@

%.o: CFLAGS += -c
%.o: %.c
	$(CC) $(CFLAGS) $^ -o $@ 

$(LIB): $(LDIR)/*.o
	$(AR) $(ARFLAGS) $@ $^	

$(LDIR)/%.o: CFLAGS += -c
$(LDIR)/%.o: $(LDIR)/%.c $(LDIR)/%.h
	$(CC) $(CFLAGS) $< -o $@ 


.PHONY: clean
clean: 
	rm -f $(OBJ) $(LDIR)/*.{o,so} $(TARGET)

