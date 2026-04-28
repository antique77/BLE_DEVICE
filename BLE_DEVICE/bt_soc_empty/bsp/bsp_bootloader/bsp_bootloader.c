/**
 * @file bsp_bootloader.c
 * @brief Gecko Bootloader 软件触发模块实现
 *
 * ★ 双模式接收架构:
 *   优先从中断驱动的环形缓冲区读取数据,
 *   若缓冲区为空则 fallback 到硬件轮询方式 (应对低功耗模式下中断阻塞).
 */

#include "bsp_bootloader.h"
#include "./bsp/bsp_uart/bsp_uart.h"

#include "em_device.h"   /* NVIC_SystemReset */
#include "em_usart.h"    /* USART_StatusGet, USART0->RXDATA */
#include <string.h>
#include <stdio.h>        /* printf */

/** 串口命令接收缓冲区 */
static uint8_t  bl_cmd_buf[BL_CMD_BUFFER_SIZE];
static uint16_t bl_cmd_len  = 0;

/**
 * 软件进入 Gecko Bootloader 升级模式
 *
 * 原理: 向 RAM[0x20000000] 写入 BootloaderResetCause_t 结构体,
 *       然后执行软复位。BL 启动时检测到此标志, 停留在 XMODEM 升级模式。
 *
 * BootloaderResetCause_t (小端序):
 *   [0x20000000] reason    = 0x0202
 *   [0x20000002] signature = 0xF00F
 *   合并为 uint32: 0xF00F0202
 */
void enter_bootloader(void)
{
    volatile uint32_t *reset_cause = (volatile uint32_t *)0x20000000UL;

    /* 写入复位原因 + 有效签名 */
    *reset_cause = (((uint32_t)BOOTLOADER_RESET_SIGNATURE_VALID << 16)
                  | (uint32_t)BOOTLOADER_RESET_REASON_BOOTLOAD);

    /* 提示用户(通过 printf 重定向到 USART0 输出) */
    printf("\r\n[BL] Entering bootloader upgrade mode...\r\n");
    printf("[BL] Device will reset and wait for XMODEM...\r\n");

    /*
     * ★ 关键：等待 USART0 TX FIFO 全部发送完成
     *
     * USART_Tx()/printf() 是非阻塞的，数据只放入 TX FIFO 就返回。
     * 如果不等 TX 完成就直接 NVIC_SystemReset()，
     * 上面的提示信息可能只发了一半就被截断。
     *
     * TXC (Transmit Complete) 标志位表示：
     *   - TX FIFO 已空
     *   - 最后一个字节已从移位寄存器发出
     */
    while (!(USART0->STATUS & USART_STATUS_TXC)) {
        /* 等待发送完成，不做其他事 */
    }

    /* 清除 TXC 中断标志（通过 IFC 寄存器，STATUS 是只读的） */
    USART_IntClear(USART0, USART_IF_TXC);

    /* 系统软复位 → BL 将接管控制权 */
    NVIC_SystemReset();
}

/**
 * 处理单个接收字符: 回显 + 命令检测
 * @return true=需要进入bootloader(不返回), false=继续处理
 */
static bool process_rx_char(uint8_t ch)
{
    /* 回显字符(方便终端操作) */
    USART_Tx(USART0, ch);

    /* 缓冲区未溢出则存入 */
    if (bl_cmd_len < BL_CMD_BUFFER_SIZE - 1) {
        bl_cmd_buf[bl_cmd_len++] = ch;
    } else {
        /* 缓冲区满, 直接重置 */
        bl_cmd_len = 0;
        return false;
    }

    /* 检测行结束符 (\r 或 \n) */
    if (ch == '\r' || ch == '\n') {
        /* 匹配 "boot" / "BOOT" 指令 (大小写均可) */
        if (bl_cmd_len >= 4 &&
            ((bl_cmd_buf[0] == 'b' || bl_cmd_buf[0] == 'B')) &&
            ((bl_cmd_buf[1] == 'o' || bl_cmd_buf[1] == 'O')) &&
            ((bl_cmd_buf[2] == 'o' || bl_cmd_buf[2] == 'O')) &&
            ((bl_cmd_buf[3] == 't' || bl_cmd_buf[3] == 'T')))
        {
            printf("\r\n[BL] Command received! Jumping to bootloader...\r\n");
            enter_bootloader();  /* 不返回 */
            return true;         /* 实际不会执行到这里 */
        }

        /* 不是 boot 命令, 清空缓冲区等待下一次输入 */
        bl_cmd_len = 0;
    }
    return false;
}

/**
 * 非阻塞方式处理 USART0 接收数据（双模式：中断 + 轮询 fallback）
 *
 * 在主循环中每周期调用一次, 功能:
 *   1. 优先从环形缓冲区(中断驱动)读取已接收的字节
 *   2. 若缓冲区空 → fallback: 直接轮询 USART0 硬件 RXDATAV
 *   3. 回显每个字符到终端
 *   4. 检测 "boot<CR/LF>" 命令 (大小写不敏感)
 *   5. 匹配成功则进入 bootloader 升级模式
 */
void uart_cmd_poll(void)
{
    uint8_t ch;

    /* 方式1: 从中断驱动的环形缓冲区读取 */
    while (uart_read_byte(&ch)) {
        if (process_rx_char(ch)) {
            return;  /* 进入 bootloader, 不返回 */
        }
    }

    /* 方式2: 硬件轮询 Fallback
     *
     * 当系统中断被暂时屏蔽时, ISR 可能延迟触发,
     * 此时直接从硬件寄存器轮询读取确保不丢失数据.
     */
    while (USART_StatusGet(USART0) & USART_STATUS_RXDATAV) {
        uint32_t raw = USART0->RXDATA;

        /* 检查帧错误/校验错误 */
        if (raw & (_USART_RXDATAX_FERR_MASK | _USART_RXDATAX_PERR_MASK)) {
            continue;
        }

        ch = (uint8_t)(raw & 0xFF);

        if (process_rx_char(ch)) {
            return;  /* 进入 bootloader, 不返回 */
        }
    }
}
