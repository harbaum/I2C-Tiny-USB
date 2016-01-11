/*
 * i2c_usb.c - test application for the i2c-tiby-usb interface
 *             http://www.harbaum.org/till/i2c_tiny_usb
 *
 * $Id: i2c_usb.c,v 1.5 2007/01/05 19:30:43 harbaum Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <usb.h>

/* ds1621 chip address (A0-A2 tied low) */
#define DS1621_ADDR  0x48

/* pcf8574 chip address (A0-A2 tied low) */
#define PCF8574_ADDR  0x20

#define LOOPS 100

#define USB_CTRL_IN    (USB_TYPE_CLASS | USB_ENDPOINT_IN)
#define USB_CTRL_OUT   (USB_TYPE_CLASS)

/* the vendor and product id was donated by ftdi ... many thanks!*/
#define I2C_TINY_USB_VID  0x0403
#define I2C_TINY_USB_PID  0xc631

#ifdef WIN
#include <windows.h>
#include <winbase.h>
#define usleep(t) Sleep((t) / 1000)
#endif

#define I2C_M_RD		0x01

/* commands via USB, must e.g. match command ids firmware */
#define CMD_ECHO       0
#define CMD_GET_FUNC   1
#define CMD_SET_DELAY  2
#define CMD_GET_STATUS 3
#define CMD_I2C_IO     4
#define CMD_I2C_BEGIN  1  // flag to I2C_IO
#define CMD_I2C_END    2  // flag to I2C_IO

#define STATUS_IDLE          0
#define STATUS_ADDRESS_ACK   1
#define STATUS_ADDRESS_NAK   2

usb_dev_handle      *handle = NULL;

/* write a set of bytes to the i2c_tiny_usb device */
int i2c_tiny_usb_write(int request, int value, int index) {
  if(usb_control_msg(handle, USB_CTRL_OUT, request, 
		      value, index, NULL, 0, 1000) < 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  }
  return 1;
}

/* read a set of bytes from the i2c_tiny_usb device */
int i2c_tiny_usb_read(unsigned char cmd, void *data, int len) {
  int                 nBytes;

  /* send control request and accept return value */
  nBytes = usb_control_msg(handle, 
	   USB_CTRL_IN, 
	   cmd, 0, 0, data, len, 1000);

  if(nBytes < 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return nBytes;
  }

  return 0;
}

/* get i2c usb interface firmware version */
void i2c_tiny_usb_get_func(void) {
  unsigned long func;
  
  if(i2c_tiny_usb_read(CMD_GET_FUNC, &func, sizeof(func)) == 0)
    printf("Functionality = %lx\n", func);
}

/* set a value in the I2C_USB interface */
void i2c_tiny_usb_set(unsigned char cmd, int value) {
  if(usb_control_msg(handle, 
	     USB_TYPE_VENDOR, cmd, value, 0, 
	     NULL, 0, 1000) < 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
  }
}

/* get the current transaction status from the i2c_tiny_usb interface */
int i2c_tiny_usb_get_status(void) {
  int i;
  unsigned char status;
  
  if((i=i2c_tiny_usb_read(CMD_GET_STATUS, &status, sizeof(status))) < 0) {
    fprintf(stderr, "Error reading status\n");
    return i;
  }

  return status;
}

/* write command and read an 8 or 16 bit value from the given chip */
int i2c_read_with_cmd(unsigned char addr, char cmd, int length) {
  unsigned char result[2];

  if((length < 0) || (length > sizeof(result))) {
    fprintf(stderr, "request exceeds %d bytes\n", sizeof(result));
    return -1;
  } 

  /* write one byte register address to chip */
  if(usb_control_msg(handle, USB_CTRL_OUT, 
		     CMD_I2C_IO + CMD_I2C_BEGIN
		     + ((!length)?CMD_I2C_END:0),
		     0, addr, &cmd, 1, 
		     1000) < 1) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  } 

  if(i2c_tiny_usb_get_status() != STATUS_ADDRESS_ACK) {
    fprintf(stderr, "write command status failed\n");
    return -1;
  }

  // just a test? return ok
  if(!length) return 0;

  if(usb_control_msg(handle, 
		     USB_CTRL_IN, 
		     CMD_I2C_IO + CMD_I2C_END,
		     I2C_M_RD, addr, (char*)result, length, 
		     1000) < 1) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  } 

  if(i2c_tiny_usb_get_status() != STATUS_ADDRESS_ACK) {
    fprintf(stderr, "read data status failed\n");
    return -1;
  }

  // return 16 bit result
  if(length == 2)
    return 256*result[0] + result[1];

  // return 8 bit result
  return result[0];  
}

