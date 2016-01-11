/* Name: usbdrvasm.S
 * Project: AVR USB driver
 * Author: Christian Starkjohann
 * Creation Date: 2004-12-29
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt) or proprietary (CommercialLicense.txt)
 * This Revision: $Id: usbdrvasm.S,v 1.2 2007/05/19 12:30:11 harbaum Exp $
 */

/*
General Description:
This module implements the assembler part of the USB driver. See usbdrv.h
for a description of the entire driver.
Since almost all of this code is timing critical, don't change unless you
really know what you are doing! Many parts require not only a maximum number
of CPU cycles, but even an exact number of cycles!


Timing constraints according to spec (in bit times):
timing subject                                      min max    CPUcycles
---------------------------------------------------------------------------
EOP of OUT/SETUP to sync pattern of DATA0 (both rx) 2   16     16-128
EOP of IN to sync pattern of DATA0 (rx, then tx)    2   7.5    16-60
DATAx (rx) to ACK/NAK/STALL (tx)                    2   7.5    16-60
*/

#include "iarcompat.h"
#ifndef __IAR_SYSTEMS_ASM__
    /* configs for io.h */
#   define __SFR_OFFSET 0
#   define _VECTOR(N)   __vector_ ## N   /* io.h does not define this for asm */
#   include <avr/io.h> /* for CPU I/O register definitions and vectors */
#endif  /* __IAR_SYSTEMS_ASM__ */
#include "usbdrv.h" /* for common defs */


/* register names */
#define x1      r16
#define x2      r17
#define shift   r18
#define cnt     r19
#define x3      r20
#define x4      r21

/* Some assembler dependent definitions and declarations: */

#ifdef __IAR_SYSTEMS_ASM__

#   define nop2     rjmp    $+2 /* jump to next instruction */
#   define XL       r26
#   define XH       r27
#   define YL       r28
#   define YH       r29
#   define ZL       r30
#   define ZH       r31
#   define lo8(x)   LOW(x)
#   define hi8(x)   ((x)>>8)    /* not HIGH to allow XLINK to make a proper range check */

    extern  usbRxBuf, usbDeviceAddr, usbNewDeviceAddr, usbInputBufOffset
    extern  usbCurrentTok, usbRxLen, usbRxToken, usbTxLen
    extern  usbTxBuf, usbMsgLen, usbTxLen1, usbTxBuf1, usbTxLen3, usbTxBuf3
    public  usbCrc16
    public  usbCrc16Append

    COMMON  INTVEC
    ORG     INT0_vect
    rjmp    SIG_INTERRUPT0
    RSEG    CODE

#else /* __IAR_SYSTEMS_ASM__ */

#   define nop2     rjmp    .+0 /* jump to next instruction */

    .text
    .global SIG_INTERRUPT0
    .type   SIG_INTERRUPT0, @function
    .global usbCrc16
    .global usbCrc16Append

#endif /* __IAR_SYSTEMS_ASM__ */


;Software-receiver engine. Strict timing! Don't change unless you can preserve timing!
;interrupt response time: 4 cycles + insn running = 7 max if interrupts always enabled
;max allowable interrupt latency: 34 cycles -> max 25 cycles interrupt disable
;max stack usage: [ret(2), YL, SREG, YH, shift, x1, x2, x3, cnt, x4] = 11 bytes
;Numbers in brackets are maximum cycles since SOF.
SIG_INTERRUPT0:
;order of registers pushed: YL, SREG [sofError], YH, shift, x1, x2, x3, cnt
    push    YL              ;2 [35] push only what is necessary to sync with edge ASAP
    in      YL, SREG        ;1 [37]
    push    YL              ;2 [39]
;----------------------------------------------------------------------------
; Synchronize with sync pattern:
;----------------------------------------------------------------------------
;sync byte (D-) pattern LSb to MSb: 01010100 [1 = idle = J, 0 = K]
;sync up with J to K edge during sync pattern -- use fastest possible loops
;first part has no timeout because it waits for IDLE or SE1 (== disconnected)
waitForJ:
    sbis    USBIN, USBMINUS ;1 [40] wait for D- == 1
    rjmp    waitForJ        ;2
