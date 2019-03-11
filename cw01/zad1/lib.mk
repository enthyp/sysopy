NAME := libfind
SDIR := lib-static
DDIR := lib-shared

SNAME := $(SDIR)/$(NAME).a
DNAME := $(DDIR)/$(NAME).so

SRC := find.c
SOBJ := $(SRC:%.c=$(SDIR)/%.o)
DOBJ := $(SRC:%.c=$(DDIR)/%.o)
INCL := $(wildcard *.h)

CC := gcc
CFLAGS := -c -Wall -g

AR := ar
ARFLAGS := rcs

.PHONY: clean all static shared



all: static shared

static: $(SNAME)
shared: $(DNAME)


$(SNAME): $(SOBJ)
	$(AR) $(ARFLAGS) $@ $^
	
$(SDIR)/%.o: %.c $(INCL) | $(SDIR)
	$(CC) $(CFLAGS) $< -o $@


$(DNAME): CFLAGS += -fPIC
$(DNAME): $(DOBJ)
	$(CC) -shared -g $^ -o $@

$(DDIR)/%.o: %.c $(INCL) | $(DDIR)
	$(CC) $(CFLAGS) $< -o $@

$(SDIR) $(DDIR):
	@mkdir $@


clean:
	@rm -rf $(SDIR) $(DDIR)

