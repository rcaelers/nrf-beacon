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

#include "nrf_bootloader.h"
#include "nrf_bootloader_app_start.h"
#include "nrf_bootloader_info.h"
#include "app_timer.h"
#include "nrf_delay.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

static void
on_error()
{
  NRF_LOG_FINAL_FLUSH();

#if NRF_MODULE_ENABLED(NRF_LOG_BACKEND_RTT)
  nrf_delay_ms(100);
#endif
#ifdef NRF_DFU_DEBUG_VERSION
  NRF_BREAKPOINT_COND;
#endif
  NVIC_SystemReset();
}

void
app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
  NRF_LOG_ERROR("%s:%d", p_file_name, line_num);
  on_error();
}

void
app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
  NRF_LOG_ERROR("Received a fault! id: 0x%08x, pc: 0x%08x, info: 0x%08x", id, pc, info);
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
  on_error();
}

void
app_error_handler_bare(uint32_t error_code)
{
  NRF_LOG_ERROR("Received an error: 0x%08x!", error_code);
  on_error();
}

static void dfu_observer(nrf_dfu_evt_type_t evt_type)
{
  switch (evt_type)
    {
    case NRF_DFU_EVT_DFU_FAILED:
    case NRF_DFU_EVT_DFU_ABORTED:
    case NRF_DFU_EVT_DFU_INITIALIZED:
      indicator_start(flash_twice_fast_indicator);
      break;
    case NRF_DFU_EVT_TRANSPORT_ACTIVATED:
      indicator_start(flash_three_times_fast_indicator);
      break;
    case NRF_DFU_EVT_DFU_STARTED:
      break;
    default:
      break;
    }
}

int
main(int argc, char **argv)
{
  uint32_t ret_val;

  ret_val = nrf_bootloader_flash_protect(0, MBR_SIZE, false);
  APP_ERROR_CHECK(ret_val);

  ret_val = nrf_bootloader_flash_protect(BOOTLOADER_START_ADDR, BOOTLOADER_SIZE, false);
  APP_ERROR_CHECK(ret_val);

  (void) NRF_LOG_INIT(app_timer_cnt_get);
  NRF_LOG_DEFAULT_BACKENDS_INIT();

  indicator_init();

  ret_val = nrf_bootloader_init(dfu_observer);
  APP_ERROR_CHECK(ret_val);

  nrf_bootloader_app_start();
}