/* write a single byte to the i2c client */
int i2c_write_byte(unsigned char addr, char data) {

  /* write one byte register address to chip */
  if(usb_control_msg(handle, USB_CTRL_OUT, 
		     CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END,
		     0, addr, &data, 1, 
		     1000) < 1) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  } 

  if(i2c_tiny_usb_get_status() != STATUS_ADDRESS_ACK) {
    fprintf(stderr, "write command status failed\n");
    return -1;
  }

  return 0;  
}

/* write a command byte and a single byte to the i2c client */
int i2c_write_cmd_and_byte(unsigned char addr, char cmd, char data) {
  char msg[2];

  msg[0] = cmd;
  msg[1] = data;

  /* write one byte register address to chip */
  if(usb_control_msg(handle, USB_CTRL_OUT, 
		     CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END,
		     0, addr, msg, 2, 
		     1000) < 1) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  } 

  if(i2c_tiny_usb_get_status() != STATUS_ADDRESS_ACK) {
    fprintf(stderr, "write command status failed\n");
    return -1;
  }

  return 0;  
}

/* write a command byte and a 16 bit value to the i2c client */
int i2c_write_cmd_and_word(unsigned char addr, char cmd, int data) {
  char msg[3];

  msg[0] = cmd;
  msg[1] = data >> 8;
  msg[2] = data & 0xff;

  /* write one byte register address to chip */
  if(usb_control_msg(handle, USB_CTRL_OUT, 
		     CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END,
		     0, addr, msg, 3, 
		     1000) < 1) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    return -1;
  } 

  if(i2c_tiny_usb_get_status() != STATUS_ADDRESS_ACK) {
    fprintf(stderr, "write command status failed\n");
    return -1;
  }

  return 0;  
}

/* read ds1621 control register */
void ds1621_read_control(void) {
  int result;

  do {
    result = i2c_read_with_cmd(DS1621_ADDR, 0xac, 1);
  } while(!(result & 0x80));
}

int main(int argc, char *argv[]) {
  struct usb_bus      *bus;
  struct usb_device   *dev;
  int i;
#ifndef WIN
  int ret;
#endif
  
  printf("--      i2c-tiny-usb test application       --\n");
  printf("--         (c) 2006 by Till Harbaum         --\n");
  printf("-- http://www.harbaum.org/till/i2c_tiny_usb --\n");

  usb_init();
  
  usb_find_busses();
  usb_find_devices();
  
  for(bus = usb_get_busses(); bus; bus = bus->next) {
    for(dev = bus->devices; dev; dev = dev->next) {
      if((dev->descriptor.idVendor == I2C_TINY_USB_VID) && 
	 (dev->descriptor.idProduct == I2C_TINY_USB_PID)) {
	
	printf("Found i2c_tiny_usb device on bus %s device %s.\n", 
	       bus->dirname, dev->filename);
	
	/* open device */
	if(!(handle = usb_open(dev))) 
	  fprintf(stderr, "Error: Cannot open the device: %s\n", 
		  usb_strerror());

	break;
      }
    }
  }
  
  if(!handle) {
    fprintf(stderr, "Error: Could not find i2c_tiny_usb device\n");

#ifdef WIN
    printf("Press return to quit\n");
    getchar();
#endif

    exit(-1);
  }

#ifndef WIN
  /* Get exclusive access to interface 0. Does not work under windows. */
  ret = usb_claim_interface(handle, 0);
  if (ret != 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());

    exit(1);
  }
