#ifndef __TIMER_H__
#define __TIMER_H__

#define TIM4_TICK (20U)                   // 틱 단위: us
#define TIM4_FREQ (1000000.0 / TIM4_TICK) // 카운트 주파수 (Hz)
#define TIM4_1MS_PLS (TIM4_FREQ / 1000.0) // 1ms당 카운트 펄스 수
#define TIM4_MAX_PLS (0xFFFFU)            // 16비트 타이머 최대 값

#define TIM5_PWM_FREQ (20000U)               // 목표 PWM 주파수 (10kHz)
#define TIM5_CNT_FREQ (TIM5_PWM_FREQ * 100U) // 카운트 클럭 (목표 주파수 * 100)

/* 모든 타이머 모듈 일괄 초기화 */
void Timer_Init(void);

/* TIM2: 스위치 롱클릭 타임아웃용 타이머 */
void TIM2_Init(void);
void TIM2_Start(int delay_ms);
int TIM2_Check_Timeout(void);
void TIM2_Stop(void);

/* TIM4: 블로킹 방식 Delay 타이머 */
void TIM4_Init(void);
void TIM4_Delay(int delay_ms);
void TIM4_Tick_Increase(void);
unsigned int TIM4_Get_Tick(void);

/* TIM5: 모터 PWM 제어 타이머 */
void TIM5_Init(void);
void TIM5_Set_Duty(int ch1_duty, int ch2_duty);
void TIM5_Stop(void);

#endif /* __TIMER_H__ */