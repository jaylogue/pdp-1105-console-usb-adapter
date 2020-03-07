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

// Default baud rate on boot
#define DEFAULT_BAUD_RATE 9600

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

// Status LED pin
#define STATUS_LED_PIN 13

// Minimum amount of time (in ms) that the status LED should remain either on or off.
#define STATUS_LED_MIN_STATE_TIME_MS 30


// ================================================================================
// GLOBALS
// ================================================================================

static uint32_t sConsoleBaudRate = DEFAULT_BAUD_RATE;
static volatile bool sReaderRunSignaled = false;
static int sLEDState = LOW;
static elapsedMillis sLEDStateTimeMS;
static bool sReaderModeActive = false;
static char sDataBuf[80];


// ================================================================================
// MISC DEFINITIONS
// ================================================================================

#define _CONCAT2(A,B) A##B
#define _CONCAT(A,B) _CONCAT2(A,B)
#define ConsoleSerial _CONCAT(Serial,CONSOLE_UART_NUM)
#define consoleSerialEvent _CONCAT(serialEvent,CONSOLE_UART_NUM)

static void onReaderRun();
static void displayActivity();


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

    // Initialize the UART connected to the PDP-11/05 console to transmit data in 8-N-1 format,
    // at the default baud rate.  The "SERIAL IN (TTL)"​ line on the PDP-11 expects the input
    // signal to be inverted as compared to a traditional TTL serial line, i.e. 0V represents
    // a serial MARK and +5V represents a serial SPACE. Conveniently, Teensy has a mode (TXINV)
    // that does exactly that.
    ConsoleSerial.begin(sConsoleBaudRate, SERIAL_8N1_TXINV);
    ConsoleSerial.setTimeout(0);

    // Configure the baud rate clock pin as a PWM output generating a square wave at 16 times
    // the desired baud rate for the console.  This drives the "CLK IN (TTL)" signal to the
    // PDP-11/05.
    pinMode(CONSOLE_CLOCK_PIN, OUTPUT);
    analogWriteFrequency(CONSOLE_CLOCK_PIN, sConsoleBaudRate*16);
    analogWrite(CONSOLE_CLOCK_PIN, 128); // Default resolution is 256, so 128 produces square wave

    // Configure the pin connected to the PDP-11's "READER RUN +" signal.  The PDP-11 drives
    // this line with a PNP transistor configured as a high-side switch to +5V.  Thus to
    // generate a readable voltage on this this line, we enable the teensy's built-in
    // pull-down resistor on the input pin.
    pinMode(READER_RUN_PIN, INPUT_PULLDOWN);

    // Arrange to receive an interrupt whenever the READER RUN signal goes high.
    attachInterrupt(READER_RUN_PIN, onReaderRun, RISING);
}

void loop()
{
    // If a USB host is attached...
    if (Serial.dtr())
    {
        // Get the baud rate configured by the USB host.
        uint32_t hostBaudRate = Serial.baud();
        
        // Clamp the requested baud rate to the defined minimum and maximum values.
        if (hostBaudRate > MAX_BAUD_RATE)
        {
            hostBaudRate = MAX_BAUD_RATE;
        }
        else if (hostBaudRate < MIN_BAUD_RATE)
        {
            hostBaudRate = MIN_BAUD_RATE;
        }
        
        // If a baud rate change is needed...
        if (hostBaudRate != sConsoleBaudRate)
        {
            // Wait for any data buffered in the console UART to drain.
            ConsoleSerial.flush();
    
            // Configure the console UART with the new baud rate.
            ConsoleSerial.begin(hostBaudRate);
            sConsoleBaudRate = hostBaudRate;
    
            // Change the console baud rate clock to 16 times the new rate.
            analogWriteFrequency(CONSOLE_CLOCK_PIN, sConsoleBaudRate*16);
            analogWrite(CONSOLE_CLOCK_PIN, 128); // re-enable PWM
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
    // Check how much space is available in the USB serial output buffer.
    int writeAvail = Serial.availableForWrite();

    // Compute the maximum number of characters to transfer (min of writeAvail
    // and the size of the data buffer).
    int transferLen = min(writeAvail, (int)sizeof(sDataBuf));

    // Transfer characters from the console UART to the USB serial device, up to
    // the maximum number of characters determined above.
    transferLen = ConsoleSerial.readBytes(sDataBuf, transferLen);
    Serial.write(sDataBuf, transferLen);

    // Display activity via the status LED.
    displayActivity();
}

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
