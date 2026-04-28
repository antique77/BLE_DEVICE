#include "em_gpio.h"
#include "em_cmu.h"
#include "em_timer.h"
#include "pin_config.h"
#include "bsp_pwm.h"


void buzzer_timer_init(void)
{
  CMU_ClockEnable(cmuClock_GPIO, true);
  CMU_ClockEnable(cmuClock_TIMER1, true);

  GPIO_PinModeSet(BUZZER_PORT, BUZZER_PIN, gpioModePushPull, 0);

  TIMER_InitCC_TypeDef cc_init = TIMER_INITCC_DEFAULT;
  cc_init.mode      = timerCCModePWM;
  cc_init.cmoa      = timerOutputActionToggle;
  cc_init.cofoa     = timerOutputActionNone;
  cc_init.cufoa     = timerOutputActionNone;
  cc_init.edge      = timerEdgeBoth;
  cc_init.outInvert = false;
  TIMER_InitCC(TIMER1, 0, &cc_init);

  GPIO->TIMERROUTE[1].CC0ROUTE =
      ((uint32_t)BUZZER_PORT << _GPIO_TIMER_CC0ROUTE_PORT_SHIFT)
    | ((uint32_t)BUZZER_PIN  << _GPIO_TIMER_CC0ROUTE_PIN_SHIFT);

  // ★ 直接50%占空比, 上电就有PWM输出
  uint32_t top_val = (19000000UL / 2500UL) - 1UL;   // 2500Hz
  TIMER_TopSet(TIMER1, top_val);
  TIMER_CompareSet(TIMER1, 0, top_val / 2);           // 50%

  TIMER_Init_TypeDef timer_init = TIMER_INIT_DEFAULT;
  timer_init.prescale = timerPrescale1;
  timer_init.enable   = true;
  TIMER_Init(TIMER1, &timer_init);

  GPIO->TIMERROUTE[1].ROUTEEN |= GPIO_TIMER_ROUTEEN_CC0PEN;

}

void buzzer_set_pwm(uint16_t freq, float duty)
{
  if (freq == 0 || duty <= 0.0f) {
    // 关闭: 断开Timer到GPIO的路由, 引脚恢复为普通GPIO输出低电平
    GPIO->TIMERROUTE[1].ROUTEEN &= ~GPIO_TIMER_ROUTEEN_CC0PEN;
    GPIO_PinOutClear(BUZZER_PORT, BUZZER_PIN);
    return;
  }

  // 开启: 确保路由连接, 设置频率和占空比 (用Set非缓冲版本, 立即生效)
  GPIO->TIMERROUTE[1].ROUTEEN |= GPIO_TIMER_ROUTEEN_CC0PEN;

  uint32_t clk_freq = 19000000UL;           // 19MHz (38MHz/prescale÷2)
  uint32_t top_val  = (clk_freq / freq) - 1;
  TIMER_TopSet(TIMER1, top_val);

  uint8_t percent = (uint8_t)(duty * 100.0f);
  if (percent > 100) percent = 100;
  uint32_t cmp_val = ((uint32_t)top_val * percent) / 100;
  TIMER_CompareSet(TIMER1, 0, cmp_val);
}


