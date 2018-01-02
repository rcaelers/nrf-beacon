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

#include "beacon_config.h"
#include "config.h"

#include "app_error.h"
#include "fstorage.h"

#include <string.h>

static const uint32_t MAGIC = 0x7F38A9B1;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  beacon_config_t config;
} storage_t;

static storage_t m_storage;

static void beacon_config_fs_evt_handler(fs_evt_t const * const evt, fs_ret_t result);

FS_REGISTER_CFG(fs_config_t fs_config) =
{
  NULL,                           // Begin pointer (set by fs_init)
  NULL,                           // End pointer (set by fs_init)
  &beacon_config_fs_evt_handler,  // Function for event callbacks.
  1,                              // Number of physical flash pages required.
  0xFE                            // Priority for flash usage.
};


static void
beacon_config_fs_evt_handler(fs_evt_t const * const evt, fs_ret_t result)
{
  if (result != FS_SUCCESS || evt->id == FS_EVT_STORE)
  {
    NVIC_SystemReset();
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
  uint32_t err_code = fs_erase(&fs_config, fs_config.p_start_addr, 1, NULL);
  APP_ERROR_CHECK(err_code);

  err_code = fs_store(&fs_config,
                      fs_config.p_start_addr,
                      (uint32_t *)&m_storage,
                      (sizeof(storage_t) + 3) / 4,
                      NULL);
  APP_ERROR_CHECK(err_code);
}

void
beacon_config_init()
{
  uint32_t err_code = fs_init();
  APP_ERROR_CHECK(err_code);

  memcpy(&m_storage, fs_config.p_start_addr, sizeof(storage_t));

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