waitForK:
;The following code results in a sampling window of 1/4 bit which meets the spec.
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    rjmp    sofError
foundK:
;{3, 5} after falling D- edge, average delay: 4 cycles [we want 4 for center sampling]
;we have 1 bit time for setup purposes, then sample again. Numbers in brackets
;are cycles from center of first sync (double K) bit after the instruction
    push    YH                  ;2 [2]
    lds     YL, usbInputBufOffset;2 [4]
    clr     YH                  ;1 [5]
    subi    YL, lo8(-(usbRxBuf));1 [6]
    sbci    YH, hi8(-(usbRxBuf));1 [7]

    sbis    USBIN, USBMINUS ;1 [8] we want two bits K [sample 1 cycle too early]
    rjmp    haveTwoBitsK    ;2 [10]
    pop     YH              ; undo the push from before
    rjmp    waitForK        ; this was not the end of sync, retry
haveTwoBitsK:
;----------------------------------------------------------------------------
; push more registers and initialize values while we sample the first bits:
;----------------------------------------------------------------------------
    push    shift           ;2 [16]
    push    x1              ;2 [12]
    push    x2              ;2 [14]

    in      x1, USBIN       ;1 [17] <-- sample bit 0
    ldi     shift, 0xff     ;1 [18]
    bst     x1, USBMINUS    ;1 [19]
    bld     shift, 0        ;1 [20]
    push    x3              ;2 [22]
    push    cnt             ;2 [24]
    
    in      x2, USBIN       ;1 [25] <-- sample bit 1
    ser     x3              ;1 [26] [inserted init instruction]
    eor     x1, x2          ;1 [27]
    bst     x1, USBMINUS    ;1 [28]
    bld     shift, 1        ;1 [29]
    ldi     cnt, USB_BUFSIZE;1 [30] [inserted init instruction]
    rjmp    rxbit2          ;2 [32]

;----------------------------------------------------------------------------
; Receiver loop (numbers in brackets are cycles within byte after instr)
;----------------------------------------------------------------------------

unstuff0:               ;1 (branch taken)
    andi    x3, ~0x01   ;1 [15]
    mov     x1, x2      ;1 [16] x2 contains last sampled (stuffed) bit
    in      x2, USBIN   ;1 [17] <-- sample bit 1 again
    ori     shift, 0x01 ;1 [18]
    rjmp    didUnstuff0 ;2 [20]

unstuff1:               ;1 (branch taken)
    mov     x2, x1      ;1 [21] x1 contains last sampled (stuffed) bit
    andi    x3, ~0x02   ;1 [22]
    ori     shift, 0x02 ;1 [23]
    nop                 ;1 [24]
    in      x1, USBIN   ;1 [25] <-- sample bit 2 again
    rjmp    didUnstuff1 ;2 [27]

unstuff2:               ;1 (branch taken)
    andi    x3, ~0x04   ;1 [29]
    ori     shift, 0x04 ;1 [30]
    mov     x1, x2      ;1 [31] x2 contains last sampled (stuffed) bit
    nop                 ;1 [32]
    in      x2, USBIN   ;1 [33] <-- sample bit 3
    rjmp    didUnstuff2 ;2 [35]

unstuff3:               ;1 (branch taken)
    in      x2, USBIN   ;1 [34] <-- sample stuffed bit 3 [one cycle too late]
    andi    x3, ~0x08   ;1 [35]
    ori     shift, 0x08 ;1 [36]
    rjmp    didUnstuff3 ;2 [38]

unstuff4:               ;1 (branch taken)
    andi    x3, ~0x10   ;1 [40]
    in      x1, USBIN   ;1 [41] <-- sample stuffed bit 4
    ori     shift, 0x10 ;1 [42]
    rjmp    didUnstuff4 ;2 [44]

