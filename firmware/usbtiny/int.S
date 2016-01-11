; ======================================================================
; USB interrupt handler
;
; This is the handler for the interrupt caused by the initial rising edge
; on the D+ USB signal. The NRZI encoding and bit stuffing are removed,
; and the packet is saved in one of the two input buffers. In some cases,
; a reply packet is sent right away.
;
; When a DATA0/DATA1 packet directly follows a SETUP or OUT packet, while
; this interrupt handler is not yet finished, there would be no time to
; return and take another interrupt. In that case, the second packet is
; decoded directly in the same invocation.
;
; This code is *extremely* time critical. For instance, there is not a
; single spare cycle in the receiver loop, and only two in the transmitter
; loop. In addition, the various code paths are laid out in such a way that
; the various USB timeouts are not violated, in particular the maximum time
; between the reception of a packet and the reply, which is 6.5 bit times
; for a detachable cable (TRSPIPD1), and 7.5 bit times for a captive cable
; (TRSPIPD2). The worst-case delay here is 51 cycles, which is just below
; the 52 cycles for a detachable cable.
;
; The interrupt handler must be reached within 34 cycles after D+ goes high
; for the first time, so the interrupts should not be disabled for longer
; than 34-4-2=28 cycles.
;
; The end-of-packet (EOP) is sampled in the second bit, because the USB
; standard allows the EOP to be delayed by up to one bit. As the EOP
; duration is two bits, this is not a problem.
;
; Stack usage including the return address: 11 bytes.
;
; Copyright (C) 2006 Dick Streefland
;
; This is free software, licensed under the terms of the GNU General
; Public License as published by the Free Software Foundation.
; ======================================================================

#include "def.h"

; ----------------------------------------------------------------------
; local data
; ----------------------------------------------------------------------
	.data
tx_ack:	.byte	USB_PID_ACK		; ACK packet
tx_nak:	.byte	USB_PID_NAK		; NAK packet
	.lcomm	token_pid, 1		; PID of most recent token packet

; ----------------------------------------------------------------------
; register definitions
; ----------------------------------------------------------------------
// receiver:
#define	count		r16
#define	usbmask		r17
#define	odd		r18
#define	byte		r19
#define	fixup		r20
#define	even		r22

// transmitter:
#define	output		odd
#define	done		fixup
#define	next		even

// control:
#define	pid		odd
#define	addr		usbmask
#define	tmp		fixup

#define	nop2		rjmp	.+0	// not .+2 for some strange reason

; ----------------------------------------------------------------------
; interrupt handler
; ----------------------------------------------------------------------
	.text
	.global	USB_INT_VECTOR
	.type	USB_INT_VECTOR, @function
; ----------------------------------------------------------------------
; This handler must be reached no later than 34 cycles after D+ goes high
; for the first time.
; ----------------------------------------------------------------------
USB_INT_VECTOR:
	; save registers
	push	count
	push	usbmask
	push	odd
	push	YH
	push	YL
	in	count, SREG
	push	count

; ----------------------------------------------------------------------
; Synchronize to the pattern 10101011 on D+. This code must be reached
; no later than 47 cycles after D+ goes high for the first time.
; ----------------------------------------------------------------------
sync:
	; wait until D+ == 0
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync			; jump if D+ == 1
resync:
	; sync on 0-->1 transition on D+ with a 2 cycle resolution
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync6			; jump if D+ == 1
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync6			; jump if D+ == 1
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync6			; jump if D+ == 1
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync6			; jump if D+ == 1
	sbic	USB_IN, USBTINY_DPLUS
	rjmp	sync6			; jump if D+ == 1
	ldi	count, 1<<USB_INT_PENDING_BIT
	out	USB_INT_PENDING, count
	rjmp	return			; ==> false start, bail out

sync6:
	; we are now between -1 and +1 cycle from the center of the bit
	; following the 0-->1 transition
	lds	YL, usb_rx_off
	clr	YH
	subi	YL, lo8(-(usb_rx_buf))	; Y = & usb_rx_buf[usb_rx_off]
	sbci	YH, hi8(-(usb_rx_buf))
	ldi	count, USB_BUFSIZE	; limit on number of bytes to receive
	ldi	usbmask, USB_MASK	; why is there no eori instruction?
	ldi	odd, USB_MASK_DPLUS

