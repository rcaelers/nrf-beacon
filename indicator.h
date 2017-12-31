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

#ifndef INDICATOR_H
#define INDICATOR_H

#include <stdint.h>

typedef enum
  {
    INDICATOR_DONE = 0,
    INDICATOR_LED_ON,
    INDICATOR_LED_OFF
  } indicator_action_t;

typedef struct
{
  indicator_action_t action;
  uint16_t duration;
} indicator_t;

void indicator_init();
void indicator_start(indicator_t *indicator);
void indicator_start_loop(indicator_t *indicator);
void indicator_stop();

extern indicator_t flash_once_indicator [];
extern indicator_t flash_twice_indicator [];
extern indicator_t flash_three_times_indicator [];
extern indicator_t flash_twice_fast_indicator [];
extern indicator_t flash_three_times_fast_indicator [];
extern indicator_t flash_four_times_fast_indicator [];

#endif // INDICATOR_H
