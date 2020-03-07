#
# Copyright 2020 Jay Logue
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

#
# Make definitions for building Teensy 3.x projects without using the Arduino IDE.
#

.DEFAULT_GOAL := all

ARDUINO_PATH ?= $(HOME)/tools/arduino
TEENSY3X_PATH ?= $(ARDUINO_PATH)/hardware/teensy/avr/cores/teensy3
TOOLCHAIN_PATH ?= $(ARDUINO_PATH)/hardware/tools/arm/bin
TOOLCHAIN_PREFIX ?= arm-none-eabi-
TEENSY_LOADER_CLI ?= $(HOME)/tools/teensy/teensy_loader_cli/teensy_loader_cli

CPU ?= cortex-m4
CHIP ?= MK20DX256
F_CPU ?= 96000000

OPTIMIZATION ?= -Os
DEBUG ?= -g

ifeq ($(VERBOSE),1)
ECHO_CMD =
else
ECHO_CMD = @
endif

CC = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)g++
AS = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)as
AR = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)gcc-ar
LD = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)ld
OBJCOPY = $(TOOLCHAIN_PATH)/$(TOOLCHAIN_PREFIX)objcopy

CCFLAGS = -fno-common -mcpu=$(CPU) -mthumb -MMD $(OPTIMIZATION) $(DEBUG) -DF_CPU=$(F_CPU) -D__$(CHIP)__ -DUSB_SERIAL -DLAYOUT_US_ENGLISH
CXXFLAGS = $(CCFLAGS) -std=gnu++0x -felide-constructors -fno-exceptions -fno-rtti
INCFLAGS = -I $(TEENSY3X_PATH)
ASFLAGS = -mcpu=$(CPU)
ARFLAGS = cr
LDFLAGS = $(OPTIMIZATION) $(DEBUG) -Wl,--gc-sections -mcpu=$(CPU) -mthumb -T$(LDSCRIPT) 

LDSCRIPT ?= $(TEENSY3X_PATH)/mk20dx256.ld

VPATH = $(TEENSY3X_PATH)

%.o : %.c
	@echo Compiling $<
	$(ECHO_CMD)$(CC) $(CCFLAGS) $(INCFLAGS) -c $< -o $@

%.o : %.cpp
	@echo Compiling $<
	$(ECHO_CMD)$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

%.o : %.ino
	@echo Compiling $<
	$(ECHO_CMD)$(CXX) $(CXXFLAGS) $(INCFLAGS) -c -x c++ $< -o $@

%.hex : %.elf
	@echo Generating hex file $@
	$(ECHO_CMD)$(OBJCOPY) -O ihex -R .eeprom $< $@

TEENSY_LIB = libteensy3x.a

TEENSY_SRCS =               \
    analog.c                \
    AudioStream.cpp         \
    avr_emulation.cpp       \
    DMAChannel.cpp          \
    eeprom.c                \
    EventResponder.cpp      \
    HardwareSerial1.cpp     \
    HardwareSerial2.cpp     \
    HardwareSerial3.cpp     \
    HardwareSerial4.cpp     \
    HardwareSerial5.cpp     \
    HardwareSerial6.cpp     \
    IntervalTimer.cpp       \
    IPAddress.cpp           \
    keylayouts.c            \
    main.cpp                \
    math_helper.c           \
    mk20dx128.c             \
    new.cpp                 \
    nonstd.c                \
    pins_teensy.c           \
    Print.cpp               \
    serial1.c               \
    serial2.c               \
    serial3.c               \
    serial4.c               \
    serial5.c               \
    serial6.c               \
    serial6_lpuart.c        \
    ser_print.c             \
    Stream.cpp              \
    Tone.cpp                \
    touch.c                 \
    usb_audio.cpp           \
    usb_desc.c              \
    usb_dev.c               \
    usb_flightsim.cpp       \
    usb_inst.cpp            \
    usb_joystick.c          \
    usb_keyboard.c          \
    usb_mem.c               \
    usb_midi.c              \
    usb_mouse.c             \
    usb_mtp.c               \
    usb_rawhid.c            \
    usb_seremu.c            \
    usb_serial.c            \
    usb_touch.c             \
    WMath.cpp               \
    WString.cpp             \
    yield.cpp
    
TEENSY_OBJS := $(TEENSY_SRCS:.cpp=.o)
TEENSY_OBJS := $(TEENSY_OBJS:.c=.o)

$(TEENSY_LIB) : $(TEENSY_OBJS)
	@echo Creating library $@
	$(ECHO_CMD)$(AR) $(ARFLAGS) $@ $?

