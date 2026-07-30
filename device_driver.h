#include "stm32f4xx.h"
#include "option.h"
#include "macro.h"
#include "malloc.h"

// Uart.c

extern void Uart2_Init(int baud);
extern void Uart1_Init(int baud);
extern void Uart1_Send_Byte(char data);
extern char Uart1_Get_Char(void);
extern char Uart1_Get_Pressed(void);
extern void Uart2_RX_Interrupt_Enable(int en);
extern volatile char Uart2_RX_Data;
extern volatile int Uart2_RX_Flag;

// SysTick.c

extern void SysTick_Run(unsigned int msec);
extern int SysTick_Check_Timeout(void);
extern unsigned int SysTick_Get_Time(void);
extern unsigned int SysTick_Get_Load_Time(void);
extern void SysTick_Stop(void);

// Led.c

extern void LED_Init(void);
extern void LED_On(void);
extern void LED_Off(void);
extern void LED_Display(int on);
extern void LED_Yellow_Init(void);
extern void LED_Yellow_Display(int on);
extern void LED_Red_Init(void);
extern void LED_Red_Display(int on);

// Clock.c

extern void Clock_Init(void);

// Key.c

extern void Key_Poll_Init(void);
extern int Key_Get_Pressed(void);
extern void Key_Wait_Key_Released(void);
extern void Key_Wait_Key_Pressed(void);
extern void Key_ISR_Enable(int en);
extern volatile int Key_Pressed_Flag;	/* PC13 : 재시작 전용 */

/* 색 선택 버튼 4개 : PC0/PC1/PC2/PC3 (EXTI0~3) */
extern void Buttons0123_ISR_Enable(int en);
extern volatile int Btn0_Flag, Btn1_Flag, Btn2_Flag, Btn3_Flag;

// Timer.c

extern void TIM2_Delay(int time);
extern void TIM2_Stopwatch_Start(void);
extern unsigned int TIM2_Stopwatch_Stop(void);
extern void TIM4_Repeat(int time);
extern int TIM4_Check_Timeout(void);
extern void TIM4_Stop(void);
extern void TIM4_Change_Value(int time);
extern void TIM3_Out_Init(void);
extern void TIM3_Out_Freq_Generation(unsigned short freq);
extern void TIM3_Out_Stop(void);
extern void TIM4_Repeat_Interrupt_Enable(int en, int time);
extern volatile int TIM4_Expired;
extern void TIM4_Delay(int time);

// ws2812.c (PA0, GPIO 비트뱅 + DWT 사이클카운터 방식)

extern void WS2812_Init(void);
extern void WS2812_Set_Pixel(int idx, unsigned char r, unsigned char g, unsigned char b);
extern void WS2812_Show(void);

// i2c.c

#define SC16IS752_IODIR				0x0A
#define SC16IS752_IOSTATE			0x0B

extern void I2C1_SC16IS752_Init(unsigned int freq);
extern void I2C1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void I2C1_SC16IS752_Config_GPIO(unsigned int config);
extern void I2C1_SC16IS752_Write_GPIO(unsigned int data);

// spi.c

extern void SPI1_SC16IS752_Init(unsigned int div);
extern void SPI1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data);
extern void SPI1_SC16IS752_Config_GPIO(unsigned int config);
extern void SPI1_SC16IS752_Write_GPIO(unsigned int data);

// Adc.c

extern void ADC1_IN6_Init(void);
extern void ADC1_Start(void);
extern void ADC1_Stop(void);
extern int ADC1_Get_Status(void);
extern int ADC1_Get_Data(void);

// DMA

extern void DMA2_Stream0_M2M_Init(void);
extern void DMA2_Stream0_M2M_Start(void * src_addr, void * dst_addr, int num);
extern void DMA1_Stream6_USART2_TX_Satrt(void * src_addr, int num);