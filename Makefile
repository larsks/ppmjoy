EXE = ppmjoy
SRCS = main.c must.c config.c vendor/tomlc99/toml.c event.c log.c args.c
OBJS = $(SRCS:.c=.o)

CPPFLAGS=-I. -Ivendor/tomlc99 -Ivendor/unity/src -Ivendor/termcolor-c/include
CFLAGS=-Wall -g
LIBS=-lasound

TEST_SRCS = $(wildcard tests/test_*.c)
TEST_OBJS = $(TEST_SRCS:.c=.o)
TESTS = $(TEST_SRCS:.c=)

TEST_EXTRA_OBJS=vendor/unity/src/unity.o

COMPILE =	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

all: $(EXE)

test: $(TESTS)
	@for test in $(TESTS); do \
		$$test || exit 1; \
	done

tests/test_config: tests/test_config.o

tests/test_args: tests/test_args.o

$(TESTS): $(filter-out main.o, $(OBJS)) $(TEST_EXTRA_OBJS)
	$(COMPILE)

$(EXE): $(OBJS)
	$(COMPILE)

clean:
	rm -f $(OBJS) $(EXE)
	rm -f $(TEST_OBJS) $(TEST_EXTRA_OBJS) $(TESTS)

realclean: clean
	rm -f makefile.deps

makefile.deps: $(SRCS)
	$(CC) -MM $(CPPFLAGS) $(SRCS) $(TEST_SRCS) > $@ || { rm -f $@; exit 1; }

include makefile.deps
