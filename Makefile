CMAKE ?= cmake
PRESET_DEBUG := debug
PRESET_RELEASE := release
BUILD_DIR := build

.PHONY: help debug release build-debug build-release run-tests tests run clean

help:
	@echo "Usage:"
	@echo "  make debug        # configure & build using the 'debug' preset"
	@echo "  make release      # configure & build using the 'release' preset"
	@echo "  make run-tests    # build (debug) and run test binaries"
	@echo "  make clean        # remove build/ directory"
	@echo "  make run          # runs the redis server"

debug:
	@echo "Configuring and building (debug)..."
	$(CMAKE) --preset $(PRESET_DEBUG)
	$(CMAKE) --build --preset $(PRESET_DEBUG)

release:
	@echo "Configuring and building (release)..."
	$(CMAKE) --preset $(PRESET_RELEASE)
	$(CMAKE) --build --preset $(PRESET_RELEASE)

build-debug: debug
build-release: release

run-tests: debug
	@echo "Running test binaries from $(BUILD_DIR)/$(PRESET_DEBUG)..."
	@bdir="$(BUILD_DIR)/$(PRESET_DEBUG)"; \
	if [ -x "$$bdir/parser_tests" ]; then \
	  echo "==> Running parser_tests"; $$bdir/parser_tests || exit $$?; \
	else echo "parser_tests not found in $$bdir"; fi; \
	if [ -x "$$bdir/map_tests" ]; then \
	  echo "==> Running map_tests"; $$bdir/map_tests || exit $$?; \
	else echo "map_tests not found in $$bdir"; fi

tests: run-tests

run: debug
	$(BUILD_DIR)/$(PRESET_DEBUG)/my_redis_server

clean:
	@echo "Removing $(BUILD_DIR)/"
	rm -rf $(BUILD_DIR)

CLANG_TIDY := run-clang-tidy
CLANG_BUILD_DIR := build/debug
JOBS := $(shell nproc 2>/dev/null || echo 1)
SRC_GLOBS := '*.c' '*.cc' '*.cpp' '*.cxx'

clang-tidy:
	git ls-files -z --cached --others --exclude-standard -- $(SRC_GLOBS) > /tmp/clang_tidy_files.$$; \
	xargs -0 -n1000 $(CLANG_TIDY) -p $(CLANG_BUILD_DIR) -j $(JOBS) < /tmp/clang_tidy_files.$$; \
	rm -f /tmp/clang_tidy_files.$$
