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
# Makefile for building and flashing the PDP-11/05 Console USB Adapter
# code without using the Arduino IDE.
#

include teensy-make-defs.mk

all : pdp-1105-console-usb-adapter.hex

pdp-1105-console-usb-adapter.elf : pdp-1105-console-usb-adapter.o $(TEENSY_LIB)
	@echo Linking $@
	$(ECHO_CMD)$(CC) $(LDFLAGS) -o pdp-1105-console-usb-adapter.elf pdp-1105-console-usb-adapter.o $(TEENSY_LIB)

flash : pdp-1105-console-usb-adapter.hex
	@echo Flashing $<
	$(ECHO_CMD)$(TEENSY_LOADER_CLI) -v --mcu=$(CHIP) -w $< 
     
clean :
	rm -f *.o *.d *.elf *.hex *.a
