/* (c) 2017 Nigel Vander Houwen */

#include "K7NVH_RTGC.h"

PACKET packet;
RX_STATE state;

ISR(WDT_vect){
	counter++;
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
	// Check CHANNEL1 continuity
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
	// Check CHANNEL2 continuity
	PORTD |= (1 << BEEP);
	_delay_ms(500);
	PORTD &= ~(1 << BEEP);
	if(PIND & (1 << CHANNEL2_SENSE)){
		_delay_ms(100);
		PORTD |= (1 << BEEP);
		_delay_ms(100);
		PORTD &= ~(1 << BEEP);
	}
	
	_delay_ms(1000);

	INPUT_Clear();
	next_in = 0;
	next_out = 0;

	// Load the TXMAC from EEPROM
	EEPROM_Read_TXMAC();

	// Initialize packet structure
	packet.state = SYNC;
	state = COFFEE;
	timer = counter; // Set timer to match counter before we enter the main loop, so we know what time has passed.

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
		run_lufa();

		// Check if there's a byte available on the UART
		if (UART_Recv_Available()){
			inCbuf[next_in++] = UART_Recv_Char();
			if (next_in == CBUFSIZE) next_in = 0;
		}
		
		// Main state table
		switch (state) {
			// COFFEE - Wait for 10s. Gives time for operator to clear area or cancel.
			case COFFEE:
				// Beep while we're waiting
				if (counter % 2 == 0) { PORTD |= (1 << BEEP); } else { PORTD &= ~(1 << BEEP); }
				if (counter > timer + (4 * 10)) {
					timer = counter; // Reset the timer variable to the current time.
					state = NOLINK; // Advance the state to NOLINK
					PORTD &= ~(1 << BEEP); // Turn off the BEEP if we're in the middle of one.
#ifdef DEBUG_STATE
					printPGMStr(PSTR("COFFEE -> NOLINK\r\n"));
#endif
				}
				break;
			// NOLINK - We don't have an active connection or it timed out. Wait for packets.
			case NOLINK:
				if (receiveMSG() && packet.payload[12] == 'C' && packet.payload[13] == 'H' && packet.payload[14] == 'K') {
					timer = counter; // Reset the timer variable to the current time.
					state = LINK; // Advance the state to LINK
#ifdef DEBUG_STATE
					printPGMStr(PSTR("NOLINK - CHK -> LINK\r\n"));
#endif
				}
				break;
			// LINK - We've got an active connection, make sure it doesn't time out, and parse incoming packets.
			case LINK:
				// If it's been more than 11s since the last message, jump back to NOLINK
				if (counter > timer + (4 * 11)) {
					state = NOLINK;
#ifdef DEBUG_STATE
					printPGMStr(PSTR("LINK -> NOLINK\r\n"));
#endif
					break;
				}
				// Parse any received and *EXPECTED* messages
				if (receiveMSG()) {
					timer = counter; // Reset the timer variable to the current time.
					if (packet.payload[12] == 'C' && packet.payload[13] == 'H' && packet.payload[14] == 'K') {
#ifdef DEBUG_STATE
						printPGMStr(PSTR("LINK - CHK\r\n"));
#endif
						break;
					}
					if (packet.payload[12] == 'A' && packet.payload[13] == 'R' && packet.payload[14] == 'M') {
						state = ARM;
#ifdef DEBUG_STATE
						printPGMStr(PSTR("LINK - ARM -> ARM\r\n"));
#endif
						break;
					}
					// If we get here, we've received a message we weren't expecting. Something else on this channel?
					state = NOLINK;
#ifdef DEBUG_STATE
					printPGMStr(PSTR("LINK - ERR -> NOLINK\r\n"));
#endif
					break;
				}
				break;
			// ARM - We've received an ARM packet. Make sure it doesn't time out, and handle a FIRE
			case ARM:
				// If it's been more than 11s since the last message, jump back to NOLINK
				if (counter > timer + (4 * 11)) {
					state = NOLINK;
#ifdef DEBUG_STATE
					printPGMStr(PSTR("ARM -> NOLINK\r\n"));
#endif
					break;
				}
				// Parse any received and *EXPECTED* messages
				if (receiveMSG()) {
					timer = counter; // Reset the timer variable to the current time.
					if (packet.payload[12] == 'C' && packet.payload[13] == 'H' && packet.payload[14] == 'K') {
						state = LINK;
#ifdef DEBUG_STATE
						printPGMStr(PSTR("ARM - CHK -> LINK\r\n"));
#endif
						break;
					}
					if (packet.payload[12] == 'A' && packet.payload[13] == 'R' && packet.payload[14] == 'M') {
#ifdef DEBUG_STATE
						printPGMStr(PSTR("ARM - ARM\r\n"));
#endif
						break;
					}
					if (packet.payload[12] == 'F' && packet.payload[13] == 'I') {
						state = FIRE;
#ifdef DEBUG_STATE
						printPGMStr(PSTR("ARM - FI0 -> FIRE\r\n"));
#endif
						fireCode = packet.payload[14] & 0x07;
						// Here we could differentiate on the different channels. For now we're going to do simultaneous firing.
						// Enable the ARM MosFET
						PORTB |= (1 << ARM_CHANNELS);
						// Enable the CHANNEL1 and CHANNEL2 MosFETs
						PORTB |= (1 << CHANNEL1);
						PORTD |= (1 << CHANNEL2);
						break;
					}
					// If we get here, we've recieved a message we weren't expecting. Something else on this channel?
					state = NOLINK;
#ifdef DEBUG_STATE
					printPGMStr(PSTR("ARM - ERR -> NOLINK\r\n"));
#endif
					break;
				}
				break;
			// FIRE - We've fired, handle disabling the channels after a delay
			case FIRE:
				// The firing circuit is currently enabled, wait 1s then turn it off again.
				if (counter > timer + (4 * 1)) {
					state = ARM;
#ifdef DEBUG_STATE
					printPGMStr(PSTR("FIRE -> ARM\r\n"));
#endif
					// Disable the CHANNEL1 and CHANNEL2 MosFETs
					PORTB &= ~(1 << CHANNEL1);
					PORTD &= ~(1 << CHANNEL2);
					// Disable the ARM MosFET
					PORTB &= ~(1 << ARM_CHANNELS);
				}
				break;
			// DEFAULT
			default:
				// If we get here, something is wrong. STOP!
				while (true);
				break;
		}
	}
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
	// TXMAC - Print the current configured TXMAC
	if (strncasecmp_P(DATA_IN, STR_Command_TXMAC, 5) == 0) {
		PRINT_TXMAC();
		return;
	}
	// DEBUG - Print a report of debugging information, including EEPROM variables
	if (strncasecmp_P(DATA_IN, STR_Command_DEBUG, 5) == 0) {
		DEBUG_Dump();
		return;
	}
	// SETMAC - Set the expected TXMAC
	if (strncasecmp_P(DATA_IN, STR_Command_SETMAC, 6) == 0) {
		DATA_IN += 6;
		uint8_t temp;
		
		// Read the space separated HEX data
		for (uint8_t i = 0; i < 8; i++) {
			temp = (unsigned int)strtol(DATA_IN, &DATA_IN, 16);
			txmac[i] = temp;
		}
		
		// Save the new TXMAC to EEPROM
		EEPROM_Write_TXMAC();
		
		// Print the new TXMAC
		PRINT_TXMAC();
		
		return;
	}
	
	// If none of the above commands were recognized, print a generic error.
	printPGMStr(STR_Unrecognized);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Printing Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Print a summary of all ports status'
