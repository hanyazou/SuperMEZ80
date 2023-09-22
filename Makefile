-include .config

PROGPORT := /dev/tty.usbmodem1444301
CONSPORT := /dev/cu.usbserial-144440
BOARD := SUPERMEZ80_SPI
#BOARD := SUPERMEZ80_CPM
#BOARD := EMUZ80_57Q
DEFS += -DSUPERMEZ80_CPM_MMU
#DEFS += -DCPM_MMU_EXERCISE
#DEFS += -DNO_MEMORY_CHECK
#DEFS += -DNO_MON_BREAKPOINT
#DEFS += -DNO_MON_STEP

PIC := 18F47Q43
#PIC := 18F47Q83
#PIC := 18F47Q84
#PIC := 18F57Q43

TEST_REPEAT := 10

PJ_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq (,$(XC8))
  ifneq (,$(wildcard /Applications/microchip/xc8/v2.40/bin/xc8))
    XC8 := /Applications/microchip/xc8/v2.40/bin/xc8
  else
    ifneq (,$(wildcard /opt/microchip/xc8/v2.36/bin/xc8))
      XC8 := /opt/microchip/xc8/v2.36/bin/xc8
    else
      $(error Missing XC8 complier. Please install XC8)
    endif
  endif
endif
XC8_OPTS := --chip=$(PIC) --std=c99
#XC8 := /Applications/microchip/xc8/v2.40/bin/xc8-cc
#XC8_OPTS := -mcpu=$(PIC) -std=c99

PP3_DIR := $(PJ_DIR)/tools/a-p-prog/sw
PP3_OPTS := -c $(PROGPORT) -s 100 -v 2 -r 30 -t $(PIC)

FATFS_DIR := $(PJ_DIR)/FatFs
DRIVERS_DIR := $(PJ_DIR)/drivers
SRC_DIR := $(PJ_DIR)/src
BUILD_DIR := $(PJ_DIR)/$(shell echo build.$(BOARD).$(PIC) | tr A-Z a-z)
CPM2_DIR := $(PJ_DIR)/cpm2
HEXFILE := $(shell echo $(BOARD)-$(PIC).hex | tr A-Z a-z)

SJASMPLUS_DIR=$(PJ_DIR)/tools/sjasmplus
SJASMPLUS=$(SJASMPLUS_DIR)/sjasmplus

FATFS_SRCS := $(FATFS_DIR)/source/ff.c
DISK_SRCS := \
    $(DRIVERS_DIR)/diskio.c $(DRIVERS_DIR)/utils.c
SRCS := $(SRC_DIR)/supermez80.c $(SRC_DIR)/disas.c $(SRC_DIR)/disas_z80.c $(SRC_DIR)/memory.c \
    $(SRC_DIR)/monitor.c $(SRC_DIR)/io.c $(SRC_DIR)/board.c

ifeq ($(BOARD),SUPERMEZ80_SPI)
SRCS += $(SRC_DIR)/boards/supermez80_spi.c
SRCS += $(SRC_DIR)/boards/supermez80_spi_ioexp.c
endif
ifeq ($(BOARD),SUPERMEZ80_CPM)
SRCS += $(SRC_DIR)/boards/supermez80_cpm.c
endif
ifeq ($(BOARD),EMUZ80_57Q)
SRCS += $(SRC_DIR)/boards/emuz80_57q.c
endif

INCS :=-I$(SRC_DIR) -I$(DRIVERS_DIR) -I$(FATFS_DIR)/source -I$(BUILD_DIR)

HDRS := $(SRC_DIR)/supermez80.h $(SRC_DIR)/picconfig.h \
        $(DRIVERS_DIR)/SPI.c $(DRIVERS_DIR)/SPI.h $(DRIVERS_DIR)/SDCard.h \
        $(DRIVERS_DIR)/mcp23s08.h \
        $(SRC_DIR)/disas.h $(SRC_DIR)/disas_z80.h \
        $(BUILD_DIR)/ipl.inc $(BUILD_DIR)/trampoline.inc $(BUILD_DIR)/mmu_exercise.inc \
        $(BUILD_DIR)/trampoline_cleanup.inc \
        $(BUILD_DIR)/trampoline_nmi.inc \
        $(BUILD_DIR)/dma_helper.inc \
        $(BUILD_DIR)/dummy.inc \
        $(DRIVERS_DIR)/pic18f47q43_spi.c \
        $(DRIVERS_DIR)/SDCard.c \
        $(DRIVERS_DIR)/mcp23s08.c \
        $(SRC_DIR)/boards/emuz80_common.c

all: $(BUILD_DIR)/$(HEXFILE) $(BUILD_DIR)/drivea.dsk

$(BUILD_DIR)/$(HEXFILE): $(SRCS) $(FATFS_SRCS) $(DISK_SRCS) $(HDRS)
	cd $(BUILD_DIR) && \
        $(XC8) $(XC8_OPTS) $(DEFS) $(INCS) $(SRCS) $(FATFS_SRCS) $(DISK_SRCS) && \
        mv supermez80.hex $(HEXFILE)

$(BUILD_DIR)/%.inc: $(SRC_DIR)/%.z80 $(SJASMPLUS)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(SJASMPLUS) --lst=$*.lst --raw=$*.bin $< && \
        cat $*.bin | xxd -i > $@

$(BUILD_DIR)/boot.bin: $(CPM2_DIR)/boot.asm $(BUILD_DIR) $(SJASMPLUS)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(SJASMPLUS) --raw=$@ $<
$(BUILD_DIR)/bios.bin: $(CPM2_DIR)/bios.asm $(BUILD_DIR) $(SJASMPLUS)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(SJASMPLUS) --raw=$@ $<

$(BUILD_DIR)/drivea.dsk: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/bios.bin
	cd $(BUILD_DIR); \
	dd if=$(CPM2_DIR)/z80pack-cpm2-1.dsk of=drivea.dsk bs=128; \
	dd if=boot.bin of=drivea.dsk bs=128 seek=0  count=1 conv=notrunc; \
	dd if=bios.bin of=drivea.dsk bs=128 seek=45 count=6 conv=notrunc

upload: $(BUILD_DIR)/$(HEXFILE) $(PP3_DIR)/pp3
	cd $(PP3_DIR) && ./pp3 $(PP3_OPTS) $(BUILD_DIR)/$(HEXFILE)

test::
	cd test && PORT=$(CONSPORT) ./test.sh

test_repeat::
	cd test && for i in $$(seq $(TEST_REPEAT)); do \
          PORT=$(CONSPORT) ./test.sh || exit 1; \
        done

test_time::
	cd test && PORT=$(CONSPORT) ./measure_time.sh

test_monitor::
	cd test && PORT=$(CONSPORT) ./monitor.sh

test_build::
	make BOARD=SUPERMEZ80_SPI PIC=18F47Q43
	make BOARD=SUPERMEZ80_CPM PIC=18F47Q43
	make BOARD=EMUZ80_57Q     PIC=18F57Q43
	ls -l build.*.*/*.hex

clean::
	rm -rf $(PJ_DIR)/build.*.*

$(SJASMPLUS):
	cd $(SJASMPLUS_DIR) && make clean && make

$(PP3_DIR)/pp3:
	cd $(PP3_DIR) && make

realclean:: clean
	cd $(SJASMPLUS_DIR) && make clean
	cd $(PP3_DIR) && make clean
