/*
 * Copyright 2020 Jay Logue
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * pdp-1105-console-usb-adapter.cpp
 *
 * A Teensy 3.1/3.2 based USB-to-serial adapter for interfacing with the console of
 * a PDP-11/05. 
 */
 
#include <Arduino.h>
#include <usb_serial.h>
#include <elapsedMillis.h>


// ================================================================================
// CONFIGURATION
// ================================================================================

// Max and min baud rates
//
// Console UART in the PDP-11/05 (e.g. an AY-5-1013) supports up to 40000 baud.
// The baud rate generator in the Teensy 3.1/3.2, however, becomes very inaccurate
// below 1200 baud.  Thus we limit the range to between 1200 and 38400.
//
#define MAX_BAUD_RATE 38400
#define MIN_BAUD_RATE 1200

// Default baud rate and serial format on boot
#define DEFAULT_BAUD_RATE 9600
#define DEFAULT_SERIAL_FORMAT SERIAL_8N1

// Output pin for the "CLK IN (TTL)" signal to the PDP-11/05 (T/16 on the SCL connector).
// This pin is used to drive the UART inside the PDP-11 console.  For this to work, the
// PDP-11's built-in baud rate generator must be disabled by grounding the "CLK DISAB (TTL)"
// line (N/12 on the SCL connector).
#define CONSOLE_CLOCK_PIN 3

// Input pin for "READER RUN +" signal from PDP-11/05 (F/06 on the SCL connector).
#define READER_RUN_PIN 2

// Teensy UART for speaking to PDP-11/05 console.  The RX pin for this UART should be
// connected to the PDP-11's "SERIAL OUT (TTL)" signal (D/04 on the SCL connector),
// while the TX pin should be connected to the "SERIAL IN (TTL)" signal (RR/36).
// Note that on Teensy 3.1/3.2 either UART 1 or 2 should be used to take advantage of
// built-in hardware buffering.
#define CONSOLE_UART_NUM 1

// Teensy UART for speaking to an auxiliary terminal (if present).  This allows an
// serial terminal to interact with the PDP-11/05 console in parallel to the host USB
// serial device.  Set this value to 0 to disable support for an auxiliary terminal.
#define AUX_TERM_UART_NUM 2

// Status LED pin
#define STATUS_LED_PIN 13

// Test mode pin.  If this pin is pulled low at boot, the system operates in test
// mode which allows for loop-back testing of the console UART.  The test mode pin
// is connected to an unused pin (TT/38) on the SCL connector.
#define TEST_MODE_PIN 4

// Minimum amount of time (in ms) that the status LED should remain either on or off.
#define STATUS_LED_MIN_STATE_TIME_MS 30


// ================================================================================
// GLOBALS
// ================================================================================

static uint32_t sConsoleBaudRate = DEFAULT_BAUD_RATE;
static uint32_t sConsoleSerialFormat = DEFAULT_SERIAL_FORMAT;
static volatile bool sReaderRunSignaled = false;
static int sLEDState = LOW;
static elapsedMillis sLEDStateTimeMS;
static bool sReaderModeActive = false;
static bool sTestMode = false;
static char sDataBuf[80];


// ================================================================================
// MISC DEFINITIONS
// ================================================================================

#define _CONCAT2(A,B) A##B
#define _CONCAT(A,B) _CONCAT2(A,B)
#define ConsoleSerial _CONCAT(Serial,CONSOLE_UART_NUM)
#define consoleSerialEvent _CONCAT(serialEvent,CONSOLE_UART_NUM)
#define AuxTermSerial _CONCAT(Serial,AUX_TERM_UART_NUM)
#define auxTermSerialEvent _CONCAT(serialEvent,AUX_TERM_UART_NUM)
#define SERIAL_TXINV (SERIAL_8N1 ^ SERIAL_8N1_TXINV)

static void onReaderRun();
static void displayActivity();
static uint32_t getHostSerialFormat();


// ================================================================================
// CODE
// ================================================================================

