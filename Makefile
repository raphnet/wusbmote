# Name: Makefile
# Project: HIDKeys
# Author: Christian Starkjohann
# Creation Date: 2006-02-02
# Tabsize: 4
# Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
# License: Proprietary, free under certain conditions. See Documentation.
# This Revision: $Id: Makefile,v 1.7 2014-05-30 01:46:07 cvs Exp $

UISP = uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/ttyS1
COMPILE = avr-gcc -Wall -Os -Iusbdrv -I. -mmcu=atmega8 -DF_CPU=12000000L #-DDEBUG_LEVEL=1
HEXFILE=wusbmote-m8.hex

OBJECTS = usbdrv/usbdrv.o usbdrv/usbdrvasm.o usbdrv/oddebug.o main.o i2c_gamepad.o i2c_mouse.o i2c_generic.o i2c.o eeprom.o config.o

# symbolic targets:
all:	$(HEXFILE)

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@
# "-x assembler-with-cpp" should not be necessary since this is the default
# file type for the .S (with capital S) extension. However, upper case
# characters are not always preserved on Windows. To ensure WinAVR
# compatibility define the file type manually.

.c.s:
	$(COMPILE) -S $< -o $@


clean:
	rm -f *.hex *.map main.elf *.o usbdrv/*.o main.s usbdrv/oddebug.s usbdrv/usbdrv.s

# file targets:
main.elf:	$(OBJECTS)
	$(COMPILE) -o main.elf $(OBJECTS) -Wl,-Map=main.map

$(HEXFILE):	main.elf
	avr-objcopy -j .text -j .data -O ihex main.elf $(HEXFILE)
	./checksize main.elf

flash: $(HEXFILE)
	$(UISP) --erase --upload --verify if=$(HEXFILE)

flash_usb: $(HEXFILE)
	avrdude -p m8 -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0

# Fuse high byte:
# 0xc9 = 1 1 0 0   1 0 0 1 <-- BOOTRST (boot reset vector at 0x0000)
#        ^ ^ ^ ^   ^ ^ ^------ BOOTSZ0
#        | | | |   | +-------- BOOTSZ1
#        | | | |   + --------- EESAVE (don't preserve EEPROM over chip erase)
#        | | | +-------------- CKOPT (full output swing)
#        | | +---------------- SPIEN (allow serial programming)
#        | +------------------ WDTON (WDT not always on)
#        +-------------------- RSTDISBL (reset pin is enabled)
# Fuse low byte:
# 0x9f = 1 0 0 1   1 1 1 1
#        ^ ^ \ /   \--+--/
#        | |  |       +------- CKSEL 3..0 (external >8M crystal)
#        | |  +--------------- SUT 1..0 (crystal osc, BOD enabled)
#        | +------------------ BODEN (BrownOut Detector enabled)
#        +-------------------- BODLEVEL (2.7V)
fuse:
	$(UISP) --wr_fuse_h=0xc9 --wr_fuse_l=0x9f

fuse_usb:
	avrdude -p m8 -P usb -c avrispmkII -Uhfuse:w:0xc9:m -Ulfuse:w:0x9f:m -B 10.0

reset:
	avrdude -p m8 -P usb -c avrispmkII -B 10.0

erase:
	avrdude -p m8 -P usb -c avrispmkII -B 1.0 -e
