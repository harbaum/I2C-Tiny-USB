This is the Readme file to Objective Development's firmware-only USB driver
for Atmel AVR microcontrollers. For more information please visit
http://www.obdev.at/avrusb/

This directory contains the USB firmware only. Copy it as-is to your own
project and add your own version of "usbconfig.h". A template for your own
"usbconfig.h" can be found in "usbconfig-prototype.h" in this directory.


TECHNICAL DOCUMENTATION
=======================
The technical documentation for the firmware driver is contained in the file
"usbdrv.h". Please read all of it carefully!


USB IDENTIFIERS
===============
Every USB device needs a vendor- and a product-identifier (VID and PID). VIDs
are obtained from usb.org for a price of 1,500 USD. Once you have a VID, you
can assign PIDs at will.

Since an entry level cost of 1,500 USD is too high for most small companies
and hobbyists, we provide a single VID/PID pair for free. If you want to use
your own VID and PID instead of our's, define the macros "USB_CFG_VENDOR_ID"
and "USB_CFG_DEVICE_ID" accordingly in "usbconfig.h".

To use our predefined VID/PID pair, you MUST conform to a couple of
requirements. See the file "USBID-License.txt" for details.

Objective Development also has some offerings which include product IDs. See
http://www.obdev.at/avrusb/ for details.


HOST DRIVER
===========
You have received this driver together with an example device implementation
and an example host driver. The host driver is based on libusb and compiles
on various Unix flavors (Linux, BSD, Mac OS X). It also compiles natively on
Windows using MinGW (see www.mingw.org) and libusb-win32 (see
libusb-win32.sourceforge.net). The "Automator" project contains a native
Windows host driver (not based on libusb) for Human Interface Devices.


DEVELOPMENT SYSTEM
==================
This driver has been developed and optimized for the GNU compiler version 3
(gcc 3). It does work well with gcc 4 and future versions will probably be
optimized for gcc 4. We recommend that you use the GNU compiler suite because
it is freely available. AVR-USB has also been ported to the IAR compiler and
assembler. It has been tested with IAR 4.10B/W32 and 4.12A/W32 on an ATmega8
with the "small" and "tiny" memory model. Please note that gcc is more
efficient for usbdrv.c because this module has been deliberately optimized
for gcc.


USING AVR-USB FOR FREE
======================
The AVR firmware driver is published under the GNU General Public License
Version 2 (GPL2). See the file "License.txt" for details.

If you decide for the free GPL2, we STRONGLY ENCOURAGE you to do the following
things IN ADDITION to the obligations from the GPL2:

(1) Publish your entire project on a web site and drop us a note with the URL.
Use the form at http://www.obdev.at/avrusb/feedback.html for your submission.

(2) Adhere to minimum publication standards. Please include AT LEAST:
    - a circuit diagram in PDF, PNG or GIF format
    - full source code for the host software
    - a Readme.txt file in ASCII format which describes the purpose of the
      project and what can be found in which directories and which files
    - a reference to http://www.obdev.at/avrusb/

(3) If you improve the driver firmware itself, please give us a free license
to your modifications for our commercial license offerings.


COMMERCIAL LICENSES FOR AVR-USB
===============================
If you don't want to publish your source code under the terms of the GPL2,
you can simply pay money for AVR-USB. As an additional benefit you get
USB PIDs for free, licensed exclusively to you. See the file
"CommercialLicense.txt" for details.

