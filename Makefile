# ==== Projekt ================================================================
TARGET   := gcemu
SRC_DIR  := src
INC_DIR  := include
BUILD    := build
BIN      := $(BUILD)/$(TARGET)

# ==== Toolchain ==============================================================
CC       := cc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes \
            -Wmissing-prototypes -Wpointer-arith -Wcast-qual -Wno-unused-parameter
CPPFLAGS := -I$(INC_DIR) -MMD -MP
CFLAGS   := $(CSTD) $(WARN)
LDFLAGS  :=
LDLIBS   :=

# ==== Build-Modus: make [BUILD_TYPE=debug|release] ===========================
BUILD_TYPE ?= debug
ifeq ($(BUILD_TYPE),debug)
  CFLAGS += -O0 -g3 -fno-omit-frame-pointer -DGCEMU_BUILD_DEBUG=1
else ifeq ($(BUILD_TYPE),release)
  CFLAGS += -O2 -DNDEBUG
else
  $(error BUILD_TYPE muss "debug" oder "release" sein, nicht "$(BUILD_TYPE)")
endif

# Sanitizer optional dazu: make SANITIZE=1
ifeq ($(SANITIZE),1)
  CFLAGS  += -fsanitize=address,undefined
  LDFLAGS += -fsanitize=address,undefined
endif

# ==== Quellen (rekursiv unter src/) ==========================================
SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD)/obj/%.o)
DEPS := $(OBJS:.o=.d)

# ==== Regeln =================================================================
.PHONY: all run clean distclean format compdb help
.DEFAULT_GOAL := all

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

run: $(BIN)
	./$(BIN) $(ARGS)

clean:
	$(RM) -r $(BUILD)/obj $(BIN)

distclean:
	$(RM) -r $(BUILD) compile_commands.json

format:
	@command -v clang-format >/dev/null || { echo "clang-format nicht gefunden"; exit 1; }
	clang-format -i $(SRCS) $(shell find $(INC_DIR) -name '*.h')

# compile_commands.json fuer clangd/IDE (braucht "bear")
compdb:
	@command -v bear >/dev/null || { echo "bear nicht gefunden (brew install bear)"; exit 1; }
	$(MAKE) clean
	bear -- $(MAKE) all

help:
	@echo "make                 - Debug-Build nach $(BIN)"
	@echo "make BUILD_TYPE=release"
	@echo "make SANITIZE=1      - mit ASan/UBSan"
	@echo "make run ARGS=rom.iso"
	@echo "make clean | distclean | format | compdb"

-include $(DEPS)
