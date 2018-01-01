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

#include "beacon.h"

#include "indicator.h"
#include "battery_service.h"
#include "beacon_config_service.h"
#include "beacon_config.h"
#include "config.h"

#include "app_timer.h"
#include "ble.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_hci.h"
#include "ble_srv_common.h"

#define NRF_LOG_MODULE_NAME "BEACON"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"

#if (NRF_SD_BLE_API_VERSION == 3)
static const int NRF_BLE_MAX_MTU_SIZE = GATT_MTU_SIZE_DEFAULT;
#endif
static const int APP_FEATURE_NOT_SUPPORTED = BLE_GATT_STATUS_ATTERR_APP_BEGIN + 2;

static uint16_t m_connection_handle = BLE_CONN_HANDLE_INVALID;
static ble_gap_sec_params_t m_sec_params;
static ble_gap_sec_keyset_t m_sec_keyset;

static void
on_ble_event(ble_evt_t *ble_evt)
{
  uint32_t err_code = NRF_SUCCESS;

  switch (ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
      NRF_LOG_INFO("Connected.\r\n");

      m_connection_handle = ble_evt->evt.gap_evt.conn_handle;
      beacon_start_advertising_non_connectable();
      indicator_start_loop(flash_three_times_indicator);

      err_code = sd_ble_gap_authenticate(m_connection_handle, &m_sec_params);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      NRF_LOG_INFO("Disconnected.\r\n");

      m_connection_handle = BLE_CONN_HANDLE_INVALID;
      beacon_start_advertising_connectable();
      indicator_stop();
      break;

    case BLE_GAP_EVT_TIMEOUT:
      if (ble_evt->evt.gap_evt.params.timeout.src == BLE_GAP_TIMEOUT_SRC_ADVERTISING)
        {
          if (!beacon_is_connected())
            {
              beacon_start_advertising();
            }
        }
      break;

    case BLE_GATTC_EVT_TIMEOUT:
      err_code = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTS_EVT_TIMEOUT:
      err_code = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_EVT_USER_MEM_REQUEST:
      err_code = sd_ble_user_mem_reply(ble_evt->evt.gattc_evt.conn_handle, NULL);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
      {
        ble_gatts_evt_rw_authorize_request_t req = ble_evt->evt.gatts_evt.params.authorize_request;

        if (req.type != BLE_GATTS_AUTHORIZE_TYPE_INVALID)
          {
            if ((req.request.write.op == BLE_GATTS_OP_PREP_WRITE_REQ)     ||
                (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_NOW) ||
                (req.request.write.op == BLE_GATTS_OP_EXEC_WRITE_REQ_CANCEL))
              {
                ble_gatts_rw_authorize_reply_params_t auth_reply;

                if (req.type == BLE_GATTS_AUTHORIZE_TYPE_WRITE)
                  {
                    auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
                  }
                else
                  {
                    auth_reply.type = BLE_GATTS_AUTHORIZE_TYPE_READ;
                  }
                auth_reply.params.write.gatt_status = APP_FEATURE_NOT_SUPPORTED;
                err_code = sd_ble_gatts_rw_authorize_reply(ble_evt->evt.gatts_evt.conn_handle, &auth_reply);
                APP_ERROR_CHECK(err_code);
              }
          }
      }
      break;

#if (NRF_SD_BLE_API_VERSION == 3)
    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
      err_code = sd_ble_gatts_exchange_mtu_reply(ble_evt->evt.gatts_evt.conn_handle, NRF_BLE_MAX_MTU_SIZE);
      APP_ERROR_CHECK(err_code);
      break;
#endif

    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
      NRF_LOG_DEBUG("SEC PARAMS REQUEST.\r\n");
      err_code = sd_ble_gap_sec_params_reply(ble_evt->evt.common_evt.conn_handle,
                                             BLE_GAP_SEC_STATUS_SUCCESS,
                                             &m_sec_params,
                                             &m_sec_keyset);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_SEC_INFO_REQUEST:
      NRF_LOG_DEBUG("SEC INFO REQUEST.\r\n");
      if (m_sec_keyset.keys_own.p_enc_key != NULL)
        {
          ble_gap_enc_info_t *enc_info = &m_sec_keyset.keys_own.p_enc_key->enc_info;
          err_code = sd_ble_gap_sec_info_reply(m_connection_handle, enc_info, NULL, NULL);
          APP_ERROR_CHECK(err_code);
        }
      else
        {
          err_code = sd_ble_gap_sec_info_reply(m_connection_handle, NULL, NULL, NULL);
          APP_ERROR_CHECK(err_code);
        }
      break;

    case BLE_GAP_EVT_AUTH_STATUS:
      NRF_LOG_DEBUG("BLE_GAP_EVT_AUTH_STATUS: status=0x%x bond=0x%x lv4: %d lv2: %d kdist_own:0x%x kdist_peer:0x%x\r\n",
                   ble_evt->evt.gap_evt.params.auth_status.auth_status,
                   ble_evt->evt.gap_evt.params.auth_status.bonded,
                   ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv4,
                   ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv2,
                   *((uint8_t *)&ble_evt->evt.gap_evt.params.auth_status.kdist_own),
                   *((uint8_t *)&ble_evt->evt.gap_evt.params.auth_status.kdist_peer));
      break;

    case BLE_GAP_EVT_CONN_SEC_UPDATE:
      break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      err_code = sd_ble_gatts_sys_attr_set(ble_evt->evt.common_evt.conn_handle, NULL, 0, 0);
      APP_ERROR_CHECK(err_code);
      break;

    default:
      break;
    }
}