void setup()
{
    // Configure the status LED pin as an output and initialize it to on.  The status LED
    // will briefly flash off as characters flow through the system.
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
    sLEDState = HIGH;
    sLEDStateTimeMS = STATUS_LED_MIN_STATE_TIME_MS;

    // Initialize USB serial device used to talk to the host.  Note that on Teensy the baud
    // rate argument here is ignored.
    Serial.begin(0);
    Serial.setTimeout(0); // Turn off default read timeout behavior

    // Configure the test mode pin as an input and query its status.  If the pin value is
    // low, enable test mode.
    //   NOTE: A short delay is necessary to compensate for any capacitance in the pin
    //   when it is not connected to anything.
    pinMode(TEST_MODE_PIN, INPUT_PULLUP);
    delayMicroseconds(10);
    sTestMode = (digitalRead(TEST_MODE_PIN) == LOW);

    // Initialize the UART connected to the PDP-11/05 console to the default baud rate and
    // serial format and disable read timeouts.
    //
    // The "SERIAL IN (TTL)"​ line on the PDP-11 expects the input signal to be inverted as
    // compared to a traditional TTL serial line--i.e. 0V represents a serial MARK and +5V
    // represents a serial SPACE. Conveniently, Teensy has a mode (enabled via the TXINV
    // bit) that does exactly that.  However, if test mode is enabled, the UART is instead
    // configured in non-inverted (normal) mode.  This makes it possible to perform a
    // simple loop-back test by grounding the test mode pin and connecting a jumper between
    // the "SERIAL IN" and "SERIAL OUT" pins on the adapter's console connector.
    ConsoleSerial.begin(sConsoleBaudRate,
                        sConsoleSerialFormat | ((sTestMode) ? 0 : SERIAL_TXINV));
    ConsoleSerial.setTimeout(0);

    // If enabled, initialize the auxiliary terminal UART to match the console UART (minus
    // the TXINV mode).
#if AUX_TERM_UART_NUM
    AuxTermSerial.begin(sConsoleBaudRate, sConsoleSerialFormat);
    AuxTermSerial.setTimeout(0);
#endif // AUX_TERM_UART_NUM

    // Configure the baud rate clock pin as a PWM output generating a square wave at 16 times
    // the desired baud rate for the console.  This drives the "CLK IN (TTL)" signal to the
    // PDP-11/05.
    pinMode(CONSOLE_CLOCK_PIN, OUTPUT);
    analogWriteFrequency(CONSOLE_CLOCK_PIN, sConsoleBaudRate*16);
    analogWrite(CONSOLE_CLOCK_PIN, 128); // Default resolution is 256, so 128 produces square wave

    // Configure the pin connected to the PDP-11's "READER RUN +" signal.  The PDP-11 drives
    // this line with a PNP transistor configured as a high-side switch to +5V.  Thus to
    // generate a readable voltage on this line, we enable the teensy's built-in pull-down
    // resistor on the input pin.
    pinMode(READER_RUN_PIN, INPUT_PULLDOWN);

    // Arrange to receive an interrupt whenever the READER RUN signal goes high.
    attachInterrupt(READER_RUN_PIN, onReaderRun, RISING);
}

void loop()
{
    // If a USB host is attached...
    if (Serial.dtr())
    {
        // Query the baud rate and serial format configured by the USB host.
        uint32_t hostBaudRate = Serial.baud();
        uint32_t hostSerialFormat = getHostSerialFormat();

        // Clamp the baud rate to the defined minimum and maximum values.
        hostBaudRate = min(hostBaudRate, MAX_BAUD_RATE);
        hostBaudRate = max(hostBaudRate, MIN_BAUD_RATE);

        // If a baud rate or format change is needed...
        if (hostBaudRate != sConsoleBaudRate || hostSerialFormat != sConsoleSerialFormat)
        {
            // Wait for any data buffered in the console UART to drain.
            ConsoleSerial.flush();
    
            // Configure the console UART with the new baud rate and format.
            ConsoleSerial.begin(hostBaudRate,
                                hostSerialFormat | ((sTestMode) ? 0 : SERIAL_TXINV));

            // Configure the auxiliary terminal UART similarly.
#if AUX_TERM_UART_NUM
            AuxTermSerial.flush();
            AuxTermSerial.begin(hostBaudRate, hostSerialFormat);
#endif

            // Change the console baud rate clock to 16 times the new rate.
            analogWriteFrequency(CONSOLE_CLOCK_PIN, hostBaudRate*16);
            analogWrite(CONSOLE_CLOCK_PIN, 128); // re-enable PWM

            sConsoleBaudRate = hostBaudRate;
            sConsoleSerialFormat = hostSerialFormat;
        }

        // Check the virtual RTS signal from the USB host.  When RTS is *disabled* by
        // the host, operate in paper tape reader mode; otherwise operate in normal mode. 
        if (!Serial.rts())
        {
            // Sample the state of the READER RUN signal immediately upon entering paper
            // tape reader mode.
            if (!sReaderModeActive)
            {
                sReaderRunSignaled = (digitalRead(READER_RUN_PIN) == HIGH);
            }

            sReaderModeActive = true;
        }
        else
        {
            sReaderModeActive = false;
        }
    }

    // If the status LED is currently off, and has been for the minimum time,
    // turn it back on.
    if (sLEDState == LOW && sLEDStateTimeMS >= STATUS_LED_MIN_STATE_TIME_MS)
    {
        digitalWrite(STATUS_LED_PIN, HIGH);
        sLEDState = HIGH;
        sLEDStateTimeMS = 0;
    }
}

