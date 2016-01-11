// ======================================================================
// USBtiny Configuration
//
// Copyright (C) 2006 Dick Streefland
//
// This is free software, licensed under the terms of the GNU General
// Public License as published by the Free Software Foundation.
// ======================================================================

// The D+ and D- USB signals should be connected to two pins of the same
// I/O port. The following macros define the port letter and the input
// bit numbers:
#if! defined (__AVR_ATtiny45__)
#define	USBTINY_PORT			C
#define	USBTINY_DPLUS			1
#define	USBTINY_DMINUS			0
#else
#define	USBTINY_PORT			B
#define	USBTINY_DPLUS			2
#define	USBTINY_DMINUS			0
#endif

// The D+ signal should be connected to an interrupt input to trigger an
// interrupt at the start of a packet. When you use the same pin for the
// D+ USB signal and the interrupt input, only two I/O pins are needed
// for the USB interface. The following macro defines the interrupt
// number:
#define	USBTINY_INT			0

// The power requirement of the USB device in mA, or 0 when the device
// is not bus powered:
#define	USBTINY_MAX_POWER		10

// The USB vendor and device IDs. These values should be unique for
// every distinct device. You can get your own vendor ID from the USB
// Implementers Forum (www.usb.org) if you have a spare $1500 to kill.
// Alternatively, you can buy a small range of device IDs from
// www.voti.nl or www.mecanique.co.uk, or be naughty and use something
// else, like for instance product ID 0x6666, which is registered as
// "Prototype product Vendor ID".
#define	USBTINY_VENDOR_ID		0x0403
#define	USBTINY_DEVICE_ID		0xc631

// The version of the device as a 16-bit number: 256*major + minor.
#define	USBTINY_DEVICE_VERSION		0x205

// The following optional macros may be used as an identification of
// your device. Undefine them when you run out of flash space.
#define	USBTINY_VENDOR_NAME		"Till Harbaum"
#define	USBTINY_DEVICE_NAME		"i2c-tiny-usb"
#undef	USBTINY_SERIAL

// Define the device class, subclass and protocol. Device class 0xff
// is "vendor specific".
#define	USBTINY_DEVICE_CLASS		0xff
#define	USBTINY_DEVICE_SUBCLASS		0
#define	USBTINY_DEVICE_PROTOCOL		0

// Define the interface class, subclass and protocol. Interface class
// 0xff is "vendor specific".
#define	USBTINY_INTERFACE_CLASS		0xff
#define	USBTINY_INTERFACE_SUBCLASS	0
#define	USBTINY_INTERFACE_PROTOCOL	0

// Normally, usb_setup() should write the reply of up to 8 bytes into the
// packet buffer, and return the reply length. When this macro is defined
// as 1, you have the option of returning 0xff instead. In that case, the
// USB driver will call a function usb_in() to obtain the data to send
// back to the host. This can be used to generate the data on-the-fly.
#define	USBTINY_CALLBACK_IN		1

// When this macro is defined as 0, OUT packets are simply ignored.
// When defined as 1, the function usb_out() is called for OUT packets.
// You need this option to send data from the host to the device in
// a control transfer.
#define	USBTINY_CALLBACK_OUT		1

// Set the macro USBTINY_ENDPOINT to 1 to add an additional endpoint,
// according to the values of the three other macros.
#define	USBTINY_ENDPOINT		0
#define	USBTINY_ENDPOINT_ADDRESS	0x81	// IN endpoint #1
#define	USBTINY_ENDPOINT_TYPE		0x00	// control transfer type
#define	USBTINY_ENDPOINT_INTERVAL	0	// ignored
