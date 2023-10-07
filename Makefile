-include .config

PROGPORT ?= /dev/tty.usbmodem1444301
CONSPORT ?= /dev/cu.usbserial-144440

BOARD ?= SUPERMEZ80_SPI
#BOARD ?= SUPERMEZ80_CPM
#BOARD ?= EMUZ80_57Q
#BOARD ?= Z8S180_57Q

PIC ?= 18F47Q43
#PIC ?= 18F47Q83
#PIC ?= 18F47Q84
#PIC ?= 18F57Q43

#Z80_CLK_HZ ?= 0UL              #  use external clock
#Z80_CLK_HZ ?= 499712UL         #  0.5 MHz (NCOxINC = 0x04000, 64MHz/64/2)
#Z80_CLK_HZ ?= 999424UL         #  1.0 MHz (NCOxINC = 0x08000, 64MHz/32/2)
#Z80_CLK_HZ ?= 1998848UL        #  2.0 MHz (NCOxINC = 0x10000, 64MHz/16/2)
#Z80_CLK_HZ ?= 3997696UL        #  4.0 MHz (NCOxINC = 0x20000, 64MHz/8/2)
#Z80_CLK_HZ ?= 4568778UL        #  4.6 MHz (NCOxINC = 0x24924, 64MHz/7/2)
#Z80_CLK_HZ ?= 5330241UL        #  5.3 MHz (NCOxINC = 0x2AAAA, 64MHz/6/2)
Z80_CLK_HZ ?= 6396277UL        #  6.4 MHz (NCOxINC = 0x33333, 64MHz/5/2)
#Z80_CLK_HZ ?= 7995392UL        #  8.0 MHz (NCOxINC = 0x40000, 64MHz/4/2)
#Z80_CLK_HZ ?= 10660482UL       # 10.7 MHz (NCOxINC = 0x55555, 64MHz/3/2)
#Z80_CLK_HZ ?= 12792615UL       # 12.8 MHz (NCOxINC = 0x66666, 64MHz/5)
#Z80_CLK_HZ ?= 15990784UL       # 16.0 MHz (NCOxINC = 0x80000, 64MHz/2/2)

Z180_CLK_MULT ?= 2
Z180_NO_DMA ?= 0
Z180_UART ?= 0

TEST_REPEAT ?= 10

PJ_DIR ?= $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

ifeq ($(origin XC8), undefined)
  ifneq (,$(wildcard /Applications/microchip/xc8/v2.40/bin/xc8))
    XC8 = /Applications/microchip/xc8/v2.40/bin/xc8
  else
    ifneq (,$(wildcard /opt/microchip/xc8/v2.36/bin/xc8))
      XC8 = /opt/microchip/xc8/v2.36/bin/xc8
    else
      $(error Missing XC8 complier. Please install XC8)
    endif
  endif
endif
XC8_OPTS ?= --chip=$(PIC) --std=c99
#XC8 ?= /Applications/microchip/xc8/v2.40/bin/xc8-cc
#XC8_OPTS ?= -mcpu=$(PIC) -std=c99

PP3_DIR ?= $(PJ_DIR)/tools/a-p-prog/sw
PP3_OPTS ?= -c $(PROGPORT) -s 100 -v 2 -r 30 -t $(PIC)

FATFS_DIR ?= $(PJ_DIR)/FatFs
DRIVERS_DIR ?= $(PJ_DIR)/drivers
SRC_DIR ?= $(PJ_DIR)/src
BUILD_DIR ?= $(PJ_DIR)/$(shell echo build.$(BOARD).$(PIC) | tr A-Z a-z)
CPM2_DIR ?= $(PJ_DIR)/cpm2
HEXFILE ?= $(shell echo $(BOARD)-$(PIC).hex | tr A-Z a-z)

ASM_DIR ?= $(PJ_DIR)/tools/zasm
ASM ?= $(ASM_DIR)/zasm
ASM_OPTS ?= --opcodes --bin --target=ram

FATFS_SRCS ?= $(FATFS_DIR)/source/ff.c
DISK_SRCS ?= \
    $(DRIVERS_DIR)/diskio.c $(DRIVERS_DIR)/utils.c
SRCS ?= $(SRC_DIR)/supermez80.c $(SRC_DIR)/disas.c $(SRC_DIR)/disas_z80.c $(SRC_DIR)/memory.c \
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
ifeq ($(BOARD),Z8S180_57Q)
SRCS += $(SRC_DIR)/boards/z8s180_57q.c
endif

DEFS += -DSUPERMEZ80_CPM_MMU
#DEFS += -DCPM_MMU_EXERCISE
#DEFS += -DNO_MEMORY_CHECK
#DEFS += -DNO_MON_BREAKPOINT
#DEFS += -DNO_MON_STEP
ifneq ($(origin Z80_CLK_HZ), undefined)
    DEFS += -DZ80_CLK_HZ=$(Z80_CLK_HZ)
endif

INCS ?=-I$(SRC_DIR) -I$(DRIVERS_DIR) -I$(FATFS_DIR)/source -I$(BUILD_DIR)

HDRS ?= $(SRC_DIR)/supermez80.h $(SRC_DIR)/picconfig.h \
        $(DRIVERS_DIR)/SPI.c $(DRIVERS_DIR)/SPI.h $(DRIVERS_DIR)/SDCard.h \
        $(DRIVERS_DIR)/mcp23s08.h \
        $(SRC_DIR)/disas.h $(SRC_DIR)/disas_z80.h \
        $(BUILD_DIR)/ipl.inc $(BUILD_DIR)/trampoline.inc $(BUILD_DIR)/mmu_exercise.inc \
        $(BUILD_DIR)/trampoline_cleanup.inc \
        $(BUILD_DIR)/trampoline_nmi.inc \
        $(BUILD_DIR)/dma_helper.inc \
        $(BUILD_DIR)/dummy.inc \
        $(BUILD_DIR)/z8s180_57q_ipl.inc \
        $(DRIVERS_DIR)/pic18f47q43_spi.c \
        $(DRIVERS_DIR)/SDCard.c \
        $(DRIVERS_DIR)/mcp23s08.c \
        $(SRC_DIR)/boards/emuz80_common.c

