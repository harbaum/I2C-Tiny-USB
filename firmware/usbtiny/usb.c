// ======================================================================
// USB driver
//
// Entry points:
// 	usb_init()	- enable the USB interrupt
// 	usb_poll()	- poll for incoming packets and process them
//
// This code communicates with the interrupt handler through a number of
// global variables, including two input buffers and one output buffer.
// Packets are queued for transmission by copying them into the output
// buffer. The interrupt handler will transmit such a packet on the
// reception of an IN packet.
//
// Standard SETUP packets are handled here. Non-standard SETUP packets
// are forwarded to the application code by calling usb_setup(). The
// macros USBTINY_CALLBACK_IN and USBTINY_CALLBACK_OUT control whether
// the callback functions usb_in() and usb_out() will be called for IN
// and OUT transfers.
//
// Maximum stack usage (gcc 3.4.3 & 4.1.0) of usb_poll(): 5 bytes plus
// possible additional stack usage in usb_setup(), usb_in() or usb_out().
//
// Copyright (C) 2006 Dick Streefland
//
// This is free software, licensed under the terms of the GNU General
// Public License as published by the Free Software Foundation.
// ======================================================================

#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "def.h"
#include "usb.h"

#define	LE(word)			(word) & 0xff, (word) >> 8

// ----------------------------------------------------------------------
// USB constants
// ----------------------------------------------------------------------

enum
{
	DESCRIPTOR_TYPE_DEVICE = 1,
	DESCRIPTOR_TYPE_CONFIGURATION,
	DESCRIPTOR_TYPE_STRING,
	DESCRIPTOR_TYPE_INTERFACE,
	DESCRIPTOR_TYPE_ENDPOINT,
};

// ----------------------------------------------------------------------
// Interrupt handler interface
// ----------------------------------------------------------------------

byte_t	usb_rx_buf[2*USB_BUFSIZE];	// two input buffers
byte_t	usb_rx_off;			// buffer offset: 0 or USB_BUFSIZE
byte_t	usb_rx_len;			// buffer size, 0 means empty
byte_t	usb_rx_token;			// PID of token packet: SETUP or OUT

byte_t	usb_tx_buf[USB_BUFSIZE];	// output buffer
byte_t	usb_tx_len;			// output buffer size, 0 means empty

byte_t	usb_address;			// assigned USB address

// ----------------------------------------------------------------------
// Local data
// ----------------------------------------------------------------------

enum
{
	TX_STATE_IDLE = 0,		// transmitter idle
	TX_STATE_RAM,			// usb_tx_data is a RAM address
	TX_STATE_ROM,			// usb_tx_data is a ROM address
	TX_STATE_CALLBACK,		// call usb_in() to obtain transmit data
};

static	byte_t	usb_tx_state;		// TX_STATE_*, see enum above
static	byte_t	usb_tx_total;		// total transmit size
static	byte_t*	usb_tx_data;		// pointer to data to transmit
static	byte_t	new_address;		// new device address

#if	defined USBTINY_VENDOR_NAME
struct
{
	byte_t	length;
	byte_t	type;
	int	string[sizeof(USBTINY_VENDOR_NAME)-1];
}	string_vendor PROGMEM =
{
	2 * sizeof(USBTINY_VENDOR_NAME),
	DESCRIPTOR_TYPE_STRING,
	{ CAT2(L, USBTINY_VENDOR_NAME) }
};
#  define	VENDOR_NAME_ID	1
#else
#  define	VENDOR_NAME_ID	0
#endif

#if	defined USBTINY_DEVICE_NAME
struct
{
	byte_t	length;
	byte_t	type;
	int	string[sizeof(USBTINY_DEVICE_NAME)-1];
}	string_device PROGMEM =
{
	2 * sizeof(USBTINY_DEVICE_NAME),
	DESCRIPTOR_TYPE_STRING,
	{ CAT2(L, USBTINY_DEVICE_NAME) }
};
#  define	DEVICE_NAME_ID	2
#else
#  define	DEVICE_NAME_ID	0
#endif

#if	defined USBTINY_SERIAL
struct
{
	byte_t	length;
	byte_t	type;
	int	string[sizeof(USBTINY_SERIAL)-1];
}	string_serial PROGMEM =
{
	2 * sizeof(USBTINY_SERIAL),
	DESCRIPTOR_TYPE_STRING,
	{ CAT2(L, USBTINY_SERIAL) }
};
#  define	SERIAL_ID	3
#else
#  define	SERIAL_ID	0
#endif

