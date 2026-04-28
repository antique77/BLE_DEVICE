/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_bt_api.h"
#include "sl_main_init.h"
#include "sl_component_catalog.h"
#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif
#include "app_assert.h"
#include "app.h"
#include "sl_gpio.h"
#include "em_cmu.h"
#include "gatt_db.h"
#include "em_i2c.h"
#include <stdio.h>

#include "./bsp/bsp_i2c/bsp_i2c.h"
#include "./bsp/bsp_uart/bsp_uart.h"
#include "./bsp/bsp_pwm/bsp_pwm.h"
#include "./bsp/bsp_bootloader/bsp_bootloader.h" 

static uint8_t advertising_set_handle = 0xff;

static bool     buzzer_enabled   = false;
static uint16_t buzzer_freq       = 2500;
static uint8_t  connection_handle = 0xFF;

char APP_Version[] = "1.0.0";

void app_init(void)
{
  volatile uint32_t *reset_cause = (volatile uint32_t *)0x20000000UL;
  *reset_cause = 0x00000000UL;


  UART_Config(115200);
  i2c_protocol_init();    /* ★ 初始化I2C协议层 (必须在 I2C_Config 之前!) */
  /* 串口启动测试消息 */
  printf("\r\n=== EFR32 BG22 Ready ===\r\n");
  printf("\r\n=== APP Version: %s ===\r\n", APP_Version);
  I2C_Config();
  buzzer_timer_init();
  buzzer_set_pwm(0, 0);
}

void app_process_action(void)
{
  if (app_is_process_required()) {
  }
  uart_cmd_poll();
  //uart_echo();
}

void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {

    case sl_bt_evt_system_boot_id:
    {
      const char *my_name = "EFR32_Buzzer";

      /* 串口输出：系统启动完成 */
      printf("[SYS] Boot OK, starting advertise\r\n");

      sc = sl_bt_gatt_server_write_attribute_value(
          gattdb_device_name, 0, strlen(my_name)+1, (uint8_t *)my_name);
      app_assert_status(sc);

      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle, 160, 160, 0, 0);
      app_assert_status(sc);

      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;
    }

    case sl_bt_evt_connection_opened_id:
      connection_handle = evt->data.evt_connection_opened.connection;

      /* 串口输出：BLE已连接 */
      {
        uint8_t msg_conn[] = "[BLE] Connected!\r\n";
        UART_Send_String(USART0, msg_conn, sizeof(msg_conn) - 1);
      }
      break;

    case sl_bt_evt_connection_closed_id:
      connection_handle = 0xFF;
      buzzer_enabled = false;
      buzzer_set_pwm(0, 0);

      /* 串口输出：BLE断开 */
      printf("[BLE] Disconnected, restarting advertise\r\n");

      sc = sl_bt_legacy_advertiser_generate_data(
          advertising_set_handle, sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);
      sc = sl_bt_legacy_advertiser_start(
          advertising_set_handle, sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    case sl_bt_evt_gatt_server_user_write_request_id:
    {
      uint8_t characteristic = evt->data.evt_gatt_server_user_write_request.characteristic;
      uint8_t *data          = evt->data.evt_gatt_server_user_write_request.value.data;
      uint8_t data_len       = evt->data.evt_gatt_server_user_write_request.value.len;
      uint8_t conn           = evt->data.evt_gatt_server_user_write_request.connection;

      
      if (characteristic == gattdb_buzzer_switch) {
        if (data_len >= 1) {
          buzzer_enabled = (data[0] != 0x00);

          /* 串口输出：蜂鸣器开关状态 */
          {
            uint8_t msg_sw[] = "[BLE] Buzzer switch";
            UART_Send_String(USART0, msg_sw, sizeof(msg_sw) - 1);
            UART_SendByte(USART0,(uint8_t)data[0]);
          }

          if (buzzer_enabled) {
            buzzer_set_pwm(buzzer_freq, 0.5f);
          } else {
            buzzer_set_pwm(0, 0);
          }

          sc = sl_bt_gatt_server_send_user_write_response(
            conn, characteristic, SL_STATUS_OK);
          app_assert_status(sc);
        }
      } else if (characteristic == gattdb_buzzer_frequecy) {
        /* 串口调试：打印收到的原始数据（hex dump） */

        if (data_len >= 1) {  // ★ 只要有数据就解析，不再强制要求 \0 结尾
          uint16_t new_freq = 0;
          int valid_digits = 0;

          /* 遍历所有字节，提取数字字符（兼容无 \0 结尾的情况） */
          for (int i = 0; i < data_len; i++) {
            if (data[i] >= '0' && data[i] <= '9') {
              new_freq = new_freq * 10 + (data[i] - '0');
              valid_digits++;
            }
            /* 遇到 \0 或非数字字符则提前结束（兼容有 \0 结尾的情况） */
            else if (data[i] == '\0') {
              break;
            }
          }

          /* 只要解析到至少1个有效数字就接受 */
          if (valid_digits > 0 && new_freq >= 500 && new_freq <= 8000) {
            buzzer_freq = new_freq;

            /* 串口输出：解析后的频率值 */
            if (buzzer_enabled) {
              buzzer_set_pwm(buzzer_freq, 0.5f);
            }
          }

          sc = sl_bt_gatt_server_send_user_write_response(
            conn, characteristic, SL_STATUS_OK);
          app_assert_status(sc);
        }
      } else {
        sc = sl_bt_gatt_server_send_user_write_response(
          conn, characteristic, SL_STATUS_INVALID_PARAMETER);
        app_assert_status(sc);
      }
      break;
    }

    case sl_bt_evt_gatt_server_attribute_value_id:
    {
      uint16_t attribute = evt->data.evt_gatt_server_attribute_value.attribute;
      uint8_t  *data     = evt->data.evt_gatt_server_attribute_value.value.data;
      uint8_t  data_len  = evt->data.evt_gatt_server_attribute_value.value.len;

      if (attribute == gattdb_buzzer_switch) {
        if (data_len >= 1) {
          buzzer_enabled = (data[0] != 0x00);
          /* 串口输出：蜂鸣器开关状态 */
          printf("[BLE] Buzzer switch : 0x%02X \r\n", (uint8_t)data[0]);
          if (buzzer_enabled) {
            buzzer_set_pwm(buzzer_freq, 0.5f);
          } else {
            buzzer_set_pwm(0, 0);
          }
        }
      } else if (attribute == gattdb_buzzer_frequecy) {
        if (data_len >= 1) {  // ★ 只要有数据就解析
          uint16_t new_freq = 0;
          int valid_digits = 0;
          for (int i = 0; i < data_len; i++) {
            if (data[i] >= '0' && data[i] <= '9') {
              new_freq = new_freq * 10 + (data[i] - '0');
              valid_digits++;
            }
            else if (data[i] == '\0') {
              break;
            }
          }
            /* 串口输出：解析后的频率值 */
          printf("[BLE] Freq ->  Hz:%d \r\n", new_freq);
          
          if (valid_digits > 0 && new_freq >= 500 && new_freq <= 8000) {
            buzzer_freq = new_freq;
            if (buzzer_enabled) {
              buzzer_set_pwm(buzzer_freq, 0.5f);
            }
          }
        }
      }
      break;
    }

    default:
      break;
  }
}
