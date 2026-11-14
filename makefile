# json.h build and test targets. Run `make help` for the list.

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Wno-unused-function -g -I.
SAN     = -fsanitize=address,undefined -fno-sanitize-recover=all
OPT     = -O2
LDLIBS  = -lm

# base.h arenas are never freed by design, so LeakSanitizer would report every
# run as a leak and change the exit code, which the suite runner relies on.
export ASAN_OPTIONS = detect_leaks=0

BUILD   = build
HEADERS = json.h base.h

# JSONTestSuite is vendored into tests/JSONTestSuite. `make update-suite`
# refreshes it from SUITE_COMMIT; bump that on purpose and review the diff.
SUITE_REPO   = https://github.com/nst/JSONTestSuite
SUITE_COMMIT = 1ef36fa01286573e846ac449e8683f8833c5b26a
SUITE_DIR    = tests/JSONTestSuite

.PHONY: all test test-release suite suite-verbose example check update-suite clean help

all: $(BUILD)/test $(BUILD)/json_suite

# --- binaries ---------------------------------------------------------------
# Sanitized builds are the default for testing: they turn silent memory bugs
# into crashes. The -release builds catch bugs that only show up when optimized.

$(BUILD)/test: tests/test.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $< -o $@ $(LDLIBS)

$(BUILD)/test-release: tests/test.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(OPT) $< -o $@ $(LDLIBS)

$(BUILD)/json_suite: tests/json_suite.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $< -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $@

# --- test runs --------------------------------------------------------------

test: $(BUILD)/test
	./$(BUILD)/test

test-release: $(BUILD)/test-release
	./$(BUILD)/test-release

suite: $(BUILD)/json_suite
	@sh tests/run_suite.sh ./$(BUILD)/json_suite $(SUITE_DIR)

suite-verbose: $(BUILD)/json_suite
	@sh tests/run_suite.sh ./$(BUILD)/json_suite $(SUITE_DIR) -v

$(BUILD)/example: examples/main.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) $(SAN) $< -o $@ $(LDLIBS)

example: $(BUILD)/example
	./$(BUILD)/example

check: test test-release suite example

# --- maintenance ------------------------------------------------------------

update-suite:
	@tmp=$$(mktemp -d) && \
	git clone -q $(SUITE_REPO) $$tmp && \
	git -C $$tmp checkout -q $(SUITE_COMMIT) && \
	rm -rf $(SUITE_DIR) && mkdir -p $(SUITE_DIR) && \
	cp $$tmp/test_parsing/*.json $(SUITE_DIR)/ && \
	cp $$tmp/LICENSE $(SUITE_DIR)/LICENSE && \
	echo "$(SUITE_REPO) @ $(SUITE_COMMIT)" > $(SUITE_DIR)/SOURCE && \
	rm -rf $$tmp && \
	echo "vendored $$(ls $(SUITE_DIR)/*.json | wc -l) cases into $(SUITE_DIR)"

clean:
	rm -rf $(BUILD)

help:
	@echo "make               build the test binaries"
	@echo "make test          unit tests (ASan + UBSan)"
	@echo "make test-release  unit tests at -O2, no sanitizers"
	@echo "make suite         JSONTestSuite (ASan + UBSan)"
	@echo "make suite-verbose same, listing what happened to each i_ case"
	@echo "make check         all of the above; run before committing"
	@echo "make update-suite  re-vendor JSONTestSuite at SUITE_COMMIT"
	@echo "make clean         remove $(BUILD)/"
	@echo "Override the compiler with: make CC=clang check"