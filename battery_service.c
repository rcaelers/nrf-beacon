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
#include <string.h>

#include "battery.h"
#include "battery_service.h"

#include "config.h"

#include "app_timer.h"
#include "ble_bas.h"

static ble_bas_t m_bas;
APP_TIMER_DEF(m_battery_timer_id);

static void
on_battery_service_timer(void *context)
{
  battery_sample_voltage();
}

static void
on_battery_voltage(uint16_t voltage)
{
  int battery_percentage = battery_level_in_percent(voltage);
  uint32_t err_code = ble_bas_battery_level_update(&m_bas, battery_percentage);

  if ( (err_code != NRF_SUCCESS) &&
       (err_code != NRF_ERROR_INVALID_STATE) &&
       (err_code != BLE_ERROR_NO_TX_PACKETS) &&
       (err_code != BLE_ERROR_GATTS_SYS_ATTR_MISSING)
       )
    {
      APP_ERROR_HANDLER(err_code);
    }
}

static void
on_bas_evt(ble_bas_t *bas, ble_bas_evt_t *evt)
{
  uint32_t err_code;
  switch (evt->evt_type)
    {
    case BLE_BAS_EVT_NOTIFICATION_ENABLED:
      err_code = app_timer_start(m_battery_timer_id, APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER), NULL);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_BAS_EVT_NOTIFICATION_DISABLED:
      err_code = app_timer_stop(m_battery_timer_id);
      APP_ERROR_CHECK(err_code);
      break;
    }
}

void
battery_service_ble_event_dispatch(ble_evt_t *ble_evt)
{
  ble_bas_on_ble_evt(&m_bas, ble_evt);
}

void
battery_service_init()
{
  ble_bas_init_t bas_init_obj;
  memset(&bas_init_obj, 0, sizeof(bas_init_obj));
  bas_init_obj.evt_handler          = on_bas_evt;
  bas_init_obj.support_notification = true;
  bas_init_obj.p_report_ref         = NULL;
  bas_init_obj.initial_batt_level   = 100;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_char_attr_md.read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_char_attr_md.cccd_write_perm);
  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init_obj.battery_level_report_read_perm);
  BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init_obj.battery_level_char_attr_md.write_perm);

  uint32_t err_code = ble_bas_init(&m_bas, &bas_init_obj);
  APP_ERROR_CHECK(err_code);

  err_code = app_timer_create(&m_battery_timer_id, APP_TIMER_MODE_REPEATED, on_battery_service_timer);
  APP_ERROR_CHECK(err_code);

  battery_init(on_battery_voltage);
}
