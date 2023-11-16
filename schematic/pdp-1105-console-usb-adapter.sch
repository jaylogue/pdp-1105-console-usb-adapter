EESchema Schematic File Version 4
EELAYER 30 0
EELAYER END
$Descr A 11000 8500
encoding utf-8
Sheet 1 1
Title "PDP-11/05 Console USB Adapter"
Date "2020-03-07"
Rev "1.2"
Comp ""
Comment1 "Licensed under a Creative Commons Attribution 4.0 International License"
Comment2 "Copyright © 2020 Jay Logue"
Comment3 ""
Comment4 ""
$EndDescr
$Comp
L pdp-1105-console-usb-adapter-rescue:Teensy3.1-teensy U1
U 1 1 5E611984
P 6700 3500
F 0 "U1" H 6700 5137 60  0000 C CNN
F 1 "Teensy 3.1/3.2" H 6700 5031 60  0000 C CNN
F 2 "Teensy:Teensy30_31_32_LC" H 6700 2750 60  0001 C CNN
F 3 "" H 6700 2750 60  0000 C CNN
	1    6700 3500
	1    0    0    -1  
$EndComp
$Comp
L Connector_Generic:Conn_02x20_Odd_Even J1
U 1 1 5E613419
P 4000 3050
F 0 "J1" H 4050 4350 50  0000 C CNN
F 1 "Console Connector" H 4050 4250 50  0000 C CNN
F 2 "Connector_IDC:IDC-Header_2x20_P2.54mm_Vertical" H 4000 3050 50  0001 C CNN
F 3 "~" H 4000 3050 50  0001 C CNN
	1    4000 3050
	1    0    0    -1  
$EndComp
Wire Wire Line
	3800 2150 3650 2150
Wire Wire Line
	3650 2150 3650 2000
Wire Wire Line
	3650 2000 4400 2000
Wire Wire Line
	4400 2000 4400 2150
Connection ~ 4400 2150
Wire Wire Line
	4400 2150 4300 2150
Wire Wire Line
	5600 2250 4300 2250
Wire Wire Line
	5600 2350 5500 2350
Wire Wire Line
	5500 2350 5500 3850
Wire Wire Line
	5500 3850 4300 3850
Wire Wire Line
	4300 2650 4400 2650
Wire Wire Line
	4400 2650 4400 2150
Wire Wire Line
	3650 2150 3650 4050
Wire Wire Line
	3650 4050 3800 4050
Connection ~ 3650 2150
Wire Wire Line
	3650 4050 3650 4200
Wire Wire Line
	3650 4200 4400 4200
Wire Wire Line
	4400 4200 4400 4050
Wire Wire Line
	4400 4050 4300 4050
Connection ~ 3650 4050
Wire Wire Line
	4300 2350 5400 2350
Wire Wire Line
	5400 2350 5400 2450
Wire Wire Line
	5400 2450 5600 2450
Wire Wire Line
	4400 2150 5600 2150
Wire Wire Line
	5400 2850 4300 2850
Wire Wire Line
	5600 2550 5400 2550
Wire Wire Line
	5400 2550 5400 2850
$EndSCHEMATC