all: $(BUILD_DIR)/$(HEXFILE) \
    $(BUILD_DIR)/CPMDISKS.PIO/drivea.dsk \
    $(BUILD_DIR)/CPMDISKS.180/drivea.dsk

$(BUILD_DIR)/$(HEXFILE): $(SRCS) $(FATFS_SRCS) $(DISK_SRCS) $(HDRS)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(XC8) $(XC8_OPTS) $(DEFS) $(INCS) $(SRCS) $(FATFS_SRCS) $(DISK_SRCS) && \
        mv supermez80.hex $(HEXFILE)

$(BUILD_DIR)/%.inc: $(SRC_DIR)/%.z80 $(ASM) $(BUILD_DIR)/config_asm.inc
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(ASM) $(ASM_OPTS) -l $*.lst -o $*.bin $< && \
        cat $*.bin | xxd -i > $@

$(BUILD_DIR)/%.inc: $(SRC_DIR)/boards/%.z80 $(ASM) $(BUILD_DIR)/config_asm.inc
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        cp $< . && \
        $(ASM) $(ASM_OPTS) -l $*.lst -o $*.bin $*.z80 && \
        cat $*.bin | xxd -i > $@

$(BUILD_DIR)/boot.bin: $(CPM2_DIR)/boot.asm $(BUILD_DIR) $(ASM)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(ASM) $(ASM_OPTS) -o $@ $<
$(BUILD_DIR)/bios.bin: $(CPM2_DIR)/bios.asm $(BUILD_DIR) $(ASM)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        $(ASM) $(ASM_OPTS) -o $@ $<

$(BUILD_DIR)/CPMDISKS.PIO/drivea.dsk: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/bios.bin
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
	mkdir -p CPMDISKS.PIO; \
	dd if=$(CPM2_DIR)/z80pack-cpm2-1.dsk of=$@ bs=128; \
	dd if=boot.bin of=$@ bs=128 seek=0  count=1 conv=notrunc; \
	dd if=bios.bin of=$@ bs=128 seek=45 count=6 conv=notrunc

$(BUILD_DIR)/boot_z180.bin: $(CPM2_DIR)/boot_z180.asm $(BUILD_DIR) $(ASM)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        cp $< . && \
        $(ASM) $(ASM_OPTS) -o $@ boot_z180.asm
$(BUILD_DIR)/bios_z180.bin: $(CPM2_DIR)/bios_z180.asm $(BUILD_DIR) $(ASM)
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
        cp $< . && \
        $(ASM) $(ASM_OPTS) -o $@ bios_z180.asm

$(BUILD_DIR)/CPMDISKS.180/drivea.dsk: $(BUILD_DIR)/boot_z180.bin $(BUILD_DIR)/bios_z180.bin
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
	mkdir -p CPMDISKS.180; \
	dd if=$(CPM2_DIR)/z80pack-cpm2-1.dsk of=$@ bs=128; \
	dd if=boot_z180.bin of=$@ bs=128 seek=0  count=1 conv=notrunc; \
	dd if=bios_z180.bin of=$@ bs=128 seek=45 count=6 conv=notrunc

$(CPM2_DIR)/boot_z180.asm: $(BUILD_DIR)/config_asm.inc
$(CPM2_DIR)/bios_z180.asm: $(BUILD_DIR)/config_asm.inc

$(BUILD_DIR)/config_asm.inc: Makefile
	mkdir -p $(BUILD_DIR) && cd $(BUILD_DIR) && \
	rm -f $@
	if [ "$(Z180_UART)" != 0 ]; then \
	    echo "UART_180	EQU	1" >> $@; \
	    echo "UART_PIC	EQU	0" >> $@; \
	else \
	    echo "UART_180	EQU	0" >> $@; \
	    echo "UART_PIC	EQU	1" >> $@; \
	fi
	if [ "$(Z180_NO_DMA)" != 0 ]; then \
	    echo "BYTE_RW	EQU	1" >> $@; \
	else \
	    echo "BYTE_RW	EQU	0" >> $@; \
	fi
	if [ "$(Z180_CLK_MULT)" == 0 ]; then \
	    echo "CLOCK_0	EQU	1" >> $@; \
	else \
	    echo "CLOCK_0	EQU	0" >> $@; \
	fi
	if [ "$(Z180_CLK_MULT)" == 1 ]; then \
	    echo "CLOCK_1	EQU	1" >> $@; \
	else \
	    echo "CLOCK_1	EQU	0" >> $@; \
	fi
	if [ "$(Z180_CLK_MULT)" == 2 ]; then \
	    echo "CLOCK_2	EQU	1" >> $@; \
	else \
	    echo "CLOCK_2	EQU	0" >> $@; \
	fi
	@echo ===================================
	@cat $@
	@echo ===================================

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
	make BOARD=Z8S180_57Q     PIC=18F57Q43
	ls -l build.*.*/*.hex

clean::
	rm -rf $(PJ_DIR)/build.*.*

$(ASM):
	cd $(ASM_DIR) && make

$(PP3_DIR)/pp3:
	cd $(PP3_DIR) && make

realclean:: clean
	cd $(ASM_DIR) && make clean
	cd $(PP3_DIR) && make clean
