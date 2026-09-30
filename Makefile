
# ==== Projekt ================================================================
TARGET   := gcemu
SRC_DIR  := src
INC_DIR  := include
BUILD    := build

# Globale Konfiguration: wird ueber "-include" automatisch in jede
# Uebersetzungseinheit eingefuegt, muss also nirgends von Hand inkludiert
# werden. Aenderungen daran loesen ueber die .d-Dateien einen Rebuild aus.
CONFIG_H := $(SRC_DIR)/core/config/config.h

# ==== Toolchain ==============================================================
CC       := cc
CXX      := c++

CSTD     := -std=c11

WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
            -Wstrict-prototypes -Wmissing-prototypes \
            -Wpointer-arith -Wcast-qual -Wno-unused-parameter

CPPFLAGS := -I$(INC_DIR) -I$(SRC_DIR) -include $(CONFIG_H) -MMD -MP
CFLAGS   := $(CSTD) $(WARN)

# Assembler: derselbe Treiber wie fuer C, damit Ziel-Architektur, Sysroot und
# Linker-Konventionen automatisch zusammenpassen.
AS       := $(CC)
ASFLAGS  :=

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

LDFLAGS := 
LDLIBS  :=

# macOS: GLFW aus Homebrew + Apples OpenGL-Framework
ifeq ($(UNAME_S),Darwin)
    GLFW_PREFIX := $(shell brew --prefix glfw)
    LDFLAGS += -L$(GLFW_PREFIX)/lib
    LDLIBS  += -lglfw -framework OpenGL

    # SDL3 aus Homebrew (brew install sdl3)
    SDL3_PREFIX := $(shell brew --prefix sdl3)
    CPPFLAGS += -I$(SDL3_PREFIX)/include
    LDFLAGS  += -L$(SDL3_PREFIX)/lib -Wl,-rpath,$(SDL3_PREFIX)/lib
    LDLIBS   += -lSDL3

    # raylib aus Homebrew (brew install raylib)
    RAYLIB_PREFIX := $(shell brew --prefix raylib)
    CPPFLAGS += -I$(RAYLIB_PREFIX)/include
    LDFLAGS  += -L$(RAYLIB_PREFIX)/lib -Wl,-rpath,$(RAYLIB_PREFIX)/lib
    LDLIBS   += -lraylib

    # OpenBLAS aus Homebrew (brew install openblas) fuer <cblas.h>. Die
    # Formel ist keg-only, Header und Lib liegen also nicht im Standardpfad.
    OPENBLAS_PREFIX := $(shell brew --prefix openblas)
    CPPFLAGS += -I$(OPENBLAS_PREFIX)/include
    LDFLAGS  += -L$(OPENBLAS_PREFIX)/lib -Wl,-rpath,$(OPENBLAS_PREFIX)/lib
    LDLIBS   += -lopenblas
else
    LDLIBS   += -lopenblas -lraylib
endif

# Debugger fuer "make debug". Standard ist gdb; per Kommandozeile
# ueberschreibbar, z.B. "make debug DEBUGGER=lldb" (auf macOS meist der
# einfachere Weg, da gdb dort signiert werden muss).
DEBUGGER ?= lldb

# ==== Build-Modus ============================================================
# Verwendung:
#   make                           Debug (Standard)
#   make BUILD_TYPE=debug
#   make BUILD_TYPE=release        oder kurz:  make release
#   make release NATIVE=1          zusaetzlich auf die eigene CPU zuschneiden
#
# Debug- und Release-Build haben getrennte Ausgabeverzeichnisse (siehe
# OUT_DIR), ein Wechsel des BUILD_TYPE braucht also kein "make clean".

BUILD_TYPE ?= debug

ifeq ($(BUILD_TYPE),debug)
    OPTFLAGS := -O0 -g3 -fno-omit-frame-pointer
    CFLAGS   += -DGCEMU_BUILD_DEBUG=1
    ASFLAGS  += -g
