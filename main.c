// Copyright (C) 2017, 2018 Rob Caelers <rob.caelers@gmail.com>
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

#include "indicator.h"
#include "button.h"
#include "beacon.h"
#include "beacon_config.h"

#include "config.h"

#include "app_timer.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

static void
log_init()
{
  ret_code_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void
timers_init()
{
  ret_code_t err_code = app_timer_init();
  APP_ERROR_CHECK(err_code);
}

static void
power_management_init()
{
  ret_code_t err_code;
  err_code = nrf_pwr_mgmt_init();
  APP_ERROR_CHECK(err_code);
}

static void
power_manage()
{
  if (NRF_LOG_PROCESS() == false)
    {
      nrf_pwr_mgmt_run();
    }
}

static void
softdevice_init()
{
  ret_code_t err_code;

  err_code = nrf_sdh_enable_request();
  APP_ERROR_CHECK(err_code);

  uint32_t ram_start = 0;
  err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_sdh_ble_enable(&ram_start);
  APP_ERROR_CHECK(err_code);
}

static void
on_button_callback(button_event_t event, int duration)
{
  switch (event)
    {
    case BUTTON_EVENT_PRESS:
      NRF_LOG_DEBUG("Button press\n");
      if (duration == 2 && !beacon_is_connected())
      {
        indicator_start(flash_twice_fast_indicator);
      }
      else if (duration == 5)
      {
        indicator_start(flash_three_times_fast_indicator);
      }
      else if (duration == 10)
      {
        indicator_start(flash_four_times_fast_indicator);
      }
      else if (duration == 20)
      {
        indicator_start(flash_four_times_fast_indicator);
      }
      break;

    case BUTTON_EVENT_RELEASE:
      NRF_LOG_DEBUG("Button release\n");
      if (duration >= 20)
      {
        NVIC_SystemReset();
      }
      else if (duration >= 10)
      {
        beacon_config_reset();
      }
      else if (duration >= 2 && duration < 5 && !beacon_is_connected())
      {
        beacon_start_advertising_connectable();
      }
      break;
    }
}

void
assert_nrf_callback(uint16_t line_num, const uint8_t *file_name)
{
  app_error_handler(0xdeadbeef, line_num, file_name);
}

void
app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  NRF_LOG_ERROR("Fatal\n");
  NRF_LOG_FINAL_FLUSH();

  switch (id)
    {
    case NRF_FAULT_ID_SDK_ASSERT:
      NRF_LOG_INFO("Assertion failed\n");
      if (((assert_info_t *)(info))->p_file_name)
        {
          NRF_LOG_INFO("Line: %u\n", (unsigned int) ((assert_info_t *)(info))->line_num);
          NRF_LOG_INFO("File: %s\n", (uint32_t)((assert_info_t *)(info))->p_file_name);
        }
      NRF_LOG_INFO("\n");
      break;

    case NRF_FAULT_ID_SDK_ERROR:
      NRF_LOG_INFO("Application error\n");
      if (((error_info_t *)(info))->p_file_name)
        {
          NRF_LOG_INFO("Line: %u\n", (unsigned int) ((error_info_t *)(info))->line_num);
          NRF_LOG_INFO("File: %s\n", (uint32_t)((error_info_t *)(info))->p_file_name);
        }
      NRF_LOG_INFO("Error: 0x%X\n\n", (unsigned int) ((error_info_t *)(info))->err_code);
      break;
    }
}

int
main()
{
  log_init();
  NRF_LOG_INFO("START\n");

  timers_init();
  button_init(on_button_callback);
  indicator_init();
  power_management_init();
  softdevice_init();

  beacon_config_init();
  beacon_init();

  beacon_start_advertising();

  indicator_start(flash_three_times_indicator);

  for (;;)
    {
      power_manage();
    }
}
