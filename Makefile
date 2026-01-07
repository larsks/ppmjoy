EXE = ppmjoy
SRCS = main.c must.c config.c vendor/tomlc99/toml.c event.c log.c
OBJS = $(SRCS:.c=.o)

CPPFLAGS=-I. -Ivendor/tomlc99 -Ivendor/unity/src -Ivendor/termcolor-c/include
CFLAGS=-Wall -g
LIBS=-lasound

TEST_SRCS = $(wildcard tests/test_*.c)
TEST_OBJS = $(TEST_SRCS:.c=.o)
TESTS = $(TEST_SRCS:.c=)

all: $(EXE)

test: $(TESTS)
	@for test in $(TESTS); do \
		$$test || exit 1; \
	done

tests/test_config: tests/test_config.o config.o must.o vendor/unity/src/unity.o vendor/tomlc99/toml.o

$(EXE): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(EXE)
	rm -f $(TEST_OBJS) $(TESTS)

realclean: clean
	rm -f makefile.deps

makefile.deps: $(SRCS)
	$(CC) -MM $(CPPFLAGS) $(SRCS) $(TEST_SRCS) > $@ || { rm -f $@; exit 1; }

include makefile.deps