unstuff5:               ;1 (branch taken)
    andi    x3, ~0x20   ;1 [48]
    in      x2, USBIN   ;1 [49] <-- sample stuffed bit 5
    ori     shift, 0x20 ;1 [50]
    rjmp    didUnstuff5 ;2 [52]

unstuff6:               ;1 (branch taken)
    andi    x3, ~0x40   ;1 [56]
    in      x1, USBIN   ;1 [57] <-- sample stuffed bit 6
    ori     shift, 0x40 ;1 [58]
    rjmp    didUnstuff6 ;2 [60]

; extra jobs done during bit interval:
; bit 0:    store, clear [SE0 is unreliable here due to bit dribbling in hubs]
; bit 1:    se0 check
; bit 2:    overflow check
; bit 3:    recovery from delay [bit 0 tasks took too long]
; bit 4:    none
; bit 5:    none
; bit 6:    none
; bit 7:    jump, eor
rxLoop:
    eor     x3, shift   ;1 [0] reconstruct: x3 is 0 at bit locations we changed, 1 at others
    in      x1, USBIN   ;1 [1] <-- sample bit 0
    st      y+, x3      ;2 [3] store data
    ser     x3          ;1 [4]
    nop                 ;1 [5]
    eor     x2, x1      ;1 [6]
    bst     x2, USBMINUS;1 [7]
    bld     shift, 0    ;1 [8]
    in      x2, USBIN   ;1 [9] <-- sample bit 1 (or possibly bit 0 stuffed)
    andi    x2, USBMASK ;1 [10]
    breq    se0         ;1 [11] SE0 check for bit 1
    andi    shift, 0xf9 ;1 [12]
didUnstuff0:
    breq    unstuff0    ;1 [13]
    eor     x1, x2      ;1 [14]
    bst     x1, USBMINUS;1 [15]
    bld     shift, 1    ;1 [16]
rxbit2:
    in      x1, USBIN   ;1 [17] <-- sample bit 2 (or possibly bit 1 stuffed)
    andi    shift, 0xf3 ;1 [18]
    breq    unstuff1    ;1 [19] do remaining work for bit 1
didUnstuff1:
    subi    cnt, 1      ;1 [20]
    brcs    overflow    ;1 [21] loop control
    eor     x2, x1      ;1 [22]
    bst     x2, USBMINUS;1 [23]
    bld     shift, 2    ;1 [24]
    in      x2, USBIN   ;1 [25] <-- sample bit 3 (or possibly bit 2 stuffed)
    andi    shift, 0xe7 ;1 [26]
    breq    unstuff2    ;1 [27]
didUnstuff2:
    eor     x1, x2      ;1 [28]
    bst     x1, USBMINUS;1 [29]
    bld     shift, 3    ;1 [30]
didUnstuff3:
    andi    shift, 0xcf ;1 [31]
    breq    unstuff3    ;1 [32]
    in      x1, USBIN   ;1 [33] <-- sample bit 4
    eor     x2, x1      ;1 [34]
    bst     x2, USBMINUS;1 [35]
    bld     shift, 4    ;1 [36]
didUnstuff4:
    andi    shift, 0x9f ;1 [37]
    breq    unstuff4    ;1 [38]
    nop2                ;2 [40]
    in      x2, USBIN   ;1 [41] <-- sample bit 5
    eor     x1, x2      ;1 [42]
    bst     x1, USBMINUS;1 [43]
    bld     shift, 5    ;1 [44]
didUnstuff5:
    andi    shift, 0x3f ;1 [45]
    breq    unstuff5    ;1 [46]
    nop2                ;2 [48]
    in      x1, USBIN   ;1 [49] <-- sample bit 6
    eor     x2, x1      ;1 [50]
    bst     x2, USBMINUS;1 [51]
    bld     shift, 6    ;1 [52]
didUnstuff6:
    cpi     shift, 0x02 ;1 [53]
    brlo    unstuff6    ;1 [54]
    nop2                ;2 [56]
    in      x2, USBIN   ;1 [57] <-- sample bit 7
    eor     x1, x2      ;1 [58]
    bst     x1, USBMINUS;1 [59]
    bld     shift, 7    ;1 [60]
