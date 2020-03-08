# PDP-11/05 Console USB Adapter

The *PDP-11/05 Console USB Adapter* is a USB-to-serial bridge designed to interface the
console of a [PDP-11/05](http://gunkies.org/wiki/PDP-11/05) to a modern USB-enabled
host. The project is based on the excellent [Teensy 3.1/3.2](https://www.pjrc.com/teensy/teensy31.html) development board
from [PJRC](https://www.pjrc.com/), which provides an Arduino-compatible development
environment running on a highly-capable ARM Cortex-M processor.

![PDP-11/05 Console USB Adapter #1](images/pdp-1105-adapter-1.jpg)

## Features

- **External baud rate generator** (allows baud rates higher than 2400)


- **Adjustable baud rate** (from 1200 to 38400 baud)


- **Auto-baud selection from the USB host**


- **Experimental paper tape reader mode**


- **Single component design** (just the Teensy plus a connector)

## Theory of Operation

The PDP-11/05 Console USB Adapter operates as a simple USB-to-serial bridge, shuttling characters back and forth between a
host connected via USB to a UART connected to the PDP-11's console.  The Console Adapter uses the hardware USB interface built-in
to the Teensy processor to appear as a virtual RS-232 interface to the host.  It then uses a real hardware UART (also built-in
to the processor) to relay data to the PDP-11.  In this way, the Console Adapter operates very similarly to a common
FTDI USB UART chip (e.g. an FT232H), except that its behavior can be fully customized via firmware.

Rather conveniently, the serial signals exposed by the PDP-11 console are all TTL (0-5V), instead of traditional EIA
levels (±12 V). Although the Teensy 3.1/3.2 is a 3 volt system, its inputs and output are 5 volt TTL-compatible, allowing
it to be directly interfaced to the older system without level shifting.  _Note that this is not true of all
Teensy models, so make substitutions carefully._

The serial input line on the PDP-11/05 is curious in that its level are inverted as compared to a traditional TTL serial
line.  Specifically, 0V represents a serial MARK and +5V represents a serial SPACE.  This is accommodated in the Console
Adapter by using a special mode of the internal UART that inverts just the TX signal.

### Baud Rate Generator

The PDP-11/05 console includes a built-in baud rate generator which generates the 16X clock signal needed to drive
the console UART.  This internal generator is limited to a maximum of 2400 baud.  Conveniently, the generator
can be overridden via signals on the console connector, allowing the system to be run up to 40000 baud by means of
an external clock.  The Console Adapter employs a PWM on the Teensy to generate an external UART clock for
the PDP-11 console.  The frequency of this clock is automatically adjusted to match the baud rate of the Teensy's UART,
keeping the two systems in sync.
 
### Host Selectable Baud Rate

The Console USB Adapter operates as a USB device, which makes it capable of receiving standard control messages from
the host requesting changes to serial parameters, such as baud rate.  The Console Adapter code listens for these baud
rate change messages and automatically adjusts the baud rate of the hardware UART connected to the PDP-11.  This also
triggers a corresponding change to the console baud rate generator described above.  This makes it possible to switch
the PDP's baud rate entirely from software on the host, e.g. from within a terminal program such as minicom, picocom or PuTTY.

### Paper Tape Reader Mode

*NOTE: This feature is experimental*

The PDP-11/05 console includes a signal called "READER RUN" which is designed to drive a low-speed paper tape reader
such as an [ASR-33](https://en.wikipedia.org/wiki/Teletype_Model_33). When the PDP asserts the "READER RUN" signal, 
the paper tape reader is expected to read one character from the tape and send it to the computer. In the Console Adapter,
this signal is fed to a GPIO pin on the Teensy which uses it to emulate the behavior of a legacy tape reader.

The paper tape reader mode in the Console Adapter is controlled using the virtual RTS signal from the USB host.
Under normal circumstances (e.g. when using a terminal program on the USB host) the host asserts the virtual RTS signal over
the USB connection to the Console Adapter indicating it has data to send.  When Console Adapter sees this signal, it
operates in normal (non-paper tape reader) mode.

If the host *de-asserts* RTS, the Console Adapter switches into paper tape reader mode.  In this mode, characters received
from the USB host are buffered in the Console Adapter until the PDP-11 asserts the "READER RUN" signal.  Each time "READER RUN"
is asserted, the Console Adapter feeds one character into the hardware UART attached to the PDP and waits for "READER RUN"
to be assered again.  Characters flowing the other direction (from the PDP to the USB host) are unaffected.

Technically the PDP-11 has two "READER RUN" lines (+ and -) which are intended to be used in a 20ma current loop circuit.  However,
the positive side of the signal ("READER RUN +") is driven with a simple PNP transistor circuit configured as a high-side
switch to +5V.  The Teensy reads this by enabling a pull-down resistor on its input pin.  The "READER RUN -" line is ignored entirely. 

### Status LED

The PDP-11/05 Console USB Adapter uses the LED on the Teensy to show status.  The status LED is lit while the system
is powered and flashes briefly whenever there is console activity.

## Schematic

The following shows the schematic for the PDP-11/05 Console USB Adapter.  The schematic uses modern pin numbering for
the console IDC connector.  For a table showing the DEC signal names, along with the archaic DEC connector pin letters,
see the following PDF: [pdp-1105-scl-connector.pdf](docs/pdp-1105-scl-connector.pdf).

![PDP-11/05 Console USB Adapter Schematic](schematic/pdp-1105-console-usb-adapter.png)


A PDF version of the schematic is available [here](schematic/pdp-1105-console-usb-adapter.pdf).

A KiCad version of the schematic is available [here](schematic/pdp-1105-console-usb-adapter.sch).

## Construction

Although one could create a PCB, the design is simple enough to hand solder on a proto board.  A short (or long)
40-pin ribbon cable with 2 female IDC connectors completes the assembly. 

[![PDP-11/05 Console USB Adapter #2](images/pdp-1105-adapter-2-small.jpg)](images/pdp-1105-adapter-2.jpg)
[![PDP-11/05 Console USB Adapter #3](images/pdp-1105-adapter-3-small.jpg)](images/pdp-1105-adapter-3.jpg)

### Building and Flashing the Firmware

The firmware for the PDP-11/05 Console USB Adapter is implemented as an Arduino sketch, using the [Teensyduino](https://www.pjrc.com/teensy/teensyduino.html)
add-on software for the Arduino IDE.  To build the firmware, first follow the instructions at the PJRC site 
to install the Arduino IDE with the Teensyduino add-on: [https://www.pjrc.com/teensy/td_download.html](https://www.pjrc.com/teensy/td_download.html)

The firmware can be built and flashed onto the device using the standard Arduino IDE GUI. Alternatively, a
gnu compatible Makefile is included for building the firmware from a unix-compatible command line.
To build with the Makefile, first set an environment variable pointing to the install location for the Arduino
IDE, then run make:

    $ export ARDUINO_PATH=$(HOME)/tools/arduino
    $ make

The 'make flash' command can be used to flash the firmware image onto an attached device.  For this to work,
the Teensy Loader command line tool must first be downloaded and built.  The tool can be found here: [https://github.com/PaulStoffregen/teensy_loader_cli](https://github.com/PaulStoffregen/teensy_loader_cli).

To flash the image using the Makefile, first set an environment variable pointing to the teensy\_loader\_cli tool, then
run 'make flash':

    $ export TEENSY_LOADER_CLI=$(HOME)/tools/teensy/teensy_loader_cli/teensy_loader_cli
    $ make flash

## License

The PDP-11/05 Console USB Adapter source code is licensed under the [Apache version 2.0 license](https://www.apache.org/licenses/LICENSE-2.0).

All documentation, including images, schematics and this README, are licensed under a [Creative Commons Attribution 4.0 International License](https://creativecommons.org/licenses/by/4.0/).
