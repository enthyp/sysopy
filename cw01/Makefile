# Basics.
NAME := find
SDIR := lib-static
DDIR := lib-shared
SNAME := $(SDIR)/lib$(NAME).a
DNAME := $(DDIR)/lib$(NAME).so
REPORT := raport2.txt

# Library description.
LIB_SRC := find.c
INCL := $(wildcard *.h)
SOBJ := $(LIB_SRC:%.c=$(SDIR)/%.o)
DOBJ := $(LIB_SRC:%.c=$(DDIR)/%.o)

# Test file description.
TEST_SRC := test_find.c
TOBJ := $(TEST_SRC:%.c=%.o)

# Tool setup.
CC := gcc
CFLAGS := -c -Wall -g
OPT := -O0

LDLIBS := -l$(NAME)

AR := ar
ARFLAGS := rcs

###

.PHONY: all
all: run-static run-shared run-dynamic

# Run tests.
.PHONY: run-static run-shared run-dynamic
run-static run-shared run-dynamic: run-%: % | grow_trees		
	./$< \
		create_table 10000 \
		search_directory tree_small file tmp_small \
		store_result tmp_small \
		remove_block 0 \
		search_directory tree_medium file tmp_medium \
		store_result tmp_medium \
		remove_block 0 \
		search_directory tree_large file tmp_large \
		store_result tmp_large \
		remove_block 0 \
		store_remove_rep tmp_large 1000 >> $(REPORT)

# Build test directory structure.
.PHONY: grow_trees
grow_trees: 
	bash dir_tree.sh 3 6 dir file tree_small
	bash dir_tree.sh 5 6 dir file tree_medium
	bash dir_tree.sh 5 6 dir file tree_large

.PHONY: cut_down_trees
cut_down_trees:
	rm -rf tree_{small,medium,large}


# Build tests.
.PHONY: static shared dynamic
static: LDFLAGS += -L$(SDIR)
shared: LDFLAGS += -L$(DDIR) -Wl,-rpath=$(DDIR)

static shared: %: build-% $(TOBJ)
	$(CC) -g -Wall $(OPT) $(word 2, $^) $(LDFLAGS) $(LDLIBS) -o $@

dynamic: build-shared
	$(CC) -g -Wall $(OPT) -ldl test_find_dll.c -o dynamic

$(TOBJ): $(TEST_SRC) 
	$(CC) $(CFLAGS) $(OPT) $^ -o $@ 


# Build libs.
.PHONY: build-static build-shared
build-static: $(SNAME)
build-shared: $(DNAME)


$(SNAME): $(SOBJ)
	$(AR) $(ARFLAGS) $@ $^
	
$(SDIR)/%.o: %.c $(INCL) | $(SDIR)
	$(CC) $(CFLAGS) $(OPT) $< -o $@


$(DNAME): CFLAGS += -fPIC
$(DNAME): $(DOBJ)
	$(CC) -shared -g $^ -o $@

$(DDIR)/%.o: %.c $(INCL) | $(DDIR)
	$(CC) $(CFLAGS) $(OPT) $< -o $@

$(SDIR) $(DDIR):
	@mkdir $@


# Clean.
.PHONY: clean
clean:
	@rm -rf $(SDIR) $(DDIR) *.o static shared dynamic tmp_{small,medium,large} #tree_{small,medium,large}


