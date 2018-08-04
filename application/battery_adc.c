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

#include "battery.h"

#include "nrf_drv_adc.h"
#include "app_error.h"

#include "config.h"

static nrf_adc_value_t m_battery_level_buffer[1];
static battery_voltage_callback_t m_callback = NULL;

#define DIODE_FWD_VOLT_DROP_MILLIVOLTS    270
#define ADC_RES_10BIT                     1024
#define ADC_REF_VBG_VOLTAGE_IN_MILLIVOLTS 1200
#define ADC_INPUT_PRESCALER               3

#define ADC_RESULT_IN_MILLI_VOLTS(ADC_VALUE) ((((ADC_VALUE) * ADC_REF_VBG_VOLTAGE_IN_MILLIVOLTS) / ADC_RES_10BIT) * ADC_INPUT_PRESCALER)

static void
on_adc_event(nrf_drv_adc_evt_t const *event)
{
  if (event->type == NRF_DRV_ADC_EVT_DONE)
    {
      uint32_t err_code = nrf_drv_adc_buffer_convert(event->data.done.p_buffer, 1);
      APP_ERROR_CHECK(err_code);

      nrf_adc_value_t adc_result = event->data.done.p_buffer[0];
      uint16_t battery_level_in_milli_volts = ADC_RESULT_IN_MILLI_VOLTS(adc_result) + DIODE_FWD_VOLT_DROP_MILLIVOLTS;

      m_callback(battery_level_in_milli_volts);
    }
}

void
battery_init(battery_voltage_callback_t callback)
{
  m_callback = callback;

  uint32_t err_code = nrf_drv_adc_init(NULL, on_adc_event);
  APP_ERROR_CHECK(err_code);

  static nrf_drv_adc_channel_t channel = NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_DISABLED);
  channel.config.config.input = (uint32_t)NRF_ADC_CONFIG_SCALING_SUPPLY_ONE_THIRD;
  nrf_drv_adc_channel_enable(&channel);

  err_code = nrf_drv_adc_buffer_convert(&m_battery_level_buffer[0], 1);
  APP_ERROR_CHECK(err_code);
}

void
battery_sample_voltage()
{
  if (!nrf_drv_adc_is_busy())
    {
      nrf_drv_adc_sample();
    }
}
