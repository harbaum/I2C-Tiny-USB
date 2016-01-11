/* Name: main.c
 * Project: i2c-tiny-usb
 * Author: Till Harbaum
 * Tabsize: 4
 * Copyright: (c) 2005 by Till Harbaum <till@harbaum.org>
 * License: GPL
 * This Revision: $Id: main.c,v 1.9 2007/06/07 13:53:47 harbaum Exp $
 *
 * $Log: main.c,v $
 * Revision 1.9  2007/06/07 13:53:47  harbaum
 * Version number fixes
 *
 * Revision 1.8  2007/05/19 12:30:11  harbaum
 * Updated USB stacks
 *
 * Revision 1.7  2007/04/22 10:34:05  harbaum
 * *** empty log message ***
 *
 * Revision 1.6  2007/01/05 19:30:58  harbaum
 * i2c clock bug fix
 *
 * Revision 1.5  2007/01/03 18:35:07  harbaum
 * usbtiny fixes and pcb layouts
 *
 * Revision 1.4  2006/12/03 21:28:59  harbaum
 * *** empty log message ***
 *
 * Revision 1.3  2006/11/22 19:12:45  harbaum
 * Added usbtiny support
 *
 * Revision 1.2  2006/11/14 19:15:13  harbaum
 * *** empty log message ***
 *
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>

#include <util/delay.h>

#ifndef USBTINY
// use avrusb library
#include "usbdrv.h"
#include "oddebug.h"
#else
// use usbtiny library 
#include "usb.h"
#include "usbtiny.h"
typedef byte_t uchar;

#if! defined (__AVR_ATtiny45__)
#define USBDDR DDRC
#define USB_CFG_IOPORT PORTC
#else
#define USBDDR DDRB
#define USB_CFG_IOPORT PORTB
#endif

#define USB_CFG_DMINUS_BIT USBTINY_DMINUS
#define USB_CFG_DPLUS_BIT USBTINY_DPLUS

#define usbInit()  usb_init()
#define usbPoll()  usb_poll()
#endif

#define ENABLE_SCL_EXPAND

/* commands from USB, must e.g. match command ids in kernel driver */
#define CMD_ECHO       0
#define CMD_GET_FUNC   1
#define CMD_SET_DELAY  2
#define CMD_GET_STATUS 3

#define CMD_I2C_IO     4
#define CMD_I2C_BEGIN  1  // flag fo I2C_IO
#define CMD_I2C_END    2  // flag fo I2C_IO

/* linux kernel flags */
#define I2C_M_TEN		0x10	/* we have a ten bit chip address */
#define I2C_M_RD		0x01
#define I2C_M_NOSTART		0x4000
#define I2C_M_REV_DIR_ADDR	0x2000
#define I2C_M_IGNORE_NAK	0x1000
#define I2C_M_NO_RD_ACK		0x0800

/* To determine what functionality is present */
#define I2C_FUNC_I2C			0x00000001
#define I2C_FUNC_10BIT_ADDR		0x00000002
#define I2C_FUNC_PROTOCOL_MANGLING	0x00000004 /* I2C_M_{REV_DIR_ADDR,NOSTART,..} */
#define I2C_FUNC_SMBUS_HWPEC_CALC	0x00000008 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_READ_WORD_DATA_PEC  0x00000800 /* SMBus 2.0 */ 
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA_PEC 0x00001000 /* SMBus 2.0 */ 
#define I2C_FUNC_SMBUS_PROC_CALL_PEC	0x00002000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL_PEC 0x00004000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0x00008000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_QUICK		0x00010000 
#define I2C_FUNC_SMBUS_READ_BYTE	0x00020000 
#define I2C_FUNC_SMBUS_WRITE_BYTE	0x00040000 
#define I2C_FUNC_SMBUS_READ_BYTE_DATA	0x00080000 
#define I2C_FUNC_SMBUS_WRITE_BYTE_DATA	0x00100000 
#define I2C_FUNC_SMBUS_READ_WORD_DATA	0x00200000 
#define I2C_FUNC_SMBUS_WRITE_WORD_DATA	0x00400000 
#define I2C_FUNC_SMBUS_PROC_CALL	0x00800000 
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0x01000000 
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA 0x02000000 
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK	0x04000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK	0x08000000 /* w/ 1-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_I2C_BLOCK_2	 0x10000000 /* I2C-like block xfer  */
#define I2C_FUNC_SMBUS_WRITE_I2C_BLOCK_2 0x20000000 /* w/ 2-byte reg. addr. */
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA_PEC  0x40000000 /* SMBus 2.0 */
#define I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC 0x80000000 /* SMBus 2.0 */

