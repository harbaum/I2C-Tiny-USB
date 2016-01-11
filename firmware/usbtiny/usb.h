// ======================================================================
// Public interface of the USB driver
//
// Copyright (C) 2006 Dick Streefland
//
// This is free software, licensed under the terms of the GNU General
// Public License as published by the Free Software Foundation.
// ======================================================================

#ifndef USB_H
#define	USB_H

typedef	unsigned char	byte_t;
typedef	unsigned int	uint_t;

// usb.c
extern	void		usb_init ( void );
extern	void		usb_poll ( void );

// crc.S
extern	void		crc ( byte_t* data, byte_t len );

// application callback functions
extern	byte_t		usb_setup ( byte_t data[8] );
extern	void		usb_out ( byte_t* data, byte_t len );
extern	byte_t		usb_in ( byte_t* data, byte_t len );

#endif	// USB_H
