# macOS helper build for the C++ ports and module linkage.
#
# Default target is `pdf`, which runs CMunge, assembles the generated module
# veneer, and links a module. Use `*-objs` targets for compile-only coverage.

SHELL := /bin/bash

TOOLCHAIN_BIN ?= /Users/piers/Develop/GitHub/aof-toolchain/bin
TOOLCHAIN_LIB ?= $(abspath $(TOOLCHAIN_BIN)/../lib)
CPP = $(TOOLCHAIN_BIN)/n++-riscos
AS = $(TOOLCHAIN_BIN)/asasm
CMUNGE = $(TOOLCHAIN_BIN)/cmunge
DRLINK = $(TOOLCHAIN_BIN)/drlink
GXX ?= g++

BUILD_DIR ?= build/macos
MODULE_BUILD_DIR ?= $(BUILD_DIR)/modules
CMHG_BUILD_DIR ?= $(BUILD_DIR)/generated
HOST_BUILD_DIR ?= $(BUILD_DIR)/host
REPLAY_OBJ_DIR ?= $(HOST_BUILD_DIR)/obj

INCLUDE_PATH =  -I$(BUILD_DIR)/include  -I.  -INorcroft

CPPFLAGS_COMMON ?= -zM $(INCLUDE_PATH)
CFLAGS_COMMON ?=
COMPILE_ONLY_FLAG ?= -c
DEPFLAG ?= -M

ASFLAGS_COMMON ?= -aof
CMUNGEFLAGS_COMMON ?= -32bit -tnorcroft
DRLINKFLAGS_COMMON ?= -c++ -m
MODULE_LDLIBS ?= $(TOOLCHAIN_LIB)/stubs.a

MODULES := pdf ps dp capture

pdf_NAME := PDriverPDF
ps_NAME := PDriverPS
dp_NAME := PDriverDP
capture_NAME := PDriverCapture

SHARED_SRCS := $(sort $(wildcard Core/*.cpp Common/*.cpp RLib/*.cpp RLib/*/*.cpp RLibX/*.cpp RLibX/*/*.cpp ROSLib/*.cpp ROSLib/*/*.cpp))

