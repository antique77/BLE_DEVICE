/**
 * @file bsp_bootloader.h
 * @brief Gecko Bootloader 软件触发模块
 * 
 * 功能:
 *   - 通过 USART0 接收特定串口指令触发进入 BL 升级模式
 *   - 提供 enter_bootloader() 接口供其他代码调用
 *   - 提供 uart_cmd_poll() 在主循环中非阻塞轮询
 *
 * 使用方法:
 *   1. 初始化: 无需初始化(自动清零)
 *   2. 主循环调用: uart_cmd_poll();
 *   3. 或直接调用:  enter_bootloader();
 */

#ifndef __BSP_BOOTLOADER_H__
#define __BSP_BOOTLOADER_H__

#include <stdint.h>

/** Bootloader 复位原因常量 (来自 SDK 的 btl_reset_info.h) */
#define BOOTLOADER_RESET_REASON_BOOTLOAD      0x0202u
#define BOOTLOADER_RESET_SIGNATURE_VALID      0xF00Fu

/** 串口命令接收缓冲区大小 */
#ifndef BL_CMD_BUFFER_SIZE
#define BL_CMD_BUFFER_SIZE    32
#endif

/**
 * 软件进入 Gecko Bootloader 升级模式 (XMODEM)
 *
 * 原理: 向 RAM[0x20000000] 写入 BootloaderResetCause_t 结构体:
 *   [0] reason    = 0x0202 (BOOTLOAD)
 *   [2] signature = 0xF00F (VALID)
 * 然后执行软复位, BL 启动时检测到此标志后停留在 XMODEM 模式等待升级.
 *
 * 注意: 此函数不返回!
 */
void enter_bootloader(void);

/**
 * 非阻塞轮询 USART0, 检测串口指令
 *
 * 支持的命令:
 *   "boot\r" / "boot\n" / "BOOT\r" / "BOOT\n"
 *
 * 匹配成功后自动调用 enter_bootloader() 进入 BL 模式.
 *
 * 需要在主循环 (app_process_action) 中每周期调用一次。
 */
void uart_cmd_poll(void);

#endif /* __BSP_BOOTLOADER_H__ */