sync7:
	; the last sync bit should also be 1
	sbis	USB_IN, USBTINY_DPLUS	; bit 7 of sync byte?
	rjmp	resync			; no, wait for next transition
	push	byte
	push	fixup
	push	even

; ----------------------------------------------------------------------
; receiver loop
; ----------------------------------------------------------------------
	in	even, USB_IN		; sample bit 0
	ldi	byte, 0x80		; load sync byte for correct unstuffing
	rjmp	rxentry			; 2 cycles

rxloop:
	in	even, USB_IN		; sample bit 0
	or	fixup, byte
	st	Y+, fixup		; 2 cycles
rxentry:
	clr	fixup
	andi	even, USB_MASK
	eor	odd, even
	subi	odd, 1
	in	odd, USB_IN		; sample bit 1
	andi	odd, USB_MASK
	breq	eop			; ==> EOP detected
	ror	byte
	cpi	byte, 0xfc
	brcc	skip0
skipped0:
	eor	even, odd
	subi	even, 1
	in	even, USB_IN		; sample bit 2
	andi	even, USB_MASK
	ror	byte
	cpi	byte, 0xfc
	brcc	skip1
skipped1:
	eor	odd, even
	subi	odd, 1
	ror	byte
	in	odd, USB_IN		; sample bit 3
	andi	odd, USB_MASK
	cpi	byte, 0xfc
	brcc	skip2
	eor	even, odd
	subi	even, 1
	ror	byte
skipped2:
	cpi	byte, 0xfc
	in	even, USB_IN		; sample bit 4
	andi	even, USB_MASK
	brcc	skip3
	eor	odd, even
	subi	odd, 1
	ror	byte
skipped4:
	cpi	byte, 0xfc
skipped3:
	brcc	skip4
	in	odd, USB_IN		; sample bit 5
	andi	odd, USB_MASK
	eor	even, odd
	subi	even, 1
	ror	byte
skipped5:
	cpi	byte, 0xfc
	brcc	skip5
	dec	count
	in	even, USB_IN		; sample bit 6
	brmi	overflow		; ==> overflow
	andi	even, USB_MASK
	eor	odd, even
	subi	odd, 1
	ror	byte
skipped6:
	cpi	byte, 0xfc
	brcc	skip6
	in	odd, USB_IN		; sample bit 7
	andi	odd, USB_MASK
	eor	even, odd
	subi	even, 1
	ror	byte
	cpi	byte, 0xfc
	brcs	rxloop			; 2 cycles
	rjmp	skip7

eop:
	rjmp	eop2
overflow:
	rjmp	ignore

; ----------------------------------------------------------------------
; out-of-line code to skip stuffing bits
; ----------------------------------------------------------------------
skip0:					; 1+6 cycles
	eor	even, usbmask
	in	odd, USB_IN		; resample bit 1
	andi	odd, USB_MASK
	cbr	byte, (1<<7)
	sbr	fixup, (1<<0)
	rjmp	skipped0

skip1:					; 2+5 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<1)
	in	even, USB_IN		; resample bit 2
	andi	even, USB_MASK
	eor	odd, usbmask
	rjmp	skipped1

skip2:					; 3+7 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<2)
	eor	even, usbmask
	in	odd, USB_IN		; resample bit 3
	andi	odd, USB_MASK
	eor	even, odd
	subi	even, 1
	ror	byte
	rjmp	skipped2

skip3:					; 4+7 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<3)
	eor	odd, usbmask
	ori	byte, 1
	in	even, USB_IN		; resample bit 4
	andi	even, USB_MASK
	eor	odd, even
	subi	odd, 1
	ror	byte
	rjmp	skipped3

skip4:					; 5 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<4)
	eor	even, usbmask
	rjmp	skipped4

skip5:					; 5 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<5)
	eor	odd, usbmask
	rjmp	skipped5

skip6:					; 5 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<6)
	eor	even, usbmask
	rjmp	skipped6

