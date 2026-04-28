#include "bsp_i2c.h"
#include <string.h>    /* 需要 memcpy (如果尚未包含) */

// 接收缓冲区
#define I2C_RX_BUFFER_SIZE 256
static uint8_t i2c_rx_buffer[I2C_RX_BUFFER_SIZE];
static volatile uint16_t i2c_rx_index = 0;
static volatile bool i2c_rx_in_progress = false;

// 发送缓冲区
#define I2C_TX_BUFFER_SIZE 256
static uint8_t i2c_tx_buffer[I2C_TX_BUFFER_SIZE];
static volatile uint16_t i2c_tx_index = 0;
static volatile uint16_t i2c_tx_length = 0;
static volatile bool i2c_tx_in_progress = false;

// 传输方向状态
static volatile bool i2c_read_request = false;  // 主机请求读操作
static volatile bool i2c_write_request = false; // 主机请求写操作

static volatile uint8_t  current_reg  = 0xFF;    /* 当前选中寄存器地址 */
static volatile uint8_t  reg_echo_val = 0x00;    /* 回显寄存器值 */
static volatile uint16_t comm_counter = 0;        /* 通信次数计数器 */
static volatile uint8_t  device_regs[REG_TOTAL_COUNT];  /* 寄存器镜像数组 */

void i2c_protocol_init(void)
{
    device_regs[REG_DEVICE_ID] = DEVICE_ID_VAL;
    device_regs[REG_VERSION]   = FW_VER_VAL;
    device_regs[REG_ECHO]      = 0x00;
    device_regs[REG_COUNTER]   = 0x00;
    reg_echo_val = 0x00;
    comm_counter = 0;
    printf("[I2C] Protocol init OK\r\n");
}


void NVIC_Config(void)
{
    /* 配置NVIC中断优先级分组 */
    NVIC_SetPriorityGrouping(2);

    // 设置 I2C0 中断：抢占优先级 1，子优先级 2
    uint32_t priority = NVIC_EncodePriority(2, 1, 2);
    /* 配置NVIC中断优先级 */
    NVIC_SetPriority(I2C0_IRQn, priority);

    NVIC_EnableIRQ(I2C0_IRQn);
}

