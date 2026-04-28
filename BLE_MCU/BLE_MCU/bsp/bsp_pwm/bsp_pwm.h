#ifndef __BSP_PWM_H__
#define __BSP_PWM_H__   

#include "em_timer.h"   // TIMER 外设驱动
#include "pin_config.h" // 引脚宏定义（BUZZER_PORT, BUZZER_PIN）

void buzzer_timer_init(void);
void buzzer_set_pwm(uint16_t freq, float duty);


#endif