didUnstuff7:
    cpi     shift, 0x04 ;1 [61]
    brsh    rxLoop      ;2 [63] loop control
unstuff7:
    andi    x3, ~0x80   ;1 [63]
    ori     shift, 0x80 ;1 [64]
    in      x2, USBIN   ;1 [65] <-- sample stuffed bit 7
    nop                 ;1 [66]
    rjmp    didUnstuff7 ;2 [68]


;----------------------------------------------------------------------------
; Processing of received packet (numbers in brackets are cycles after end of SE0)
;----------------------------------------------------------------------------
;This is the only non-error exit point for the software receiver loop
;we don't check any CRCs here because there is no time left.
#define token   x1
se0:                            ;  [0]
    subi    cnt, USB_BUFSIZE    ;1 [1]
    neg     cnt                 ;1 [2]
    cpi     cnt, 3              ;1 [3]
    ldi     x2, 1<<USB_INTR_PENDING_BIT ;1 [4]
    out     USB_INTR_PENDING, x2;1 [5] clear pending intr and check flag later. SE0 should be over.
    brlo    doReturn            ;1 [6] this is probably an ACK, NAK or similar packet
    sub     YL, cnt             ;1 [7]
    sbci    YH, 0               ;1 [8]
    ld      token, y            ;2 [10]
    cpi     token, USBPID_DATA0 ;1 [11]
    breq    handleData          ;1 [12]
    cpi     token, USBPID_DATA1 ;1 [13]
    breq    handleData          ;1 [14]
    ldd     x2, y+1             ;2 [16] ADDR and 1 bit endpoint number
    mov     x3, x2              ;1 [17] store for endpoint number
    andi    x2, 0x7f            ;1 [18] x2 is now ADDR
    lds     shift, usbDeviceAddr;2 [20]
    cp      x2, shift           ;1 [21]
overflow:                       ; This is a hack: brcs overflow will never have Z flag set
    brne    ignorePacket        ;1 [22] packet for different address
    cpi     token, USBPID_IN    ;1 [23]
    breq    handleIn            ;1 [24]
    cpi     token, USBPID_SETUP ;1 [25]
    breq    handleSetupOrOut    ;1 [26]
    cpi     token, USBPID_OUT   ;1 [27]
    breq    handleSetupOrOut    ;1 [28]
;   rjmp    ignorePacket        ;fallthrough, should not happen anyway.

ignorePacket:
    clr     shift
    sts     usbCurrentTok, shift
doReturn:
    pop     cnt
    pop     x3
    pop     x2
    pop     x1
    pop     shift
    pop     YH
sofError:
    pop     YL
    out     SREG, YL
    pop     YL
    reti

#if USB_CFG_HAVE_INTRIN_ENDPOINT && USB_CFG_HAVE_INTRIN_ENDPOINT3
handleIn3:                      ;1 [38] (branch taken)
    lds     cnt, usbTxLen3      ;2 [40]
    sbrc    cnt, 4              ;2 [42]
    rjmp    sendCntAndReti      ;0 43 + 17 = 60 until SOP
    sts     usbTxLen3, x1       ;2 [44] x1 == USBPID_NAK from above
    ldi     YL, lo8(usbTxBuf3)  ;1 [45]
    ldi     YH, hi8(usbTxBuf3)  ;1 [46]
    rjmp    usbSendAndReti      ;2 [48] + 13 = 61 until SOP (violates the spec by 1 cycle)
#endif

;Setup and Out are followed by a data packet two bit times (16 cycles) after
;the end of SE0. The sync code allows up to 40 cycles delay from the start of
;the sync pattern until the first bit is sampled. That's a total of 56 cycles.
handleSetupOrOut:               ;1 [29] (branch taken)
#if USB_CFG_IMPLEMENT_FN_WRITEOUT   /* if we have data for second OUT endpoint, set usbCurrentTok to -1 */
    sbrc    x3, 7               ;1 [30] skip if endpoint 0
    ldi     token, -1           ;1 [31] indicate that this is endpoint 1 OUT
