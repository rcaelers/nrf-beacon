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

#ifndef BEACON_CONFIG_SERVICE_H
#define BEACON_CONFIG_SERVICE_H

#include "ble.h"

// 32296067-f5f3-44cb-8cae-d03455cba9cd
#define BEACON_CONFIG_UUID_BASE                    {0x32, 0x29, 0x60, 0x67, 0xf5, 0xf3, 0x44, 0xcb, 0x8c, 0xae, 0xd0, 0x34, 0x00, 0x00, 0xa9, 0xcd}
#define BEACON_CONFIG_UUID_CONFIG_SERVICE          0x2700
#define BEACON_CONFIG_UUID_INTERVAL_CHAR           0x1000
#define BEACON_CONFIG_UUID_REMAIN_CONNECTABLE_CHAR 0x1001
#define BEACON_CONFIG_UUID_ADV_INTERVAL_CHAR       0x1002
#define BEACON_CONFIG_UUID_POWER_CHAR              0x1003
#define BEACON_CONFIG_UUID_IRK_CHAR                0x1004

void beacon_config_service_init();
void beacon_config_service_ble_event_dispatch(ble_evt_t *ble_evt);

#endif // BEACON_CONFIG_SERVICE_H