// Called to determine the serial format requested by the USB host.
uint32_t getHostSerialFormat()
{
    // Note that the console serial port on the PDP11/05 is hard-wired to
    // use 8-N-1 format.  However, with the appropriate software on the PDP
    // side, this can be made to look like 7-E-1 or 7-O-1 to the connected
    // terminal (and indeed, Mini-UNIX does exactly that).  Therefore, the
    // requested format is limited to one of those choices.
    if (Serial.stopbits() == 1)
    {
        if (Serial.numbits() == 8)
        {
            if (Serial.paritytype() == 0)
            {
                return SERIAL_8N1;
            }
        }
        else if (Serial.numbits() == 7)
        {
            if (Serial.paritytype() == 1)
            {
                return SERIAL_7O1;
            }
            else if (Serial.paritytype() == 2)
            {
                return SERIAL_7E1;
            }
        }
    }

    return DEFAULT_SERIAL_FORMAT;
}

// Called when there are characters to be read from the USB host.
void serialEvent()
{
    // Check how much space is available in the console UART output buffer.
    int writeAvail = ConsoleSerial.availableForWrite();

    // Compute the maximum number of characters to transfer (min of writeAvail
    // and the size of the data buffer).
    int transferLen = min(writeAvail, (int)sizeof(sDataBuf));

    // If operating in paper tape reader mode...
    if (sReaderModeActive)
    {
        // If the READER RUN signal from the PDP-11 has been asserted, then
        // arrange to transfer exactly *one* character from the USB host to
        // the PDP-11 console.
        if (sReaderRunSignaled)
        {
            transferLen = 1;
            sReaderRunSignaled = false;
        }

        // Otherwise, return without transferring any data and continue waiting for
        // the READER RUN signal to be asserted.
        else
        {
            return;
        }
    }

    // Transfer characters from the USB serial device to the console UART, up to
    // the maximum number of characters determined above.
    transferLen = Serial.readBytes(sDataBuf, transferLen);
    ConsoleSerial.write(sDataBuf, transferLen);

    // Display activity via the status LED.
    displayActivity();
}

// Called when there are characters to be read from the PDP-11 console.
void consoleSerialEvent()
{
    // Compute the maximum number of characters to transfer as the lesser of:
    //     - The space available in the USB serial output buffer, if a USB host is attached.
    //     - The space available in the auxiliary terminal UART output buffer
    //     - The size of the data buffer
    int transferLen = (int)sizeof(sDataBuf);
    if (Serial.dtr()) {
        transferLen = min(transferLen, Serial.availableForWrite());
    }
#if AUX_TERM_UART_NUM
    transferLen = min(transferLen, AuxTermSerial.availableForWrite());
#endif // AUX_TERM_UART_NUM

    // Read characters from the console UART, up to the maximum number of characters
    // determined above.
    transferLen = ConsoleSerial.readBytes(sDataBuf, transferLen);

    // If using a 7-bit format, zero the high bits of all the characters before 
    // transfering them.  Oddly, teensy seems to return the parity bit in the high
    // bit of the data.  This is likely an artifact of the fact that the underlying
    // hardware only supports 8-bit formats.  But it still seems like a bug.
    if (sConsoleSerialFormat == SERIAL_7E1 || sConsoleSerialFormat == SERIAL_7O1)
    {
        for (int i = 0; i < transferLen; i++)
        {
            sDataBuf[i] &= 0x7F;
        }
    }

    // Transfer characters to both the USB serial device and the auxiliary terminal UART.
#if AUX_TERM_UART_NUM
    AuxTermSerial.write(sDataBuf, transferLen);
#endif // AUX_TERM_UART_NUM
    Serial.write(sDataBuf, transferLen);

    // Display activity via the status LED.
    displayActivity();
}

#if AUX_TERM_UART_NUM

// Called when there are characters to be read from the auxiliary terminal UART.
void auxTermSerialEvent()
{
    // Do nothing if operating in paper tape reader mode.
    if (sReaderModeActive)
    {
        return;
    }

    // Check how much space is available in the console UART output buffer.
    int writeAvail = ConsoleSerial.availableForWrite();

    // Compute the maximum number of characters to transfer (min of writeAvail
    // and the size of the data buffer).
    int transferLen = min(writeAvail, (int)sizeof(sDataBuf));

    // Transfer characters from the auxiliary terminal UART to the console UART,
    // up to the maximum number of characters determined above.
    transferLen = AuxTermSerial.readBytes(sDataBuf, transferLen);
    ConsoleSerial.write(sDataBuf, transferLen);

    // Display activity via the status LED.
    displayActivity();
}

#endif // AUX_TERM_UART_NUM

// Called when the PDP-11's READER RUN signal is asserted (goes from LOW to HIGH).
//
// NOTE: This function is called in interrupt context.
//
void onReaderRun()
{
    sReaderRunSignaled = true;
}

// Flash the status LED briefly to show activity.
void displayActivity()
{
    // If the status LED is currently on, and has been for the minimum time,
    // turn it off to signal activity.
    if (sLEDState == HIGH && sLEDStateTimeMS >= STATUS_LED_MIN_STATE_TIME_MS)
    {
        digitalWrite(STATUS_LED_PIN, LOW);
        sLEDState = LOW;
        sLEDStateTimeMS = 0;
    }
}
