#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// GPIO SWCLK on PA01
#ifndef GPIO_SWCLK_PORT                         
#define GPIO_SWCLK_PORT                          SL_GPIO_PORT_A
#endif
#ifndef GPIO_SWCLK_PIN                          
#define GPIO_SWCLK_PIN                           1
#endif

// GPIO SWDIO on PA02
#ifndef GPIO_SWDIO_PORT                         
#define GPIO_SWDIO_PORT                          SL_GPIO_PORT_A
#endif
#ifndef GPIO_SWDIO_PIN                          
#define GPIO_SWDIO_PIN                           2
#endif

// GPIO SWV on PA03
#ifndef GPIO_SWV_PORT                           
#define GPIO_SWV_PORT                            SL_GPIO_PORT_A
#endif
#ifndef GPIO_SWV_PIN                            
#define GPIO_SWV_PIN                             3
#endif

// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// TIMER1 CC0 on PC05
#ifndef TIMER1_CC0_PORT                         
#define TIMER1_CC0_PORT                          SL_GPIO_PORT_C
#endif
#ifndef TIMER1_CC0_PIN                          
#define TIMER1_CC0_PIN                           5
#endif

// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[USART0]
// USART0 RX on PA06
#ifndef USART0_RX_PORT                          
#define USART0_RX_PORT                           SL_GPIO_PORT_A
#endif
#ifndef USART0_RX_PIN                           
#define USART0_RX_PIN                            6
#endif

// USART0 TX on PA05
#ifndef USART0_TX_PORT                          
#define USART0_TX_PORT                           SL_GPIO_PORT_A
#endif
#ifndef USART0_TX_PIN                           
#define USART0_TX_PIN                            5
#endif

// [USART0]$

// $[USART1]
// [USART1]$

// $[I2C1]
// [I2C1]$

// $[PDM]
// [PDM]$

// $[LETIMER0]
// [LETIMER0]$

// $[IADC0]
// [IADC0]$

// $[I2C0]
// I2C0 SCL on PB01
#ifndef I2C0_SCL_PORT                           
#define I2C0_SCL_PORT                            SL_GPIO_PORT_B
#endif
#ifndef I2C0_SCL_PIN                            
#define I2C0_SCL_PIN                             1
#endif

// I2C0 SDA on PB00
#ifndef I2C0_SDA_PORT                           
#define I2C0_SDA_PORT                            SL_GPIO_PORT_B
#endif
#ifndef I2C0_SDA_PIN                            
#define I2C0_SDA_PIN                             0
#endif

// [I2C0]$

// $[EUART0]
// [EUART0]$

// $[PTI]
// PTI DFRAME on PD03
#ifndef PTI_DFRAME_PORT                         
#define PTI_DFRAME_PORT                          SL_GPIO_PORT_D
#endif
#ifndef PTI_DFRAME_PIN                          
#define PTI_DFRAME_PIN                           3
#endif

// PTI DOUT on PD02
#ifndef PTI_DOUT_PORT                           
#define PTI_DOUT_PORT                            SL_GPIO_PORT_D
#endif
#ifndef PTI_DOUT_PIN                            
#define PTI_DOUT_PIN                             2
#endif

// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef SWCLK_PORT                              
#define SWCLK_PORT                               SL_GPIO_PORT_A
#endif
#ifndef SWCLK_PIN                               
#define SWCLK_PIN                                1
#endif

#ifndef SWDIO_PORT                              
#define SWDIO_PORT                               SL_GPIO_PORT_A
#endif
#ifndef SWDIO_PIN                               
#define SWDIO_PIN                                2
#endif

#ifndef SWV_PORT                                
#define SWV_PORT                                 SL_GPIO_PORT_A
#endif
#ifndef SWV_PIN                                 
#define SWV_PIN                                  3
#endif

#ifndef UART_TX_PORT                            
#define UART_TX_PORT                             SL_GPIO_PORT_A
#endif
#ifndef UART_TX_PIN                             
#define UART_TX_PIN                              5
#endif

#ifndef UART_RX_PORT                            
#define UART_RX_PORT                             SL_GPIO_PORT_A
#endif
#ifndef UART_RX_PIN                             
#define UART_RX_PIN                              6
#endif

#ifndef I2C_SDA_PORT                            
#define I2C_SDA_PORT                             SL_GPIO_PORT_B
#endif
#ifndef I2C_SDA_PIN                             
#define I2C_SDA_PIN                              0
#endif

#ifndef I2C_SCL_PORT                            
#define I2C_SCL_PORT                             SL_GPIO_PORT_B
#endif
#ifndef I2C_SCL_PIN                             
#define I2C_SCL_PIN                              1
#endif

#ifndef BUZZER_PORT                             
#define BUZZER_PORT                              SL_GPIO_PORT_C
#endif
#ifndef BUZZER_PIN                              
#define BUZZER_PIN                               5
#endif

// [CUSTOM_PIN_NAME]$


#endif // PIN_CONFIG_H


