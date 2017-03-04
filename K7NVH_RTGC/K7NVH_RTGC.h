/* (c) 2017 Nigel Vander Houwen */
#ifndef _K7NVH_RTGC_H_
#define _K7NVH_RTGC_H_

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Includes
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "Descriptors.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Macros
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Enable ANSI color codes to be sent. Uses a small bit of extra program space for 
// storage of color codes/modified strings.
#define ENABLECOLORS

#define SOFTWARE_STR "\r\nK7NVH RTGC"
#define HARDWARE_VERS "1.0"
#define SOFTWARE_VERS "1.0"
#define DATA_BUFF_LEN    32

// Output port controls
//#define P1EN PD0



// Timing
#define VCTL_DELAY 20 // Ticks. ~5s
#define ICTL_DELAY 1 // Ticks. ~0.25s

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Globals
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Timer
volatile unsigned long timer = 0;

// Standard file stream for the CDC interface when set up, so that the
// virtual CDC COM port can be used like any regular character stream
// in the C APIs.
static FILE USBSerialStream;

// Reused strings
#ifdef ENABLECOLORS
//	const char STR_Color_Red[] PROGMEM = "\x1b[31m";
//	const char STR_Color_Green[] PROGMEM = "\x1b[32m";
//	const char STR_Color_Blue[] PROGMEM = "\x1b[34m";
//	const char STR_Color_Cyan[] PROGMEM = "\x1b[36m";
//	const char STR_Color_Reset[] PROGMEM = "\x1b[0m";
	const char STR_Unrecognized[] PROGMEM = "\r\n\x1b[31mINVALID COMMAND\x1b[0m";
#else
	const char STR_Unrecognized[] PROGMEM = "\r\nINVALID COMMAND";
#endif	

const char STR_Backspace[] PROGMEM = "\x1b[D \x1b[D";

// Command strings
const char STR_Command_STATUS[] PROGMEM = "STATUS";
const char STR_Command_DEBUG[] PROGMEM = "DEBUG";

// State Variables
char * DATA_IN;
uint8_t DATA_IN_POS = 0;

/** LUFA CDC Class driver interface configuration and state information.
 * This structure is passed to all CDC Class driver functions, so that
 * multiple instances of the same class within a device can be
 * differentiated from one another.
 */ 
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface = {
	.Config = {
		.ControlInterfaceNumber   = INTERFACE_ID_CDC_CCI,
		.DataINEndpoint           = {
			.Address          = CDC_TX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.DataOUTEndpoint = {
			.Address          = CDC_RX_EPADDR,
			.Size             = CDC_TXRX_EPSIZE,
			.Banks            = 1,
		},
		.NotificationEndpoint = {
			.Address          = CDC_NOTIFICATION_EPADDR,
			.Size             = CDC_NOTIFICATION_EPSIZE,
			.Banks            = 1,
		},
	},
};

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Prototypes
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Set up a fake function that points to a program address where the bootloader should be
// based on the part type.
void (*bootloader)(void) = 0x0800;

// USB
static inline void run_lufa(void);

// DEBUG
static inline void DEBUG_Dump(void);

// Output
static inline void printPGMStr(PGM_P s);
static inline void PRINT_Status(void);

// Input
static inline void INPUT_Clear(void);
static inline void INPUT_Parse(void);

#endif
