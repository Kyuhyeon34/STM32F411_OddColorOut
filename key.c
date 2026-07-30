#include "device_driver.h"

void Key_Poll_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 2); 
	Macro_Write_Block(GPIOC->MODER, 0x3, 0x0, 26);
}

int Key_Get_Pressed(void)
{
	return Macro_Check_Bit_Clear(GPIOC->IDR, 13);	
}

void Key_Wait_Key_Pressed(void)
{
	while(!Macro_Check_Bit_Clear(GPIOC->IDR, 13));
}

void Key_Wait_Key_Released(void)
{
	while(!Macro_Check_Bit_Set(GPIOC->IDR, 13));
}

void Key_ISR_Enable(int en)
{
	if(en)
	{
		Macro_Set_Bit(RCC->AHB1ENR, 2); 
		Macro_Write_Block(GPIOC->MODER, 0x3, 0x0, 26);

		Macro_Set_Bit(RCC->APB2ENR, 14); 
		Macro_Write_Block(SYSCFG->EXTICR[3], 0xf, 0x2, 4);

		Macro_Set_Bit(EXTI->FTSR, 13);
		EXTI->PR = 0x1 << 13;
		
		NVIC_ClearPendingIRQ((IRQn_Type)40);
		Macro_Set_Bit(EXTI->IMR, 13);
		NVIC_EnableIRQ((IRQn_Type)40);
	}

	else
	{
		NVIC_DisableIRQ((IRQn_Type)40);
	}
}

/* 색 선택 버튼 4개 : PC0/PC1/PC2/PC3 (내부 Pull-up, 눌리면 GND로 Low). PC13은 재시작 전용 */

void Buttons0123_ISR_Enable(int en)
{
	if(en)
	{
		Macro_Set_Bit(RCC->AHB1ENR, 2);					// Port-C Clock On
		Macro_Write_Block(GPIOC->MODER, 0xff, 0x00, 0);	// PC0~3 => Input
		Macro_Write_Block(GPIOC->PUPDR, 0xff, 0x55, 0);	// PC0~3 => Pull-up

		Macro_Set_Bit(RCC->APB2ENR, 14);
		Macro_Write_Block(SYSCFG->EXTICR[0], 0xffff, 0x2222, 0);	// EXTI0~3 => Port C

		Macro_Set_Area(EXTI->FTSR, 0xf, 0);				// Falling Edge (누르면 3.3V->0V)
		EXTI->PR = 0xf << 0;

		NVIC_ClearPendingIRQ((IRQn_Type)6);
		NVIC_ClearPendingIRQ((IRQn_Type)7);
		NVIC_ClearPendingIRQ((IRQn_Type)8);
		NVIC_ClearPendingIRQ((IRQn_Type)9);

		Macro_Set_Area(EXTI->IMR, 0xf, 0);

		NVIC_EnableIRQ((IRQn_Type)6);
		NVIC_EnableIRQ((IRQn_Type)7);
		NVIC_EnableIRQ((IRQn_Type)8);
		NVIC_EnableIRQ((IRQn_Type)9);
	}

	else
	{
		NVIC_DisableIRQ((IRQn_Type)6);
		NVIC_DisableIRQ((IRQn_Type)7);
		NVIC_DisableIRQ((IRQn_Type)8);
		NVIC_DisableIRQ((IRQn_Type)9);
	}
}