else ifeq ($(BUILD_TYPE),release)
    # -O3                hoechste Optimierungsstufe des Compilers
    # -flto              Link Time Optimization: Inlining ueber Dateigrenzen
    # -fomit-frame-p.    ein Register mehr, dafuer schlechtere Backtraces
    # -f*-sections       erlaubt dem Linker, ungenutzten Code/Daten zu werfen
    # -fno-math-errno    erspart errno-Behandlung bei libm-Aufrufen
    #                    (kein -ffast-math: die Gast-FPU muss exakt bleiben)
    OPTFLAGS := -O3 -flto -fomit-frame-pointer \
                -ffunction-sections -fdata-sections -fno-math-errno
    CFLAGS   += -DNDEBUG -DGCEMU_BUILD_RELEASE=1
    LDFLAGS  += -O3 -flto

    ifeq ($(UNAME_S),Darwin)
        LDFLAGS += -Wl,-dead_strip
    else
        LDFLAGS += -Wl,--gc-sections -Wl,-s
    endif

    # NATIVE=1 erzeugt Code fuer genau diese CPU (schneller, das Binary
    # laeuft dann aber nicht mehr zwingend auf anderen Maschinen).
    ifeq ($(NATIVE),1)
        NATIVE_SUFFIX := -native
        ifneq ($(filter arm64 aarch64,$(UNAME_M)),)
            OPTFLAGS += -mcpu=native
        else
            OPTFLAGS += -march=native -mtune=native
        endif
    endif
else
    $(error BUILD_TYPE muss "debug" oder "release" sein, nicht "$(BUILD_TYPE)")
endif

CFLAGS += $(OPTFLAGS)

# ==== Sanitizer ==============================================================
# Verwendung:
#   make SANITIZE=1

ifeq ($(SANITIZE),1)
    CFLAGS  += -fsanitize=address,undefined
    LDFLAGS += -fsanitize=address,undefined
    SAN_SUFFIX := -asan
endif

ifeq ($(CLOCK_STATS),1)
    CFLAGS += -DCLOCK_STATS
    CLOCK_SUFFIX := -clock
endif

# ==== Ausgabeverzeichnis =====================================================
# Jede Variante bekommt einen eigenen Ausgabebaum (Objekte und Binary), damit
# sich Builds mit unterschiedlichen Flags nie gegenseitig ueberschreiben. Ein
# Wechsel braucht deshalb kein "make clean":
#
#   build/debug/gcemu            build/debug/obj/...
#   build/debug-asan/gcemu       Sanitizer-Build
#   build/release/gcemu          build/release/obj/...
#   build/release-native/gcemu   Release mit -mcpu/-march=native
BUILD_TAG := $(BUILD_TYPE)$(NATIVE_SUFFIX)$(SAN_SUFFIX)$(CLOCK_SUFFIX)

OUT_DIR  := $(BUILD)/$(BUILD_TAG)
OBJ_DIR  := $(OUT_DIR)/obj
BIN      := $(OUT_DIR)/$(TARGET)

