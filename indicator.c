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

#include "indicator.h"

#include "config.h"

#include "nrf_gpio.h"
#include "app_timer.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

APP_TIMER_DEF(m_timer_id);

static indicator_t *m_indicator = NULL;
static int m_index = 0;
static bool m_loop = false;

static void
led_on()
{
  nrf_gpio_pin_write(LED_PIN, 1 ^ LED_INVERT);
}

static void
led_off()
{
  nrf_gpio_pin_write(LED_PIN, 0 ^ LED_INVERT);
}

static void
indicator_process()
{
  if (m_indicator == NULL)
    {
      led_off();
      return;
    }

  indicator_t *indicator = NULL;
  do
    {
      indicator = &(m_indicator[m_index++]);

      switch (indicator->action)
        {
        case INDICATOR_LED_OFF:
          led_off();
          app_timer_start(m_timer_id, APP_TIMER_TICKS(indicator->duration, APP_TIMER_PRESCALER), NULL);
          break;

        case INDICATOR_LED_ON:
          led_on();
          app_timer_start(m_timer_id, APP_TIMER_TICKS(indicator->duration, APP_TIMER_PRESCALER), NULL);
          break;

        case INDICATOR_DONE:
          m_index = 0;
          if (!m_loop)
            {
              led_off();
            }
          break;
        }
    } while (indicator->action == INDICATOR_DONE && m_loop);
}

static void
leds_timer_handler(void *context)
{
  UNUSED_PARAMETER(context);
  indicator_process();
}

void
indicator_init()
{
  nrf_gpio_cfg_output(LED_PIN);

  uint32_t  err_code = app_timer_create(&m_timer_id, APP_TIMER_MODE_SINGLE_SHOT, leds_timer_handler);
  APP_ERROR_CHECK(err_code);
}

void
indicator_start(indicator_t *indicator)
{
  m_indicator = indicator;
  m_index = 0;
  m_loop = false;
  indicator_process();
}

void
indicator_start_loop(indicator_t *indicator)
{
  m_indicator = indicator;
  m_index = 0;
  m_loop = true;
  indicator_process();
}

void
indicator_stop()
{
  m_indicator = NULL;
  m_index = 0;
  indicator_process();
}

indicator_t flash_once_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=800},
  {.action = INDICATOR_DONE,    .duration=0},
};

indicator_t flash_twice_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=800},
  {.action = INDICATOR_DONE,    .duration=0},
};

indicator_t flash_three_times_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_LED_ON,  .duration=400},
  {.action = INDICATOR_LED_OFF, .duration=800},
  {.action = INDICATOR_DONE,    .duration=0},
};

indicator_t flash_twice_fast_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=100},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_DONE,    .duration=0},
};

indicator_t flash_three_times_fast_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=100},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=100},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_DONE,    .duration=0},
};

indicator_t flash_four_times_fast_indicator [] = {
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=100},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=100},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_LED_ON,  .duration=200},
  {.action = INDICATOR_LED_OFF, .duration=400},
  {.action = INDICATOR_DONE,    .duration=0},
};
