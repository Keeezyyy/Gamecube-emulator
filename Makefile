
# ==== Projekt ================================================================
TARGET   := gcemu
SRC_DIR  := src
INC_DIR  := include
BUILD    := build
BIN      := $(BUILD)/$(TARGET)

# ==== Toolchain ==============================================================
CC       := cc

CSTD     := -std=c11

WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
            -Wstrict-prototypes -Wmissing-prototypes \
            -Wpointer-arith -Wcast-qual -Wno-unused-parameter

CPPFLAGS := -I$(INC_DIR) -MMD -MP
CFLAGS   := $(CSTD) $(WARN)

LDFLAGS  :=
LDLIBS   :=

# ==== Build-Modus ============================================================
# Verwendung:
#   make
#   make BUILD_TYPE=debug
#   make BUILD_TYPE=release

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),debug)
    CFLAGS += -O0 -g3 -fno-omit-frame-pointer -DGCEMU_BUILD_DEBUG=1
else ifeq ($(BUILD_TYPE),release)
    CFLAGS += -O2 -DNDEBUG
else
    $(error BUILD_TYPE muss "debug" oder "release" sein, nicht "$(BUILD_TYPE)")
endif

# ==== Sanitizer ==============================================================
# Verwendung:
#   make SANITIZE=1

ifeq ($(SANITIZE),1)
    CFLAGS  += -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
endif

# ==== Quellen ================================================================
# Nur C-Dateien werden kompiliert.
SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')

OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD)/obj/%.o)
DEPS := $(OBJS:.o=.d)

# ==== Regeln =================================================================
.PHONY: all run clean distclean format compdb help

.DEFAULT_GOAL := all

# ==== Build ==================================================================

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# ==== Run ====================================================================

run: $(BIN)
	./$(BIN) $(ARGS)

# ==== Clean ==================================================================

clean:
	$(RM) -r $(BUILD)/obj $(BIN)

distclean:
	$(RM) -r $(BUILD) compile_commands.json

# ==== Format =================================================================

format:
	@command -v clang-format >/dev/null || { \
		echo "clang-format nicht gefunden"; \
		exit 1; \
	}
	clang-format -i $(SRCS) $(shell find $(INC_DIR) -type f -name '*.h')

# ==== compile_commands.json ===================================================
# Benötigt: brew install bear

compdb:
	@command -v bear >/dev/null || { \
		echo "bear nicht gefunden (brew install bear)"; \
		exit 1; \
	}
	$(MAKE) clean
	bear -- $(MAKE) all

# ==== Hilfe ==================================================================

help:
	@echo "make                       - Debug-Build"
	@echo "make BUILD_TYPE=debug      - Debug-Build"
	@echo "make BUILD_TYPE=release    - Release-Build"
	@echo "make SANITIZE=1            - ASan/UBSan aktivieren"
	@echo "make run ARGS=rom.iso      - Emulator starten"
	@echo "make clean                 - Build-Dateien entfernen"
	@echo "make distclean             - kompletten Build entfernen"
	@echo "make format                - C-Code formatieren"
	@echo "make compdb                - compile_commands.json erzeugen"

# ==== Dependency Files =======================================================

-include $(DEPS)