# ==== Quellen ================================================================
# C-Dateien und handgeschriebener Assembler. Beides landet im selben
# Objektverzeichnis und wird am Ende zusammen gelinkt.
#
#   *.s  - roher Assembler, wird NICHT vom Praeprozessor angefasst
#   *.S  - Assembler mit Praeprozessor, kann also #include/#define nutzen
#
# Nicht eingebunden: *.asm (NASM-Syntax). Der Treiber "cc" kennt dieses
# Format nicht; solche Dateien muessten erst nach *.s portiert werden.
# Das abgeloeste OpenGL-Backend liegt nur noch als Referenz im Baum und
# definiert dieselben Symbole wie das neue (load_shader, ...).
SRC_EXCLUDE := */opengl-depreciated/*

SRCS     := $(shell find $(SRC_DIR) -type f -name '*.c' -not -path '$(SRC_EXCLUDE)')
ASM_SRCS := $(shell find $(SRC_DIR) -type f \( -name '*.s' -o -name '*.S' \))

OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
ASM_OBJS := $(patsubst $(SRC_DIR)/%.s,$(OBJ_DIR)/%.o, \
              $(patsubst $(SRC_DIR)/%.S,$(OBJ_DIR)/%.o,$(ASM_SRCS)))

# ==== Vendor (Fremdcode in include/) =========================================
# Fremdbibliotheken liegen als Quellen unter include/<lib>/ und werden mit
# entschaerften Warnungen und ohne die globale config.h uebersetzt.
# Testprogramme der Fremdbibliotheken (eigene main()) bleiben draussen.
#
# Die Objekte liegen ausserhalb von $(OUT_DIR) unter build/vendor/<typ>/, damit
# "make clean" sie stehen laesst - der Fremdcode aendert sich praktisch nie und
# oaknut (C++20) ist teuer zu uebersetzen. Getrennt wird nur nach den Flags,
# die hier tatsaechlich eingehen (BUILD_TYPE, NATIVE); SANITIZE und
# CLOCK_STATS teilen sich die Objekte. Weg damit: "make vendorclean" oder
# "make distclean".
VENDOR_DIR  := $(BUILD)/vendor/$(BUILD_TYPE)$(NATIVE_SUFFIX)
VENDOR_SRCS := $(shell find $(INC_DIR) -type f -name '*.c' -not -path '*/tests/*')
VENDOR_OBJS := $(VENDOR_SRCS:$(INC_DIR)/%.c=$(VENDOR_DIR)/%.o)

VENDOR_CPPFLAGS := -I$(INC_DIR) -MMD -MP
VENDOR_CFLAGS   := $(CSTD) -w $(OPTFLAGS)

# oaknut (AArch64-Assembler, header-only C++20) mit seiner C-API. Nur die
# C-API wird uebersetzt; dafuer muss mit $(CXX) gelinkt werden, damit die
# C++-Runtime mitkommt.
OAKNUT_DIR      := $(INC_DIR)/oaknut/oaknut
OAKNUT_SRCS     := $(OAKNUT_DIR)/c_api/src/oaknut_c.cpp $(OAKNUT_DIR)/c_api/src/oaknut_enc.cpp
OAKNUT_OBJS     := $(OAKNUT_SRCS:$(INC_DIR)/%.cpp=$(VENDOR_DIR)/%.o)
OAKNUT_CPPFLAGS := -I$(OAKNUT_DIR)/include -I$(OAKNUT_DIR)/c_api/include -MMD -MP
OAKNUT_CXXFLAGS := -std=c++20 -w $(OPTFLAGS)

CPPFLAGS += -I$(OAKNUT_DIR)/c_api/include

DEPS := $(OBJS:.o=.d) $(VENDOR_OBJS:.o=.d) $(OAKNUT_OBJS:.o=.d)

# ==== Shader =================================================================
# GLSL-Quellen fuer OpenGL. Der Treiber uebersetzt sie erst zur Laufzeit
# (glShaderSource/glCompileShader), der Build prueft sie aber vorab mit
# glslangValidator (OpenGL-Semantik, kein SPIR-V), damit Syntaxfehler schon
# beim "make" auffallen, und kopiert sie danach nach ./build/shader/.
# Das Ziel ist bewusst unabhaengig vom BUILD_TYPE (Pfad relativ zur
# Projektwurzel).
#
# Die Stage steckt im Dateinamen: vert.glsl / frag.glsl oder
# <name>.vert.glsl / <name>.frag.glsl.
#
#   brew install glslang
GLSLC      := glslangValidator
SHADER_DIR := $(BUILD)/shader

# Das alte Backend liegt noch als Referenz im Baum. Seine Shader heissen
# genauso wie die neuen (vert.glsl/frag.glsl) und wuerden sie in build/shader/
# ueberdecken, deshalb bleiben sie hier draussen.
SHADER_EXCLUDE := */opengl-depreciated/*

SHADER_SRCS := $(shell find $(SRC_DIR) -type f -not -path '$(SHADER_EXCLUDE)' \
                                               \( -name 'vert.glsl' -o -name '*.vert.glsl' \
                                                -o -name 'frag.glsl' -o -name '*.frag.glsl' \))
SHADER_OUTS := $(addprefix $(SHADER_DIR)/,$(notdir $(SHADER_SRCS)))

vpath %.glsl $(sort $(dir $(SHADER_SRCS)))

# Gemeinsame Bausteine (Structs, Funktionen) liegen unter shader/lib/ und
# werden per
#
#   #include "lib/types.glsl"
#
# eingebunden, relativ zur einbindenden Datei. GLSL kennt kein #include,
# tools/shader_include.c setzt die Dateien deshalb beim Build zu einer einzigen
# zusammen. In build/shader/ liegt also immer schon das fertige Ergebnis, der
# Emulator laedt weiterhin nur vert.glsl und frag.glsl.
SHADER_LIBS    := $(shell find $(SRC_DIR) -type f -path '*/shader/lib/*.glsl' \
                                          -not -path '$(SHADER_EXCLUDE)')
SHADER_INCLUDE := $(BUILD)/tools/shader_include

# glslangValidator kennt die Grenzen des echten Treibers nicht (z.B. maximal
# 16 Vertex-Attribute unter macOS). tools/shader_check.c uebersetzt und linkt
# das Shader-Paar deshalb zusaetzlich in einem unsichtbaren OpenGL-Kontext,
# also genau so wie der Emulator zur Laufzeit. Schlaegt das fehl, bricht der
# Build ab, bevor das Binary gelinkt wird.
SHADER_CHECK      := $(BUILD)/tools/shader_check
SHADER_CHECK_LIBS := -lglfw
ifeq ($(UNAME_S),Darwin)
    SHADER_CHECK_LIBS := -L$(GLFW_PREFIX)/lib -lglfw -framework OpenGL
else
    SHADER_CHECK_LIBS += -lGL
endif
SHADER_STAMP := $(SHADER_DIR)/.linked

# ==== Regeln =================================================================
.PHONY: all shaders asm run debug release lsp clean vendorclean distclean format compdb help test test-build test-vertex test-software

# Baut nur den Assembler-Teil - praktisch beim Debuggen der .s-Dateien.
asm: $(ASM_OBJS)

.DEFAULT_GOAL := all

# ==== Build ==================================================================

all: $(BIN) shaders compile_flags.txt

shaders: $(SHADER_OUTS) $(SHADER_STAMP)

$(SHADER_CHECK): tools/shader_check.c $(VENDOR_DIR)/glad/gl.o
	@mkdir -p $(dir $@)
	$(CC) -I$(INC_DIR) $^ $(SHADER_CHECK_LIBS) -o $@

$(SHADER_STAMP): $(SHADER_OUTS) $(SHADER_CHECK)
	$(SHADER_CHECK) $(SHADER_DIR)/vert.glsl $(SHADER_DIR)/frag.glsl
	@touch $@

# Stage aus dem Dateinamen ableiten: ".../vert.glsl" und ".../x.vert.glsl"
# enden beide auf "vert.glsl".
shader_stage = $(if $(filter %vert.glsl,$(1)),vert,frag)

$(SHADER_INCLUDE): tools/shader_include.c
	@mkdir -p $(dir $@)
	$(CC) $< -o $@

# Erst zusammensetzen, dann das Ergebnis pruefen: glslangValidator sieht damit
# genau den Text, den spaeter auch der Treiber bekommt.
$(SHADER_OUTS): $(SHADER_DIR)/%.glsl: %.glsl $(SHADER_LIBS) $(SHADER_INCLUDE)
	@mkdir -p $(dir $@)
	$(SHADER_INCLUDE) $< $@.tmp
	$(GLSLC) -S $(call shader_stage,$<) $@.tmp
	mv $@.tmp $@

$(BIN): $(OBJS) $(ASM_OBJS) $(VENDOR_OBJS) $(OAKNUT_OBJS) | shaders
	@mkdir -p $(dir $@)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Assembler ohne Praeprozessor.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.s
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

# Assembler mit Praeprozessor: bekommt dieselben Include-Pfade wie C, damit
# gemeinsame Header (Offsets, Konstanten) genutzt werden koennen.
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	@mkdir -p $(dir $@)
	$(AS) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(VENDOR_DIR)/%.o: $(INC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(VENDOR_CPPFLAGS) $(VENDOR_CFLAGS) -c $< -o $@

$(VENDOR_DIR)/%.o: $(INC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(OAKNUT_CPPFLAGS) $(OAKNUT_CXXFLAGS) -c $< -o $@

# ==== clangd (LSP) ===========================================================
# compile_flags.txt gibt clangd exakt die Flags des echten Builds - inklusive
# "-include $(CONFIG_H)", damit die globalen Makros auch im Editor bekannt
# sind. Ein Argument pro Zeile; relative Pfade loest clangd gegen das
# Verzeichnis der compile_flags.txt auf, also gegen die Projektwurzel.
#
# Hinweis: existiert eine compile_commands.json (siehe "make compdb"), hat die
# fuer clangd Vorrang.
# Bewusst nicht aus $(CFLAGS) abgeleitet: sonst haette ein "make release" die
# Editor-Sicht dauerhaft auf -O3/-DNDEBUG umgestellt. clangd interessieren nur
# Includes, Standard, Warnungen und Makros - die bleiben hier auf Debug.
CLANGD_FLAGS := $(CPPFLAGS) $(CSTD) $(WARN) -DGCEMU_BUILD_DEBUG=1 \
                -Wno-unknown-warning-option

lsp: compile_flags.txt

compile_flags.txt: $(MAKEFILE_LIST)
	@printf '%s\n' $(filter-out -MMD -MP,$(CLANGD_FLAGS)) > $@

# ==== Release ================================================================
# Baut mit allen Optimierungen (-O3 + LTO, siehe oben) nach build/release/.
# Der Debug-Baum bleibt dabei unangetastet.
#
#   make release
#   make release NATIVE=1               auf die CPU dieser Maschine zuschneiden
#   make run BUILD_TYPE=release ARGS=..  Release-Binary starten

release:
	$(MAKE) BUILD_TYPE=release all

# ==== Run ====================================================================

run: $(BIN) shaders
	./$(BIN) $(ARGS)

# ==== Debug (gdb) ============================================================
# Startet den Emulator unter dem Debugger. Erzwingt einen Debug-Build
# (-O0 -g3), damit Symbole und Zeilennummern auch dann stimmen, wenn zuvor  ein
# Release-Build im Baum lag.
#
#   make debug
#   make debug ARGS=rom.iso          - Argumente an den Emulator durchreichen
#   make debug DEBUGGER=lldb         - anderen Debugger verwenden
#
# Hinweis macOS: gdb braucht ein Code-Signing-Zertifikat, sonst schlaegt das
# Anhaengen an den Prozess fehl. Ohne ein solches Setup ist lldb die
# unkompliziertere Wahl.

debug:
	@command -v $(DEBUGGER) >/dev/null || { \
		echo "$(DEBUGGER) nicht gefunden"; \
		exit 1; \
	}
	$(MAKE) BUILD_TYPE=debug $(BIN) shaders
	@case '$(DEBUGGER)' in \
		*lldb*) $(DEBUGGER) -- ./$(BIN) $(ARGS) ;; \
		*)      $(DEBUGGER) --args ./$(BIN) $(ARGS) ;; \
	esac

# ==== Clean ==================================================================

# Raeumt beide Build-Typen ab, nicht nur den gerade eingestellten. Die
# Vendor-Objekte unter build/vendor/ bleiben stehen (siehe oben).
clean:
	$(RM) -r $(BUILD)/debug* $(BUILD)/release* $(SHADER_DIR)

vendorclean:
	$(RM) -r $(BUILD)/vendor

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

# ==== Tests ==================================================================
# Die Unit-Tests fuer Gast-Instruktionen liegen komplett unter test/ und haben
# ein eigenes Makefile: sie uebersetzen die Emulatorquellen ohne src/main.c ein
# zweites Mal und haengen einen eigenen Testrunner davor.
#
#   make test               alles bauen und ausfuehren
#   make test ARGS=stw      nur Testfaelle mit "stw" im Namen
#   make test-build         nur bauen
#
# Gebraucht wird zusaetzlich ein PowerPC-Cross-Compiler fuer die
# Gastprogramme:  brew install llvm lld

test:
	$(MAKE) -C test run ARGS="$(ARGS)"

test-build:
	$(MAKE) -C test

test-vertex:
	$(MAKE) -C test run-vertex

test-software:
	$(MAKE) -C test run-software ARGS="$(ARGS)"

# ==== Hilfe ==================================================================

help:
	@echo "make                       - Debug-Build"
	@echo "make BUILD_TYPE=debug      - Debug-Build"
	@echo "make release               - Release-Build (-O3, LTO, dead-strip)"
	@echo "make BUILD_TYPE=release    - dasselbe, ausgeschrieben"
	@echo "make release NATIVE=1      - Release fuer genau diese CPU"
	@echo "make SANITIZE=1            - ASan/UBSan aktivieren"
	@echo "make CLOCK_STATS=1         - Gast-Takt (Durchschnitt) bei Ctrl+C/Abbruch ausgeben"
	@echo "make run ARGS=rom.iso      - Emulator starten (Debug-Binary)"
	@echo "make run BUILD_TYPE=release - Release-Binary starten"
	@echo "make debug ARGS=rom.iso    - Debug-Build unter gdb starten"
	@echo "make debug DEBUGGER=lldb   - stattdessen lldb verwenden"
	@echo "make clean                 - Build-Dateien entfernen (ohne Vendor-Objekte)"
	@echo "make vendorclean           - Vendor-Objekte (include/) entfernen"
	@echo "make distclean             - kompletten Build entfernen"
	@echo "make format                - C-Code formatieren"
	@echo "make asm                   - nur die Assembler-Objekte bauen"
	@echo "make shaders               - GLSL-Shader pruefen, nach build/shader/ kopieren"
	@echo "make lsp                   - compile_flags.txt fuer clangd erzeugen"
	@echo "make compdb                - compile_commands.json erzeugen"
	@echo "make test                  - Gast-Unit-Tests bauen und ausfuehren"
	@echo "make test ARGS=stw         - nur passende Testfaelle ausfuehren"
	@echo "make test-vertex           - Vertex-Loader-Tests (ohne Gast-Toolchain)"
	@echo "make test-software         - Transform-/Clipper-Tests des Software-Renderers"

# ==== Dependency Files =======================================================

-include $(DEPS)


