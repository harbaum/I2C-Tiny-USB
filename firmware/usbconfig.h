/* Name: usbconfig.h
 * Project: AVR USB driver
 * Author: Christian Starkjohann, modified by Till Harbaum
 * Creation Date: 2005-04-01
 * Tabsize: 4
 * Copyright: (c) 2005 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id: usbconfig.h,v 1.4 2007/05/19 12:30:11 harbaum Exp $
 */

#ifndef __usbconfig_h_included__
#define	__usbconfig_h_included__

/*
General Description:
This file contains parts of the USB driver which can be configured and can or
must be adapted to your hardware.
*/

/* ---------------------------- Hardware Config ---------------------------- */

#if! defined (__AVR_ATtiny45__)
#define	USB_CFG_IOPORTNAME		C
/* This is the port where the USB bus is connected. When you configure it to
 * "PORTB", the registers PORTB, PINB (=PORTB+2) and DDRB (=PORTB+1) will be
 * used.
 */
#define	USB_CFG_DMINUS_BIT		0
/* This is the bit number in USB_CFG_IOPORT where the USB D- line is connected.
 * This MUST be bit 0. All other values will result in a compile error!
 */
#define	USB_CFG_DPLUS_BIT		1
/* This is the bit number in USB_CFG_IOPORT where the USB D+ line is connected.
 * This may be any bit in the port. Please note that D+ must also be connected
 * to interrupt pin INT0!
 */
#else
#define	USB_CFG_IOPORTNAME		B
#define	USB_CFG_DMINUS_BIT		0
#define	USB_CFG_DPLUS_BIT		2
#endif

/* --------------------------- Functional Range ---------------------------- */

#define	USB_CFG_HAVE_INTRIN_ENDPOINT	0
/* Define this to 1 if you want to compile a version with two endpoints: The
 * default control endpoint 0 and an interrupt-in endpoint 1.
 */
#define	USB_CFG_INTR_POLL_INTERVAL		10
/* If you compile a version with endpoint 1 (interrupt-in), this is the poll
 * interval. The value is in milliseconds and must not be less than 10 ms for
 * low speed devices.
 */
#define	USB_CFG_IS_SELF_POWERED			0
/* Define this to 1 if the device has its own power supply. Set it to 0 if the
 * device is powered from the USB bus.
 */
#define	USB_CFG_MAX_BUS_POWER			10
/* Set this variable to the maximum USB bus power consumption of your device.
 * The value is in milliamperes. [It will be divided by two since USB
 * communicates power requirements in units of 2 mA.]
 */
#define	USB_CFG_SAMPLE_EXACT			1
/* This variable affects Sampling Jitter for USB receiving. When it is 0, the
 * driver guarantees a sampling window of 1/2 bit. The USB spec requires
 * that the receiver has at most 1/4 bit sampling window. The 1/2 bit window
 * should still work reliably enough because we work at low speed. If you want
 * to meet the spec, set this value to 1. This will unroll a loop which
 * results in bigger code size.
 * If you have problems with long cables, try setting this value to 1.
 */
#define USB_CFG_IMPLEMENT_FN_WRITE		1
/* Set this to 1 if you want usbFunctionWrite() to be called for control-out
 * transfers. Set it to 0 if you don't need it and want to save a couple of
 * bytes.
 */
#define USB_CFG_IMPLEMENT_FN_READ		1
/* Set this to 1 if you need to send control replies which are generated
 * "on the fly" when usbFunctionRead() is called. If you only want to send
 * data from a static buffer, set it to 0 and return the data from
 * usbFunctionSetup(). This saves a couple of bytes.
 */

/* -------------------------- Device Description --------------------------- */

#define	USB_CFG_VENDOR_ID		0x03, 0x04
/* USB vendor ID for the device, low byte first. */
#define	USB_CFG_DEVICE_ID		0x31, 0xc6
/* This is the ID of the device, low byte first. It is interpreted in the
 * scope of the vendor ID. The only requirement is that no two devices may
 * share the same product and vendor IDs. Not even if the devices are never
 * on the same bus together!
 */
#define	USB_CFG_DEVICE_VERSION	0x05, 0x01
/* Version number of the device: Minor number first, then major number.
 */
#define	USB_CFG_VENDOR_NAME		'T', 'i', 'l', 'l', ' ', 'H', 'a', 'r', 'b', 'a', 'u', 'm'
#define	USB_CFG_VENDOR_NAME_LEN	12
/* These two values define the vendor name returned by the USB device. The name
 * must be given as a list of characters under single quotes. The characters
 * are interpreted as Unicode (UTF-16) entities.
 * If you don't want a vendor name string, undefine these macros.
 */
#define	USB_CFG_DEVICE_NAME		'i','2','c','-','t','i','n','y','-','u','s','b'
#define	USB_CFG_DEVICE_NAME_LEN	12
/* Same as above for the device name. If you don't want a device name, undefine
 * the macros.
 */
#define	USB_CFG_DEVICE_CLASS	0xff
#define	USB_CFG_DEVICE_SUBCLASS	0
/* See USB specification if you want to conform to an existing device class.
 */
#define	USB_CFG_INTERFACE_CLASS		0
#define	USB_CFG_INTERFACE_SUBCLASS	0
#define	USB_CFG_INTERFACE_PROTOCOL	0
/* See USB specification if you want to conform to an existing device class or
 * protocol.
 */


#endif /* __usbconfig_h_included__ */