static void
gap_privacy_init()
{
  beacon_config_t *config = beacon_config_get();
  if (config->interval > 0)
  {
    ble_gap_irk_t irk = { 0 };
    memcpy(&irk.irk, config->irk, BLE_GAP_SEC_KEY_LEN);

#if (NRF_SD_BLE_API_VERSION == 2)
    ble_gap_addr_t gap_address;
    gap_address.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
    uint32_t err_code = sd_ble_gap_address_set(BLE_GAP_ADDR_CYCLE_MODE_AUTO, &gap_address);
    APP_ERROR_CHECK(err_code);

    ble_opt_t opt;
    opt.gap_opt.privacy.p_irk = &irk;
    opt.gap_opt.privacy.interval_s = config->interval  * 60;
    sd_ble_opt_set(BLE_GAP_OPT_PRIVACY, &opt);

#elif (NRF_SD_BLE_API_VERSION == 3)
    ble_gap_privacy_params_t ble_gap_privacy_params = {0};
    ble_gap_privacy_params.privacy_mode = BLE_GAP_PRIVACY_MODE_DEVICE_PRIVACY;
    ble_gap_privacy_params.private_addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
    ble_gap_privacy_params.private_addr_cycle_s = config->interval * 60;
    ble_gap_privacy_params.p_device_irk = &irk;
    uint32_t err_code = sd_ble_gap_privacy_set(&ble_gap_privacy_params);
    APP_ERROR_CHECK(err_code);
#endif
  }
}

static void
gap_txpower_init()
{
  beacon_config_t *config = beacon_config_get();
  int8_t power = config->power;

  if (power <= -40)
  {
    power = -40;
  }
  else if (power <= -30)
  {
    power = -30;
  }
  else if (power <= -20)
  {
    power = -20;
  }
  else if (power <= -16)
  {
    power = -16;
  }
  else if (power <= -12)
  {
    power = -12;
  }
  else if (power <= -8)
  {
    power = -8;
  }
  else if (power <= -4)
  {
    power = -4;
  }
  else if (power <= 0)
  {
    power = 0;
  }
  else
  {
    power = 4;
  }

  uint32_t err_code = sd_ble_gap_tx_power_set(power);
  APP_ERROR_CHECK(err_code);
}

static void
gap_pin_init()
{
  beacon_config_t *config = beacon_config_get();

  ble_opt_t ble_opt;
  ble_opt.gap_opt.passkey.p_passkey = config->pin;
  uint32_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PASSKEY, &ble_opt);  
  APP_ERROR_CHECK(err_code);
}

static void
gap_params_init()
{
  uint32_t err_code;
  ble_gap_conn_params_t gap_conn_params;
  ble_gap_conn_sec_mode_t sec_mode;

  BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

  err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)DEVICE_NAME, strlen(DEVICE_NAME));
  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gap_appearance_set(BLE_APPEARANCE_GENERIC_TAG);
  APP_ERROR_CHECK(err_code);

  memset(&gap_conn_params, 0, sizeof(gap_conn_params));
  gap_conn_params.min_conn_interval = MSEC_TO_UNITS(50, UNIT_1_25_MS);
  gap_conn_params.max_conn_interval = MSEC_TO_UNITS(90, UNIT_1_25_MS);
  gap_conn_params.slave_latency = 0;
  gap_conn_params.conn_sup_timeout  = MSEC_TO_UNITS(4000, UNIT_10_MS);

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);

  gap_privacy_init();
  gap_txpower_init();
  gap_pin_init();
}