skip7:					; 7 cycles
	cbr	byte, (1<<7)
	sbr	fixup, (1<<7)
	eor	odd, usbmask
	nop2
	rjmp	rxloop

; ----------------------------------------------------------------------
; end-of-packet detected (worst-case: 3 cycles after end of SE0)
; ----------------------------------------------------------------------
eop2:
	; clear pending interrupt (SE0+3)
	ldi	byte, 1<<USB_INT_PENDING_BIT
	out	USB_INT_PENDING, byte	; clear pending bit at end of packet
	; ignore packets shorter than 3 bytes
	subi	count, USB_BUFSIZE
	neg	count			; count = packet length
	cpi	count, 3
	brlo	ignore
	; get PID
	sub	YL, count
	ld	pid, Y
	; check for DATA0/DATA1 first, as this is the critical path (SE0+12)
	cpi	pid, USB_PID_DATA0
	breq	is_data			; handle DATA0 packet
	cpi	pid, USB_PID_DATA1
	breq	is_data			; handle DATA1 packet
	; check ADDR (SE0+16)
	ldd	addr, Y+1
	andi	addr, 0x7f
	lds	tmp, usb_address
	cp	addr, tmp		; is this packet for me?
	brne	ignore			; no, ignore
	; check for other PIDs (SE0+23)
	cpi	pid, USB_PID_IN
	breq	is_in			; handle IN packet
	cpi	pid, USB_PID_SETUP
	breq	is_setup_out		; handle SETUP packet
	cpi	pid, USB_PID_OUT
	breq	is_setup_out		; handle OUT packet

; ----------------------------------------------------------------------
; exit point for ignored packets
; ----------------------------------------------------------------------
ignore:
	clr	tmp
	sts	token_pid, tmp
	pop	even
	pop	fixup
	pop	byte
	rjmp	return

; ----------------------------------------------------------------------
; Handle SETUP/OUT (SE0+30)
; ----------------------------------------------------------------------
is_setup_out:
	sts	token_pid, pid		; save PID of token packet
	pop	even
	pop	fixup
	pop	byte
	in	count, USB_INT_PENDING	; next packet already started?
	sbrc	count, USB_INT_PENDING_BIT
	rjmp	sync			; yes, get it right away (SE0+42)

; ----------------------------------------------------------------------
; restore registers and return from interrupt
; ----------------------------------------------------------------------
return:
	pop	count
	out	SREG, count
	pop	YL
	pop	YH
	pop	odd
	pop	usbmask
	pop	count
	reti

; ----------------------------------------------------------------------
; Handle IN (SE0+26)
; ----------------------------------------------------------------------
is_in:
	lds	count, usb_tx_len
	tst	count			; data ready?
	breq	nak			; no, reply with NAK
	lds	tmp, usb_rx_len
	tst	tmp			; unprocessed input packet?
	brne	nak			; yes, don't send old data for new packet
	sts	usb_tx_len, tmp		; buffer is available again (after reti)
	ldi	YL, lo8(usb_tx_buf)
	ldi	YH, hi8(usb_tx_buf)
	rjmp	send_packet		; SE0+40, SE0 --> SOP <= 51

; ----------------------------------------------------------------------
; Handle DATA0/DATA1 (SE0+17)
; ----------------------------------------------------------------------
is_data:
	lds	pid, token_pid
	tst	pid			; data following our SETUP/OUT
	breq	ignore			; no, ignore
	lds	tmp, usb_rx_len
	tst	tmp			; buffer free?
	brne	nak			; no, reply with NAK
	sts	usb_rx_len, count	; pass buffer length
	sts	usb_rx_token, pid	; pass PID of token (SETUP or OUT)
	lds	count, usb_rx_off	; switch to other input buffer
	ldi	tmp, USB_BUFSIZE
	sub	tmp, count
	sts	usb_rx_off, tmp

; ----------------------------------------------------------------------
; send ACK packet (SE0+35)
; ----------------------------------------------------------------------
ack:
	ldi	YL, lo8(tx_ack)
	ldi	YH, hi8(tx_ack)
	rjmp	send_token

; ----------------------------------------------------------------------
; send NAK packet (SE0+36)
; ----------------------------------------------------------------------
nak:
	ldi	YL, lo8(tx_nak)
	ldi	YH, hi8(tx_nak)