#endif
    sts     usbCurrentTok, token;2 [33]
    pop     cnt                 ;2 [35]
    pop     x3                  ;2 [37]
    pop     x2                  ;2 [39]
    pop     x1                  ;2 [41]
    pop     shift               ;2 [43]
    pop     YH                  ;2 [45]
    in      YL, USB_INTR_PENDING;1 [46]
    sbrc    YL, USB_INTR_PENDING_BIT;1 [47] check whether data is already arriving
    rjmp    waitForJ            ;2 [49] save the pops and pushes -- a new interrupt is aready pending
    rjmp    sofError            ;2 not an error, but it does the pops and reti we want


handleData:                     ;1 [15] (branch taken)
    lds     token, usbCurrentTok;2 [17]
    tst     token               ;1 [18]
    breq    doReturn            ;1 [19]
    lds     x2, usbRxLen        ;2 [21]
    tst     x2                  ;1 [22]
    brne    sendNakAndReti      ;1 [23]
; 2006-03-11: The following two lines fix a problem where the device was not
; recognized if usbPoll() was called less frequently than once every 4 ms.
    cpi     cnt, 4              ;1 [24] zero sized data packets are status phase only -- ignore and ack
    brmi    sendAckAndReti      ;1 [25] keep rx buffer clean -- we must not NAK next SETUP
    sts     usbRxLen, cnt       ;2 [27] store received data, swap buffers
    sts     usbRxToken, token   ;2 [29]
    lds     x2, usbInputBufOffset;2 [31] swap buffers
    ldi     cnt, USB_BUFSIZE    ;1 [32]
    sub     cnt, x2             ;1 [33]
    sts     usbInputBufOffset, cnt;2 [35] buffers now swapped
    rjmp    sendAckAndReti      ;2 [37] + 19 = 56 until SOP

handleIn:                       ;1 [25] (branch taken)
;We don't send any data as long as the C code has not processed the current
;input data and potentially updated the output data. That's more efficient
;in terms of code size than clearing the tx buffers when a packet is received.
    lds     x1, usbRxLen        ;2 [27]
    cpi     x1, 1               ;1 [28] negative values are flow control, 0 means "buffer free"
    brge    sendNakAndReti      ;1 [29] unprocessed input packet?
    ldi     x1, USBPID_NAK      ;1 [30] prepare value for usbTxLen
#if USB_CFG_HAVE_INTRIN_ENDPOINT
    sbrc    x3, 7               ;2 [33] x3 contains addr + endpoint
    rjmp    handleIn1           ;0
#endif
    lds     cnt, usbTxLen       ;2 [34]
    sbrc    cnt, 4              ;2 [36] all handshake tokens have bit 4 set
    rjmp    sendCntAndReti      ;0 37 + 17 = 54 until SOP
    sts     usbTxLen, x1        ;2 [38] x1 == USBPID_NAK from above
    ldi     YL, lo8(usbTxBuf)   ;1 [39]
    ldi     YH, hi8(usbTxBuf)   ;1 [40]
    rjmp    usbSendAndReti      ;2 [42] + 14 = 56 until SOP

; Comment about when to set usbTxLen to USBPID_NAK:
; We should set it back when we receive the ACK from the host. This would
; be simple to implement: One static variable which stores whether the last
; tx was for endpoint 0 or 1 and a compare in the receiver to distinguish the
; ACK. However, we set it back immediately when we send the package,
; assuming that no error occurs and the host sends an ACK. We save one byte
; RAM this way and avoid potential problems with endless retries. The rest of
; the driver assumes error-free transfers anyway.

#if USB_CFG_HAVE_INTRIN_ENDPOINT    /* placed here due to relative jump range */
handleIn1:                      ;1 [33] (branch taken)
#if USB_CFG_HAVE_INTRIN_ENDPOINT3
; 2006-06-10 as suggested by O.Tamura: support second INTR IN / BULK IN endpoint
    ldd     x2, y+2             ;2 [35]
    sbrc    x2, 0               ;2 [37]
    rjmp    handleIn3           ;0
