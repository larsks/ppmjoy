EXE = ppmjoy
SRCS = main.c must.c config.c vendor/tomlc99/toml.c event.c log.c args.c
OBJS = $(SRCS:.c=.o)

PPMDIAG_SRCS = ppmdiag.c event.c log.c must.c
PPMDIAG_OBJS = $(PPMDIAG_SRCS:.c=.o)

CPPFLAGS=-I. -Ivendor/tomlc99 -Ivendor/unity/src -Ivendor/termcolor-c/include
CFLAGS=-Wall -g
LIBS=-lasound

# Coverage flags (only used for coverage target)
COVERAGE_CFLAGS = --coverage
COVERAGE_LDFLAGS = --coverage
COVERAGE_DIR = coverage

TEST_SRCS = $(wildcard tests/test_*.c)
TEST_OBJS = $(TEST_SRCS:.c=.o)
TESTS = $(TEST_SRCS:.c=)

TEST_EXTRA_OBJS=vendor/unity/src/unity.o

# Coverage build creates separate .o files in coverage/ directory
COVERAGE_OBJS = $(patsubst %.o,$(COVERAGE_DIR)/%.o,$(filter-out main.o, $(OBJS)))
COVERAGE_TEST_OBJS = $(patsubst %.o,$(COVERAGE_DIR)/%.o,$(TEST_OBJS))
COVERAGE_TEST_EXTRA_OBJS = $(patsubst %.o,$(COVERAGE_DIR)/%.o,$(TEST_EXTRA_OBJS))
COVERAGE_TESTS = $(patsubst tests/%,$(COVERAGE_DIR)/tests/%,$(TESTS))

COMPILE =	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

all: $(EXE) ppmdiag

test: $(TESTS)
	@for test in $(TESTS); do \
		echo "=== $$test ==="; \
		$$test || exit 1; \
	done

tests/test_config: tests/test_config.o

tests/test_args: tests/test_args.o

tests/test_event: tests/test_event.o tests/helpers.o

$(TESTS): $(filter-out main.o, $(OBJS)) $(TEST_EXTRA_OBJS)
	$(COMPILE)

$(EXE): $(OBJS)
	$(COMPILE)

ppmdiag: $(PPMDIAG_OBJS)
	$(COMPILE)

# Coverage compilation rules - build everything in coverage/ subdir
$(COVERAGE_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(COVERAGE_CFLAGS) $(CPPFLAGS) -c -o $@ $<

# Coverage test dependencies
$(COVERAGE_DIR)/tests/test_config: $(COVERAGE_DIR)/tests/test_config.o

$(COVERAGE_DIR)/tests/test_args: $(COVERAGE_DIR)/tests/test_args.o

$(COVERAGE_DIR)/tests/test_event: $(COVERAGE_DIR)/tests/test_event.o $(COVERAGE_DIR)/tests/helpers.o

# Link coverage test executables
$(COVERAGE_TESTS): $(COVERAGE_OBJS) $(COVERAGE_TEST_EXTRA_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(COVERAGE_LDFLAGS) -o $@ $(filter %.o,$^) $(LIBS)

# Run tests with coverage instrumentation
coverage-run: $(COVERAGE_TESTS)
	@echo "Running tests with coverage instrumentation..."
	@for test in $(COVERAGE_TESTS); do \
		echo "=== $$test ==="; \
		$$test || exit 1; \
	done

# Generate coverage report (gcov-based, no lcov required)
coverage-report: coverage-run
	@echo "Generating coverage report..."
	@mkdir -p $(COVERAGE_DIR)/report
	@echo ""
	@for srcfile in $(filter-out main.c vendor/%, $(SRCS)); do \
		gcov -o $(COVERAGE_DIR) $$srcfile 2>&1 | grep -v "Creating"; \
	done
	@mv *.gcov $(COVERAGE_DIR)/report/ 2>/dev/null || true

$(COVERAGE_DIR)/coverage_filtered.info: coverage-run
	@mkdir -p $(COVERAGE_DIR)
	@lcov --capture --directory $(COVERAGE_DIR) --output-file $(COVERAGE_DIR)/coverage.info --quiet
	@lcov --remove $(COVERAGE_DIR)/coverage.info '*/vendor/*' '*/tests/*' --output-file $(COVERAGE_DIR)/coverage_filtered.info --quiet --ignore-errors unused

$(COVERAGE_DIR)/coverage.txt: coverage-run $(COVERAGE_DIR)/coverage_filtered.info
	@lcov --list $(COVERAGE_DIR)/coverage_filtered.info > $@ || { rm -f $@; exit 1; }

coverage-text: $(COVERAGE_DIR)/coverage.txt

coverage-html: coverage-run $(COVERAGE_DIR)/coverage_filtered.info
	@echo "Generating HTML coverage report with lcov..."
	@genhtml $(COVERAGE_DIR)/coverage_filtered.info --output-directory $(COVERAGE_DIR)/html --quiet --ignore-errors source
	@echo "HTML coverage report generated at $(COVERAGE_DIR)/html/index.html"
	@echo "Open with: xdg-open $(COVERAGE_DIR)/html/index.html"

coverage: coverage-report

clean:
	rm -f $(OBJS) $(EXE)
	rm -f $(PPMDIAG_OBJS) ppmdiag
	rm -f $(TEST_OBJS) $(TEST_EXTRA_OBJS) $(TESTS)
	rm -rf $(COVERAGE_DIR)
	rm -f *.gcov *.gcda *.gcno

realclean: clean
	rm -f makefile.deps keys.h

makefile.deps: $(SRCS) keys.h
	$(CC) -MM $(CPPFLAGS) $(SRCS) $(TEST_SRCS) > $@ || { rm -f $@; exit 1; }

keys.h: make_keys_h.sh
	sh $< > $@ || { rm -f $@; exit 1; }

include makefile.deps

.PHONY: all test clean realclean coverage coverage-run coverage-report coverage-html
