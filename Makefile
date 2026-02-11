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
LDFLAGS := -lm -lcurl -lsqlite3 -lpthread
LDFLAGS_DEBUG := -fsanitize=address,undefined
LDFLAGS_RELEASE := -flto -Wl,--as-needed -pie

# ── Source files ──────────────────────────────────────────────

CORE_SRC := \
	src/core/sea_arena.c \
	src/core/sea_log.c \
	src/core/sea_db.c \
	src/core/sea_config.c

SENSES_SRC := \
	src/senses/sea_json.c \
	src/senses/sea_http.c

SHIELD_SRC := \
	src/shield/sea_shield.c

TELEGRAM_SRC := \
	src/telegram/sea_telegram.c

BRAIN_SRC := \
	src/brain/sea_agent.c

A2A_SRC := \
	src/a2a/sea_a2a.c

BUS_SRC := \
	src/bus/sea_bus.c

CHANNEL_SRC := \
	src/channels/sea_channel.c \
	src/channels/channel_telegram.c

SESSION_SRC := \
	src/session/sea_session.c

MEMORY_SRC := \
	src/memory/sea_memory.c

HANDS_SRC := \
	src/hands/sea_tools.c \
	src/hands/impl/tool_echo.c \
	src/hands/impl/tool_system_status.c \
	src/hands/impl/tool_file_read.c \
	src/hands/impl/tool_file_write.c \
	src/hands/impl/tool_shell_exec.c \
	src/hands/impl/tool_web_fetch.c \
	src/hands/impl/tool_task_manage.c \
	src/hands/impl/tool_db_query.c \
	src/hands/impl/tool_exa_search.c \
	src/hands/impl/tool_text_summarize.c \
	src/hands/impl/tool_text_transform.c \
	src/hands/impl/tool_json_format.c \
	src/hands/impl/tool_hash_compute.c \
	src/hands/impl/tool_env_get.c \
	src/hands/impl/tool_dir_list.c \
	src/hands/impl/tool_file_info.c \
	src/hands/impl/tool_process_list.c \
	src/hands/impl/tool_dns_lookup.c \
	src/hands/impl/tool_timestamp.c \
	src/hands/impl/tool_math_eval.c \
	src/hands/impl/tool_uuid_gen.c \
	src/hands/impl/tool_random_gen.c \
	src/hands/impl/tool_url_parse.c \
	src/hands/impl/tool_encode_decode.c \
	src/hands/impl/tool_regex_match.c \
	src/hands/impl/tool_csv_parse.c \
	src/hands/impl/tool_diff_text.c \
	src/hands/impl/tool_grep_text.c \
	src/hands/impl/tool_wc.c \
	src/hands/impl/tool_head_tail.c \
	src/hands/impl/tool_sort_text.c \
	src/hands/impl/tool_net_info.c \
	src/hands/impl/tool_cron_parse.c \
	src/hands/impl/tool_disk_usage.c \
	src/hands/impl/tool_syslog_read.c \
	src/hands/impl/tool_json_query.c \
	src/hands/impl/tool_http_request.c \
	src/hands/impl/tool_string_replace.c \
	src/hands/impl/tool_calendar.c \
	src/hands/impl/tool_checksum_file.c \
	src/hands/impl/tool_file_search.c \
	src/hands/impl/tool_uptime.c \
	src/hands/impl/tool_ip_info.c \
	src/hands/impl/tool_whois_lookup.c \
	src/hands/impl/tool_ssl_check.c \
	src/hands/impl/tool_json_to_csv.c \
	src/hands/impl/tool_weather.c \
	src/hands/impl/tool_unit_convert.c \
	src/hands/impl/tool_password_gen.c \
	src/hands/impl/tool_count_lines.c

MAIN_SRC := src/main.c

ALL_SRC := $(CORE_SRC) $(SENSES_SRC) $(SHIELD_SRC) $(TELEGRAM_SRC) $(BRAIN_SRC) $(A2A_SRC) $(BUS_SRC) $(CHANNEL_SRC) $(SESSION_SRC) $(MEMORY_SRC) $(HANDS_SRC) $(MAIN_SRC)
ALL_OBJ := $(ALL_SRC:.c=.o)

