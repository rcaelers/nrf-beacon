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

#include "beacon.h"

#include "battery_service.h"
#include "beacon_config.h"
#include "beacon_config_service.h"
#include "config.h"
#include "dfu.h"
#include "indicator.h"

#include "app_timer.h"
#include "ble_advdata.h"
#include "ble_conn_params.h"
#include "ble_conn_state.h"
#include "fds.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "nrf_log.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "peer_manager.h"

static ble_gap_sec_params_t m_sec_params;
static uint16_t m_connection_handle = BLE_CONN_HANDLE_INVALID;
static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t m_enc_scan_response_data_not_connectable[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t m_enc_scan_response_data_connectable[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static bool m_connectable = false;

NRF_BLE_GATT_DEF(m_gatt);
NRF_BLE_QWR_DEF(m_qwr);

static ble_gap_adv_data_t m_adv_data_not_connectable =
  {
   .adv_data =
   {
    .p_data = m_enc_advdata,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
   },
   .scan_rsp_data =
   {
    .p_data = m_enc_scan_response_data_not_connectable,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
   }
  };

static ble_gap_adv_data_t m_adv_data_connectable =
  {
   .adv_data =
   {
    .p_data = m_enc_advdata,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
   },
   .scan_rsp_data =
   {
    .p_data = m_enc_scan_response_data_connectable,
    .len = BLE_GAP_ADV_SET_DATA_SIZE_MAX
   }
  };

static void
on_ble_event(ble_evt_t const *ble_evt, void *context)
{
  uint32_t err_code = NRF_SUCCESS;

  switch (ble_evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
      NRF_LOG_INFO("Connected.\r\n");

      m_connection_handle = ble_evt->evt.gap_evt.conn_handle;
      beacon_start_advertising_non_connectable();
      indicator_start_loop(flash_three_times_indicator);

      err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_connection_handle);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GAP_EVT_DISCONNECTED:
      NRF_LOG_INFO("Disconnected.\r\n");

      m_connection_handle = BLE_CONN_HANDLE_INVALID;

      indicator_stop();
      beacon_start_advertising();
      break;

    case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
      {
        ble_gap_phys_t const phys =
          {
           .rx_phys = BLE_GAP_PHY_AUTO,
           .tx_phys = BLE_GAP_PHY_AUTO,
          };
        err_code = sd_ble_gap_phy_update(ble_evt->evt.gap_evt.conn_handle, &phys);
        APP_ERROR_CHECK(err_code);
      }
      break;

    case BLE_GAP_EVT_ADV_SET_TERMINATED:
      NRF_LOG_DEBUG("Advertising timeout.");
      if (!beacon_is_connected())
        {
          beacon_start_advertising();
        }
      break;

    case BLE_GATTC_EVT_TIMEOUT:
      NRF_LOG_DEBUG("GATT client timeout.");
      err_code = sd_ble_gap_disconnect(ble_evt->evt.gattc_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTS_EVT_TIMEOUT:
      NRF_LOG_DEBUG("GATT server timeout.");
      err_code = sd_ble_gap_disconnect(ble_evt->evt.gatts_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      APP_ERROR_CHECK(err_code);
      break;

    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
      NRF_LOG_DEBUG("GATT server sys attr missing.");
      err_code = sd_ble_gatts_sys_attr_set(ble_evt->evt.common_evt.conn_handle, NULL, 0, 0);
      APP_ERROR_CHECK(err_code);
      break;

    default:
      break;
    }
}

static void
pm_evt_handler(pm_evt_t const * p_evt)
{
  ret_code_t err_code;

  switch (p_evt->evt_id)
    {
    case PM_EVT_BONDED_PEER_CONNECTED:
      {
        NRF_LOG_INFO("Connected to a previously bonded device.");
      }
      break;

    case PM_EVT_CONN_SEC_SUCCEEDED:
      {
      }
      break;

    case PM_EVT_CONN_SEC_FAILED:
      {
      }
      break;

    case PM_EVT_CONN_SEC_CONFIG_REQ:
      {
        // Reject pairing request from an already bonded peer.
        pm_conn_sec_config_t conn_sec_config = {.allow_repairing = false};
        pm_conn_sec_config_reply(p_evt->conn_handle, &conn_sec_config);
      }
      break;

    case PM_EVT_STORAGE_FULL:
      {
        // Run garbage collection on the flash.
        err_code = fds_gc();
        if (err_code == FDS_ERR_NO_SPACE_IN_QUEUES)
          {
            // Retry.
          }
        else
          {
            APP_ERROR_CHECK(err_code);
          }
      } break;

    case PM_EVT_PEERS_DELETE_SUCCEEDED:
      {
        NRF_LOG_INFO("Peers deleted successfully.");
        beacon_start_advertising();
      }
      break;

    case PM_EVT_PEER_DATA_UPDATE_FAILED:
      {
        NRF_LOG_INFO("Peer data update failed.");
        APP_ERROR_CHECK(p_evt->params.peer_data_update_failed.error);
      }
      break;

    case PM_EVT_PEER_DELETE_FAILED:
      {
        NRF_LOG_INFO("Peer delete failed.");
        APP_ERROR_CHECK(p_evt->params.peer_delete_failed.error);
      }
      break;

    case PM_EVT_PEERS_DELETE_FAILED:
      {
        NRF_LOG_INFO("Peers delete failed.");
        APP_ERROR_CHECK(p_evt->params.peers_delete_failed_evt.error);
      }
      break;

    case PM_EVT_ERROR_UNEXPECTED:
      {
        NRF_LOG_INFO("Unexpected.");
        APP_ERROR_CHECK(p_evt->params.error_unexpected.error);
      }
      break;

    case PM_EVT_CONN_SEC_START:
    case PM_EVT_PEER_DATA_UPDATE_SUCCEEDED:
    case PM_EVT_PEER_DELETE_SUCCEEDED:
    case PM_EVT_LOCAL_DB_CACHE_APPLIED:
    case PM_EVT_LOCAL_DB_CACHE_APPLY_FAILED:
    case PM_EVT_SERVICE_CHANGED_IND_SENT:
    case PM_EVT_SERVICE_CHANGED_IND_CONFIRMED:
    default:
      break;
    }
}

static void
nrf_qwr_error_handler(uint32_t nrf_error)
{
  APP_ERROR_HANDLER(nrf_error);
}

static void
gap_privacy_init()
{
  beacon_config_t *config = beacon_config_get();
  if (config->interval > 0 && !m_connectable)
    {
      ble_gap_irk_t irk = { 0 };
      memcpy(&irk.irk, config->irk, BLE_GAP_SEC_KEY_LEN);

      pm_privacy_params_t privacy_params = {0};
      privacy_params.privacy_mode = BLE_GAP_PRIVACY_MODE_DEVICE_PRIVACY;
      privacy_params.private_addr_type = BLE_GAP_ADDR_TYPE_RANDOM_PRIVATE_RESOLVABLE;
      privacy_params.private_addr_cycle_s = config->interval * 60;
      privacy_params.p_device_irk = &irk;

      uint32_t err_code = pm_privacy_set(&privacy_params);
      APP_ERROR_CHECK(err_code);
    }
  else
    {
      pm_privacy_params_t privacy_params = {0};
      privacy_params.privacy_mode = BLE_GAP_PRIVACY_MODE_OFF;

      ble_gap_addr_t dev_addr = {0};
      dev_addr.addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
      uint32_t err_code = pm_id_addr_set(&dev_addr);

      err_code = pm_privacy_set(&privacy_params);
      APP_ERROR_CHECK(err_code);
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

  ret_code_t err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_adv_handle, power);
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
  gap_conn_params.min_conn_interval = MSEC_TO_UNITS(100, UNIT_1_25_MS);
  gap_conn_params.max_conn_interval = MSEC_TO_UNITS(200, UNIT_1_25_MS);
  gap_conn_params.slave_latency = 0;
  gap_conn_params.conn_sup_timeout = MSEC_TO_UNITS(4000, UNIT_10_MS);

  err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
  APP_ERROR_CHECK(err_code);

}

static void
gap_init()
{
  gap_params_init();
  gap_privacy_init();
}

static void
gatt_init()
{
  ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
  APP_ERROR_CHECK(err_code);
}

static void
qwr_service_init()
{
  nrf_ble_qwr_init_t qwr_init = {0};
  uint32_t err_code = NRF_SUCCESS;

  qwr_init.error_handler = nrf_qwr_error_handler;

  err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
  APP_ERROR_CHECK(err_code);
}

static void
advertising_data_init()
{
  ret_code_t err_code;
  ble_advdata_t advdata;
  ble_advdata_t srdata_not_connectable;
  ble_advdata_t srdata_connectable;

  memset(&advdata, 0, sizeof(advdata));
  advdata.name_type = BLE_ADVDATA_NO_NAME;
  advdata.include_appearance = false;
  advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

  memset(&srdata_not_connectable, 0, sizeof(srdata_not_connectable));
  srdata_not_connectable.name_type = BLE_ADVDATA_NO_NAME;

  memset(&srdata_connectable, 0, sizeof(srdata_connectable));
  srdata_connectable.name_type = BLE_ADVDATA_FULL_NAME;

  err_code = ble_advdata_encode(&advdata, m_adv_data_connectable.adv_data.p_data, &m_adv_data_connectable.adv_data.len);
  APP_ERROR_CHECK(err_code);

  err_code = ble_advdata_encode(&srdata_not_connectable, m_adv_data_not_connectable.scan_rsp_data.p_data, &m_adv_data_not_connectable.scan_rsp_data.len);
  APP_ERROR_CHECK(err_code);

  err_code = ble_advdata_encode(&srdata_connectable, m_adv_data_connectable.scan_rsp_data.p_data, &m_adv_data_connectable.scan_rsp_data.len);
  APP_ERROR_CHECK(err_code);
}

static void
services_init()
{
  battery_service_init();
  beacon_config_service_init();
  qwr_service_init();
  dfu_services_init();
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

  conn_params_init.first_conn_params_update_delay = APP_TIMER_TICKS(5000);
  conn_params_init.next_conn_params_update_delay  = APP_TIMER_TICKS(30000);
  conn_params_init.max_conn_params_update_count   = 3;
  conn_params_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
  conn_params_init.disconnect_on_fail             = false;
  conn_params_init.evt_handler                    = on_conn_params_event;
  conn_params_init.error_handler                  = on_conn_params_error;

  uint32_t err_code = ble_conn_params_init(&conn_params_init);
  APP_ERROR_CHECK(err_code);
}

static void
peer_manager_init()
{
  ret_code_t           err_code;

  err_code = pm_init();
  APP_ERROR_CHECK(err_code);

  memset(&m_sec_params, 0, sizeof(ble_gap_sec_params_t));

  m_sec_params.bond           = 1;
  m_sec_params.mitm           = 1;
  m_sec_params.lesc           = 0;
  m_sec_params.keypress       = 0;
  m_sec_params.io_caps        = BLE_GAP_IO_CAPS_DISPLAY_ONLY;
  m_sec_params.oob            = 0;
  m_sec_params.min_key_size   = 7;
  m_sec_params.max_key_size   = 16;
  m_sec_params.kdist_own.enc  = 1;
  m_sec_params.kdist_own.id   = 1;
  m_sec_params.kdist_peer.enc = 1;
  m_sec_params.kdist_peer.id  = 1;

  gap_pin_init();

  err_code = pm_sec_params_set(&m_sec_params);
  APP_ERROR_CHECK(err_code);

  err_code = pm_register(pm_evt_handler);
  APP_ERROR_CHECK(err_code);
}

void
beacon_init()
{
  peer_manager_init();
  gap_init();
  gatt_init();
  advertising_data_init();
  services_init();
  conn_params_init();

  NRF_SDH_BLE_OBSERVER(m_ble_observer, 3, on_ble_event, NULL);
}

void
beacon_start_advertising_connectable()
{
  beacon_config_t *config = beacon_config_get();

  beacon_stop_advertising();
  m_connectable = true;

  gap_privacy_init();

  ble_gap_adv_params_t adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
  adv_params.duration        = 60 * 100;
  adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
  adv_params.p_peer_addr     = NULL;
  adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
  adv_params.interval        = MSEC_TO_UNITS(config->adv_interval, UNIT_0_625_MS);

  uint32_t err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data_connectable, &adv_params);
  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  APP_ERROR_CHECK(err_code);
  gap_txpower_init();

  if (! config->remain_connectable)
    {
      indicator_start_loop(flash_once_indicator);
    }
}

void
beacon_start_advertising_non_connectable()
{
  beacon_config_t *config = beacon_config_get();

  beacon_stop_advertising();
  m_connectable = false;

  gap_privacy_init();

  ble_gap_adv_params_t adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
  adv_params.duration        = BLE_GAP_ADV_TIMEOUT_GENERAL_UNLIMITED;
  adv_params.properties.type = BLE_GAP_ADV_TYPE_NONCONNECTABLE_SCANNABLE_UNDIRECTED;
  adv_params.p_peer_addr     = NULL;
  adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
  adv_params.interval        = MSEC_TO_UNITS(config->adv_interval, UNIT_0_625_MS);

  uint32_t err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data_not_connectable, &adv_params);

  APP_ERROR_CHECK(err_code);

  err_code = sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
  APP_ERROR_CHECK(err_code);

  gap_txpower_init();

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
  if (m_adv_handle != BLE_GAP_ADV_SET_HANDLE_NOT_SET)
    {
      uint32_t err_code = sd_ble_gap_adv_stop(m_adv_handle);
      if (err_code != NRF_ERROR_INVALID_STATE)
        {
          APP_ERROR_CHECK(err_code);
        }
    }
}

bool
beacon_is_connected()
{
  return m_connection_handle != BLE_CONN_HANDLE_INVALID;
}
