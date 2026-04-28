/**
 * @file bsp_uart.c
 * @brief USART0 串口驱动 - 中断驱动接收
 *
 * 架构：
 * ┌──────────┐     ┌────────────────────┐     ┌─────────────┐
 * │ 外部设备  │────▶│ USART0 RXDATAV ISR │──▶│ 环形缓冲区   │
 * │(USB转串口)│     │ (自动采集数据)      │     │ (FIFO 128B) │
 * └──────────┘     └────────────────────┘     └──────┬──────┘
 *                                                  │ 主循环
 *                                                  ▼
 *                                         ┌─────────────────┐
 *                                         │ uart_echo()      │
 *                                         │ 非阻塞读+回显 TX  │
 *                                         └─────────────────┘
 */

#include <stdbool.h>
#include <stddef.h>
#include "sl_gpio.h"
#include "em_gpio.h"
#include "em_cmu.h"
#include "em_usart.h"
#include "em_core.h"
#include "pin_config.h"
#include "bsp_uart.h"

#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
#include "sl_power_manager.h"
#endif

#include <stdio.h>

/* ================================================================
 *                    环形缓冲区（Ring Buffer）
 * ================================================================ */
static uint8_t  rx_ring_buf[UART_RX_BUFFER_SIZE];
static volatile uint16_t rx_head = 0;  /* 写入位置 (ISR) */
static volatile uint16_t rx_tail = 0;  /* 读取位置 (主循环) */

/* ★ ISR 调试计数器（用于确认中断是否触发） */
static volatile uint32_t isr_rx_count    = 0;  /* RX ISR 总触发次数 */
static volatile uint32_t isr_rx_bytes    = 0;  /* 总接收字节数 */
static volatile uint32_t isr_rx_overrun  = 0;  /* 缓冲区溢出丢弃数 */
static volatile uint32_t isr_rx_errors   = 0;  /* 接收错误次数 */

/* 缓冲区中可读数据的字节数 */
static inline uint16_t ring_available(void)
{
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    uint16_t avail = (rx_head - rx_tail + UART_RX_BUFFER_SIZE) % UART_RX_BUFFER_SIZE;
    CORE_EXIT_CRITICAL();
    return avail;
}

/* 向缓冲区写入一个字节 (仅 ISR 调用) */
static inline void ring_push(uint8_t byte)
{
    uint16_t next = (rx_head + 1) % UART_RX_BUFFER_SIZE;
    if (next != rx_tail) {
        rx_ring_buf[rx_head] = byte;
        rx_head = next;
    } else {
        isr_rx_overrun++;  /* 缓冲区满，丢弃数据 */
    }
}

