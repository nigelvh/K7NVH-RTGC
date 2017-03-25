K7NVH Rocket Tube Ground Control
=======

This ground control receiver is designed to be placed inside the rocket tube to improve integration where external ground control systems are less desirable.

As this system is designed to be placed inside a rocket tube where visibility is non-existant or at best, very limited, user feedback is provided by a audio beeper.

This system has two channels for firing ignitors, but at present there is no provision to fire them separately. Any fire command will fire both channels simultaneously.

The board has two inputs for power, one supply for the electronics, and one for the ignitors. Both are polarity sensitive, and have reverse polarity protection. Polarity is marked on the bottom of the board.

The electronics supply is the BATT connector alone on one side of the board, and can range from 3.9VDC to 15VDC. At 9V the board draws approximately 30mA, giving several hours of runtime on a conventional alkaline 9V.

The ignitor supply is the FIRE BATT connector in line with the FIRE1 and FIRE2 channels. The supply can range up to 15V. It is anticipated that this will be used with single LiPo cells.

Programming of the device is available by DFU, which works with dfu-programmer on OSX or Linux, and Atmel's FLIP programming software on Windows. DFU mode may be entered by connecting to the USB serial device, and entering the CTRL-SHIFT-^ key combination. The device will immediately stop serial functions and enter DFU mode.

While in the USB serial console, some commands are available to aid configuration. See below.

## TONE INDICATIONS
On bootup the system provides the following tones:

	1/2 second startup tone.
	
	1/2 second channel 1 indicator.
	
	1/10 second channel 1 continuity (absent if no continuity).
	
	1/2 second channel 2 indicator.
	
	1/10 second channel 2 continuity (absent if no continuity).
	
	10 seconds of 1 beep every second during which the system will accept no fire commands to ensure pad safety.

After bootup, the tones will indicate current state:
	No Link -> No tones.
	Link -> Short beep every 5 seconds.
	Arm -> Fast beeps (2 every second).
	Fire -> Solid tone.

## COMMANDS
The system recognizes the following commands on the serial console:

### TXMAC
The 'TXMAC' command will return the currently configured TXMAC address. This should match the XBee MAC address of the radio attached to the transmitter station for proper operation.

```plain
#RTGC > TXMAC
TXMAC: 0 13 a2 0 41 26 92 47
```

### SETMAC
The 'SETMAC' command allows you to specify a new TXMAC address. You can use this to configure the system to respond to a different transmitter station. This new value gets stored in EEPROM for any subsequent boots, and takes effect immediately.

```plain
#RTGC > SETMAC 00 13 a2 00 41 26 92 47
TXMAC: 0 13 a2 0 41 26 92 47
```

### DEBUG
The 'DEBUG' command displays some additional state information, but does not change anything. The first line is the Hardware and Software version numbers. The second line is the TXMAC read directly from EEPROM, this should never differ from the 'TXMAC' output, but the 'TXMAC' command reads from memory, so any differences here indicate a bug. The third line shows the binary state continuity of the two channels, 1 being good continuity, 0 being no continuity.

```plain
#RTGC > DEBUG
V1.0,1.0
EEPROM TXMAC: 0 13 a2 0 41 26 92 47
Continuity: 1 1
```
