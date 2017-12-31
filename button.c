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

#include <stdint.h>

#include "config.h"

#include "button.h"

#include "app_button.h"
#include "app_timer.h"

static button_callback_t m_callback = NULL;
static int m_count = 0;

APP_TIMER_DEF(m_button_timer_id);

static void
on_button_timer(void *context)
{
  m_count++;
  m_callback(BUTTON_EVENT_PRESS, m_count);
}

static void
on_button_event_handler(uint8_t pin_no, uint8_t button_action)
{
  switch (button_action)
    {
    case APP_BUTTON_PUSH:
      {
        m_count = 0;

        uint32_t err_code = app_timer_start(m_button_timer_id, APP_TIMER_TICKS(1000, APP_TIMER_PRESCALER), NULL);
        APP_ERROR_CHECK(err_code);

        m_callback(BUTTON_EVENT_PRESS, m_count);
      }
      break;

    case APP_BUTTON_RELEASE:
      {
        uint32_t err_code = app_timer_stop(m_button_timer_id);
        APP_ERROR_CHECK(err_code);

        m_callback(BUTTON_EVENT_RELEASE, m_count);
      }
      break;
    }
}

void
button_init(button_callback_t callback)
{
  m_callback = callback;

  static app_button_cfg_t buttons_cfgs[] =
    {
      {
        .pin_no         = BUTTON_PIN,
        .active_state   = APP_BUTTON_ACTIVE_LOW,
        .pull_cfg       = NRF_GPIO_PIN_PULLUP,
        .button_handler = on_button_event_handler
      }
    };

  uint32_t err_code = app_timer_create(&m_button_timer_id, APP_TIMER_MODE_REPEATED, on_button_timer);
  APP_ERROR_CHECK(err_code);

  err_code = app_button_init(buttons_cfgs, sizeof(buttons_cfgs) / sizeof(app_button_cfg_t), APP_TIMER_TICKS(100, APP_TIMER_PRESCALER));
  APP_ERROR_CHECK(err_code);

  err_code = app_button_enable();
  APP_ERROR_CHECK(err_code);
}
