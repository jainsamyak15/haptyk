# SPDX-License-Identifier: LicenseRef-Proprietary
# Copyright (c) 2026 The Keyfeel Authors. All rights reserved.
#
# Build entry point for the C core (libkfcore), dev tools and core tests.
# The macOS app target is built by Xcode from M3 (project.yml, ADR-0003).
#
#   make help          list targets
#   make CONFIG=release ...   debug (default) | release | asan | tsan

SHELL         := /bin/bash
# Ignored by GNU Make 3.81 (macOS default), so no recipe relies on it.
.SHELLFLAGS   := -eu -o pipefail -c
.DEFAULT_GOAL := help
MAKEFLAGS     += --no-builtin-rules --no-builtin-variables

UNAME_S := $(shell uname -s)
CONFIG  ?= debug
BUILD   := build/$(CONFIG)
OBJ     := $(BUILD)/obj
BIN     := $(BUILD)/bin

ifeq ($(filter $(CONFIG),debug release asan tsan),)
  $(error CONFIG must be one of: debug release asan tsan)
endif

# ---------------------------------------------------------------- toolchain
ifeq ($(UNAME_S),Darwin)
  DEFAULT_CC := xcrun clang
  NM         := xcrun nm
  ARCHIVE     = xcrun libtool -static -o $@ $^
  PLATFORM   := -arch arm64 -mmacosx-version-min=13.0
  FRAMEWORKS := -framework CoreFoundation -framework IOKit -framework ApplicationServices \
                -framework AudioToolbox -framework CoreAudio -framework Foundation
  CLANG      := xcrun clang
  LINT_SYSROOT = -isysroot $(shell xcrun --show-sdk-path)
else
  # Host builds exist only so the portable core can be unit-tested in CI.
  DEFAULT_CC := cc
  NM         := nm
  ARCHIVE     = ar rcs $@ $^
  PLATFORM   := -D_POSIX_C_SOURCE=200809L
  FRAMEWORKS :=
  CLANG      := clang
  LINT_SYSROOT :=
endif

ifneq ($(filter default undefined,$(origin CC)),)
  CC := $(DEFAULT_CC)
endif

PYTHON := $(if $(wildcard Tools/py/.venv/bin/python),Tools/py/.venv/bin/python,python3)

# Lint tools: PATH first, then the Xcode toolchain, then Homebrew LLVM. Expanded lazily, so
# only `make format`/`lint` pay for the lookup.
find_tool = $(firstword $(shell command -v $(1) 2>/dev/null) \
              $(shell xcrun --find $(1) 2>/dev/null) \
              $(wildcard /opt/homebrew/opt/llvm/bin/$(1) /usr/local/opt/llvm/bin/$(1)))
CLANG_FORMAT = $(call find_tool,clang-format)
CLANG_TIDY   = $(call find_tool,clang-tidy)

# ---------------------------------------------------------------- flags
CSTD     := -std=c11
INCLUDES := -ICore/include -ICore/src
WARNINGS := -Wall -Wextra -Wpedantic -Werror -Wshadow -Wdouble-promotion -Wstrict-prototypes \
            -Wmissing-prototypes -Wformat=2 -Wundef -Wvla -Wcast-qual -Wpointer-arith

CFLAGS_debug   := -O0 -g -DKF_DEBUG=1
CFLAGS_release := -O3 -DNDEBUG
CFLAGS_asan    := -O1 -g -DKF_DEBUG=1 -fsanitize=address,undefined -fno-omit-frame-pointer \
                  -fno-sanitize-recover=all
CFLAGS_tsan    := -O1 -g -DKF_DEBUG=1 -fsanitize=thread
LDFLAGS_asan   := -fsanitize=address,undefined
LDFLAGS_tsan   := -fsanitize=thread

CFLAGS  := $(CSTD) $(WARNINGS) $(PLATFORM) $(INCLUDES) $(CFLAGS_$(CONFIG)) -MMD -MP
LDFLAGS := $(PLATFORM) $(LDFLAGS_$(CONFIG))
LDLIBS  := -lm -lpthread

# Objective-C (macOS only): ARC, nullability checked. App sources see only Core/include, so
# the app cannot include Core/src (docs/ARCHITECTURE.md); dev tools add Core/src below.
OBJC_WARNINGS := -Wall -Wextra -Werror -Wshadow -Wformat=2 -Wundef -Wstrict-prototypes \
                 -Wmissing-prototypes -Wnullable-to-nonnull-conversion -Wunguarded-availability \
                 -Wobjc-missing-super-calls -Wimplicit-retain-self
