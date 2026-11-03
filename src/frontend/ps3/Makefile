# melonDS for PS3 (PSL1GHT)
#
# UNVERIFIED: never run against a real PS3 toolchain. It assumes a standard
# ps3toolchain/PSL1GHT install (PS3DEV and PSL1GHT set, ppu-gcc/ppu-g++ on the
# PATH) and that $(PSL1GHT)/ppu_rules provides the usual "%.self: %.elf" rule.
# See README.md for what to expect and the known blockers.
#
# The core source list below is taken from the host CMake build of the "core"
# target with the JIT, OpenGL renderer and GDB stub disabled.

ifeq ($(strip $(PSL1GHT)),)
$(error "Set PSL1GHT in your environment, e.g. export PSL1GHT=$$PS3DEV")
endif

include $(PSL1GHT)/ppu_rules

TARGET  := melonds-ps3
BUILD   := build
SRC     := ../..

CORE_CPP := \
	ARCodeFile ARDatabaseDAT AREngine ARM ARMInterpreter ARMInterpreter_ALU \
	ARMInterpreter_Branch ARMInterpreter_LoadStore CP15 CRC32 DMA DMA_Timings \
	DSP_HLE/AACUcode DSP_HLE/G711Ucode DSP_HLE/GraphicsUcode DSP_HLE/UcodeBase \
	DSi DSi_AES DSi_Camera DSi_DSP DSi_I2C DSi_I2S DSi_NAND DSi_NDMA DSi_NWifi \
	DSi_SD DSi_SPI_TSC FATIO FATStorage FreeBIOS GBACart GBACartMotionPak GPU \
	GPU2D GPU2D_Soft GPU3D GPU3D_Soft GPU3D_Texcache GPU_Soft Mic NDS NDSCart \
	NDSCart/CartCommon NDSCart/CartHomebrew NDSCart/CartR4 NDSCart/CartRetail \
	NDSCart/CartRetailBT NDSCart/CartRetailIR NDSCart/CartRetailNAND \
	NDSCart/CartSD ROMList RTC SPI SPI_Firmware SPU Savestate Utils Wifi WifiAP \
	teakra/src/ahbm teakra/src/apbp teakra/src/btdmp teakra/src/disassembler \
	teakra/src/dma teakra/src/memory_interface teakra/src/mmio teakra/src/parser \
	teakra/src/processor teakra/src/teakra teakra/src/timer

CORE_C := \
	blip-buf/blip_buf fatfs/ff fatfs/ffsystem fatfs/ffunicode sha1/sha1 \
	tiny-AES-c/aes xxhash/xxhash

# Frontend: shared code plus the single-threaded sync backend and PS3 pieces.
# rsxutil.c/.h are this directory's own minimal display helper.
FRONT_CPP := main_ps3 EmuCore Platform_PS3 Sync_single Video_PS3 Input_PS3 Audio_PS3
FRONT_C   := rsxutil

OBJS := \
	$(addprefix $(BUILD)/core/,$(addsuffix .o,$(CORE_CPP) $(CORE_C))) \
	$(addprefix $(BUILD)/front/,$(addsuffix .o,$(FRONT_CPP) $(FRONT_C)))

INCLUDES := -I. -Igen -I$(SRC) -I$(SRC)/teakra/include -I$(SRC)/teakra/src \
	-I$(PSL1GHT)/ppu/include -I$(PORTLIBS)/include

COMMON := -mcpu=cell -O3 -fwrapv -DMELONDS_PS3=1 -DMELONDS_PS3_PROFILE=1 $(INCLUDES)
# std::mutex / std::to_string stand-ins (see compat.h); C++ only
CXXCOMPAT := -include compat.h
CXXFLAGS := $(COMMON) $(CXXCOMPAT) -std=gnu++17 -Wno-invalid-offsetof
CFLAGS := $(COMMON)

LIBS := -L$(PSL1GHT)/ppu/lib -laudio -lrsx -lgcm_sys -lio -lsysutil -lrt -llv2 -lm

.PHONY: all clean pkg
all: $(TARGET).self

# ---- installable package ---------------------------------------------------
#   make pkg   ->  melonds-ps3.pkg           (debug type; RPCS3, debug firmware)
#                  melonds-ps3.gnpdrm.pkg    (finalized; custom firmware)
# Needs make_self_npdrm and package_finalize from ps3dev. If they complain about
# libcrypto.so.1.1, run tools/build-shim.sh and then
#   LD_LIBRARY_PATH=$PWD/tools make pkg
# PARAM.SFO and the package itself are made by tools/ps3pkg.py (Python 3).
LINKED_ELF := $(BUILD)/$(TARGET).linked.elf
TITLE := melonDS PS3
APPID := MELONDS00
PS3PKG ?= python3 tools/ps3pkg.py
PKGDIR := $(BUILD)/pkg

pkg: $(TARGET).gnpdrm.pkg

$(TARGET).pkg: $(LINKED_ELF)
	rm -rf $(PKGDIR)
	mkdir -p $(PKGDIR)/USRDIR
	cp $(ICON0) $(PKGDIR)/ICON0.PNG
	$(SELF_NPDRM) $< $(PKGDIR)/USRDIR/EBOOT.BIN $(CONTENTID) > /dev/null
	$(PS3PKG) sfo --xml $(SFOXML) --title "$(TITLE)" --appid "$(APPID)" $(PKGDIR)/PARAM.SFO
	$(PS3PKG) pkg --contentid $(CONTENTID) $(PKGDIR) $@

$(TARGET).gnpdrm.pkg: $(TARGET).pkg
	cp $< $@
	$(PACKAGE_FINALIZE) $@

# Fake-sign with fself (ships with ps3dev). The stock "%.self" rule from
# ppu_rules calls make_self, which needs libcrypto.so.1.1. A fake-signed SELF
# runs on custom firmware and in RPCS3, not on stock firmware.
FSELF ?= fself
$(TARGET).self: $(LINKED_ELF)
	$(FSELF) $< $@

# The step that was missing at first: ppu_rules never signs the raw linker
# output. It strips a copy and runs sprxlinker on it, which rewrites the
# system-library (sysutil, io, audio, ...) imports into the form the loader
# needs (and sets the Cell OS/ABI byte). Without this the console starts the
# program, cannot resolve its imports, and drops straight back to the XMB.
$(LINKED_ELF): $(TARGET).elf
	@mkdir -p $(BUILD)
	$(STRIP) $< -o $@
	$(SPRX) $@

$(TARGET).elf: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

$(BUILD)/core/%.o: $(SRC)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/core/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/front/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BUILD)/front/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(OBJS:.o=.d)

clean:
	rm -rf $(BUILD) $(TARGET).elf $(TARGET).self $(TARGET).pkg $(TARGET).gnpdrm.pkg
