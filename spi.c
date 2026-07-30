#include "device_driver.h"

/* SPI1-SCK: PB3, SPI1-MISO: PB4, SPI1-MOSI: PB5, nCS: PA8 */

void SPI1_SC16IS752_Init(unsigned int div)
{
	/* __builtin_ctz 함수 : 컴파일러 빌트인 함수, 0번 비트부터 0의 개수를 반환 */

	int n = (__builtin_ctz(div) & 0x7) - 1; 
	volatile int i;

	Macro_Set_Bit(RCC->AHB1ENR, 0); 					// Port-A Clock On
	Macro_Set_Bit(RCC->AHB1ENR, 1); 					// Port-B Clock On
	Macro_Set_Bit(RCC->APB2ENR, 12); 					// SPI1 Clock On

	Macro_Clear_Bit(RCC->APB2RSTR, 12); 				// SPI1 Reset
	Macro_Set_Bit(RCC->APB2RSTR, 12);
	for(i = 0; i < 1000; i++);
	Macro_Clear_Bit(RCC->APB2RSTR, 12);

	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 16);		// PA8(nCS) => GP-Out
	Macro_Clear_Bit(GPIOA->OTYPER, 8);					// PA8(nCS) => Push-Pull
	Macro_Set_Bit(GPIOA->ODR, 8); 						// PA8(nCS) => High

	Macro_Write_Block(GPIOB->MODER, 0x3f, 0x2a, 6);  	// PB[5:3] => ALT
	Macro_Write_Block(GPIOB->AFR[0], 0xfff, 0x555, 12); // PB[5:3] => AF05
	Macro_Write_Block(GPIOB->OTYPER, 0x7, 0x0, 6); 		// PB[5:3] => Push-Pull
	Macro_Write_Block(GPIOB->OSPEEDR, 0x3f, 0x2a, 12); 	// PB[5:3] => Fast Speed

	SPI1->CR2 = (0<<4)|(1<<2);
	SPI1->CR1 = (1<<11)|(0<<10)|(0<<9)|(0<<8)|(0<<7)|(n<<3)|(1<<2)|(0<<1)|(0<<0);
	Macro_Set_Bit(SPI1->CR1, 6);
}

#define SPI1_CS_HIGH()		(Macro_Set_Bit(GPIOA->ODR, 8))
#define SPI1_CS_LOW()		(Macro_Clear_Bit(GPIOA->ODR, 8))

void SPI1_SC16IS752_Write_Reg(unsigned int addr, unsigned int data)
{
	SPI1_CS_HIGH();
	SPI1_CS_LOW();

	SPI1->DR = (0 << 15) | ((addr & 0xf) << 11) | (data & 0xff);
	while(Macro_Check_Bit_Clear(SPI1->SR, 1));
	while(Macro_Check_Bit_Set(SPI1->SR, 7));

	SPI1_CS_HIGH();
}

void SPI1_SC16IS752_Config_GPIO(unsigned int config)
{
	SPI1_SC16IS752_Write_Reg(SC16IS752_IODIR, config);
}

void SPI1_SC16IS752_Write_GPIO(unsigned int data)
{
	SPI1_SC16IS752_Write_Reg(SC16IS752_IOSTATE, data);
}