#endif
    lds     cnt, usbTxLen1      ;2 [39]
    sbrc    cnt, 4              ;2 [41] all handshake tokens have bit 4 set
    rjmp    sendCntAndReti      ;0 42 + 17 = 59 until SOP
    sts     usbTxLen1, x1       ;2 [43] x1 == USBPID_NAK from above
    ldi     YL, lo8(usbTxBuf1)  ;1 [44]
    ldi     YH, hi8(usbTxBuf1)  ;1 [45]
    rjmp    usbSendAndReti      ;2 [47] + 13 = 60 until SOP
#endif


;----------------------------------------------------------------------------
; Transmitting data
;----------------------------------------------------------------------------

bitstuff0:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    ldi     x2, 0           ;1
    out     USBOUT, x1      ;1 <-- out
    rjmp    didStuff0       ;2 branch back 2 cycles earlier
bitstuff1:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff1       ;2 we know that C is clear, jump back to do OUT and ror 0 into x2
bitstuff2:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff2       ;2 jump back 4 cycles earlier and do out and ror 0 into x2
bitstuff3:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff3       ;2 jump back earlier and ror 0 into x2
bitstuff4:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    ldi     x2, 0           ;1
    out     USBOUT, x1      ;1 <-- out
    rjmp    didStuff4       ;2 jump back 2 cycles earlier

sendNakAndReti:                 ;0 [-19] 19 cycles until SOP
    ldi     x3, USBPID_NAK      ;1 [-18]
    rjmp    usbSendX3           ;2 [-16]
sendAckAndReti:                 ;0 [-19] 19 cycles until SOP
    ldi     x3, USBPID_ACK      ;1 [-18]
    rjmp    usbSendX3           ;2 [-16]
sendCntAndReti:                 ;0 [-17] 17 cycles until SOP
    mov     x3, cnt             ;1 [-16]
usbSendX3:                      ;0 [-16]
    ldi     YL, 20              ;1 [-15] 'x3' is R20
    ldi     YH, 0               ;1 [-14]
    ldi     cnt, 2              ;1 [-13]
;   rjmp    usbSendAndReti      fallthrough

; USB spec says:
; idle = J
; J = (D+ = 0), (D- = 1) or USBOUT = 0x01
; K = (D+ = 1), (D- = 0) or USBOUT = 0x02
; Spec allows 7.5 bit times from EOP to SOP for replies (= 60 cycles)

;usbSend:
;pointer to data in 'Y'
;number of bytes in 'cnt' -- including sync byte
;uses: x1...x4, shift, cnt, Y
;Numbers in brackets are time since first bit of sync pattern is sent
usbSendAndReti:             ;0 [-13] timing: 13 cycles until SOP
    in      x2, USBDDR      ;1 [-12]
    ori     x2, USBMASK     ;1 [-11]
    sbi     USBOUT, USBMINUS;2 [-9] prepare idle state; D+ and D- must have been 0 (no pullups)
    in      x1, USBOUT      ;1 [-8] port mirror for tx loop
    out     USBDDR, x2      ;1 [-7] <- acquire bus
; need not init x2 (bitstuff history) because sync starts with 0
    push    x4              ;2 [-5]
    ldi     x4, USBMASK     ;1 [-4] exor mask
    ldi     shift, 0x80     ;1 [-3] sync byte is first byte sent
txLoop:                     ;       [62]
    sbrs    shift, 0        ;1 [-2] [62]
    eor     x1, x4          ;1 [-1] [63]
    out     USBOUT, x1      ;1 [0] <-- out bit 0
    ror     shift           ;1 [1]
    ror     x2              ;1 [2]
didStuff0:
    cpi     x2, 0xfc        ;1 [3]
    brsh    bitstuff0       ;1 [4]
    sbrs    shift, 0        ;1 [5]
    eor     x1, x4          ;1 [6]
    ror     shift           ;1 [7]
