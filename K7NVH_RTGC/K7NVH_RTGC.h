/* (c) 2017 Nigel Vander Houwen */
#ifndef _K7NVH_RTGC_H_
#define _K7NVH_RTGC_H_

// Set the UART baud rate, must be defined before the util/setbaud.h include
#define BAUD 9600

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
#include <util/setbaud.h>

#include <LUFA/Drivers/USB/USB.h>
#include <LUFA/Platform/Platform.h>

#include "Descriptors.h"

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Macros
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Enable ANSI color codes to be sent. Uses a small bit of extra program space for 
// storage of color codes/modified strings.
#define ENABLECOLORS

// Enable printing STATE debug messages over the USB console
#define DEBUG_STATE


#define SOFTWARE_STR "\r\nK7NVH RTGC"
#define HARDWARE_VERS "1.0"
#define SOFTWARE_VERS "1.0"
#define DATA_BUFF_LEN    32

// Output port controls
#define ARM_CHANNELS PB3
#define CHANNEL1 PB1
#define CHANNEL2 PD4
#define CHANNEL1_SENSE PB2
#define CHANNEL2_SENSE PD5
#define BEEP PD1

// EEPROM Offsets
#define EEPROM_OFFSET_TXMAC 0 // 8 bytes at offset 0

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Globals
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Timer
volatile unsigned long counter = 0;
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
const char STR_Command_TXMAC[] PROGMEM = "TXMAC";
const char STR_Command_DEBUG[] PROGMEM = "DEBUG";
const char STR_Command_SETMAC[] PROGMEM = "SETMAC";

// State Variables
char * DATA_IN;
uint8_t DATA_IN_POS = 0;
typedef enum {NOCHANGE, OFF, VERYSLOW, SLOW, FAST, ON} BEEP_MODE;

// Launch RX
#define CBUFSIZE (200)
#define MAX_PAYLOAD (100)
typedef enum {COFFEE, NOLINK, LINK, ARM, FIRE} RX_STATE;
typedef enum {SYNC, CNTH, CNTL, PYLD, CHKS} PKT_STATE;
typedef struct {
  PKT_STATE state;
  int length;
  int index;
  uint8_t checksum;
  uint8_t payload[MAX_PAYLOAD];
} PACKET;
uint8_t inCbuf[CBUFSIZE], next_in, next_out, fireCode;
uint8_t txmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


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
#ifdef __AVR_ATmega32U2__
	void (*bootloader)(void) = 0x3800;
#endif

// USB
static inline void run_lufa(void);

// DEBUG
static inline void DEBUG_Dump(void);

// Output
static inline void printPGMStr(PGM_P s);
static inline void PRINT_TXMAC(void);

// Input
static inline void INPUT_Clear(void);
static inline void INPUT_Parse(void);

// UART
static inline uint8_t UART_Recv_Available(void);
static inline char UART_Recv_Char(void);
static inline void UART_Send_Char(char c);

// EEPROM Read & Write
static inline void EEPROM_Read_TXMAC(void);
static inline void EEPROM_Write_TXMAC(void);

// Utility
uint8_t receiveMSG(void);
int16_t getPayload(void);

#endif
