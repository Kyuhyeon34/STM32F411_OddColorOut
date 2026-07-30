#include "device_driver.h"

void LED_Init(void)
{
	/* 아래 코드 수정 금지 : Port-A Clock Enable */
	Macro_Set_Bit(RCC->AHB1ENR, 0); 

	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 10);
	Macro_Clear_Bit(GPIOA->OTYPER, 5);
	Macro_Clear_Bit(GPIOA->ODR, 5); 
}

void LED_On(void)
{
	Macro_Set_Bit(GPIOA->ODR, 5); 
}

void LED_Off(void)
{
	Macro_Clear_Bit(GPIOA->ODR, 5); 
}

void LED_Display(int on)
{
	Macro_Write_Block(GPIOA->ODR, 0x1, on & 0x1, 5);
}

/* 노랑 LED = PB1 (STATE_STOPPING 표시) */

void LED_Yellow_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);				// Port-B Clock On

	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 2);	// PB1 => Output
	Macro_Clear_Bit(GPIOB->OTYPER, 1);
	Macro_Clear_Bit(GPIOB->ODR, 1);
}

void LED_Yellow_Display(int on)
{
	Macro_Write_Block(GPIOB->ODR, 0x1, on & 0x1, 1);
}

/* 빨강 LED = PB2 (STATE_LANE_DEPARTURE / STATE_AEB 표시) */

void LED_Red_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 1);				// Port-B Clock On

	Macro_Write_Block(GPIOB->MODER, 0x3, 0x1, 4);	// PB2 => Output
	Macro_Clear_Bit(GPIOB->OTYPER, 2);
	Macro_Clear_Bit(GPIOB->ODR, 2);
}

void LED_Red_Display(int on)
{
	Macro_Write_Block(GPIOB->ODR, 0x1, on & 0x1, 2);
}