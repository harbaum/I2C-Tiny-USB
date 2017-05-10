/*
* ----------------------------------------------------------------------------
* “THE COFFEEWARE LICENSE” (Revision 1):
* <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
* can do whatever you want with this stuff. If we meet some day, and you think
* this stuff is worth it, you can buy me a coffee in return.
* -----------------------------------------------------------------------------
*/

#ifndef DIGITAL
#define DIGITAL

#include <avr/io.h>

/*-----------------------------------------------------------------------------
/ Useful symbolic definitions.
/----------------------------------------------------------------------------*/
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define ENABLE 1
#define DISABLE 0

/*-----------------------------------------------------------------------------
/ Definition: Writes high/low to a digital output pin.
/ Example: 
/
/	digitalWrite(A,6,LOW);
/	digitalWrite(A,7,HIGH);
/----------------------------------------------------------------------------*/
#define digitalWrite(port,pin,state) state ? (PORT ## port |= (1<<pin)) : (PORT ## port &= ~(1<<pin))  

/*-----------------------------------------------------------------------------
/ Definition: Sets the mode of a digital pin as input/output.
/ Example:
/
/ 	pinMode(A,6,OUTPUT);
/	pinMode(A,6,INPUT);
/----------------------------------------------------------------------------*/
#define pinMode(port,pin,state) state ? (DDR ## port |= (1<<pin)) : (DDR ## port &= ~(1<<pin))

/*-----------------------------------------------------------------------------
/ Definition: Reads the value of a digital input pin.
/ Returns: Zero for low, nonzero for high.
/ Example: If PORTA6 high, set pin PORTA7 high; else low.
/
/	if(digitalRead(A,6))
/ 		digitalWrite(A,7,HIGH);
/	else
/		digitalWrite(A,7,LOW);
/----------------------------------------------------------------------------*/
#define digitalRead(port,pin) (PIN ## port & (1<<pin))

/*-----------------------------------------------------------------------------
/ Definition: Toggles the state of a digital output pin.
/ Example:
/
/	togglePin(A,6);
/----------------------------------------------------------------------------*/
#define togglePin(port,pin) (PIN ## port |= (1<<pin))

/*-----------------------------------------------------------------------------
/ Definition: Enables/disables the internal pullup resistor for a digital
/	input pin.
/ Example:
/
/	internalPullup(A,6,ENABLE);
/	internalPullup(A,6,DISABLE);
/----------------------------------------------------------------------------*/
#define internalPullup(port,pin,state) state ? (PORT ## port |= (1<<pin)) : (PORT ## port &= ~(1<<pin))

#endif