#define I2C_FUNC_SMBUS_BYTE I2C_FUNC_SMBUS_READ_BYTE | \
                            I2C_FUNC_SMBUS_WRITE_BYTE
#define I2C_FUNC_SMBUS_BYTE_DATA I2C_FUNC_SMBUS_READ_BYTE_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_BYTE_DATA
#define I2C_FUNC_SMBUS_WORD_DATA I2C_FUNC_SMBUS_READ_WORD_DATA | \
                                 I2C_FUNC_SMBUS_WRITE_WORD_DATA
#define I2C_FUNC_SMBUS_BLOCK_DATA I2C_FUNC_SMBUS_READ_BLOCK_DATA | \
                                  I2C_FUNC_SMBUS_WRITE_BLOCK_DATA
#define I2C_FUNC_SMBUS_I2C_BLOCK I2C_FUNC_SMBUS_READ_I2C_BLOCK | \
                                  I2C_FUNC_SMBUS_WRITE_I2C_BLOCK

#define I2C_FUNC_SMBUS_EMUL I2C_FUNC_SMBUS_QUICK | \
                            I2C_FUNC_SMBUS_BYTE | \
                            I2C_FUNC_SMBUS_BYTE_DATA | \
                            I2C_FUNC_SMBUS_WORD_DATA | \
                            I2C_FUNC_SMBUS_PROC_CALL | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA | \
                            I2C_FUNC_SMBUS_WRITE_BLOCK_DATA_PEC | \
                            I2C_FUNC_SMBUS_I2C_BLOCK

/* the currently support capability is quite limited */
const unsigned long func PROGMEM = I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;

#ifdef DEBUG
#define DEBUGF(format, args...) printf_P(PSTR(format), ##args)

