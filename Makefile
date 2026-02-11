# Sea-Claw Makefile — Arch-detecting build system
#
# Usage:
#   make all        Build everything
#   make test       Build and run tests
#   make clean      Remove build artifacts
#   make install    Install to /usr/local/bin
#
# Override architecture:
#   make ARCH=x86
#   make ARCH=arm
#   make ARCH=generic

# ── Detect architecture ───────────────────────────────────────

UNAME_M := $(shell uname -m)

ifndef ARCH
  ifeq ($(UNAME_M),x86_64)
    ARCH := x86
  else ifeq ($(UNAME_M),aarch64)
    ARCH := arm
  else ifeq ($(UNAME_M),arm64)
    ARCH := arm
  else
    ARCH := generic
  endif
endif

# ── Compiler settings ─────────────────────────────────────────

CC      := gcc
CSTD    := -std=c11
WARNS   := -Wall -Wextra -Werror -Wpedantic
INCLUDES := -I./include
DEFINES  := -D_GNU_SOURCE

# Base optimization
CFLAGS_BASE := $(CSTD) $(WARNS) $(INCLUDES) $(DEFINES)

# Architecture-specific flags
ifeq ($(ARCH),x86)
  ARCH_FLAGS := -mavx2 -mfma -DSEA_ARCH_X86
  $(info [BUILD] Architecture: x86 (AVX2/FMA))
else ifeq ($(ARCH),arm)
  ARCH_FLAGS := -DSEA_ARCH_ARM
  $(info [BUILD] Architecture: ARM (NEON))
else
  ARCH_FLAGS := -DSEA_ARCH_GENERIC
  $(info [BUILD] Architecture: generic (portable C))
endif

# Build modes
CFLAGS_DEBUG   := $(CFLAGS_BASE) $(ARCH_FLAGS) -O0 -g -DDEBUG -fsanitize=address,undefined
CFLAGS_RELEASE := $(CFLAGS_BASE) $(ARCH_FLAGS) -O3 -march=native -flto \
                  -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE

# Default to debug for development
CFLAGS := $(CFLAGS_DEBUG)

# Linker
LDFLAGS := -lm -lcurl -lsqlite3
LDFLAGS_DEBUG := -fsanitize=address,undefined
LDFLAGS_RELEASE := -flto -Wl,--as-needed -pie

# ── Source files ──────────────────────────────────────────────

CORE_SRC := \
	src/core/sea_arena.c \
	src/core/sea_log.c \
	src/core/sea_db.c

SENSES_SRC := \
	src/senses/sea_json.c \
	src/senses/sea_http.c

SHIELD_SRC := \
	src/shield/sea_shield.c

TELEGRAM_SRC := \
	src/telegram/sea_telegram.c

HANDS_SRC := \
	src/hands/sea_tools.c \
	src/hands/impl/tool_echo.c \
	src/hands/impl/tool_system_status.c

MAIN_SRC := src/main.c

ALL_SRC := $(CORE_SRC) $(SENSES_SRC) $(SHIELD_SRC) $(TELEGRAM_SRC) $(HANDS_SRC) $(MAIN_SRC)
ALL_OBJ := $(ALL_SRC:.c=.o)

TEST_ARENA_SRC := tests/test_arena.c
TEST_ARENA_OBJ := $(TEST_ARENA_SRC:.c=.o)

TEST_JSON_SRC := tests/test_json.c
TEST_JSON_OBJ := $(TEST_JSON_SRC:.c=.o)

TEST_SHIELD_SRC := tests/test_shield.c
TEST_SHIELD_OBJ := $(TEST_SHIELD_SRC:.c=.o)

TEST_DB_SRC := tests/test_db.c
TEST_DB_OBJ := $(TEST_DB_SRC:.c=.o)

# ── Output ────────────────────────────────────────────────────

BIN     := sea_claw
DISTDIR := dist
TESTBIN_ARENA  := test_arena
TESTBIN_JSON   := test_json
TESTBIN_SHIELD := test_shield
TESTBIN_DB     := test_db

# ── Targets ───────────────────────────────────────────────────

.PHONY: all clean test install release debug

all: $(BIN)
	@echo ""
	@echo "  \033[32m✓\033[0m Built $(BIN) ($(ARCH), debug)"
	@echo "  Run: ./$(BIN)"
	@echo ""

$(BIN): $(ALL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Release build ─────────────────────────────────────────────

release: CFLAGS := $(CFLAGS_RELEASE)
release: clean $(ALL_OBJ)
	$(CC) $(CFLAGS_RELEASE) -o $(BIN) $(ALL_OBJ) $(LDFLAGS) $(LDFLAGS_RELEASE)
	strip $(BIN)
	@mkdir -p $(DISTDIR)
	cp $(BIN) $(DISTDIR)/
	@ls -lh $(DISTDIR)/$(BIN)
	@echo ""
	@echo "  \033[32m✓\033[0m Release build: $(DISTDIR)/$(BIN)"
	@echo ""

# ── Debug build (explicit) ────────────────────────────────────

debug: CFLAGS := $(CFLAGS_DEBUG)
debug: clean all

# ── Tests ─────────────────────────────────────────────────────

test: $(TESTBIN_ARENA) $(TESTBIN_JSON) $(TESTBIN_SHIELD) $(TESTBIN_DB)
	@echo ""
	@echo "  Running tests..."
	@echo "  ────────────────"
	./$(TESTBIN_ARENA)
	./$(TESTBIN_JSON)
	./$(TESTBIN_SHIELD)
	./$(TESTBIN_DB)
	@echo ""

$(TESTBIN_ARENA): $(TEST_ARENA_OBJ) src/core/sea_arena.o src/core/sea_log.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_JSON): $(TEST_JSON_OBJ) src/core/sea_arena.o src/core/sea_log.o src/senses/sea_json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_SHIELD): $(TEST_SHIELD_OBJ) src/core/sea_log.o src/shield/sea_shield.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_DB): $(TEST_DB_OBJ) src/core/sea_arena.o src/core/sea_log.o src/core/sea_db.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

# ── Clean ─────────────────────────────────────────────────────

clean:
	rm -f $(BIN) $(TESTBIN_ARENA) $(TESTBIN_JSON) $(TESTBIN_SHIELD) $(TESTBIN_DB)
	find src tests -name '*.o' -delete 2>/dev/null || true
	@echo "  Cleaned."

# ── Install ───────────────────────────────────────────────────

install: release
	install -m 755 $(DISTDIR)/$(BIN) /usr/local/bin/$(BIN)
	@echo "  Installed to /usr/local/bin/$(BIN)"