define DEFINE_MODULE
$(1)_DIR := $$($(1)_NAME)
$(2) ?= -I$$($(1)_CMHG_DIR) -I$$($(1)_DIR)
$(3) ?=
$(1)_EXCLUDE_SRCS ?=
$(1)_CMHG_SPEC := $$($(1)_DIR)/PDriver.cmhg
$(1)_CMHG_DIR := $(CMHG_BUILD_DIR)/$$($(1)_NAME)
$(1)_CMHG_HDR := $$($(1)_CMHG_DIR)/cmhg.h
$(1)_CMHG_ASM := $$($(1)_CMHG_DIR)/cmhg.s
$(1)_CMHG_GEN_DEP := $$($(1)_CMHG_DIR)/cmunge.d
$(1)_SRCS := $$(sort $$(filter-out $$($(1)_EXCLUDE_SRCS),$$(SHARED_SRCS) $$(wildcard $$($(1)_DIR)/*.cpp)))
$(1)_OBJS := $$(patsubst %.cpp,$(BUILD_DIR)/$(1)/%.o,$$($(1)_SRCS))
$(1)_DEPS := $$($(1)_OBJS:.o=.d)
$(1)_CMHG_OBJ := $(BUILD_DIR)/$(1)/$$($(1)_DIR)/cmhg.o
$(1)_CMHG_OBJ_DEP := $$($(1)_CMHG_OBJ:.o=.d)
$(1)_MODULE ?= $(MODULE_BUILD_DIR)/$$($(1)_NAME)

$(BUILD_DIR)/$(1)/%.o: %.cpp $$($(1)_CMHG_HDR)
	@mkdir -p "$$(@D)"
	$(CPP) $(CPPFLAGS_COMMON) $$($(2)) $(CFLAGS_COMMON) $$($(3)) $(DEPFLAG) "$$<" | sed "s|^[^:]*:|$$@:|" > "$$(@:.o=.d)"
	$(CPP) $(CPPFLAGS_COMMON) $$($(2)) $(CFLAGS_COMMON) $$($(3)) $(COMPILE_ONLY_FLAG) "$$<" -o "$$@"

ifeq ($$(wildcard $$($(1)_CMHG_SPEC)),)
$$($(1)_CMHG_HDR) $$($(1)_CMHG_ASM):
	@echo "error: missing $$($(1)_CMHG_SPEC)"; exit 1
else
$$($(1)_CMHG_HDR) $$($(1)_CMHG_ASM): $$($(1)_CMHG_SPEC)
	@mkdir -p "$$($(1)_CMHG_DIR)"
	$(CMUNGE) $(CMUNGEFLAGS_COMMON) -depend "$$($(1)_CMHG_GEN_DEP)" -d "$$($(1)_CMHG_HDR)" -s "$$($(1)_CMHG_ASM)" "$$<"
endif

$$($(1)_CMHG_OBJ): $$($(1)_CMHG_ASM)
	@mkdir -p "$$(@D)"
	$(AS) $(ASFLAGS_COMMON) -Depend="$$(@:.o=.d)" -i. -i$(BUILD_DIR)/include -i$$($(1)_CMHG_DIR) -i$$($(1)_DIR) -o "$$@" "$$<"

$(1)-objs: check-cc $$($(1)_OBJS)
	@echo "Built $$($(1)_NAME) objects: $$(words $$($(1)_OBJS))"

$(1): check-module-tools $$($(1)_MODULE)
	@echo "Built module: $$($(1)_MODULE)"

$$($(1)_MODULE): $$($(1)_CMHG_OBJ) $$($(1)_OBJS)
	@mkdir -p "$$(@D)"
	$(DRLINK) $(DRLINKFLAGS_COMMON) -o "$$@" $$^ $(MODULE_LDLIBS)
endef

dp_EXCLUDE_SRCS += Common/Ascii85.cpp
capture_EXCLUDE_SRCS += Common/Ascii85.cpp
CFLAGS_CAPTURE ?= -DCaptureTrace=1

$(eval $(call DEFINE_MODULE,pdf,CPPFLAGS_PDF,CFLAGS_PDF))
$(eval $(call DEFINE_MODULE,ps,CPPFLAGS_PS,CFLAGS_PS))
$(eval $(call DEFINE_MODULE,dp,CPPFLAGS_DP,CFLAGS_DP))
$(eval $(call DEFINE_MODULE,capture,CPPFLAGS_CAPTURE,CFLAGS_CAPTURE))

REPLAY_COMMON_SRCS ?= \
 HostCompat/Replay.cpp \
 HostCompat/ReplayShims.cpp \
 Common/ColourPixelValues.cpp \
 Common/PlotDraw.cpp \
 Core/Colour.cpp \
 Core/ColourUtils.cpp \
 Core/ErrorBlocks.cpp \
 Core/FontBlock.cpp \
 Core/Job.cpp \
 Core/Module.cpp \
 Core/Workspace.cpp \
 RLibX/Draw.cpp \
 RLibX/Font/Identifier.cpp \
 RLibX/JPEG.cpp \
 RLibX/Sprite.cpp \
 ROSLib/OSSpriteOp.cpp

REPLAY_CPPFLAGS_COMMON ?= -I. -IHostCompat/include -DPrinterNumber=0 -DVersionNumber=400
REPLAY_CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -fdiagnostics-color=always
REPLAY_LDFLAGS ?=
REPLAY_LDLIBS ?=

capture_REPLAY_SRCS := $(REPLAY_COMMON_SRCS) \
 PDriverCapture/Capture.cpp \
 Common/OutputBuffer.cpp

pdf_REPLAY_SRCS := $(REPLAY_COMMON_SRCS) $(filter-out PDriverPDF/PDriverPDF.cpp,$(sort $(wildcard PDriverPDF/*.cpp))) \
 Common/Ascii85.cpp \
 Common/ColTrans.cpp \
 Common/OutputBuffer.cpp \
 Common/PrinterFontName.cpp \
 Common/Font.cpp

ps_REPLAY_SRCS := $(REPLAY_COMMON_SRCS) $(filter-out PDriverPS/PDriverPS.cpp,$(sort $(wildcard PDriverPS/*.cpp))) \
 Common/Ascii85.cpp \
 Common/ColTrans.cpp \
 Common/OutputBuffer.cpp \
 Common/PrinterFontName.cpp \
 Common/Font.cpp

dp_REPLAY_SRCS := $(REPLAY_COMMON_SRCS) $(filter-out PDriverDP/PDriverDP.cpp,$(sort $(wildcard PDriverDP/*.cpp))) \
 Common/ColTrans.cpp \
 Common/OutputBuffer.cpp \
 Common/PrinterFontName.cpp \
 Common/Font.cpp

capture_REPLAY_CPPFLAGS_EXTRA := -DREPLAY_BACKEND_CAPTURE=1

define DEFINE_REPLAY
$(1)_REPLAY_BIN ?= $(HOST_BUILD_DIR)/replay-$(1)
$(1)_REPLAY_OBJ_DIR := $(REPLAY_OBJ_DIR)/$(1)
$(1)_REPLAY_OBJS := $$(addprefix $$($(1)_REPLAY_OBJ_DIR)/,$$($(1)_REPLAY_SRCS:.cpp=.o))
$(1)_REPLAY_DEPS := $$($(1)_REPLAY_OBJS:.o=.d)
$(1)_REPLAY_CPPFLAGS ?= $(REPLAY_CPPFLAGS_COMMON) -I$$($(1)_CMHG_DIR) -I$$($(1)_DIR) $$($(1)_REPLAY_CPPFLAGS_EXTRA)

$$($(1)_REPLAY_OBJ_DIR)/%.o: %.cpp $$($(1)_CMHG_HDR)
	@mkdir -p "$$(@D)"
	$(GXX) $(REPLAY_CXXFLAGS) $$($(1)_REPLAY_CPPFLAGS) -MMD -MP -c "$$<" -o "$$@"

$$($(1)_REPLAY_BIN): $$($(1)_REPLAY_OBJS)
	@mkdir -p "$$(@D)"
	$(GXX) $(REPLAY_CXXFLAGS) $(REPLAY_LDFLAGS) $$^ $(REPLAY_LDLIBS) -o "$$@"

replay-$(1): check-gxx $$($(1)_REPLAY_BIN)
	@echo "Built host replay binary: $$($(1)_REPLAY_BIN)"
endef

$(eval $(call DEFINE_REPLAY,capture))
$(eval $(call DEFINE_REPLAY,pdf))
$(eval $(call DEFINE_REPLAY,ps))
$(eval $(call DEFINE_REPLAY,dp))

REPLAY_TARGETS := $(addprefix replay-,$(MODULES))
REPLAY_BINS := $(foreach module,$(MODULES),$($(module)_REPLAY_BIN))
REPLAY_DEPS := $(foreach module,$(MODULES),$($(module)_REPLAY_DEPS))

MODULE_DEPS := $(foreach module,$(MODULES),$($(module)_DEPS) $($(module)_CMHG_GEN_DEP) $($(module)_CMHG_OBJ_DEP))

.DEFAULT_GOAL := pdf

.PHONY: help all all-objs modules replay replay-capture replay-pdf replay-ps replay-dp clean         check-cc check-as check-cmunge check-drlink check-module-tools check-gxx         $(MODULES) $(addsuffix -objs,$(MODULES))

help:
	@echo "Targets:"
	@echo "  pdf-objs      Compile Core/Common/RLib + PDriverPDF objects"
	@echo "  pdf           Run cmunge, assemble, and link the PDriverPDF module"
	@echo "  ps-objs       Compile Core/Common/RLib + PDriverPS objects"
	@echo "  ps            Run cmunge, assemble, and link the PDriverPS module"
	@echo "  dp-objs       Compile Core/Common/RLib + PDriverDP objects"
	@echo "  dp            Run cmunge, assemble, and link the PDriverDP module"
	@echo "  capture-objs  Compile Core/Common/RLib + PDriverCapture objects"
	@echo "  capture       Run cmunge, assemble, and link the PDriverCapture module"
	@echo "  replay        Build all host replay binaries with g++"
	@echo "  replay-capture Build the host replay binary for PDriverCapture"
	@echo "  replay-pdf    Build the host replay binary for PDriverPDF"
	@echo "  replay-ps     Build the host replay binary for PDriverPS"
	@echo "  replay-dp     Build the host replay binary for PDriverDP"
	@echo "  all-objs      Compile all four RISC OS variants"
	@echo "  modules       Link all four RISC OS modules"
	@echo "  all           Build all modules, then replay"
	@echo "  clean         Remove $(BUILD_DIR)"
	@echo
	@echo "Variables (override as needed):"
	@echo "  TOOLCHAIN_BIN=$(TOOLCHAIN_BIN)"
	@echo "  CPP=$(CPP)"
	@echo "  AS=$(AS)"
	@echo "  CMUNGE=$(CMUNGE)"
	@echo "  DRLINK=$(DRLINK)"
	@echo "  TOOLCHAIN_LIB=$(TOOLCHAIN_LIB)"
	@echo "  MODULE_LDLIBS=$(MODULE_LDLIBS)"
	@echo "  GXX=$(GXX)"
	@echo "  BUILD_DIR=$(BUILD_DIR)"

all-objs: $(addsuffix -objs,$(MODULES))

modules: $(MODULES)

# Temporarily removed 'replay' from all since it was only able to replay a
# capture from PDriverCapture to PDriverCapture. Changing the APIs meant
# regular updating, when this isn't very useful in its current state.
all: modules replay

replay: $(REPLAY_TARGETS)
	@echo "Built host replay binaries: $(REPLAY_BINS)"

check-cc:
	@command -v "$(CPP)" >/dev/null 2>&1 || {         echo "error: compiler '$(CPP)' not found in PATH";         exit 1;     }

check-as:
	@command -v "$(AS)" >/dev/null 2>&1 || {         echo "error: assembler '$(AS)' not found in PATH";         exit 1;     }

check-cmunge:
	@command -v "$(CMUNGE)" >/dev/null 2>&1 || {         echo "error: cmunge '$(CMUNGE)' not found in PATH";         exit 1;     }

check-drlink:
	@command -v "$(DRLINK)" >/dev/null 2>&1 || {         echo "error: linker '$(DRLINK)' not found in PATH";         exit 1;     }

check-module-tools: check-cc check-as check-cmunge check-drlink

check-gxx:
	@command -v "$(GXX)" >/dev/null 2>&1 || {         echo "error: compiler '$(GXX)' not found in PATH";         exit 1;     }

clean:
	rm -rf "$(BUILD_DIR)"

-include $(MODULE_DEPS) $(REPLAY_DEPS)