/* ------------------------------------------------------------------------- */
static int uart_putchar(char c, FILE *stream) {
  if (c == '\n')
    uart_putchar('\r', stream);
  loop_until_bit_is_set(UCSRA, UDRE);
  UDR = c;
  return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM(uart_putchar, NULL,
                                         _FDEV_SETUP_WRITE);
#else
#define DEBUGF(format, args...)
#endif

/* ------------------------------------------------------------------------- */
#define DEFAULT_DELAY 10  // default 10us (100khz)
static unsigned short clock_delay  = DEFAULT_DELAY;
static unsigned short clock_delay2 = DEFAULT_DELAY/2;

static unsigned short expected;
static unsigned char saved_cmd;

#if! defined (__AVR_ATtiny45__)
#define I2C_PORT   PORTC
#define I2C_PIN    PINC
#define I2C_DDR    DDRC
#define I2C_SDA    _BV(4)
#define I2C_SCL    _BV(5)
#else
#define I2C_PORT   PORTB
#define I2C_PIN    PINB
#define I2C_DDR    DDRB
#define I2C_SDA    _BV(1)
#define I2C_SCL    _BV(5)
#endif

static void i2c_io_set_sda(uchar hi) {
  if(hi) {
    I2C_DDR  &= ~I2C_SDA;    // high -> input
    I2C_PORT |=  I2C_SDA;    // with pullup
  } else {
    I2C_DDR  |=  I2C_SDA;    // low -> output
    I2C_PORT &= ~I2C_SDA;    // drive low
  }
}

static uchar i2c_io_get_sda(void) {
  return(I2C_PIN & I2C_SDA);
}

static void i2c_io_set_scl(uchar hi) {
#ifdef ENABLE_SCL_EXPAND
  _delay_loop_2(clock_delay2);
  if(hi) {
    I2C_DDR &= ~I2C_SCL;          // port is input
    I2C_PORT |= I2C_SCL;          // enable pullup

    // wait while pin is pulled low by client
    while(!(I2C_PIN & I2C_SCL));
  } else {
    I2C_DDR |= I2C_SCL;           // port is output
    I2C_PORT &= ~I2C_SCL;         // drive it low
  }
  _delay_loop_2(clock_delay);
#else
  _delay_loop_2(clock_delay2);
  if(hi) I2C_PORT |=  I2C_SCL;    // port is high
  else   I2C_PORT &= ~I2C_SCL;    // port is low
  _delay_loop_2(clock_delay);
#endif
}

static void i2c_init(void) {
  /* init the sda/scl pins */
  I2C_DDR &= ~I2C_SDA;            // port is input
  I2C_PORT |= I2C_SDA;            // enable pullup
#ifdef ENABLE_SCL_EXPAND
  I2C_DDR &= ~I2C_SCL;            // port is input
  I2C_PORT |= I2C_SCL;            // enable pullup
#else
  I2C_DDR |= I2C_SCL;             // port is output
#endif

  /* no bytes to be expected */
  expected = 0;
}

/* clock HI, delay, then LO */
static void i2c_scl_toggle(void) {
  i2c_io_set_scl(1);
  i2c_io_set_scl(0);
}

/* i2c start condition */
static void i2c_start(void) {
  i2c_io_set_sda(0);
  i2c_io_set_scl(0);
}

/* i2c repeated start condition */
static void i2c_repstart(void) 
{
  /* scl, sda may not be high */
  i2c_io_set_sda(1);
  i2c_io_set_scl(1);
  
  i2c_io_set_sda(0);
  i2c_io_set_scl(0);
}

/* i2c stop condition */
void i2c_stop(void) {
  i2c_io_set_sda(0);
  i2c_io_set_scl(1);
  i2c_io_set_sda(1);
}

uchar i2c_put_u08(uchar b) {
  char i;

  for (i=7;i>=0;i--) {
    if ( b & (1<<i) )  i2c_io_set_sda(1);
    else               i2c_io_set_sda(0);
    
    i2c_scl_toggle();           // clock HI, delay, then LO
  }
  
  i2c_io_set_sda(1);            // leave SDL HI
  i2c_io_set_scl(1);            // clock back up

  b = i2c_io_get_sda();         // get the ACK bit
  i2c_io_set_scl(0);            // not really ??

  return(b == 0);               // return ACK value
}

uchar i2c_get_u08(uchar last) {
  char i;
  uchar c,b = 0;

  i2c_io_set_sda(1);            // make sure pullups are activated
  i2c_io_set_scl(0);            // clock LOW

  for(i=7;i>=0;i--) {
    i2c_io_set_scl(1);          // clock HI
    c = i2c_io_get_sda();
    b <<= 1;
    if(c) b |= 1;
    i2c_io_set_scl(0);          // clock LO
  }

  if(last) i2c_io_set_sda(1);   // set NAK
  else     i2c_io_set_sda(0);   // set ACK

  i2c_scl_toggle();             // clock pulse
  i2c_io_set_sda(1);            // leave with SDL HI

  return b;                     // return received byte
}

#ifdef DEBUG
void i2c_scan(void) {
  uchar i = 0;

  for(i=0;i<127;i++) {
    i2c_start();                  // do start transition
    if(i2c_put_u08(i << 1))       // send DEVICE address
      DEBUGF("I2C device at address 0x%x\n", i);

    i2c_stop();
  }
}
#endif

/* ------------------------------------------------------------------------- */

struct i2c_cmd {
  unsigned char type;
  unsigned char cmd;
  unsigned short flags;
  unsigned short addr;
  unsigned short len;  
};

#define STATUS_IDLE          0
#define STATUS_ADDRESS_ACK   1
#define STATUS_ADDRESS_NAK   2

static uchar status = STATUS_IDLE;

static uchar i2c_do(struct i2c_cmd *cmd) {
  uchar addr;

  DEBUGF("i2c %s at 0x%02x, len = %d\n", 
	   (cmd->flags&I2C_M_RD)?"rd":"wr", cmd->addr, cmd->len); 

  /* normal 7bit address */
  addr = ( cmd->addr << 1 );
  if (cmd->flags & I2C_M_RD )
    addr |= 1;

  if(cmd->cmd & CMD_I2C_BEGIN) 
    i2c_start();
  else 
    i2c_repstart();    

  // send DEVICE address
  if(!i2c_put_u08(addr)) {
    DEBUGF("I2C read: address error @ %x\n", addr);

    status = STATUS_ADDRESS_NAK;
    expected = 0;
    i2c_stop();
  } else {  
    status = STATUS_ADDRESS_ACK;
    expected = cmd->len;
    saved_cmd = cmd->cmd;

    /* check if transfer is already done (or failed) */
    if((cmd->cmd & CMD_I2C_END) && !expected) 
      i2c_stop();
  }

  /* more data to be expected? */
#ifndef USBTINY
  return(cmd->len?0xff:0x00);
#else
  return(((cmd->flags & I2C_M_RD) && cmd->len)?0xff:0x00);
#endif
}

#ifndef USBTINY
uchar	usbFunctionSetup(uchar data[8]) {
  static uchar replyBuf[4];
  usbMsgPtr = replyBuf;
#else
extern	byte_t	usb_setup ( byte_t data[8] )
{
  byte_t *replyBuf = data;
#endif

  DEBUGF("Setup %x %x %x %x\n", data[0], data[1], data[2], data[3]);

  switch(data[1]) {

  case CMD_ECHO: // echo (for transfer reliability testing)
    replyBuf[0] = data[2];
    replyBuf[1] = data[3];
    return 2;
    break;

  case CMD_GET_FUNC:
    memcpy_P(replyBuf, &func, sizeof(func));
    return sizeof(func);
    break;

  case CMD_SET_DELAY:
    /* The delay function used delays 4 system ticks per cycle. */
    /* This gives 1/3us at 12Mhz per cycle. The delay function is */
    /* called twice per clock edge and thus four times per full cycle. */ 
    /* Thus it is called one time per edge with the full delay */ 
    /* value and one time with the half one. Resulting in */
    /* 2 * n * 1/3 + 2 * 1/2 n * 1/3 = n us. */
    clock_delay = *(unsigned short*)(data+2);
    if(!clock_delay) clock_delay = 1;
    clock_delay2 = clock_delay/2;
    if(!clock_delay2) clock_delay2 = 1;

    DEBUGF("request for delay %dus\n", clock_delay); 
    break;

  case CMD_I2C_IO:
  case CMD_I2C_IO + CMD_I2C_BEGIN:
  case CMD_I2C_IO                 + CMD_I2C_END:
  case CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END:
    // these are only allowed as class transfers

    return i2c_do((struct i2c_cmd*)data);
    break;

  case CMD_GET_STATUS:
    replyBuf[0] = status;
    return 1;
    break;

  default:
    // must not happen ...
    break;
  }

  return 0;  // reply len
}


/*---------------------------------------------------------------------------*/
/* usbFunctionRead                                                           */
/*---------------------------------------------------------------------------*/

#ifndef USBTINY
uchar usbFunctionRead( uchar *data, uchar len )
#else
extern	byte_t	usb_in ( byte_t* data, byte_t len )
#endif
{
  uchar i;

  DEBUGF("read %d bytes, %d exp\n", len, expected);

  if(status == STATUS_ADDRESS_ACK) {
    if(len > expected) {
      DEBUGF("exceeds!\n");
      len = expected;
    }

    // consume bytes
    for(i=0;i<len;i++) {
      expected--;
      *data = i2c_get_u08(expected == 0);
      DEBUGF("data = %x\n", *data);
      data++;
    }

    // end transfer on last byte
    if((saved_cmd & CMD_I2C_END) && !expected) 
      i2c_stop();

  } else {
    DEBUGF("not in ack state\n");
    memset(data, 0, len);
  }
  return len;
}

/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/

#ifndef USBTINY
uchar usbFunctionWrite( uchar *data, uchar len )
#else
extern	void	usb_out ( byte_t* data, byte_t len )
#endif
{
  uchar i, err=0;

  DEBUGF("write %d bytes, %d exp\n", len, expected);

  if(status == STATUS_ADDRESS_ACK) {
    if(len > expected) {
      DEBUGF("exceeds!\n");
      len = expected;
    }

    // consume bytes
    for(i=0;i<len;i++) {
      expected--;
      DEBUGF("data = %x\n", *data);
      if(!i2c_put_u08(*data++))
	err = 1;
    }

    // end transfer on last byte
    if((saved_cmd & CMD_I2C_END) && !expected) 
      i2c_stop();

    if(err) {
      DEBUGF("write failed\n");
      //TODO: set status
    }

  } else {
    DEBUGF("not in ack state\n");
    memset(data, 0, len);
  }

#ifndef USBTINY
  return len;
#endif
}


/* ------------------------------------------------------------------------- */

int	main(void) {
  wdt_enable(WDTO_1S);

#if DEBUG_LEVEL > 0
  /* let debug routines init the uart if they want to */
  odDebugInit();
#else
#ifdef DEBUG
  /* quick'n dirty uart init */
  UCSRB |= _BV(TXEN);
  UBRRL = F_CPU / (19200 * 16L) - 1;
#endif
#endif

#ifdef DEBUG
  stdout = &mystdout;
#endif

  DEBUGF("i2c-tiny-usb - (c) 2006 by Till Harbaum\n");

  i2c_init();

#ifdef DEBUG
  i2c_scan();
#endif

  /* clear usb ports */
  USB_CFG_IOPORT   &= (uchar)~((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));

  /* make usb data lines outputs */
  USBDDR    |= ((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));

  /* USB Reset by device only required on Watchdog Reset */
  _delay_loop_2(40000);   // 10ms

  /* make usb data lines inputs */
  USBDDR &= ~((1<<USB_CFG_DMINUS_BIT)|(1<<USB_CFG_DPLUS_BIT));

  usbInit();

  sei();
  for(;;) {	/* main event loop */
    wdt_reset();
    usbPoll();
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