void I2C_Config(void)
{
    /* 定义I2C初始化结构体 */
    I2C_Init_TypeDef I2C_InitStruct = I2C_INIT_DEFAULT;

    /* 开启I2C对应时钟源 */
    CMU_ClockEnable(cmuClock_I2C0, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    /* 配置I2C初始化 */
    I2C_InitStruct.enable = false;
    I2C_InitStruct.master = false;
    I2C_InitStruct.freq = I2C_FREQ_FAST_MAX;
    I2C_InitStruct.clhr = i2cClockHLRStandard;
    I2C_Init(I2C0, &I2C_InitStruct);

    // 设置从机地址
    I2C_SlaveAddressSet(I2C0, I2C_SLAVE_ADDRESS);

    /* 配置GPIO引脚为I2C模式 */
    GPIO_PinModeSet(I2C0_SCL_PORT,I2C0_SCL_PIN,gpioModeWiredAndPullUp,1);
    GPIO_PinModeSet(I2C0_SDA_PORT,I2C0_SDA_PIN,gpioModeWiredAndPullUp,1);

    GPIO->I2CROUTE[0].SCLROUTE = ((
        uint32_t)I2C0_SCL_PORT << _GPIO_I2C_SCLROUTE_PORT_SHIFT)
      | ((uint32_t)I2C0_SCL_PIN  << _GPIO_I2C_SCLROUTE_PIN_SHIFT);
    GPIO->I2CROUTE[0].SDAROUTE = ((
        uint32_t)I2C0_SDA_PORT << _GPIO_I2C_SDAROUTE_PORT_SHIFT)
      | ((uint32_t)I2C0_SDA_PIN  << _GPIO_I2C_SDAROUTE_PIN_SHIFT);

    /* 配置NVIC中断 */
    NVIC_Config();
    /* 使能I2C */
    I2C_Enable(I2C0, true);
    /* 使能I2C中断 */
    I2C_IntEnable(I2C0, I2C_IEN_ADDR | I2C_IEN_RXDATAV | I2C_IEN_TXC | I2C_IEN_SSTOP);
}

void I2C0_IRQHandler(void)
{
    // 1. 获取当前激活的中断标志
    uint32_t flags = I2C_IntGetEnabled(I2C0);
    
    // 2. 处理地址匹配中断（ADDR）
    if (flags & I2C_IF_ADDR) {
        I2C_IntClear(I2C0, I2C_IF_ADDR);
        handle_address_match();
    }
    
    // 3. 处理接收数据有效中断（RXDATAV）
    if (flags & I2C_IF_RXDATAV) {
        I2C_IntClear(I2C0, I2C_IF_RXDATAV);
        handle_receive_data();
    }
    
    // 4. 处理发送完成中断（TXC）
    if (flags & I2C_IF_TXC) {
        I2C_IntClear(I2C0, I2C_IF_TXC);
        handle_transmit_complete();
    }
    
    // 5. 处理从停止条件中断（SSTOP）
    if (flags & I2C_IF_SSTOP) {
        I2C_IntClear(I2C0, I2C_IF_SSTOP);
        handle_stop_condition();
    }
}

static void handle_address_match(void)
{
    // 读取状态寄存器判断传输方向
    uint32_t status = I2C0->STATUS;
    
    if (status & I2C_STATE_TRANSMITTER) {
        // 主机请求读取数据（从机发送模式）
        i2c_read_request = true;
        i2c_write_request = false;
        i2c_tx_in_progress = true;
        i2c_tx_index = 0;
        
        // 准备第一个要发送的数据字节
        if (i2c_tx_length > 0) {
            I2C0->TXDATA = i2c_tx_buffer[0];
            i2c_tx_index = 1;
        } else {
            // 如果没有数据要发送，发送0xFF
            I2C0->TXDATA = 0xFF;
        }
    } else {
        // 主机请求写入数据（从机接收模式）
        i2c_write_request = true;
        i2c_read_request = false;
        i2c_rx_in_progress = true;
        i2c_rx_index = 0;
    }
}

static void handle_receive_data(void)
{
    if (!i2c_write_request) return;
    
    // 从RXDATA寄存器读取数据
    uint8_t data = (uint8_t)(I2C0->RXDATA);
    
    // 检查缓冲区是否已满
    if (i2c_rx_index < I2C_RX_BUFFER_SIZE) {
        i2c_rx_buffer[i2c_rx_index++] = data;
    }
    
    // 可选：处理特定协议，如第一个字节为寄存器地址等
    /* ★ 协议解析: 第一个字节=目标寄存器地址 */
    if (i2c_rx_index == 1) {
        current_reg = i2c_rx_buffer[0];          /* 保存寄存器地址 */
        protocol_prepare_tx(current_reg);         /* 预填充发送缓冲区 */
    } else if (i2c_rx_index > 1) {
        protocol_handle_write(current_reg, i2c_rx_buffer[i2c_rx_index - 1]);
        /* 自动递增地址(支持连续写) */
        if (current_reg < REG_TOTAL_COUNT - 1) current_reg++;
    }
}

static void handle_transmit_complete(void)
{
    if (!i2c_read_request) return;
    
    // 检查是否还有数据要发送
    if (i2c_tx_index < i2c_tx_length) {
        // 发送下一个字节
        I2C0->TXDATA = i2c_tx_buffer[i2c_tx_index++];
    } else {
        // 所有数据已发送完毕
        i2c_tx_in_progress = false;
        // 可以发送0xFF或保持总线
        I2C0->TXDATA = 0xFF;
    }
}

static void handle_stop_condition(void)
{
    // 传输结束，重置状态
    i2c_read_request = false;
    i2c_write_request = false;
    i2c_rx_in_progress = false;
    i2c_tx_in_progress = false;
    
    // 通知应用程序有新数据到达（如果正在接收）
    if (i2c_rx_index > 0) {
        comm_counter++;                                   /* 计数+1 */
        device_regs[REG_COUNTER] = (uint8_t)(comm_counter & 0xFF);
        printf("[I2C] RX %d bytes, reg=0x%02X, cnt=%u\r\n",
               (int)i2c_rx_index, (int)i2c_rx_buffer[0],
               (unsigned int)comm_counter);
        i2c_rx_index = 0;
    }
}

bool i2c_prepare_tx_data(const uint8_t *data, uint16_t length)
{
    if (length > I2C_TX_BUFFER_SIZE || i2c_tx_in_progress) {
        return false;
    }
    
    memcpy(i2c_tx_buffer, data, length);
    i2c_tx_length = length;
    i2c_tx_index = 0;
    
    return true;
}

uint16_t i2c_get_rx_data(uint8_t *buffer, uint16_t max_length)
{
    uint16_t copy_length = i2c_rx_index;
    if (copy_length > max_length) {
        copy_length = max_length;
    }
    
    memcpy(buffer, i2c_rx_buffer, copy_length);
    return copy_length;
}

bool i2c_is_data_available(void)
{
    return (i2c_rx_index > 0);
}

/**
 * 根据寄存器地址预填充发送缓冲区
 * 当主机发起读操作时, 此函数决定从机返回什么数据
 */
static void protocol_prepare_tx(uint8_t reg)
{
    uint8_t tx_buf[I2C_TX_BUFFER_SIZE];
    int len = 0;

    /* 同步动态寄存器到镜像数组 */
    device_regs[REG_ECHO]    = reg_echo_val;
    device_regs[REG_COUNTER] = (uint8_t)(comm_counter & 0xFF);

    /* 从当前地址开始, 连续填充 TX 缓冲区 */
    for (int i = 0; i < REG_TOTAL_COUNT && len < I2C_TX_BUFFER_SIZE; i++) {
        if (reg + i < REG_TOTAL_COUNT) {
            tx_buf[len++] = device_regs[reg + i];
        } else {
            tx_buf[len++] = 0xFF;  /* 超范围返回0xFF */
        }
    }

    i2c_prepare_tx_data(tx_buf, (uint16_t)len);
}

/**
 * 处理主机写入操作
 */
static void protocol_handle_write(uint8_t reg, uint8_t val)
{
    switch (reg) {
    case REG_DEVICE_ID:
    case REG_VERSION:
    case REG_COUNTER:
        break;  /* 只读寄存器, 忽略写入 */

    case REG_ECHO:
        reg_echo_val = val;
        device_regs[REG_ECHO] = val;
        printf("[I2C] ECHO <- 0x%02X\r\n", val);
        break;

    default:
        break;  /* 未知寄存器, 忽略 */
    }
}