#endif
  
  /* do some testing */
  i2c_tiny_usb_get_func();

  /* try to set i2c clock to 100kHz (10us), will actually result in ~50kHz */
  /* since the software generated i2c clock isn't too exact. in fact setting */
  /* it to 10us doesn't do anything at all since this already is the default */
  i2c_tiny_usb_set(CMD_SET_DELAY, 10);

  /* -------- begin of ds1621 client processing --------- */
  printf("Probing for DS1621 ... ");

  /* try to access ds1621 at address DS1621_ADDR */
  if(usb_control_msg(handle, USB_CTRL_IN, 
		     CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END,
		     0, DS1621_ADDR, NULL, 0, 
		     1000) < 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    goto quit;
  } 
  
  if(i2c_tiny_usb_get_status() == STATUS_ADDRESS_ACK) {
    int temp;

    printf("success at address 0x%02x\n", DS1621_ADDR);

    /* activate one shot mode */
    if(i2c_write_cmd_and_byte(DS1621_ADDR, 0xac, 0x01) < 0)
      goto quit;

    /* wait 10ms */
    usleep(10000);

#if 0
    /* write default limits */
    /* high threshold: +15 deg celsius */
    i2c_write_cmd_and_word(DS1621_ADDR, 0xa1, 0x0f00);  /* 15 deg celsius */
    usleep(10000);
    /* low threshold: +10 deg celsius */
    i2c_write_cmd_and_word(DS1621_ADDR, 0xa2, 0x0a00);  
    usleep(10000);
#endif
    
    /* display limits */
    temp = i2c_read_with_cmd(DS1621_ADDR, 0xa1, 2);
    printf("high temperature threshold = %d.%03d\n", 
	   temp>>8, 1000 * (temp & 0xff) / 256);
    temp = i2c_read_with_cmd(DS1621_ADDR, 0xa2, 2);
    printf("low temperature threshold = %d.%03d\n", 
	   temp>>8, 1000 * (temp & 0xff) / 256);
    
    printf("Getting %d temperature readings:\n", LOOPS);
    for(i=0;i<LOOPS;i++) {
      int temp;
      int counter, slope;
      
      /* just write command 0xee to start conversion */
      if(i2c_read_with_cmd(DS1621_ADDR, 0xee, 0) < 0) 
	goto quit;
      
      ds1621_read_control();
      
      temp = i2c_read_with_cmd(DS1621_ADDR, 0xaa, 2);
      if(temp < 0) 
	goto quit;
 
      /* read counter and slope values */
      counter = i2c_read_with_cmd(DS1621_ADDR, 0xa8, 1);
      slope = i2c_read_with_cmd(DS1621_ADDR, 0xa9, 1);
    
      /* use counter and slope to adjust temperature (see ds1621 datasheet) */
      temp = (temp & 0xff00) - 256/4;
      temp += 256 * (slope - counter) / slope;
      
      printf("temp = %d.%03d\n", temp>>8, 1000 * (temp & 0xff) / 256);
    }
  } else
    printf("failed\n");
  /* -------- end of ds1621 client processing --------- */

  /* -------- begin of pcf8574 client processing --------- */
  printf("Probing for PCF8574 ... ");

  /* try to access pcf8574 at address PCF8574_ADDR */
  if(usb_control_msg(handle, USB_CTRL_IN, 
		     CMD_I2C_IO + CMD_I2C_BEGIN + CMD_I2C_END,
		     0, PCF8574_ADDR, NULL, 0, 
		     1000) < 0) {
    fprintf(stderr, "USB error: %s\n", usb_strerror());
    goto quit;
  } 
  
  if(i2c_tiny_usb_get_status() == STATUS_ADDRESS_ACK) {
    unsigned char bit_mask = 0xfe;

    printf("success at address 0x%02x\n", PCF8574_ADDR);

    printf("Cycling 0 bit %d times.\n", LOOPS);
    /* just rotate a single 0 bit through the outputs */

    for(i=0;i<LOOPS;i++) {
      if(i2c_write_byte(PCF8574_ADDR, bit_mask) < 0)
	goto quit;
      
      /* rotate the byte */
      bit_mask = (bit_mask << 1) | 1;
      if(bit_mask == 0xff) 
	bit_mask = 0xfe;

      usleep(100000);
    }
  } else
    printf("failed\n");
  /* -------- end of pcf8574 client processing --------- */

 quit:
#ifndef WIN
  ret = usb_release_interface(handle, 0);
  if (ret)
    fprintf(stderr, "USB error: %s\n", usb_strerror());
#endif

  usb_close(handle);

#ifdef WIN
  printf("Press return to quit\n");
  getchar();
#endif

  return 0;
}
