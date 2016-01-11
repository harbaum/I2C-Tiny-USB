// ======================================================================
// Common definitions for the USB driver
//
// Copyright (C) 2006 Dick Streefland
//
// This is free software, licensed under the terms of the GNU General
// Public License as published by the Free Software Foundation.
// ======================================================================

#ifdef __ASSEMBLER__
#define	__SFR_OFFSET		0
#endif
#include <avr/io.h>
#include "usbtiny.h"

// Preprocessor magic
#define	CAT2(a,b)		CAT2EXP(a, b)
#define	CAT2EXP(a,b)		a ## b
#define	CAT3(a,b,c)		CAT3EXP(a, b, c)
#define	CAT3EXP(a,b,c)		a ## b ## c

// I/O Ports
#define	USB_IN			CAT2(PIN, USBTINY_PORT)
#define	USB_OUT			CAT2(PORT, USBTINY_PORT)
#define	USB_DDR			CAT2(DDR, USBTINY_PORT)

// I/O bit masks
#define	USB_MASK_DMINUS		(1 << (USBTINY_DMINUS))
#define	USB_MASK_DPLUS		(1 << (USBTINY_DPLUS))
#define	USB_MASK		(USB_MASK_DMINUS | USB_MASK_DPLUS)

// Interrupt configuration
#if	defined	EICRA
#  define USB_INT_CONFIG	EICRA
#else
#  define USB_INT_CONFIG	MCUCR
#endif
#define	USB_INT_CONFIG_SET	((1 << CAT3(ISC,USBTINY_INT,1)) | (1 << CAT3(ISC,USBTINY_INT,0)))
#if	defined SIG_INT0
#  define USB_INT_VECTOR	CAT2(SIG_INT, USBTINY_INT)
#else
#  define USB_INT_VECTOR	CAT2(SIG_INTERRUPT, USBTINY_INT)
#endif

// Interrupt enable
#if	defined	GIMSK
#  define USB_INT_ENABLE	GIMSK
#elif	defined	EIMSK
#  define USB_INT_ENABLE	EIMSK
#else
#  define USB_INT_ENABLE	GICR
#endif
#define	USB_INT_ENABLE_BIT	CAT2(INT,USBTINY_INT)

// Interrupt pending bit
#if	defined	EIFR
#  define USB_INT_PENDING	EIFR
#else
#  define USB_INT_PENDING	GIFR
#endif
#define	USB_INT_PENDING_BIT	CAT2(INTF,USBTINY_INT)

// USB PID values
#define	USB_PID_SETUP		0x2d
#define	USB_PID_OUT		0xe1
#define	USB_PID_IN		0x69
#define	USB_PID_DATA0		0xc3
#define	USB_PID_DATA1		0x4b
#define	USB_PID_ACK		0xd2
#define	USB_PID_NAK		0x5a
#define	USB_PID_STALL		0x1e

// Various constants
#define	USB_BUFSIZE		11	// PID + data + CRC
