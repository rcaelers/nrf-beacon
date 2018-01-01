// Copyright (C) 2017 Rob Caelers <rob.caelers@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef CONFIG_H
#define CONFIG_H

// Default settings

#define DEVICE_NAME  "Beacon"
#define BEACON_CONFIG_PIN "123456"
#define BEACON_CONFIG_INTERVAL 15
#define BEACON_CONFIG_REMAIN_CONNECTABLE 0
#define BEACON_CONFIG_ADV_INTERVAL 350
#define BEACON_CONFIG_POWER 4

// hexdump -n 16 -v -e '/1 "0x%02X, " ' /dev/urandon
#define BEACON_CONFIG_IRK { 0xE7, 0x2C, 0xCA, 0x33, 0xB0, 0x3F, 0xCE, 0xAA, 0x6D, 0x34, 0xCF, 0xD9, 0xF6, 0xC0, 0x3A, 0xC2 }

// Board configuration

#define APP_TIMER_PRESCALER (0)
#define IS_SRVC_CHANGED_CHARACT_PRESENT (1)

#if defined(BOARD_TRACKR)
#define LED_PIN    19
#define LED_INVERT  0
#define BUTTON_PIN 17
#elif defined(BOARD_SPARKFUN)
#define LED_PIN     7
#define LED_INVERT  1
#define BUTTON_PIN  6
#else
#error Unknown board
#endif

#endif
