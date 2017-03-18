/* (c) 2017 Nigel Vander Houwen */

#include "K7NVH_RTGC.h"

PACKET packet;
RX_STATE state;

ISR(WDT_vect){
	timer++;
	
//	if ((timer) % VCTL_DELAY == 0){ check_voltage = 1; }
//	if ((timer) % ICTL_DELAY == 0){ check_current = 1; }
}

// Main program entry point.
int main(void) {
	// Initialize some variables
	int16_t BYTE_IN = -1;
	DATA_IN = malloc(DATA_BUFF_LEN);
	
	// Set the watchdog timer to interrupt for timekeeping
	MCUSR &= ~(1 << WDRF);
	WDTCSR |= 0b00011000; // Set WDCE and WDE to enable WDT changes
	WDTCSR = 0b01000011; // Enable watchdog interrupt and Interrupt at 0.25s

	// Save some power by disabling peripherals.
	PRR0 &= 0b11010111;
	PRR1 &= 0b11111110;
	ACSR &= 0b10111111;
	ACSR |= 0b10001000;
	
	// Configure default states for pins
	PORTB &= ~(1 << ARM_CHANNELS) | ~(1 << CHANNEL1);
	PORTD &= ~(1 << CHANNEL2) | ~(1 << BEEP);
	
	// Configure output pins
	DDRB |= (1 << ARM_CHANNELS) | (1 << CHANNEL1);
	DDRD |= (1 << CHANNEL2) | (1 << BEEP);

	// Divide 16MHz crystal down to 1MHz for CPU clock.
	clock_prescale_set(clock_div_16);

	// Init USB hardware and create a regular character stream for the
	// USB interface so that it can be used with the stdio.h functions
	USB_Init();
	CDC_Device_CreateStream(&VirtualSerial_CDC_Interface, &USBSerialStream);

	// Enable interrupts and run the USB routines
	GlobalInterruptEnable();
	run_lufa();

	// Configure the UART (UBRRH_VALUE, UBRRL_VALUE, and USE_2X are defined by <util/setbaud.h>)
	UBRR1H = UBRRH_VALUE;
	UBRR1L = UBRRL_VALUE;
#if USE_2X
	UCSR1A |= (1 << U2X1);
#else
	UCSR1A &= ~(1 << U2X1);
#endif
	UCSR1C = (1 << UCSZ11) | (1 << UCSZ10); // 8 bit data
	UCSR1B = (1 << RXEN1) | (1 << TXEN1); // Enable RX & TX

	// Wait 5 seconds so that we can open a console to catch startup messages
	_delay_ms(5000);

	// Print startup message
	printPGMStr(PSTR(SOFTWARE_STR));
	fprintf(&USBSerialStream, " V%s,%s", HARDWARE_VERS, SOFTWARE_VERS);
	run_lufa();
	
	// Play startup tone
	PORTD |= (1 << BEEP);
	_delay_ms(500);
	PORTD &= ~(1 << BEEP);
	
	_delay_ms(500);
	// Check CHANNEL2 continuity
	PORTD |= (1 << BEEP);
	_delay_ms(500);
	PORTD &= ~(1 << BEEP);
	if(PINB & (1 << CHANNEL1_SENSE)){
		_delay_ms(100);
		PORTD |= (1 << BEEP);
		_delay_ms(100);
		PORTD &= ~(1 << BEEP);
	}
	_delay_ms(500);
	// Check CHANNEL1 continuity
	PORTD |= (1 << BEEP);
	_delay_ms(500);
	PORTD &= ~(1 << BEEP);
	if(PIND & (1 << CHANNEL2_SENSE)){
		_delay_ms(100);
		PORTD |= (1 << BEEP);
		_delay_ms(100);
		PORTD &= ~(1 << BEEP);
	}

	INPUT_Clear();

	// Initialize packet structure
	packet.state = SYNC;
	state = COFFEE;

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Main system loop
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	for (;;) {
		// Read a byte from the USB serial stream
		BYTE_IN = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
		// USB Serial stream will return <0 if no bytes are available.
		if (BYTE_IN >= 0) {
			// Echo the char we just received back out the serial stream so the user's 
			// console will display it.
			fputc(BYTE_IN, &USBSerialStream);
			
			// TESTING
			UART_Send_Char(BYTE_IN);
			
			if (BYTE_IN == 30) {
				// Ctrl-^ jump into the bootloader
				TIMSK0 = 0b00000000; // Interrupt will mess with the bootloader
				bootloader();
			}
		}
		
		run_lufa();
		
		if (UART_Recv_Available()){
			fputc(UART_Recv_Char(), &USBSerialStream);
		}
		
		run_lufa();
	}
/*
	for (;;) {
		// Read a byte from the USB serial stream
		BYTE_IN = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

		// USB Serial stream will return <0 if no bytes are available.
		if (BYTE_IN >= 0) {
			// Echo the char we just received back out the serial stream so the user's 
			// console will display it.
			fputc(BYTE_IN, &USBSerialStream);
			
			
			// TESTING
			UART_Send_Char(BYTE_IN);

			// Switch on the input byte to determine what is is and what to do.
			switch (BYTE_IN) {
				case 8:
				case 127:
					// Handle Backspace chars.
					if (DATA_IN_POS > 0){
						DATA_IN_POS--;
						DATA_IN[DATA_IN_POS] = 0;
						printPGMStr(STR_Backspace);
					}
					break;

				case '\n':
				case '\r':
					// Newline, Parse our command
					INPUT_Parse();
					INPUT_Clear();
					break;

				case 3:
					// Ctrl-c bail out on partial command
					INPUT_Clear();
					break;
					
				case 30:
					// Ctrl-^ jump into the bootloader
					TIMSK0 = 0b00000000; // Interrupt will mess with the bootloader
					bootloader();
					break; // We should never get here...

				default:
					// Normal char buffering
					if (DATA_IN_POS < (DATA_BUFF_LEN - 1)) {
						DATA_IN[DATA_IN_POS] = BYTE_IN;
						DATA_IN_POS++;
						DATA_IN[DATA_IN_POS] = 0;
					} else {
						// Input is too long.
						printPGMStr(STR_Unrecognized);
						INPUT_Clear();
					}
					break;
			}
		}
		
		// Keep the LUFA USB stuff fed regularly.
		run_lufa();
	}
*/
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Command Parsing Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Flush out our data input buffer, reset our position variable, and print a new prompt.
static inline void INPUT_Clear(void) {
	memset(&DATA_IN[0], 0, sizeof(DATA_IN));
	DATA_IN_POS = 0;
	
#ifdef ENABLECOLORS
	printPGMStr(PSTR("\r\n\r\n#\x1b[32mRTGC \x1b[36m>\x1b[0m "));
#else
	printPGMStr(PSTR("\r\n\r\n#RTGC > "));
#endif
}

// We've gotten a new command, parse out what they want.
static inline void INPUT_Parse(void) {
	// STATUS - Print a port status summary for all ports
	if (strncasecmp_P(DATA_IN, STR_Command_STATUS, 6) == 0) {
		PRINT_Status();
		return;
	}
	// DEBUG - Print a report of debugging information, including EEPROM variables
	if (strncasecmp_P(DATA_IN, STR_Command_DEBUG, 10) == 0) {
		DEBUG_Dump();
		return;
	}
	
	// If none of the above commands were recognized, print a generic error.
	printPGMStr(STR_Unrecognized);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Printing Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Print a summary of all ports status'
static inline void PRINT_Status(void) {
	// Voltage
	//main_voltage = ADC_Read_Main_Voltage();
	//printPGMStr(PSTR("\r\n"));
	//fprintf(&USBSerialStream, "%.2fV", main_voltage);
	//alt_voltage = ADC_Read_Alt_Voltage();
	//printPGMStr(PSTR("\r\n"));
	//fprintf(&USBSerialStream, "%.2fV", alt_voltage);

}

// Print a PGM stored string
static inline void printPGMStr(PGM_P s) {
	char c;
	while((c = pgm_read_byte(s++)) != 0) fputc(c, &USBSerialStream);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Debugging Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Dump debugging data
static inline void DEBUG_Dump(void) {
	// Print hardware and software versions
	fprintf(&USBSerialStream, "\r\nV%s,%s", HARDWARE_VERS, SOFTWARE_VERS);

}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ USB Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Run the LUFA USB tasks (except reading)
static inline void run_lufa(void) {
	//CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);
	CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
	USB_USBTask();
}

// Event handler for the library USB Connection event.
void EVENT_USB_Device_Connect(void) {
	// We're enumerated. Act on that as desired.
}

// Event handler for the library USB Disconnection event.
void EVENT_USB_Device_Disconnect(void) {
	// We're no longer enumerated. Act on that as desired.
}

// Event handler for the library USB Configuration Changed event.
void EVENT_USB_Device_ConfigurationChanged(void) {
	bool ConfigSuccess = true;
	ConfigSuccess &= CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
	// USB is ready. Act on that as desired.
}

// Event handler for the library USB Control Request reception event.
void EVENT_USB_Device_ControlRequest(void) {
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ UART Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

static inline uint8_t UART_Recv_Available(void) {
	return (UCSR1A & (1 << RXC1));
}

static inline char UART_Recv_Char(void) {
	while (!(UCSR1A & (1 << RXC1)));
	return UDR1;
}

static inline void UART_Send_Char(char c) {
	while (!(UCSR1A & (1 << UDRE1)));
	UDR1 = c;
}