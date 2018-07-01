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

#include "beacon_config.h"
#include "config.h"

#include "app_error.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_pwr_mgmt.h"

#include <string.h>

static const uint32_t MAGIC = 0x7F38A9B1;
static const uint32_t PHY_PAGES = 2;
static const uint32_t PHY_PAGE_SIZE = 1024;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  beacon_config_t config;
} storage_t;

static storage_t m_storage;

static void fstorage_evt_handler(nrf_fstorage_evt_t *evt);

NRF_FSTORAGE_DEF(nrf_fstorage_t m_fs) =
  {
   .evt_handler = fstorage_evt_handler,
  };

static
void fstorage_evt_handler(nrf_fstorage_evt_t *evt)
{
  if (evt->result != NRF_SUCCESS)
    {
      return;
    }

  switch (evt->id)
    {
    case NRF_FSTORAGE_EVT_WRITE_RESULT:
      {
        NVIC_SystemReset();
      }
      break;

    default:
      break;
    }
}

static void
beacon_config_set_to_defaults()
{
  m_storage.magic = MAGIC;
  m_storage.version = BEACON_CONFIG_VERSION;
  m_storage.config.interval = BEACON_CONFIG_INTERVAL;
  m_storage.config.remain_connectable = BEACON_CONFIG_REMAIN_CONNECTABLE;
  m_storage.config.adv_interval = BEACON_CONFIG_ADV_INTERVAL;
  m_storage.config.power = BEACON_CONFIG_POWER;

  memcpy(&m_storage.config.pin, BEACON_CONFIG_PIN, 6);
  m_storage.config.pin[6] = 0;

  char irk[16] = BEACON_CONFIG_IRK;
  memcpy(&m_storage.config.irk, irk, BLE_GAP_SEC_KEY_LEN);
}

static void
wait_for_flash_ready()
{
  while (nrf_fstorage_is_busy(&m_fs))
    {
      if (NRF_LOG_PROCESS() == false)
        {
          nrf_pwr_mgmt_run();
        }
    }
}

static uint32_t
flash_end_addr()
{
  uint32_t const bootloader_addr = NRF_UICR->NRFFW[0];
  uint32_t const page_sz         = NRF_FICR->CODEPAGESIZE;
  uint32_t const code_sz         = NRF_FICR->CODESIZE;

  NRF_LOG_INFO("addrs %x %d %d.", bootloader_addr, page_sz, code_sz);

  return (bootloader_addr != 0xFFFFFFFF) ? bootloader_addr : (code_sz * page_sz);
}

static void
flash_bounds_set()
{
  uint32_t flash_size  = (PHY_PAGES * PHY_PAGE_SIZE * sizeof(uint32_t));
  m_fs.end_addr   = flash_end_addr();
  m_fs.start_addr = m_fs.end_addr - flash_size;
}

void
beacon_config_save()
{
  uint32_t err_code = nrf_fstorage_erase(&m_fs, m_fs.start_addr, PHY_PAGES, NULL);
  APP_ERROR_CHECK(err_code);
  wait_for_flash_ready(&m_fs);

  err_code = nrf_fstorage_write(&m_fs, m_fs.start_addr, &m_storage, sizeof(m_storage), NULL);
  APP_ERROR_CHECK(err_code);
  wait_for_flash_ready(&m_fs);
}

void
beacon_config_init()
{
  flash_bounds_set();

  uint32_t err_code = nrf_fstorage_init(&m_fs, &nrf_fstorage_sd, NULL);
  APP_ERROR_CHECK(err_code);

  err_code = nrf_fstorage_read(&m_fs, m_fs.start_addr, &m_storage, sizeof(m_storage));
  APP_ERROR_CHECK(err_code);
  wait_for_flash_ready(&m_fs);

  if (m_storage.magic != MAGIC || m_storage.version != BEACON_CONFIG_VERSION)
    {
      beacon_config_reset();
    }
}

void
beacon_config_reset()
{
  beacon_config_set_to_defaults();
  beacon_config_save();
}

beacon_config_t *
beacon_config_get()
{
  return &m_storage.config;
}
