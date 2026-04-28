#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#include "em_usart.h"

#define UART_RX_BUFFER_SIZE 128

/* UART 初始化 */
void UART_Config(uint32_t baudrate);

/* 发送函数 */
uint8_t  UART_SendByte(USART_TypeDef *USARTx, uint8_t byte);
uint16_t UART_SendDoubleByte(USART_TypeDef *USARTx, uint16_t Double_byte);
void     UART_Send_String(USART_TypeDef *USARTx, uint8_t *string, uint32_t size);

/* 接收函数（从中断缓冲区读取，兼容旧接口） */
void     UART_ReceiveByte(USART_TypeDef *USARTx, uint8_t* byte);
uint32_t UART_ReceiveBuffer(USART_TypeDef *USARTx, uint8_t* buffer);

/* printf 重定向初始化 */
int UART_Printf_Init(void);

/*
 * 非阻塞回显（主循环调用）
 * 返回本次处理的字节数
 */
uint16_t uart_echo(void);

/* 查询接收缓冲区可用数据量 */
uint16_t uart_rx_available(void);

/*
 * 从环形缓冲区安全读取一个字节
 * 返回 true=成功读取, false=缓冲区空
 */
bool uart_read_byte(uint8_t *byte);

/* 调试：打印 ISR 统计信息 (触发次数/字节数/溢出/错误) */
void uart_debug_print_stats(void);

#endif
