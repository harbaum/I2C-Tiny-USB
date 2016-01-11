i2c-tiny-usb - (c) 2006 by Till Harbaum 
---------------------------------------
http://www.harbaum.org/till/i2c_tiny_usb

The firmware code itself is distributed under the GPL, but
one of the usb codes comes under a separate license. Plase see 
the .txt files in usbdrv for details.

The default configuration is for a attiny45. The Makefile.mega8
allows to compile the device for the Atmega8 cpu. This includes
the possibility to use the atmega8 rs232 for debugging.

The attiny45 has to be programmed in high voltage serial 
programming (hsvp) mode since this application needs the
reset pin to be reconfigured for other use.

This project supports two usb implementations: the avrusb and
usbtiny. Due to this four Makefiles exist:

Makefile-avrusb.tiny45    - build with avrusb for Attiny45
Makefile-avrusb.mega8     - build with avrusb for Atmega8
Makefile-usbtiny.tiny45   - build with usbtiny for Attiny45
Makefile-usbtiny.mega8    - build with usbtiny for Atmega8

Just type 
 make -f Makefile-avrusb.xxx program
or
 make -f Makefile-usbtiny.xxx flash

to compile and upload the file. Please adjust e.g. programmer
settings in the Makefile.

If you don't want to recompile the firmware yourself you might
use the included firmware.hex which is a prebuilt binary for the
attiny45. Plase make sure you adjust the fuses accordingly.
They need to be set to "external crystal > 8Mhz" and the RESET
pin has to be disabled in order to be re-used for application
specific purposes. See Makefile-avrusb.tiny45 for more details.