OBJCFLAGS := -std=gnu11 -fobjc-arc $(OBJC_WARNINGS) $(PLATFORM) -ICore/include -IApp/Sources \
             $(CFLAGS_$(CONFIG)) -MMD -MP

# ---------------------------------------------------------------- sources
CORE_SRCS := $(sort $(shell find Core/src -name '*.c'))
CORE_OBJS := $(CORE_SRCS:%.c=$(OBJ)/%.o)
CORE_LIB  := $(BUILD)/libkfcore.a

TEST_SRCS := $(sort $(wildcard Tests/Core/*.c))
TEST_OBJS := $(TEST_SRCS:%.c=$(OBJ)/%.o)
TEST_BIN  := $(BIN)/kf-core-tests

PORTABLE_TOOLS := latency-bench
MAC_TOOLS      := hid-dump sensor-log audio-info kf-cli engine-stress mix-bench
TOOLS          := $(PORTABLE_TOOLS) $(if $(filter Darwin,$(UNAME_S)),$(MAC_TOOLS))
TOOL_BINS      := $(TOOLS:%=$(BIN)/%)
TOOL_SRCS      := $(wildcard $(TOOLS:%=Tools/%/main.c))
TOOL_OBJS      := $(TOOLS:%=$(OBJ)/Tools/%/main.o)

# App classes the Objective-C dev tools link, so both validate packs identically.
TOOL_APP_OBJS := $(OBJ)/App/Sources/Packs/KFPackManifest.o

# The menu bar app (macOS): Objective-C plus the app's own C, linked against libkfcore.
APP_NAME       := Keyfeel
APP_BUNDLE     := $(BUILD)/$(APP_NAME).app
APP_EXE        := $(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)
APP_PLIST      := $(APP_BUNDLE)/Contents/Info.plist
APP_SRCS       := $(if $(filter Darwin,$(UNAME_S)),$(sort $(shell find App/Sources -name '*.m' -o -name '*.c')))
APP_OBJS       := $(patsubst %,$(OBJ)/%.o,$(basename $(APP_SRCS)))
APP_LIB_OBJS   := $(filter-out $(OBJ)/App/Sources/Application/main.o,$(APP_OBJS))
APP_FRAMEWORKS := $(FRAMEWORKS) -framework AppKit -framework ServiceManagement \
                  -framework UniformTypeIdentifiers -framework MetricKit \
                  -framework CoreImage
xcconfig        = $(shell sed -n 's/^$(1) = //p' Config/App.xcconfig)

APP_TEST_SRCS := $(if $(filter Darwin,$(UNAME_S)),$(sort $(wildcard Tests/App/*.m)))
APP_TEST_OBJS := $(APP_TEST_SRCS:%.m=$(OBJ)/%.o)
APP_TEST_BIN  := $(BIN)/kf-app-tests

# Code shared by the macOS tools (IOKit helpers). kf_tool.h is header-only and portable.
TOOL_COMMON_SRCS := $(if $(filter Darwin,$(UNAME_S)),$(sort $(wildcard Tools/common/*.c)))
TOOL_COMMON_OBJS := $(TOOL_COMMON_SRCS:%.c=$(OBJ)/%.o)

FORMAT_SRCS = $(shell find Core Tests Tools App -type f \( -name '*.c' -o -name '*.h' -o -name '*.m' \) \
                -not -path '*/.venv/*' | sort)

# ---------------------------------------------------------------- targets
.PHONY: help all core tools app run-app test test-app test-asan test-tsan test-all test-py bench \
        check-rt analyze lint format format-check test-pack packs review bootstrap hooks clean ci

help: ## List targets
	@grep -hE '^[a-zA-Z_-]+:.*## ' $(MAKEFILE_LIST) | \
	  awk 'BEGIN {FS = ":.*## "}; {printf "  \033[1m%-14s\033[0m %s\n", $$1, $$2}'

all: core tools test ## Build the core, the tools and run the tests

core: $(CORE_LIB) ## Build libkfcore.a

tools: $(TOOL_BINS) ## Build dev tools into build/<config>/bin
ifneq ($(UNAME_S),Darwin)
	@echo "note: macOS-only tools skipped on $(UNAME_S): $(MAC_TOOLS)"
endif

test: $(TEST_BIN) ## Run core unit tests
	@$(TEST_BIN)

test-app: $(APP_TEST_BIN) ## Unit tests for the app layer (Objective-C)
	@$(APP_TEST_BIN)

app: $(APP_EXE) $(APP_PLIST) ## Build build/<config>/Keyfeel.app, ad-hoc signed
	@rm -rf $(APP_BUNDLE)/Contents/Resources
	@mkdir -p $(APP_BUNDLE)/Contents/Resources/Packs $(APP_BUNDLE)/Contents/Resources/Labs
	@cp App/Resources/calibration-defaults.json $(APP_BUNDLE)/Contents/Resources/
	@find App/Resources/Packs -maxdepth 1 -name '*.kfpack' -exec cp -R {} $(APP_BUNDLE)/Contents/Resources/Packs/ \;
	@find App/Resources/Labs -maxdepth 1 -type f ! -name '.*' -exec cp {} $(APP_BUNDLE)/Contents/Resources/Labs/ \;
ifneq ($(CONFIG),release)
	@[ -d build/packs/Test-Click.kfpack ] || $(PYTHON) Tools/py/make_test_pack.py --out build/packs
	@cp -R build/packs/Test-Click.kfpack $(APP_BUNDLE)/Contents/Resources/Packs/
endif
	@codesign --force --sign - --options runtime --entitlements App/Resources/Keyfeel.entitlements \
	  $(APP_BUNDLE) 2>&1 | grep -v 'replacing existing signature' || true
	@codesign --verify --strict $(APP_BUNDLE)
	@echo "app: $(APP_BUNDLE) ($$(du -sh $(APP_BUNDLE) | cut -f1))"

run-app: app ## Build and open the app
	@open $(APP_BUNDLE)

test-asan: ## Run core tests under AddressSanitizer + UndefinedBehaviorSanitizer
	@$(MAKE) --no-print-directory CONFIG=asan test

test-tsan: ## Run core tests under ThreadSanitizer
	@KF_STRESS_ITEMS=$${KF_STRESS_ITEMS:-1000000} $(MAKE) --no-print-directory CONFIG=tsan test

test-all: test test-asan test-tsan ## Run tests plain and under every sanitizer

test-py: ## Unit tests for the Python dev tools (needs `make bootstrap`)
	@$(PYTHON) -m unittest discover --start-directory Tools/py --pattern 'test_*.py'

bench: ## Hot-path latency and render time against the budgets (release build)
	@$(MAKE) --no-print-directory CONFIG=release build/release/bin/latency-bench
	@build/release/bin/latency-bench
ifeq ($(UNAME_S),Darwin)
	@$(MAKE) --no-print-directory CONFIG=release build/release/bin/mix-bench
	@build/release/bin/mix-bench
endif

check-rt: ## Fail if a real-time unit (*_rt.c) imports a banned symbol
	@$(MAKE) --no-print-directory CONFIG=release core
	@scripts/check-rt-symbols.sh build/release/obj "$(NM)"

analyze: ## Clang Static Analyzer over the core, the tools and the app
	@for f in $(CORE_SRCS) $(TOOL_SRCS) $(TOOL_COMMON_SRCS); do \
	  $(CLANG) --analyze -Xclang -analyzer-werror $(CSTD) $(PLATFORM) $(INCLUDES) $(TOOL_INCLUDES) \
	    $$f -o /dev/null || exit 1; \
	done
	@for f in $(filter %.m,$(APP_SRCS)); do \
	  $(CLANG) --analyze -Xclang -analyzer-werror -std=gnu11 -fobjc-arc $(PLATFORM) -ICore/include \
	    -IApp/Sources $$f -o /dev/null || exit 1; \
	done
	@for f in Core/src/sensor/kf_sensor.c Core/src/sensor/kf_sensor_rt.c; do \
	  $(CC) $(CFLAGS) -DKF_SENSOR_LAYOUT_MEASURED=1 -fsyntax-only $$f || exit 1; \
	done
	@echo "analyze: clean"

lint: ## clang-tidy over the core, tests and tools
	@tidy='$(CLANG_TIDY)'; [[ -n "$$tidy" ]] || \
	  { echo "clang-tidy not found: install LLVM (Homebrew: llvm) or put it on PATH"; exit 1; }; \
	  "$$tidy" --quiet $(CORE_SRCS) $(TEST_SRCS) $(TOOL_SRCS) $(TOOL_COMMON_SRCS) -- \
	  $(CSTD) $(PLATFORM) $(LINT_SYSROOT) $(INCLUDES) $(TOOL_INCLUDES)
	@echo "lint: clean"

format: ## Reformat C and Objective-C sources in place
	@fmt='$(CLANG_FORMAT)'; [[ -n "$$fmt" ]] || { echo "clang-format not found"; exit 1; }; \
	  "$$fmt" -i $(FORMAT_SRCS)

format-check: ## Fail if any source is not clang-formatted
	@fmt='$(CLANG_FORMAT)'; [[ -n "$$fmt" ]] || { echo "clang-format not found"; exit 1; }; \
	  "$$fmt" --dry-run --Werror $(FORMAT_SRCS)
	@echo "format: clean"

packs: ## Rebuild the bundled packs from Packs/packs.json (fetches missing previews)
	@$(PYTHON) Tools/py/build_packs.py --download

review: ## Build Packs/review/: every bundled and candidate pack with a demo, to judge by ear
	@$(PYTHON) Tools/py/build_review.py

test-pack: ## Generate the synthetic Test Click pack into build/packs
	@$(PYTHON) Tools/py/make_test_pack.py --out build/packs

hooks: ## Install the git pre-commit hook (format, core tests, real-time symbols)
	@ln -sf ../../scripts/pre-commit.sh .git/hooks/pre-commit
	@echo "hooks: pre-commit installed"

bootstrap: ## Check the toolchain and create the repo-local Python venv
	@scripts/bootstrap.sh

ci: format-check lint analyze test test-asan test-tsan test-py test-app check-rt tools app ## Everything CI runs

clean: ## Remove build output
	@rm -rf build

# ---------------------------------------------------------------- rules
# Tests and tools may include the shared tool headers; the core may not.
TOOL_INCLUDES := -ITools/common
$(OBJ)/Tools/%.o $(OBJ)/Tests/%.o: CFLAGS += $(TOOL_INCLUDES)

$(OBJ)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/Tools/%.o: OBJCFLAGS += -ICore/src $(TOOL_INCLUDES)
$(OBJ)/Tests/App/%.o: OBJCFLAGS += -ICore/src -Wno-nullable-to-nonnull-conversion # Tests assert on nils.
$(OBJ)/App/%.o: OBJCFLAGS += $(if $(filter release,$(CONFIG)),-Os) # Spec: -Os for UI code.
$(OBJ)/App/%.o: CFLAGS += -IApp/Sources
$(OBJ)/%.o: %.m
	@mkdir -p $(@D)
	$(CC) $(OBJCFLAGS) -c $< -o $@

$(CORE_LIB): $(CORE_OBJS)
	@mkdir -p $(@D)
	@rm -f $@
	$(ARCHIVE)

$(TEST_BIN): $(TEST_OBJS) $(CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(TEST_OBJS) $(CORE_LIB) $(FRAMEWORKS) $(LDLIBS) -o $@

# Named as targets so make keeps them (not intermediates) and still rebuilds them normally.
$(TOOL_OBJS):
$(MAC_TOOLS:%=$(BIN)/%): $(TOOL_COMMON_OBJS)
$(BIN)/kf-cli: $(TOOL_APP_OBJS)
$(BIN)/mix-bench: LDLIBS += -framework Accelerate

$(BIN)/%: $(OBJ)/Tools/%/main.o $(CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(filter %.o,$^) $(CORE_LIB) $(FRAMEWORKS) $(LDLIBS) \
	  $(if $(filter Darwin,$(UNAME_S)),-fobjc-link-runtime) -o $@

$(APP_EXE): $(APP_OBJS) $(CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(APP_OBJS) $(CORE_LIB) $(APP_FRAMEWORKS) $(LDLIBS) -fobjc-link-runtime -o $@

$(APP_PLIST): App/Resources/Info.plist Config/App.xcconfig Config/Base.xcconfig
	@mkdir -p $(@D)
	@sed -e 's/\$$(EXECUTABLE_NAME)/$(APP_NAME)/g' \
	     -e 's/\$$(PRODUCT_NAME)/$(call xcconfig,PRODUCT_NAME)/g' \
	     -e 's/\$$(PRODUCT_BUNDLE_IDENTIFIER)/$(call xcconfig,PRODUCT_BUNDLE_IDENTIFIER)/g' \
	     -e 's/\$$(MARKETING_VERSION)/$(call xcconfig,MARKETING_VERSION)/g' \
	     -e 's/\$$(CURRENT_PROJECT_VERSION)/$(call xcconfig,CURRENT_PROJECT_VERSION)/g' \
	     -e 's/\$$(MACOSX_DEPLOYMENT_TARGET)/13.0/g' $< > $@
	@plutil -lint -s $@

$(APP_TEST_BIN): $(APP_TEST_OBJS) $(APP_LIB_OBJS) $(CORE_LIB)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $(APP_TEST_OBJS) $(APP_LIB_OBJS) $(CORE_LIB) $(APP_FRAMEWORKS) $(LDLIBS) \
	  -fobjc-link-runtime -o $@

-include $(CORE_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(TOOLS:%=$(OBJ)/Tools/%/main.d) \
         $(TOOL_COMMON_OBJS:.o=.d) $(TOOL_APP_OBJS:.o=.d) $(APP_OBJS:.o=.d) $(APP_TEST_OBJS:.o=.d)