#if	VENDOR_NAME_ID || DEVICE_NAME_ID || SERIAL_ID
static	byte_t	string_langid [] PROGMEM =
{
	4,				// bLength
	DESCRIPTOR_TYPE_STRING,		// bDescriptorType (string)
	LE(0x0409),			// wLANGID[0] (American English)
};
#endif

// Device Descriptor
static	byte_t	descr_device [18] PROGMEM =
{
	18,				// bLength
	DESCRIPTOR_TYPE_DEVICE,		// bDescriptorType
	LE(0x0110),			// bcdUSB
	USBTINY_DEVICE_CLASS,		// bDeviceClass
	USBTINY_DEVICE_SUBCLASS,	// bDeviceSubClass
	USBTINY_DEVICE_PROTOCOL,	// bDeviceProtocol
	8,				// bMaxPacketSize0
	LE(USBTINY_VENDOR_ID),		// idVendor
	LE(USBTINY_DEVICE_ID),		// idProduct
	LE(USBTINY_DEVICE_VERSION),	// bcdDevice
	VENDOR_NAME_ID,			// iManufacturer
	DEVICE_NAME_ID,			// iProduct
	SERIAL_ID,			// iSerialNumber
	1,				// bNumConfigurations
};

// Configuration Descriptor
static	byte_t	descr_config [] PROGMEM =
{
	9,				// bLength
	DESCRIPTOR_TYPE_CONFIGURATION,	// bDescriptorType
	LE(9+9+7*USBTINY_ENDPOINT),	// wTotalLength
	1,				// bNumInterfaces
	1,				// bConfigurationValue
	0,				// iConfiguration
	(USBTINY_MAX_POWER ? 0x80 : 0xc0), // bmAttributes
	(USBTINY_MAX_POWER + 1) / 2,	// MaxPower

	// Standard Interface Descriptor
	9,				// bLength
	DESCRIPTOR_TYPE_INTERFACE,	// bDescriptorType
	0,				// bInterfaceNumber
	0,				// bAlternateSetting
	USBTINY_ENDPOINT,		// bNumEndpoints
	USBTINY_INTERFACE_CLASS,	// bInterfaceClass
	USBTINY_INTERFACE_SUBCLASS,	// bInterfaceSubClass
	USBTINY_INTERFACE_PROTOCOL,	// bInterfaceProtocol
	0,				// iInterface

#if	USBTINY_ENDPOINT
	// Additional Endpoint
	7,				// bLength
	DESCRIPTOR_TYPE_ENDPOINT,	// bDescriptorType
	USBTINY_ENDPOINT_ADDRESS,	// bEndpointAddress
	USBTINY_ENDPOINT_TYPE,		// bmAttributes
	LE(8),				// wMaxPacketSize
	USBTINY_ENDPOINT_INTERVAL,	// bInterval
#endif
};

// ----------------------------------------------------------------------
// Inspect an incoming packet.
// ----------------------------------------------------------------------
static	void	usb_receive ( byte_t* data, byte_t rx_len )
{
	byte_t	len;
	byte_t	type;
	byte_t	limit;

	usb_tx_state = TX_STATE_RAM;
	len = 0;
	if	( usb_rx_token == USB_PID_SETUP )
	{
		limit = data[6];
		if	( data[7] )
		{
			limit = 255;
		}
		type = data[0] & 0x60;
		if	( type == 0x00 )
		{	// Standard request
			if	( data[1] == 0 )	// GET_STATUS
			{
				len = 2;
#if	USBTINY_MAX_POWER == 0
				data[0] = (data[0] == 0x80);
#else
				data[0] = 0;
#endif
				data[1] = 0;
			}
			else if	( data[1] == 5 )	// SET_ADDRESS
			{
				new_address = data[2];
			}
			else if	( data[1] == 6 )	// GET_DESCRIPTOR
			{
				usb_tx_state = TX_STATE_ROM;
				if	( data[3] == 1 )
				{	// DEVICE
					data = (byte_t*) &descr_device;
					len = sizeof(descr_device);
				}
				else if	( data[3] == 2 )
				{	// CONFIGURATION
					data = (byte_t*) &descr_config;
					len = sizeof(descr_config);
				}
#if	VENDOR_NAME_ID || DEVICE_NAME_ID || SERIAL_ID
				else if	( data[3] == 3 )
				{	// STRING
					if	( data[2] == 0 )
					{
						data = (byte_t*) &string_langid;
						len = sizeof(string_langid);
					}
#if	VENDOR_NAME_ID
					else if	( data[2] == VENDOR_NAME_ID )
					{
						data = (byte_t*) &string_vendor;
						len = sizeof(string_vendor);
					}
#endif
#if	DEVICE_NAME_ID
					else if ( data[2] == DEVICE_NAME_ID )
					{
						data = (byte_t*) &string_device;
						len = sizeof(string_device);
					}
#endif
#if	SERIAL_ID
					else if ( data[2] == SERIAL_ID )
					{
						data = (byte_t*) &string_serial;
						len = sizeof(string_serial);
					}
#endif
				}
#endif
			}
			else if	( data[1] == 8 )	// GET_CONFIGURATION
			{
				data[0] = 1;		// return bConfigurationValue
				len = 1;
			}
			else if	( data[1] == 10 )	// GET_INTERFACE
			{
				data[0] = 0;
				len = 1;
			}
		}
		else
		{	// Class or Vendor request
			len = usb_setup( data );
#if	USBTINY_CALLBACK_IN
			if	( len == 0xff )
			{
				usb_tx_state = TX_STATE_CALLBACK;
			}
#endif
		}
		if	( len > limit )
		{
			len = limit;
		}
		usb_tx_data = data;
	}
#if	USBTINY_CALLBACK_OUT
	else if	( rx_len > 0 )
	{	// usb_rx_token == USB_PID_OUT
		usb_out( data, rx_len );
	}
#endif
	usb_tx_total  = len;
	usb_tx_buf[0] = USB_PID_DATA0;	// next data packet will be DATA1
}