/* 从缓冲区读取一个字节，返回是否成功 (主循环调用) */
static inline bool ring_pop(uint8_t *byte)
{
    CORE_DECLARE_IRQ_STATE;
    CORE_ENTER_CRITICAL();
    if (rx_head == rx_tail) {
        CORE_EXIT_CRITICAL();
        return false;
    }
    *byte = rx_ring_buf[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUFFER_SIZE;
    CORE_EXIT_CRITICAL();
    return true;
}

void NVIC_UART_Config(void)
{
    NVIC_ClearPendingIRQ(USART0_RX_IRQn);
    /* 配置NVIC中断优先级分组 */
    NVIC_SetPriorityGrouping(2); //全局只需要配置一次

    // 设置 I2C0 中断：抢占优先级 1，子优先级 2
    uint32_t priority = NVIC_EncodePriority(2, 1, 3);
    /* 8. NVIC 配置 */

    NVIC_SetPriority(USART0_RX_IRQn, priority);  /* 高优先级 */

    NVIC_EnableIRQ(USART0_RX_IRQn);
}

/* ================================================================
 *                    UART 初始化配置
 *
 * 注意：不配置 GPIO_ExtIntConfig！
 *       GPIO 外部中断与 USART0 RX 在同一引脚上共存时，
 *       在 EFR32 BG22 上可能导致接收异常。
 *       低功耗唤醒通过其他方式实现。
 * ================================================================ */
void UART_Config(uint32_t baudrate)
{
    USART_InitAsync_TypeDef UART_InitStruct = USART_INITASYNC_DEFAULT;

    /* 1. 开启时钟 */
    CMU_ClockEnable(cmuClock_USART0, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    /* 2. 配置引脚 - RX 使用带滤波+上拉的输入模式 */
    GPIO_PinModeSet(UART_TX_PORT, UART_TX_PIN, gpioModePushPull, 1);
    GPIO_PinModeSet(UART_RX_PORT, UART_RX_PIN, gpioModeInputPull, 1);

    /* 3. 先清除路由, 再重新配置 (防止残留路由干扰) */
    GPIO->USARTROUTE[0].ROUTEEN = 0;

    /* 4. USART0 路由到物理引脚 (EFR32 BG22 Series 2 必须) */
    GPIO->USARTROUTE[0].TXROUTE =
        ((uint32_t)UART_TX_PORT << _GPIO_USART_TXROUTE_PORT_SHIFT)
      | ((uint32_t)UART_TX_PIN  << _GPIO_USART_TXROUTE_PIN_SHIFT);

    GPIO->USARTROUTE[0].RXROUTE =
        ((uint32_t)UART_RX_PORT << _GPIO_USART_RXROUTE_PORT_SHIFT)
      | ((uint32_t)UART_RX_PIN  << _GPIO_USART_RXROUTE_PIN_SHIFT);

    GPIO->USARTROUTE[0].ROUTEEN =
        (GPIO_USART_ROUTEEN_TXPEN | GPIO_USART_ROUTEEN_RXPEN);

    /* 5. 初始化 USART0 */
    UART_InitStruct.baudrate = baudrate;
    USART_InitAsync(USART0, &UART_InitStruct);

    /* 6. 使能收发 */
    USART_Enable(USART0, usartEnableTx | usartEnableRx);

    /* 7. 清空接收 FIFO 和所有挂起的中断标志 */
    USART_IntClear(USART0, USART_IF_TXC | USART_IF_TXBL
                          | USART_IF_RXDATAV | USART_IF_RXFULL
                          | USART_IF_RXOF | USART_IF_RXUF
                          | USART_IF_TXOF | USART_IF_TXUF
                          | USART_IF_PERR | USART_IF_FERR
                          | USART_IF_CCF  | USART_IF_MPAF);
    while (USART_StatusGet(USART0) & USART_STATUS_RXDATAV) {
        (void)USART0->RXDATA;
    }

    NVIC_UART_Config();  

    /* 8. ★ 使能 USART0 RXDATAV 中断 */
    USART_IntEnable(USART0, USART_IF_RXDATAV);


#if defined(SL_CATALOG_POWER_MANAGER_PRESENT)
    /*
     * ★ EM1 电源要求
     *
     * EM1 功耗约 50-100μA (vs EM0 ~150μA, EM2 ~1.5μA)
     * 在 EM1 下 USART0 时钟保持运行，RXDATAV 中断正常工作
     */
    sl_power_manager_add_em_requirement(SL_POWER_MANAGER_EM1);
#endif

    /* 重置调试计数器 */
    isr_rx_count   = 0;
    isr_rx_bytes   = 0;
    isr_rx_overrun = 0;
    isr_rx_errors  = 0;

    printf("[UART] Init OK: IRQ enabled, EM1 required\r\n");
}

/* ================================================================
 *                    发送函数
 * ================================================================ */
uint8_t UART_SendByte(USART_TypeDef *USARTx, uint8_t byte)
{
    USART_Tx(USARTx, byte);
    return byte;
}

uint16_t UART_SendDoubleByte(USART_TypeDef *USARTx, uint16_t Double_byte)
{
    USART_TxDouble(USARTx, Double_byte);
    return Double_byte;
}

void UART_Send_String(USART_TypeDef *USARTx, uint8_t *string, uint32_t size)
{
    uint32_t len = size;
    while (len--) {
        USART_Tx(USARTx, *string++);
    }
}

/* ================================================================
 *                    接收函数（从中断缓冲区读取）
 * ================================================================ */
void UART_ReceiveByte(USART_TypeDef *USARTx, uint8_t* byte)
{
    (void)USARTx;
    if (!ring_pop(byte)) {
        *byte = 0;
    }
}

uint32_t UART_ReceiveBuffer(USART_TypeDef *USARTx, uint8_t* buffer)
{
    (void)USARTx;
    uint32_t len = 0;
    while (len < UART_RX_BUFFER_SIZE && ring_pop(buffer)) {
        if (*buffer == '\0') break;
        buffer++;
        len++;
    }
    return len;
}

/* ================================================================
 *                  printf 重定向
 * ================================================================ */
__attribute__((used))
int _write(int file, const char *ptr, int len)
{
    (void)file;
    if (ptr != NULL && len > 0) {
        for (int i = 0; i < len; i++) {
            USART_Tx(USART0, (uint8_t)ptr[i]);
        }
    }
    return len;
}

int UART_Printf_Init(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    return 0;
}

/* ================================================================
 *               USART0 RX 中断服务程序 (ISR)
 * ================================================================ */
void USART0_RX_IRQHandler(void)
{
    uint32_t flags = USART_IntGet(USART0);
    isr_rx_count++;  /* ★ 触发计数 */

    if (flags & USART_IF_RXDATAV) {
        /* 批量读取所有可用数据 */
        while (USART_StatusGet(USART0) & USART_STATUS_RXDATAV) {
            uint32_t raw = USART0->RXDATA;  /* 读全部位含状态 */

            /* ★ 检查帧错误/校验错误 */
            if (raw & (_USART_RXDATAX_FERR_MASK | _USART_RXDATAX_PERR_MASK)) {
                isr_rx_errors++;
            }

            ring_push((uint8_t)(raw & 0xFF));
            isr_rx_bytes++;
        }
    }

    USART_IntClear(USART0, flags);
}

/* ================================================================
 *              非阻塞回显函数（主循环调用）
 * ================================================================ */
uint16_t uart_echo(void)
{
    uint8_t byte;
    uint16_t count = 0;

    while (ring_pop(&byte)) {
        USART_Tx(USART0, byte);
        count++;
    }

    return count;
}

/*
 * ★ 从环形缓冲区安全读取一个字节
 *
 * 返回 true=成功读取, false=缓冲区为空
 * 解决 UART_ReceiveByte() 返回 0 时无法区分"读到0x00"和"无数据"的问题
 */
bool uart_read_byte(uint8_t *byte)
{
    return ring_pop(byte);
}

uint16_t uart_rx_available(void)
{
    return ring_available();
}

/*
 * ★ 调试辅助：获取 ISR 统计信息
 *     可在 app_process_action() 或 GATT 写入中调用并打印，
 *     用于确认 USART0 RX 中断是否正常工作
 */
void uart_debug_print_stats(void)
{
    printf("[UART-D] ISR=%lu bytes=%lu overrun=%lu err=%lu avail=%u\r\n",
           (unsigned long)isr_rx_count,
           (unsigned long)isr_rx_bytes,
           (unsigned long)isr_rx_overrun,
           (unsigned long)isr_rx_errors,
           (unsigned)ring_available());
}
