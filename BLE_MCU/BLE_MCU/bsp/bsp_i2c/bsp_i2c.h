#ifndef __BSP_I2C_H__
#define __BSP_I2C_H__   

#include "em_i2c.h"
#include "sl_gpio.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "pin_config.h"

#define I2C_SLAVE_ADDRESS 0xA6
#define REG_DEVICE_ID     0x00    /* [R] 设备ID = 0xE3 */
#define REG_VERSION       0x01    /* [R] 固件版本 V1.0 = 0x10 */
#define REG_ECHO          0x02    /* [R/W] 回显寄存器 (测试用) */
#define REG_COUNTER       0x03    /* [R] 通信计数器 */
#define REG_TOTAL_COUNT   4       /* 寄存器总数 */

/* ---- 固定值 ---- */
#define DEVICE_ID_VAL     0xE3
#define FW_VER_VAL        0x10    /* V1.0 */

void I2C_Config(void);
void NVIC_Config(void);


// 新增的函数声明
static void handle_address_match(void);
static void handle_receive_data(void);
static void handle_transmit_complete(void);
static void handle_stop_condition(void);

bool i2c_prepare_tx_data(const uint8_t *data, uint16_t length);
uint16_t i2c_get_rx_data(uint8_t *buffer, uint16_t max_length);
bool i2c_is_data_available(void);

// 可选：回调函数类型定义
typedef void (*i2c_rx_callback_t)(uint8_t *data, uint16_t length);
void i2c_set_rx_callback(i2c_rx_callback_t callback);
/* ---- 协议层函数声明 ---- */
void i2c_protocol_init(void);      /* 初始化寄存器默认值 */
static void protocol_prepare_tx(uint8_t reg);
static void protocol_handle_write(uint8_t reg, uint8_t val);
#endif