// ----------------------------------------------------------------------
// Load the transmit buffer with the next packet.
// ----------------------------------------------------------------------
static	void	usb_transmit ( void )
{
	byte_t	len;
	byte_t*	src;
	byte_t*	dst;
	byte_t	i;
	byte_t	b;

	usb_tx_buf[0] ^= (USB_PID_DATA0 ^ USB_PID_DATA1);
	len = usb_tx_total;
	if	( len > 8 )
	{
		len = 8;
	}
	dst = usb_tx_buf + 1;
	if	( len > 0 )
	{
#if	USBTINY_CALLBACK_IN
		if	( usb_tx_state == TX_STATE_CALLBACK )
		{
			len = usb_in( dst, len );
		}
		else
#endif
		{
			src = usb_tx_data;
			if	( usb_tx_state == TX_STATE_RAM )
			{
				for	( i = 0; i < len; i++ )
				{
					*dst++ = *src++;
				}
			}
			else	// usb_tx_state == TX_STATE_ROM
			{
				for	( i = 0; i < len; i++ )
				{
					b = pgm_read_byte( src );
					src++;
					*dst++ = b;
				}
			}
			usb_tx_data = src;
		}
		usb_tx_total -= len;
	}
	crc( usb_tx_buf + 1, len );
	usb_tx_len = len + 3;
	if	( len < 8 )
	{	// this is the last packet
		usb_tx_state = TX_STATE_IDLE;
	}
}

// ----------------------------------------------------------------------
// Initialize the low-level USB driver.
// ----------------------------------------------------------------------
extern	void	usb_init ( void )
{
	USB_INT_CONFIG |= USB_INT_CONFIG_SET;
	USB_INT_ENABLE |= (1 << USB_INT_ENABLE_BIT);
	sei();
}

// ----------------------------------------------------------------------
// Poll USB driver:
// - check for incoming USB packets
// - refill an empty transmit buffer
// - check for USB bus reset
// ----------------------------------------------------------------------
extern	void	usb_poll ( void )
{
	byte_t	i;

	// check for incoming USB packets
	if	( usb_rx_len != 0 )
	{
		usb_receive( usb_rx_buf + USB_BUFSIZE - usb_rx_off + 1, usb_rx_len - 3 );
		usb_tx_len = 0;	// abort pending transmission
		usb_rx_len = 0;	// accept next packet
	}
	// refill an empty transmit buffer, when the transmitter is active
	if	( usb_tx_len == 0 )
	{
		if	( usb_tx_state != TX_STATE_IDLE )
		{
			usb_transmit();
		}
		else
		{	// change the USB address at the end of a transfer
			usb_address = new_address;
		}
	}
	// check for USB bus reset
	for	( i = 10; i > 0 && ! (USB_IN & USB_MASK_DMINUS); i-- )
	{
	}
	if	( i == 0 )
	{	// SE0 for more than 2.5uS is a reset
	         cli();
                 usb_tx_len=0;
                 usb_rx_len=0;
                 new_address = 0;
                 sei();
	}
}
