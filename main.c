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

#include "indicator.h"
#include "button.h"
#include "beacon.h"
#include "beacon_config.h"

#include "config.h"

#include "app_timer.h"
#include "app_timer_appsh.h"
#include "softdevice_handler.h"
#include "fstorage.h"

#define NRF_LOG_MODULE_NAME "APP"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#if (NRF_SD_BLE_API_VERSION == 3)
static const int NRF_BLE_MAX_MTU_SIZE = GATT_MTU_SIZE_DEFAULT;
#endif

#define CENTRAL_LINK_COUNT 0
#define PERIPHERAL_LINK_COUNT 1

#define NRF_CLOCK_LFCLKSRC                                            \
  {   .source        = NRF_CLOCK_LF_SRC_XTAL,                         \
      .rc_ctiv       = 0,                                             \
      .rc_temp_ctiv  = 0,                                             \
      .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM}

void
assert_nrf_callback(uint16_t line_num, const uint8_t *file_name)
{
  app_error_handler(0xdeadbeef, line_num, file_name);
}

static void
timers_init()
{
  APP_TIMER_INIT(APP_TIMER_PRESCALER, 5, false);
}

static void
power_manage()
{
  uint32_t err_code = sd_app_evt_wait();
  APP_ERROR_CHECK(err_code);
}

static void
on_ble_event_dispatch(ble_evt_t *ble_evt)
{
  beacon_ble_event_dispatch(ble_evt);
}

static void
on_sys_event_dispatch(uint32_t sys_evt)
{
  fs_sys_event_handler(sys_evt);
}

static void
softdevice_init()
{
  uint32_t err_code;

  nrf_clock_lf_cfg_t clock_lf_cfg = NRF_CLOCK_LFCLKSRC;
  SOFTDEVICE_HANDLER_INIT(&clock_lf_cfg, NULL);

  ble_enable_params_t ble_enable_params;
  err_code = softdevice_enable_get_default_config(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT, &ble_enable_params);
  APP_ERROR_CHECK(err_code);

  CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT, PERIPHERAL_LINK_COUNT);

  ble_enable_params.gatts_enable_params.service_changed = IS_SRVC_CHANGED_CHARACT_PRESENT;

#if (NRF_SD_BLE_API_VERSION == 3)
  ble_enable_params.gatt_enable_params.att_mtu = NRF_BLE_MAX_MTU_SIZE;
#endif

  err_code = softdevice_enable(&ble_enable_params);
  APP_ERROR_CHECK(err_code);

  err_code = softdevice_ble_evt_handler_set(on_ble_event_dispatch);
  APP_ERROR_CHECK(err_code);

  err_code = softdevice_sys_evt_handler_set(on_sys_event_dispatch);
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
  uint32_t err_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(err_code);

  timers_init();
  button_init(on_button_callback);
  indicator_init();

  softdevice_init();

  beacon_config_init();
  beacon_init();

  beacon_start_advertising();

  indicator_start(flash_three_times_indicator);

  for (;;)
    {
      if (NRF_LOG_PROCESS() == false)
        {
          power_manage();
        }
    }
}