didStuff1:
    out     USBOUT, x1      ;1 [8] <-- out bit 1
    ror     x2              ;1 [9]
    cpi     x2, 0xfc        ;1 [10]
    brsh    bitstuff1       ;1 [11]
    sbrs    shift, 0        ;1 [12]
    eor     x1, x4          ;1 [13]
    ror     shift           ;1 [14]
didStuff2:
    ror     x2              ;1 [15]
    out     USBOUT, x1      ;1 [16] <-- out bit 2
    cpi     x2, 0xfc        ;1 [17]
    brsh    bitstuff2       ;1 [18]
    sbrs    shift, 0        ;1 [19]
    eor     x1, x4          ;1 [20]
    ror     shift           ;1 [21]
didStuff3:
    ror     x2              ;1 [22]
    cpi     x2, 0xfc        ;1 [23]
    out     USBOUT, x1      ;1 [24] <-- out bit 3
    brsh    bitstuff3       ;1 [25]
    nop2                    ;2 [27]
    ld      x3, y+          ;2 [29]
    sbrs    shift, 0        ;1 [30]
    eor     x1, x4          ;1 [31]
    out     USBOUT, x1      ;1 [32] <-- out bit 4
    ror     shift           ;1 [33]
    ror     x2              ;1 [34]
didStuff4:
    cpi     x2, 0xfc        ;1 [35]
    brsh    bitstuff4       ;1 [36]
    sbrs    shift, 0        ;1 [37]
    eor     x1, x4          ;1 [38]
    ror     shift           ;1 [39]
didStuff5:
    out     USBOUT, x1      ;1 [40] <-- out bit 5
    ror     x2              ;1 [41]
    cpi     x2, 0xfc        ;1 [42]
    brsh    bitstuff5       ;1 [43]
    sbrs    shift, 0        ;1 [44]
    eor     x1, x4          ;1 [45]
    ror     shift           ;1 [46]
didStuff6:
    ror     x2              ;1 [47]
    out     USBOUT, x1      ;1 [48] <-- out bit 6
    cpi     x2, 0xfc        ;1 [49]
    brsh    bitstuff6       ;1 [50]
    sbrs    shift, 0        ;1 [51]
    eor     x1, x4          ;1 [52]
    ror     shift           ;1 [53]
didStuff7:
    ror     x2              ;1 [54]
    cpi     x2, 0xfc        ;1 [55]
    out     USBOUT, x1      ;1 [56] <-- out bit 7
    brsh    bitstuff7       ;1 [57]
    mov     shift, x3       ;1 [58]
    dec     cnt             ;1 [59]
    brne    txLoop          ;1/2 [60/61]
;make SE0:
    cbr     x1, USBMASK     ;1 [61] prepare SE0 [spec says EOP may be 15 to 18 cycles]
    pop     x4              ;2 [63]
;brackets are cycles from start of SE0 now
    out     USBOUT, x1      ;1 [0] <-- out SE0 -- from now 2 bits = 16 cycles until bus idle
    nop2                    ;2 [2]
;2006-03-06: moved transfer of new address to usbDeviceAddr from C-Code to asm:
;set address only after data packet was sent, not after handshake
    lds     x2, usbNewDeviceAddr;2 [4]
    subi    YL, 20 + 2      ;1 [5]
    sbci    YH, 0           ;1 [6]
    breq    skipAddrAssign  ;2 [8]
    sts     usbDeviceAddr, x2;0  if not skipped: SE0 is one cycle longer
skipAddrAssign:
;end of usbDeviceAddress transfer
    ldi     x2, 1<<USB_INTR_PENDING_BIT;1 [9] int0 occurred during TX -- clear pending flag
    out     USB_INTR_PENDING, x2;1 [10]
    ori     x1, USBIDLE     ;1 [11]
    in      x2, USBDDR      ;1 [12]
    cbr     x2, USBMASK     ;1 [13] set both pins to input
    mov     x3, x1          ;1 [14]
    cbr     x3, USBMASK     ;1 [15] configure no pullup on both pins
    out     USBOUT, x1      ;1 [16] <-- out J (idle) -- end of SE0 (EOP signal)
    out     USBDDR, x2      ;1 [17] <-- release bus now
    out     USBOUT, x3      ;1 [18] <-- ensure no pull-up resistors are active
    rjmp    doReturn