TEST_ARENA_SRC := tests/test_arena.c
TEST_ARENA_OBJ := $(TEST_ARENA_SRC:.c=.o)

TEST_JSON_SRC := tests/test_json.c
TEST_JSON_OBJ := $(TEST_JSON_SRC:.c=.o)

TEST_SHIELD_SRC := tests/test_shield.c
TEST_SHIELD_OBJ := $(TEST_SHIELD_SRC:.c=.o)

TEST_DB_SRC := tests/test_db.c
TEST_DB_OBJ := $(TEST_DB_SRC:.c=.o)

TEST_CONFIG_SRC := tests/test_config.c
TEST_CONFIG_OBJ := $(TEST_CONFIG_SRC:.c=.o)

TEST_BUS_SRC := tests/test_bus.c
TEST_BUS_OBJ := $(TEST_BUS_SRC:.c=.o)

TEST_SESSION_SRC := tests/test_session.c
TEST_SESSION_OBJ := $(TEST_SESSION_SRC:.c=.o)

TEST_MEMORY_SRC := tests/test_memory.c
TEST_MEMORY_OBJ := $(TEST_MEMORY_SRC:.c=.o)

# ── Output ────────────────────────────────────────────────────

BIN     := sea_claw
DISTDIR := dist
TESTBIN_ARENA  := test_arena
TESTBIN_JSON   := test_json
TESTBIN_SHIELD := test_shield
TESTBIN_DB     := test_db
TESTBIN_CONFIG := test_config
TESTBIN_BUS    := test_bus
TESTBIN_SESSION := test_session
TESTBIN_MEMORY  := test_memory

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

test: $(TESTBIN_ARENA) $(TESTBIN_JSON) $(TESTBIN_SHIELD) $(TESTBIN_DB) $(TESTBIN_CONFIG) $(TESTBIN_BUS) $(TESTBIN_SESSION) $(TESTBIN_MEMORY)
	@echo ""
	@echo "  Running tests..."
	@echo "  ────────────────"
	./$(TESTBIN_ARENA)
	./$(TESTBIN_JSON)
	./$(TESTBIN_SHIELD)
	./$(TESTBIN_DB)
	./$(TESTBIN_CONFIG)
	./$(TESTBIN_BUS)
	./$(TESTBIN_SESSION)
	./$(TESTBIN_MEMORY)
	@echo ""

$(TESTBIN_ARENA): $(TEST_ARENA_OBJ) src/core/sea_arena.o src/core/sea_log.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_JSON): $(TEST_JSON_OBJ) src/core/sea_arena.o src/core/sea_log.o src/senses/sea_json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_SHIELD): $(TEST_SHIELD_OBJ) src/core/sea_log.o src/shield/sea_shield.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_DB): $(TEST_DB_OBJ) src/core/sea_arena.o src/core/sea_log.o src/core/sea_db.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_CONFIG): $(TEST_CONFIG_OBJ) src/core/sea_arena.o src/core/sea_log.o src/core/sea_config.o src/senses/sea_json.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_BUS): $(TEST_BUS_OBJ) src/bus/sea_bus.o src/core/sea_arena.o src/core/sea_log.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_SESSION): $(TEST_SESSION_OBJ) src/session/sea_session.o src/core/sea_arena.o src/core/sea_log.o src/core/sea_db.o src/brain/sea_agent.o src/senses/sea_http.o src/senses/sea_json.o src/shield/sea_shield.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

$(TESTBIN_MEMORY): $(TEST_MEMORY_OBJ) src/memory/sea_memory.o src/core/sea_arena.o src/core/sea_log.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDFLAGS_DEBUG)

# ── Clean ─────────────────────────────────────────────────────

clean:
	rm -f $(BIN) $(TESTBIN_ARENA) $(TESTBIN_JSON) $(TESTBIN_SHIELD) $(TESTBIN_DB) $(TESTBIN_CONFIG) $(TESTBIN_BUS) $(TESTBIN_SESSION) $(TESTBIN_MEMORY)
	find src tests -name '*.o' -delete 2>/dev/null || true
	@echo "  Cleaned."

# ── Install ───────────────────────────────────────────────────

install: release
	install -m 755 $(DISTDIR)/$(BIN) /usr/local/bin/$(BIN)
	@echo "  Installed to /usr/local/bin/$(BIN)"
