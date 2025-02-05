#!/usr/bin/make -f
# Makefile for DISTRHO Plugins #
# ---------------------------- #
# Created by falkTX
#

# --------------------------------------------------------------
# Build config

PREFIX  ?= /usr/local
DESTDIR ?=
SYSDEPS ?= false

# --------------------------------------------------------------
# List of modules to build, based on their C++ model name

MODULES += SpringReverb

# --------------------------------------------------------------
# Import base definitions

USE_NANOVG_FBO = true
include ../dpf/Makefile.base.mk

# --------------------------------------------------------------
# Build setup

BUILD_DIR = ../build/lv2export

ifeq ($(MACOS),true)
BASE_FLAGS += -DARCH_MAC
else ifeq ($(WINDOWS),true)
BASE_FLAGS += -DARCH_WIN
else
BASE_FLAGS += -DARCH_LIN
endif

BASE_FLAGS += -fno-finite-math-only
BASE_FLAGS += -I../dpf/dgl/src/nanovg
BASE_FLAGS += -I../dpf/distrho
BASE_FLAGS += -I../include
BASE_FLAGS += -I../include/neon-compat
BASE_FLAGS += -I../src/Rack/include
ifeq ($(SYSDEPS),true)
BASE_FLAGS += $(shell pkg-config --cflags jansson libarchive samplerate speexdsp)
else
BASE_FLAGS += -I../src/Rack/dep/include
endif
BASE_FLAGS += -I../src/Rack/dep/filesystem/include
BASE_FLAGS += -I../src/Rack/dep/fuzzysearchdatabase/src
BASE_FLAGS += -I../src/Rack/dep/glfw/include
BASE_FLAGS += -I../src/Rack/dep/nanosvg/src
# BASE_FLAGS += -IRack/dep/osdialog
BASE_FLAGS += -I../src/Rack/dep/oui-blendish
BASE_FLAGS += -I../src/Rack/dep/pffft
BASE_FLAGS += -pthread

ifeq ($(WINDOWS),true)
BASE_FLAGS += -D_USE_MATH_DEFINES
BASE_FLAGS += -DWIN32_LEAN_AND_MEAN
BASE_FLAGS += -I../include/mingw-compat
BASE_FLAGS += -I../include/mingw-std-threads
endif

ifeq ($(HEADLESS),true)
BASE_FLAGS += -DHEADLESS
endif

ifeq ($(WITH_LTO),true)
BASE_FLAGS += -fno-strict-aliasing -flto
endif

# --------------------------------------------------------------
# FIXME lots of warnings from VCV side

BASE_FLAGS += -Wno-unused-parameter
BASE_FLAGS += -Wno-unused-variable

# --------------------------------------------------------------
# extra linker flags

LINK_FLAGS += -pthread

ifneq ($(HAIKU_OR_MACOS_OR_WINDOWS),true)
LINK_FLAGS += -ldl
endif

ifeq ($(MACOS),true)
LINK_FLAGS += -framework IOKit
else ifeq ($(WINDOWS),true)
# needed by VCVRack
EXTRA_LIBS += -ldbghelp -lshlwapi
# needed by JW-Modules
EXTRA_LIBS += -lws2_32 -lwinmm
endif

ifeq ($(SYSDEPS),true)
EXTRA_LIBS += $(shell pkg-config --libs jansson libarchive samplerate speexdsp)
endif

ifeq ($(WITH_LTO),true)
LINK_FLAGS += -fno-strict-aliasing -flto -Werror=odr -Werror=lto-type-mismatch
endif

# --------------------------------------------------------------
# Build targets

BUILD_FILES  = test.cpp
BUILD_FILES += dep.cpp
BUILD_FILES += dep2.cpp
BUILD_FILES += ../src/Rack/src/logger.cpp
BUILD_FILES += ../src/Rack/src/random.cpp
BUILD_FILES += ../src/Rack/src/string.cpp
BUILD_FILES += ../src/Rack/src/system.cpp
BUILD_FILES += ../src/Rack/src/tinyexpr.c
BUILD_FILES += ../src/Rack/src/Quantity.cpp
BUILD_FILES += ../src/Rack/src/engine/Module.cpp
BUILD_FILES += ../src/Rack/src/engine/ParamQuantity.cpp
BUILD_FILES += ../src/Rack/src/engine/PortInfo.cpp
ifneq ($(SYSDEPS),true)
BUILD_FILES += ../src/Rack/dep/pffft/pffft.c
BUILD_FILES += ../src/Rack/dep/pffft/fftpack.c
endif

TARGET = test

all: $(TARGET)

EXTRA_LIBS  = ../plugins/plugins.a

ifneq ($(SYSDEPS),true)
EXTRA_LIBS += ../src/Rack/dep/lib/libjansson.a
EXTRA_LIBS += ../src/Rack/dep/lib/libsamplerate.a
EXTRA_LIBS += ../src/Rack/dep/lib/libspeexdsp.a
ifeq ($(WINDOWS),true)
EXTRA_LIBS += ../src/Rack/dep/lib/libarchive_static.a
else
EXTRA_LIBS += ../src/Rack/dep/lib/libarchive.a
endif
EXTRA_LIBS += ../src/Rack/dep/lib/libzstd.a
endif

# --------------------------------------------------------------
# Build commands

OBJS = $(BUILD_FILES:%=$(BUILD_DIR)/%.o)

$(TARGET): $(OBJS)
	-@mkdir -p $(shell dirname $@)
	$(CXX) $^ $(BUILD_CXX_FLAGS) $(LINK_FLAGS) $(EXTRA_LIBS) -o $@

$(BUILD_DIR)/%.c.o: %.c
	-@mkdir -p "$(shell dirname $(BUILD_DIR)/$<)"
	@echo "Compiling $<"
	$(SILENT)$(CC) $< $(BUILD_C_FLAGS) -c -o $@

$(BUILD_DIR)/%.cpp.o: %.cpp
	-@mkdir -p "$(shell dirname $(BUILD_DIR)/$<)"
	@echo "Compiling $<"
	$(SILENT)$(CXX) $< $(BUILD_CXX_FLAGS) -c -o $@