bitstuff5:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff5       ;2 same trick as above...
bitstuff6:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff6       ;2 same trick as above...
bitstuff7:                  ;1 (for branch taken)
    eor     x1, x4          ;1
    rjmp    didStuff7       ;2 same trick as above...


;----------------------------------------------------------------------------
; Utility functions
;----------------------------------------------------------------------------

#ifdef __IAR_SYSTEMS_ASM__
/* Register assignments for usbCrc16 on IAR cc */
/* Calling conventions on IAR:
 * First parameter passed in r16/r17, second in r18/r19 and so on.
 * Callee must preserve r4-r15, r24-r29 (r28/r29 is frame pointer)
 * Result is passed in r16/r17
 * In case of the "tiny" memory model, pointers are only 8 bit with no
 * padding. We therefore pass argument 1 as "16 bit unsigned".
 */
RTMODEL "__rt_version", "3"
/* The line above will generate an error if cc calling conventions change.
 * The value "3" above is valid for IAR 4.10B/W32
 */
#   define argLen   r18 /* argument 2 */
#   define argPtrL  r16 /* argument 1 */
#   define argPtrH  r17 /* argument 1 */

#   define resCrcL  r16 /* result */
#   define resCrcH  r17 /* result */

#   define ptrL     ZL
#   define ptrH     ZH
#   define ptr      Z
#   define byte     r22
#   define bitCnt   r19
#   define polyL    r20
#   define polyH    r21
#   define scratch  r23

#else  /* __IAR_SYSTEMS_ASM__ */ 
/* Register assignments for usbCrc16 on gcc */
/* Calling conventions on gcc:
 * First parameter passed in r24/r25, second in r22/23 and so on.
 * Callee must preserve r1-r17, r28/r29
 * Result is passed in r24/r25
 */
#   define argLen   r22 /* argument 2 */
#   define argPtrL  r24 /* argument 1 */
#   define argPtrH  r25 /* argument 1 */

#   define resCrcL  r24 /* result */
#   define resCrcH  r25 /* result */

#   define ptrL     XL
#   define ptrH     XH
#   define ptr      x
#   define byte     r18
#   define bitCnt   r19
#   define polyL    r20
#   define polyH    r21
#   define scratch  r23

#endif

; extern unsigned usbCrc16(unsigned char *data, unsigned char len);
; data: r24/25
; len: r22
; temp variables:
;   r18: data byte
;   r19: bit counter
;   r20/21: polynomial
;   r23: scratch
;   r24/25: crc-sum
;   r26/27=X: ptr
usbCrc16:
    mov     ptrL, argPtrL
    mov     ptrH, argPtrH
    ldi     resCrcL, 0xff
    ldi     resCrcH, 0xff
    ldi     polyL, lo8(0xa001)
    ldi     polyH, hi8(0xa001)
crcByteLoop:
    subi    argLen, 1
    brcs    crcReady
    ld      byte, ptr+
    ldi     bitCnt, 8
crcBitLoop:
    mov     scratch, byte
    eor     scratch, resCrcL
    lsr     resCrcH
    ror     resCrcL
    lsr     byte
    sbrs    scratch, 0
    rjmp    crcNoXor
    eor     resCrcL, polyL
    eor     resCrcH, polyH
crcNoXor:
    dec     bitCnt
    brne    crcBitLoop
    rjmp    crcByteLoop
crcReady:
    com     resCrcL
    com     resCrcH
    ret

; extern unsigned usbCrc16Append(unsigned char *data, unsigned char len);
usbCrc16Append:
    rcall   usbCrc16
    st      ptr+, resCrcL
    st      ptr+, resCrcH
    ret
