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
#include <string.h>

#include "beacon_config.h"
#include "config.h"

#include "app_error.h"
#include "fds.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

static const uint32_t MAGIC = 0x7F5849B1;

#define CONFIG_FILE     (0xF010)
#define CONFIG_REC_KEY  (0x7010)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  beacon_config_t config;
} storage_t;

static storage_t m_storage =
  {
   .magic = 0,
   .version = 0,
   .config = {},
  };

static fds_record_t const m_record =
  {
   .file_id           = CONFIG_FILE,
   .key               = CONFIG_REC_KEY,
   .data.p_data       = &m_storage,
   .data.length_words = (sizeof(m_storage) + 3) / sizeof(uint32_t),
  };

static bool volatile m_fds_initialized;

static void
fds_evt_handler(fds_evt_t const * evt)
{
  NRF_LOG_DEBUG("Event: %d received (%d)", evt->id, evt->result);

  switch (evt->id)
    {
    case FDS_EVT_INIT:
      if (evt->result == FDS_SUCCESS)
        {
          m_fds_initialized = true;
        }
      break;

    case FDS_EVT_WRITE:
      {
        if (evt->result == FDS_SUCCESS)
          {
            NRF_LOG_INFO("Record ID:\t0x%04x",  evt->write.record_id);
            NRF_LOG_INFO("File ID:\t0x%04x",    evt->write.file_id);
            NRF_LOG_INFO("Record key:\t0x%04x", evt->write.record_key);
          }
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

void
beacon_config_save()
{
  ret_code_t rc;
  fds_record_desc_t desc = {0};
  fds_find_token_t tok = {0};

  rc = fds_record_find(CONFIG_FILE, CONFIG_REC_KEY, &desc, &tok);

  if (rc == FDS_SUCCESS)
    {
      rc = fds_record_update(&desc, &m_record);
      APP_ERROR_CHECK(rc);
    }
}

static void
wait_for_fds_ready()
{
  while (!m_fds_initialized)
    {
      (void) sd_app_evt_wait();
    }
}

void
beacon_config_init()
{
  ret_code_t rc;

  (void) fds_register(fds_evt_handler);

  rc = fds_init();
  APP_ERROR_CHECK(rc);

  wait_for_fds_ready();

  fds_record_desc_t desc = {0};
  fds_find_token_t tok  = {0};

  rc = fds_record_find(CONFIG_FILE, CONFIG_REC_KEY, &desc, &tok);
  if (rc == FDS_SUCCESS)
    {
      fds_flash_record_t record = {0};

      rc = fds_record_open(&desc, &record);
      APP_ERROR_CHECK(rc);

      memcpy(&m_storage, record.p_data, sizeof(storage_t));

      NRF_LOG_INFO("Config file found.");
      NRF_LOG_INFO("Magic = %d", m_storage.magic);
      NRF_LOG_INFO("version = %d", m_storage.version);
      NRF_LOG_INFO("Interval = %d", m_storage.config.interval);
      NRF_LOG_INFO("Remain connectable = %d", m_storage.config.remain_connectable);
      NRF_LOG_INFO("Interval = %d", m_storage.config.adv_interval);
      NRF_LOG_INFO("Power = %d", m_storage.config.power);
      NRF_LOG_INFO("Pin = %s", m_storage.config.pin);

      rc = fds_record_close(&desc);
      APP_ERROR_CHECK(rc);

      if (m_storage.magic != MAGIC || m_storage.version != BEACON_CONFIG_VERSION)
        {
          NRF_LOG_INFO("Magic/Version incorrect resetting.");
          beacon_config_set_to_defaults();

          rc = fds_record_update(&desc, &m_record);
          APP_ERROR_CHECK(rc);
        }
    }
  else
    {
      beacon_config_set_to_defaults();
      rc = fds_record_write(&desc, &m_record);
      APP_ERROR_CHECK(rc);
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