static inline void PRINT_TXMAC(void) {
	printPGMStr(PSTR("\r\nTXMAC:"));
	for (uint8_t i = 0; i < 8; i++) {
		fprintf(&USBSerialStream, " %x", txmac[i]);
	}
	printPGMStr(PSTR("\r\n"));
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

	// Print EEPROM TXMAC
	printPGMStr(PSTR("\r\nEEPROM TXMAC:"));
	for (uint8_t i = 0; i < 8; i++) {
		fprintf(&USBSerialStream, " %x", eeprom_read_byte((uint8_t*)(EEPROM_OFFSET_TXMAC + i)));
	}
	
	// Print Continuity
	printPGMStr(PSTR("\r\nContinuity: "));
	fprintf(&USBSerialStream, "%i %i", ((PINB & (1 << CHANNEL1_SENSE)) >> 2), ((PIND & (1 << CHANNEL2_SENSE)) >> 5));
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

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ UART Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Read the stored port name
static inline void EEPROM_Read_TXMAC() {
	// Read the 8 bytes from EEPROM into the txmac variable.
	for (uint8_t i = 0; i < 8; i++) {
		txmac[i] = eeprom_read_byte((uint8_t*)(EEPROM_OFFSET_TXMAC+i));
	}
}
// Write the port name to EEPROM
static inline void EEPROM_Write_TXMAC() {
	// Write the 8 bytes from the txmac variable into EEPROM
	for (uint8_t i = 0; i < 8; i++) {
		eeprom_update_byte((uint8_t*)(EEPROM_OFFSET_TXMAC+i), txmac[i]);
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// ~~ Utility Functions
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

uint8_t receiveMSG(void) {
	static uint8_t i;
	static uint8_t maccheck;
	while (getPayload() == 0) {
		maccheck = true;
		if (packet.length != 15) { continue; }
		if (packet.payload[0] != 0x90) {
			continue;
		}
		i = 0;
		while (maccheck && (i < sizeof(txmac))) {
			if (packet.payload[i + 1] != txmac[i++]) {
				maccheck = false;
			}
		}
		if (maccheck)
			return true;
	}
	return false;
}

int16_t getPayload(void) {
	int now = next_in;
	uint8_t dat;
	if (next_out == now)
		return -1;
	while (next_out != now) {
		dat = inCbuf[next_out];
		next_out += 1;
		if (next_out == CBUFSIZE)
			next_out = 0;
		switch (packet.state) {
			case SYNC:
				if (dat == 0x7E)
					packet.state = CNTH;
				break;
			case CNTH:
				packet.length = (dat << 8);
				packet.state = CNTL;
				break;
			case CNTL:
				packet.length += dat;
				if (packet.length > MAX_PAYLOAD) {
					packet.state = SYNC;
				} else {
					packet.index = 0;
					packet.checksum = 0;
					packet.state = PYLD;
				}
				break;
			case PYLD:
				packet.checksum += dat;
				packet.payload[packet.index++] = dat;
				if (packet.index == packet.length)
					packet.state = CHKS;
				break;
			case CHKS:
				packet.checksum += dat;
				packet.state = SYNC;
				if (packet.checksum == 0xff)
					return 0;
				break;
			default:
				break;
		}
	}
	return -1;
}