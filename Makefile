
# ==== Projekt ================================================================
TARGET   := gcemu
SRC_DIR  := src
INC_DIR  := include
BUILD    := build
BIN      := $(BUILD)/$(TARGET)

# Globale Konfiguration: wird ueber "-include" automatisch in jede
# Uebersetzungseinheit eingefuegt, muss also nirgends von Hand inkludiert
# werden. Aenderungen daran loesen ueber die .d-Dateien einen Rebuild aus.
CONFIG_H := $(SRC_DIR)/core/config/config.h

# ==== Toolchain ==============================================================
CC       := cc

CSTD     := -std=c11

WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
            -Wstrict-prototypes -Wmissing-prototypes \
            -Wpointer-arith -Wcast-qual -Wno-unused-parameter

CPPFLAGS := -I$(INC_DIR) -I$(SRC_DIR) -include $(CONFIG_H) -MMD -MP
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

# ==== Vendor (Fremdcode in include/) =========================================
# Fremdbibliotheken liegen als Quellen unter include/<lib>/ und werden mit
# entschaerften Warnungen und ohne die globale config.h uebersetzt.
VENDOR_SRCS := $(shell find $(INC_DIR) -type f -name '*.c')
VENDOR_OBJS := $(VENDOR_SRCS:$(INC_DIR)/%.c=$(BUILD)/obj/vendor/%.o)

VENDOR_CPPFLAGS := -I$(INC_DIR) -MMD -MP
VENDOR_CFLAGS   := $(CSTD) -w

DEPS := $(OBJS:.o=.d) $(VENDOR_OBJS:.o=.d)

# ==== Regeln =================================================================
.PHONY: all run lsp clean distclean format compdb help

.DEFAULT_GOAL := all

# ==== Build ==================================================================

all: $(BIN) compile_flags.txt

$(BIN): $(OBJS) $(VENDOR_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BUILD)/obj/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/obj/vendor/%.o: $(INC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CPPFLAGS) $(VENDOR_CFLAGS) -c $< -o $@

# ==== clangd (LSP) ===========================================================
# compile_flags.txt gibt clangd exakt die Flags des echten Builds - inklusive
# "-include $(CONFIG_H)", damit die globalen Makros auch im Editor bekannt
# sind. Ein Argument pro Zeile; relative Pfade loest clangd gegen das
# Verzeichnis der compile_flags.txt auf, also gegen die Projektwurzel.
#
# Hinweis: existiert eine compile_commands.json (siehe "make compdb"), hat die
# fuer clangd Vorrang.
CLANGD_FLAGS := $(CPPFLAGS) $(CFLAGS) -Wno-unknown-warning-option

lsp: compile_flags.txt

compile_flags.txt: $(MAKEFILE_LIST)
	@printf '%s\n' $(filter-out -MMD -MP,$(CLANGD_FLAGS)) > $@

# ==== Run ====================================================================

run: $(BIN)
	./$(BIN) $(ARGS)

# ==== Clean ==================================================================

clean:
	$(RM) -r $(BUILD)/obj $(BIN)

distclean: clean
	$(RM) -r $(BUILD) compile_commands.json compile_flags.txt

# ==== Format =================================================================

format:
	@command -v clang-format >/dev/null || { \
		echo "clang-format nicht gefunden"; \
		exit 1; \
	}
	clang-format -i $(SRCS) $(shell find $(SRC_DIR) -type f -name '*.h')

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
	@echo "make lsp                   - compile_flags.txt fuer clangd erzeugen"
	@echo "make compdb                - compile_commands.json erzeugen"

# ==== Dependency Files =======================================================

-include $(DEPS)


