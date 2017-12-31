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
#include <stdbool.h>

#include "app_error.h"
#include "beacon_config_service.h"

#include "beacon_config.h"

#include <string.h>

typedef enum { ACCESS_TYPE_DENY, ACCESS_TYPE_INSECURE, ACCESS_TYPE_SECURE } access_type_t;
typedef struct
{
  uint16_t uuid;
  access_type_t read;
  access_type_t write;
  uint8_t len;
  void *value;
  ble_gatts_char_handles_t *handles;
  const char *description;
  uint8_t format;
} characteristic_config_t;

static uint16_t m_service_handle;
static uint16_t m_connection_handle = BLE_CONN_HANDLE_INVALID;
static ble_gatts_char_handles_t m_handles_interval;
static ble_gatts_char_handles_t m_handles_remain_connectable;
static ble_gatts_char_handles_t m_handles_adv_interval;
static ble_gatts_char_handles_t m_handles_power;
static ble_gatts_char_handles_t m_handles_irk;
static ble_gatts_char_handles_t m_handles_pin;
static uint16_t m_config_changed = false;

static void
characteristic_add(const characteristic_config_t *characteristic_config)
{
  uint32_t err_code;

  ble_uuid128_t base_uuid = { BEACON_CONFIG_UUID_BASE };
  ble_uuid_t uuid;
  err_code = sd_ble_uuid_vs_add(&base_uuid, &uuid.type);
  APP_ERROR_CHECK(err_code);
  uuid.uuid = characteristic_config->uuid;

  ble_gatts_attr_md_t attr_md;
  memset(&attr_md, 0, sizeof(attr_md));
  attr_md.vloc    = BLE_GATTS_VLOC_USER;

  ble_gatts_attr_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.p_uuid    = &uuid;
  attr.p_attr_md = &attr_md;
  attr.init_len  = characteristic_config->len;
  attr.p_value   = characteristic_config->value;
  attr.max_len   = characteristic_config->len;

  ble_gatts_char_md_t char_md;
  memset(&char_md, 0, sizeof(char_md));

  switch (characteristic_config->read)
    {
    case ACCESS_TYPE_DENY:
      BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.read_perm);
      break;
    case ACCESS_TYPE_SECURE:
      char_md.char_props.read = 1;
      BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.read_perm);
      break;
    case ACCESS_TYPE_INSECURE:
      char_md.char_props.read = 1;
      BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
      break;
    }

  switch (characteristic_config->write)
    {
    case ACCESS_TYPE_DENY:
      BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&attr_md.write_perm);
      break;
    case ACCESS_TYPE_SECURE:
      char_md.char_props.write  = 1;
      BLE_GAP_CONN_SEC_MODE_SET_ENC_WITH_MITM(&attr_md.write_perm);
      break;
    case ACCESS_TYPE_INSECURE:
      char_md.char_props.write = 1;
      BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
      break;
    }

  if (characteristic_config->description)
  {
    char_md.p_char_user_desc = (uint8_t*) characteristic_config->description;
    char_md.char_user_desc_size = strlen(characteristic_config->description);
    char_md.char_user_desc_max_size = char_md.char_user_desc_size;
  }

  ble_gatts_char_pf_t pf = { 0 };
  if (characteristic_config->format != 0)
  {
    pf.format =	characteristic_config->format;
    pf.name_space= BLE_GATT_CPF_NAMESPACE_BTSIG;
    pf.desc= BLE_GATT_CPF_NAMESPACE_DESCRIPTION_UNKNOWN;
    pf.unit = 0x2700;
    char_md.p_char_pf = &pf;
  }

  err_code = sd_ble_gatts_characteristic_add(m_service_handle, &char_md, &attr, characteristic_config->handles);
  APP_ERROR_CHECK(err_code);
}

static void
on_write(ble_evt_t *ble_evt)
{
  m_config_changed = true;
}

static void
on_connect(ble_evt_t *ble_evt)
{
  m_connection_handle = ble_evt->evt.gap_evt.conn_handle;
}

static void
on_disconnect(ble_evt_t *ble_evt)
{
  UNUSED_PARAMETER(ble_evt);
  m_connection_handle = BLE_CONN_HANDLE_INVALID;

  if (m_config_changed)
  {
    beacon_config_save();
    m_config_changed = false;
  }
}

void
beacon_config_service_ble_event_dispatch(ble_evt_t *ble_evt)
{
  switch (ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
      on_connect(ble_evt);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      on_disconnect(ble_evt);
      break;

    case BLE_GATTS_EVT_WRITE:
      on_write(ble_evt);
      break;

    default:
      break;
    }
}

void
beacon_config_service_init()
{
  ble_uuid128_t base_uuid = { BEACON_CONFIG_UUID_BASE };
  ble_uuid_t service_uuid;
  service_uuid.uuid = BEACON_CONFIG_UUID_CONFIG_SERVICE;

  uint32_t err_code = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid, &m_service_handle);
  APP_ERROR_CHECK(err_code);

  beacon_config_t *config = beacon_config_get();

  characteristic_config_t interval_config = 
      {
        .uuid = BEACON_CONFIG_UUID_INTERVAL_CHAR,
        .read = ACCESS_TYPE_INSECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->interval),
        .value = &(config->interval),
        .handles = &m_handles_interval,
        .description = "BDA cycle interval", 
        .format = BLE_GATT_CPF_FORMAT_UINT8,
      };
  characteristic_add(&interval_config);

  characteristic_config_t remain_connectable_config = 
      {
        .uuid = BEACON_CONFIG_UUID_REMAIN_CONNECTABLE_CHAR,
        .read = ACCESS_TYPE_INSECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->remain_connectable),
        .value = &(config->remain_connectable),
        .handles = &m_handles_remain_connectable,
        .description = "Remain connectable",
        .format = BLE_GATT_CPF_FORMAT_BOOLEAN,
      };
  characteristic_add(&remain_connectable_config);

  characteristic_config_t adv_interval_config = 
      {
        .uuid = BEACON_CONFIG_UUID_ADV_INTERVAL_CHAR,
        .read = ACCESS_TYPE_INSECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->adv_interval),
        .value = &(config->adv_interval),
        .handles = &m_handles_adv_interval,
        .description = "Adv interval",
        .format = BLE_GATT_CPF_FORMAT_UINT8,
      };
  characteristic_add(&adv_interval_config);

  characteristic_config_t power_config = 
      {
        .uuid = BEACON_CONFIG_UUID_POWER_CHAR,
        .read = ACCESS_TYPE_INSECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->power),
        .value = &(config->power),
        .handles = &m_handles_power,
        .description = "Power",
        .format = BLE_GATT_CPF_FORMAT_SINT8,
      };
  characteristic_add(&power_config);

  characteristic_config_t pin_config = 
      {
        .uuid = BEACON_CONFIG_UUID_PIN_CHAR,
        .read = ACCESS_TYPE_SECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->pin),
        .value = &(config->pin),
        .handles = &m_handles_pin,
        .description = "PIN",
        .format = BLE_GATT_CPF_FORMAT_UTF8S,
      };
  characteristic_add(&pin_config);

  characteristic_config_t irk_config = 
      {
        .uuid = BEACON_CONFIG_UUID_IRK_CHAR,
        .read = ACCESS_TYPE_SECURE,
        .write = ACCESS_TYPE_SECURE,
        .len = sizeof(config->irk),
        .value = &(config->irk),
        .handles = &m_handles_irk,
        .description = "IRK",
      };
  characteristic_add(&irk_config);
}
