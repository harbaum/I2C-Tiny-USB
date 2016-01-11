# ======================================================================
# Makefile for i2c-tiny-usb
#
# Copyright (C) 2006 Till Harbaum
#
# This is free software, licensed under the terms of the GNU General
# Public License as published by the Free Software Foundation.
# ======================================================================
USBTINY         = ./usbtiny
TARGET_ARCH     = -DF_CPU=12000000 -DUSBTINY -mmcu=attiny45
OBJECTS         = main.o
TTY             = /dev/ttyUSB0
# TTY             = /dev/ttyS0
FLASH_CMD       = avrdude -P$(TTY) -c stk500hvsp -p attiny45 -U lfuse:w:0xdf:m -U hfuse:w:0x5f:m -U flash:w:main.hex
STACK           = 32
FLASH           = 4096
SRAM            = 256

include $(USBTINY)/common.mk
