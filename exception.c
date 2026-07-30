#include "device_driver.h"
#include <stdio.h>

void _Invalid_ISR(void)
{
	unsigned int r = Macro_Extract_Area(SCB->ICSR, 0x1ff, 0);
	printf("\nInvalid_Exception: %d!\n", r);
	printf("Invalid_ISR: %d!\n", r - 16);
	for(;;);
}

volatile int TIM4_Expired = 0;

void TIM4_IRQHandler(void)
{
	Macro_Clear_Bit(TIM4->SR, 0);
	NVIC_ClearPendingIRQ(30);
	TIM4_Expired = 1;
}

volatile int DMA2_STREAM0_DONE = 0;

void DMA2_Stream0_IRQHandler(void)
{
	DMA2->LIFCR = 0x3F << 0;
	NVIC_ClearPendingIRQ(56);
    DMA2_STREAM0_DONE = 1;
}

volatile int DMA1_STREAM6_DONE = 0;

void DMA1_Stream6_IRQHandler(void)
{
	DMA1->HIFCR = 0x3F << 0;
	NVIC_ClearPendingIRQ(15);
    DMA1_STREAM6_DONE = 1;
}

volatile int Btn0_Flag = 0;	// PC0 : 1번 색 선택
volatile int Btn1_Flag = 0;	// PC1 : 2번 색 선택
volatile int Btn2_Flag = 0;	// PC2 : 3번 색 선택

void EXTI0_IRQHandler(void)
{
	EXTI->PR = 0x1 << 0;
	NVIC_ClearPendingIRQ(6);
	Btn0_Flag = 1;
}

void EXTI1_IRQHandler(void)
{
	EXTI->PR = 0x1 << 1;
	NVIC_ClearPendingIRQ(7);
	Btn1_Flag = 1;
}

void EXTI2_IRQHandler(void)
{
	EXTI->PR = 0x1 << 2;
	NVIC_ClearPendingIRQ(8);
	Btn2_Flag = 1;
}

volatile int Btn3_Flag = 0;	// PC3 : 4번 색 선택

void EXTI3_IRQHandler(void)
{
	EXTI->PR = 0x1 << 3;
	NVIC_ClearPendingIRQ(9);
	Btn3_Flag = 1;
}

volatile int Key_Pressed_Flag = 0;	// PC13 : 재시작 전용 버튼 인터럽트 플래그

void EXTI15_10_IRQHandler(void)
{
	if(Macro_Check_Bit_Set(EXTI->PR, 13))
	{
		EXTI->PR = 0x1 << 13;
		NVIC_ClearPendingIRQ(40);
		Key_Pressed_Flag = 1;
	}
}

volatile char Uart2_RX_Data = 0;	// TeraTerm에서 수신한 문자(예: Spacebar)
volatile int Uart2_RX_Flag = 0;

void USART2_IRQHandler(void)
{
	if(Macro_Check_Bit_Set(USART2->SR, 5))
	{
		Uart2_RX_Data = (char)USART2->DR;
		Uart2_RX_Flag = 1;
	}

	NVIC_ClearPendingIRQ(38);
}