static void
advertising_data_init()
{
  ble_advdata_t advdata;
  memset(&advdata, 0, sizeof(advdata));
  advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
  advdata.name_type = BLE_ADVDATA_NO_NAME;

  ble_advdata_t srdata;
  memset(&srdata, 0, sizeof(srdata));
  srdata.name_type = BLE_ADVDATA_FULL_NAME;

  uint32_t err_code = ble_advdata_set(&advdata, &srdata);
  APP_ERROR_CHECK(err_code);
}

static void
services_init()
{
  battery_service_init();
  beacon_config_service_init();
}

static void
on_conn_params_event(ble_conn_params_evt_t * evt)
{
  if (evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
      uint32_t err_code = sd_ble_gap_disconnect(m_connection_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
      APP_ERROR_CHECK(err_code);
    }
}

static void
on_conn_params_error(uint32_t error)
{
  APP_ERROR_HANDLER(error);
}

static void
conn_params_init()
{
  ble_conn_params_init_t conn_params_init;
  memset(&conn_params_init, 0, sizeof(conn_params_init));

  conn_params_init.first_conn_params_update_delay = APP_TIMER_TICKS(5000, APP_TIMER_PRESCALER);
  conn_params_init.next_conn_params_update_delay  = APP_TIMER_TICKS(30000, APP_TIMER_PRESCALER);
  conn_params_init.max_conn_params_update_count   = 3;
  conn_params_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
  conn_params_init.disconnect_on_fail             = false;
  conn_params_init.evt_handler                    = on_conn_params_event;
  conn_params_init.error_handler                  = on_conn_params_error;

  uint32_t err_code = ble_conn_params_init(&conn_params_init);
  APP_ERROR_CHECK(err_code);
}

static void
sec_param_init()
{
  memset(&m_sec_params, 0, sizeof(m_sec_params));

  // m_sec_params.bond           = 0;
  m_sec_params.mitm           = 1;
  // m_sec_params.lesc           = 0;
  // m_sec_params.keypress       = 0;
  m_sec_params.io_caps        = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
  // m_sec_params.oob            = 0;
  m_sec_params.min_key_size   = 7;
  m_sec_params.max_key_size   = 16;

  memset(&m_sec_keyset, 0, sizeof(m_sec_keyset));
}

void
beacon_init()
{
  sec_param_init();
  gap_params_init();
  advertising_data_init();
  services_init();
  conn_params_init();
}

void
beacon_start_advertising_connectable()
{
  beacon_config_t *config = beacon_config_get();

  ble_gap_adv_params_t adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.type        = BLE_GAP_ADV_TYPE_ADV_IND;
  adv_params.fp          = BLE_GAP_ADV_FP_ANY;
  adv_params.interval    = MSEC_TO_UNITS(config->adv_interval, UNIT_0_625_MS);
  adv_params.timeout     = 0;

  beacon_stop_advertising();

  uint32_t err_code = sd_ble_gap_adv_start(&adv_params);
  APP_ERROR_CHECK(err_code);

  if (! config->remain_connectable)
  {
    indicator_start_loop(flash_once_indicator);
  }
}

void
beacon_start_advertising_non_connectable()
{
  beacon_config_t *config = beacon_config_get();

  ble_gap_adv_params_t adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.type        = BLE_GAP_ADV_TYPE_ADV_NONCONN_IND;
  adv_params.fp          = BLE_GAP_ADV_FP_ANY;
  adv_params.interval    = MSEC_TO_UNITS(config->adv_interval, UNIT_0_625_MS);
  adv_params.timeout     = 0;

  beacon_stop_advertising();

  uint32_t err_code = sd_ble_gap_adv_start(&adv_params);
  APP_ERROR_CHECK(err_code);

  indicator_stop();
}

void
beacon_start_advertising()
{
  beacon_config_t *config = beacon_config_get();
  if (config->remain_connectable)
  {
    beacon_start_advertising_connectable();
  }
  else
  {
    beacon_start_advertising_non_connectable();
  }
}

void
beacon_stop_advertising()
{
  uint32_t err_code = sd_ble_gap_adv_stop();
  if (err_code != NRF_ERROR_INVALID_STATE)
    {
      APP_ERROR_CHECK(err_code);
    }
}

bool
beacon_is_connected()
{
  return m_connection_handle != BLE_CONN_HANDLE_INVALID;
}

void
beacon_ble_event_dispatch(ble_evt_t *ble_evt)
{
  ble_conn_params_on_ble_evt(ble_evt);
  on_ble_event(ble_evt);
  battery_service_ble_event_dispatch(ble_evt);
  beacon_config_service_ble_event_dispatch(ble_evt);
}