send_token:
	ldi	count, 1		; SE0+40, SE0 --> SOP <= 51

; ----------------------------------------------------------------------
; acquire the bus and send a packet (11 cycles to SOP)
; ----------------------------------------------------------------------
send_packet:
	in	output, USB_OUT
	cbr	output, USB_MASK
	ori	output, USB_MASK_DMINUS
	in	usbmask, USB_DDR
	ori	usbmask, USB_MASK
	out	USB_OUT, output		; idle state
	out	USB_DDR, usbmask	; acquire bus
	ldi	usbmask, USB_MASK
	ldi	byte, 0x80		; start with sync byte

; ----------------------------------------------------------------------
; transmitter loop
; ----------------------------------------------------------------------
txloop:
	sbrs	byte, 0
	eor	output, usbmask
	out	USB_OUT, output		; output bit 0
	ror	byte
	ror	done
stuffed0:
	cpi	done, 0xfc
	brcc	stuff0
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
stuffed1:
	out	USB_OUT, output		; output bit 1
	ror	done
	cpi	done, 0xfc
	brcc	stuff1
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
	nop
stuffed2:
	out	USB_OUT, output		; output bit 2
	ror	done
	cpi	done, 0xfc
	brcc	stuff2
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
	nop
stuffed3:
	out	USB_OUT, output		; output bit 3
	ror	done
	cpi	done, 0xfc
	brcc	stuff3
	sbrs	byte, 0
	eor	output, usbmask
	ld	next, Y+		; 2 cycles
	out	USB_OUT, output		; output bit 4
	ror	byte
	ror	done
stuffed4:
	cpi	done, 0xfc
	brcc	stuff4
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
stuffed5:
	out	USB_OUT, output		; output bit 5
	ror	done
	cpi	done, 0xfc
	brcc	stuff5
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
stuffed6:
	ror	done
	out	USB_OUT, output		; output bit 6
	cpi	done, 0xfc
	brcc	stuff6
	sbrs	byte, 0
	eor	output, usbmask
	ror	byte
	mov	byte, next
stuffed7:
	ror	done
	out	USB_OUT, output		; output bit 7
	cpi	done, 0xfc
	brcc	stuff7
	dec	count
	brpl	txloop			; 2 cycles

	rjmp	gen_eop

; ----------------------------------------------------------------------
; out-of-line code to insert stuffing bits
; ----------------------------------------------------------------------
stuff0:					; 2+3
	eor	output, usbmask
	clr	done
	out	USB_OUT, output
	rjmp	stuffed0

stuff1:					; 3
	eor	output, usbmask
	rjmp	stuffed1

stuff2:					; 3
	eor	output, usbmask
	rjmp	stuffed2

stuff3:					; 3
	eor	output, usbmask
	rjmp	stuffed3

stuff4:					; 2+3
	eor	output, usbmask
	clr	done
	out	USB_OUT, output
	rjmp	stuffed4

stuff5:					; 3
	eor	output, usbmask
	rjmp	stuffed5

stuff6:					; 3
	eor	output, usbmask
	rjmp	stuffed6

stuff7:					; 3
	eor	output, usbmask
	rjmp	stuffed7

; ----------------------------------------------------------------------
; generate EOP, release the bus, and return from interrupt
; ----------------------------------------------------------------------
gen_eop:
	cbr	output, USB_MASK
	out	USB_OUT, output		; output SE0 for 2 bit times
	pop	even
	pop	fixup
	pop	byte
	ldi	count, 1<<USB_INT_PENDING_BIT
	out	USB_INT_PENDING, count	; interrupt was triggered by transmit
	pop	YH			; this is the saved SREG
	pop	YL
	in	usbmask, USB_DDR
	mov	count, output
	ori	output, USB_MASK_DMINUS
	out	USB_OUT, output		; output J state for 1 bit time
	cbr	usbmask, USB_MASK
	out	SREG, YH
	pop	YH
	pop	odd			; is the same register as output!
	nop
	out	USB_DDR, usbmask	; release bus
	out	USB_OUT, count		; disable D- pullup
	pop	usbmask
	pop	count
	reti
