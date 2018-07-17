// Copyright (C) 2018 Rob Caelers <rob.caelers@gmail.com>
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

#include "config.h"

#include "dfu.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_timer.h"
#include "nrf_sdh.h"
#include "nrf_sdm.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_power.h"
#include "ble_dfu.h"
#include "nrf_bootloader_info.h"
#include "nrf_log.h"

static bool
app_shutdown_handler(nrf_pwr_mgmt_evt_t event)
{
  switch (event)
    {
    case NRF_PWR_MGMT_EVT_PREPARE_DFU:
      NRF_LOG_INFO("Power management wants to reset to DFU mode.");
      uint32_t err_code;
      err_code = sd_softdevice_disable();
      APP_ERROR_CHECK(err_code);
      err_code = app_timer_stop_all();
      APP_ERROR_CHECK(err_code);
      break;

    default:
      return true;
    }

  NRF_LOG_INFO("Power management allowed to reset to DFU mode.");
  return true;
}

NRF_PWR_MGMT_HANDLER_REGISTER(app_shutdown_handler, 0);

static void
buttonless_dfu_sdh_state_observer(nrf_sdh_state_evt_t state, void *context)
{
  if (state == NRF_SDH_EVT_STATE_DISABLED)
    {
      nrf_power_gpregret2_set(BOOTLOADER_DFU_SKIP_CRC);
      nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
    }
}

NRF_SDH_STATE_OBSERVER(m_buttonless_dfu_state_obs, 0) =
  {
   .handler = buttonless_dfu_sdh_state_observer,
  };

static void
ble_dfu_evt_handler(ble_dfu_buttonless_evt_type_t event)
{
  NRF_LOG_INFO("ble_dfu_evt_handler %d.", event);
  switch (event)
    {
    case BLE_DFU_EVT_BOOTLOADER_ENTER_PREPARE:
      NRF_LOG_INFO("Device is preparing to enter bootloader mode.");
      // YOUR_JOB: Disconnect all bonded devices that currently are connected.
      //           This is required to receive a service changed indication
      //           on bootup after a successful (or aborted) Device Firmware Update.
      break;

    case BLE_DFU_EVT_BOOTLOADER_ENTER:
      // YOUR_JOB: Write app-specific unwritten data to FLASH, control finalization of this
      //           by delaying reset by reporting false in app_shutdown_handler
      NRF_LOG_INFO("Device will enter bootloader mode.");
      break;

    case BLE_DFU_EVT_BOOTLOADER_ENTER_FAILED:
      NRF_LOG_ERROR("Request to enter bootloader mode failed asynchroneously.");
      // YOUR_JOB: Take corrective measures to resolve the issue
      //           like calling APP_ERROR_CHECK to reset the device.
      break;

    case BLE_DFU_EVT_RESPONSE_SEND_ERROR:
      NRF_LOG_ERROR("Request to send a response to client failed.");
      // YOUR_JOB: Take corrective measures to resolve the issue
      //           like calling APP_ERROR_CHECK to reset the device.
      APP_ERROR_CHECK(false);
      break;

    default:
      NRF_LOG_ERROR("Unknown event from ble_dfu_buttonless.");
      break;
    }
}

void
dfu_services_init()
{
  uint32_t err_code;
  ble_dfu_buttonless_init_t dfus_init = {0};

  err_code = ble_dfu_buttonless_async_svci_init();
  APP_ERROR_CHECK(err_code);

  dfus_init.evt_handler = ble_dfu_evt_handler;

  err_code = ble_dfu_buttonless_init(&dfus_init);
  APP_ERROR_CHECK(err_code);
}
