CMAKE ?= cmake
CLANG_FORMAT ?= clang-format
CLANG_TIDY ?= clang-tidy
RUN_CLANG_TIDY ?= run-clang-tidy
TIDY_BUILD_DIR ?= build/v2/debug

# C++ sources checked by format/tidy (v2 only; thirdparty is excluded).
V2_SOURCES = $(shell git ls-files 'v2/src/*.cc' 'v2/src/*.h' 'v2/tests/*.cc')

.PHONY: help all debug release v1-debug v1-release v2-debug v2-release test clean \
	format format-check tidy-check

help:
	@echo "Usage:"
	@echo "  make debug        # configure & build v1 and v2 (Debug)"
	@echo "  make release      # configure & build v1 and v2 (Release)"
	@echo "  make v1-debug     # build only v1 (Debug)"
	@echo "  make v1-release   # build only v1 (Release)"
	@echo "  make v2-debug     # build only v2 (Debug)"
	@echo "  make v2-release   # build only v2 (Release)"
	@echo "  make test         # build v2 (debug) and run its unit + e2e tests"
	@echo "  make format       # apply clang-format to v2 sources"
	@echo "  make format-check # fail if v2 sources are not clang-formatted"
	@echo "  make tidy-check   # run clang-tidy over v2 (needs a configured debug build)"
	@echo "  make clean        # remove v1/build and v2/build"
	@echo ""
	@echo "Build output: build/v1/<preset>/ and build/v2/<preset>/"

all: debug

debug: v1-debug v2-debug
release: v1-release v2-release

v1-debug:
	@echo "==> v1 (debug)"
	cd v1 && $(CMAKE) --preset debug && $(CMAKE) --build --preset debug

v1-release:
	@echo "==> v1 (release)"
	cd v1 && $(CMAKE) --preset release && $(CMAKE) --build --preset release

v2-debug:
	@echo "==> v2 (debug)"
	cd v2 && $(CMAKE) --preset debug && $(CMAKE) --build --preset debug

v2-release:
	@echo "==> v2 (release)"
	cd v2 && $(CMAKE) --preset release && $(CMAKE) --build --preset release

# Unit tests are registered with CTest via gtest_discover_tests; the e2e
# suite starts its own server from build/v2/debug.
test: v2-debug
	@echo "==> v2 unit tests"
	ctest --test-dir build/v2/debug --output-on-failure
	@echo "==> v2 e2e tests"
	SERVER_BIN=$(CURDIR)/build/v2/debug/my_redis_server v2/tests/e2e/run_all.sh

format:
	$(CLANG_FORMAT) -i $(V2_SOURCES)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(V2_SOURCES)

# Uses build/v2/debug/compile_commands.json, so run `make v2-debug` (or just
# configure) first. Headers are covered via HeaderFilterRegex in v2/.clang-tidy.
tidy-check:
	$(RUN_CLANG_TIDY) -clang-tidy-binary $(CLANG_TIDY) -p $(TIDY_BUILD_DIR) -quiet '$(CURDIR)/v2/(src|tests)/'

clean:
	@echo "Removing build/"
	rm -rf build
