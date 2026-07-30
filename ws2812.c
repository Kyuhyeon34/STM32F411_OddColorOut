#include "device_driver.h"

/*
 * WS2812B x4 (PA0 한 줄에 직렬 연결)
 *
 * PA0을 GPIO 출력으로 직접 토글하는 방식(bit-bang). DWT 사이클 카운터로 비트 타이밍을 맞춘다.
 * (원래 TIM5+DMA로 구현했으나 실기에서 신호가 전혀 안 나가는 문제가 있어, 실제 동작 검증된
 *  bit-bang 방식으로 교체함. WS2812_Init/Set_Pixel/Show 인터페이스는 그대로라 main.c는 무수정)
 *
 * 타이밍 (SYSCLK = 96MHz 가정, 1 cycle = 10.417ns)
 *  T0H 0.4us  -> 38 cycle
 *  T1H 0.85us -> 82 cycle
 *  1비트 주기 1.25us -> 120 cycle
 */

#define WS_NUM_LEDS			(4)
#define WS_PERIOD_CYCLES	(120)
#define WS_T0H_CYCLES		(38)
#define WS_T1H_CYCLES		(82)

static unsigned char led_grb[WS_NUM_LEDS][3];	/* 저장 순서 : G, R, B */

static void WS2812_Send_Bit(int bit)
{
	unsigned int start = DWT->CYCCNT;
	unsigned int high_cycles = bit ? WS_T1H_CYCLES : WS_T0H_CYCLES;

	GPIOA->BSRR = (1U << 0);				/* PA0 High */

	while((DWT->CYCCNT - start) < high_cycles);

	GPIOA->BSRR = (1U << (0 + 16));		/* PA0 Low */

	while((DWT->CYCCNT - start) < WS_PERIOD_CYCLES);
}

void WS2812_Init(void)
{
	Macro_Set_Bit(RCC->AHB1ENR, 0);				// Port-A Clock On

	Macro_Write_Block(GPIOA->MODER, 0x3, 0x1, 0);	// PA0 => General Purpose Output
	Macro_Write_Block(GPIOA->OSPEEDR, 0x3, 0x3, 0);// PA0 => Very High Speed
	Macro_Clear_Bit(GPIOA->OTYPER, 0);				// PA0 => Push-Pull
	Macro_Write_Block(GPIOA->PUPDR, 0x3, 0x0, 0);	// PA0 => No Pull
	GPIOA->BSRR = (1U << (0 + 16));				// 초기상태 Low

	/* DWT 사이클 카운터 활성화 (비트 타이밍 측정용) */
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* idx = 0~3, r/g/b = 0~255 (실제로는 눈부심 방지를 위해 낮은 값 사용 권장) */

void WS2812_Set_Pixel(int idx, unsigned char r, unsigned char g, unsigned char b)
{
	if(idx < 0 || idx >= WS_NUM_LEDS) return;

	led_grb[idx][0] = g;
	led_grb[idx][1] = r;
	led_grb[idx][2] = b;
}

/* WS2812_Set_Pixel()로 4개 픽셀을 채운 뒤 호출 -> 여기서 실제 전송(블로킹) */

void WS2812_Show(void)
{
	int led, byte;
	int bit;
	volatile unsigned int start;

	__disable_irq();	/* 전송 중 인터럽트로 타이밍이 흐트러지는 것 방지 */

	for(led = 0; led < WS_NUM_LEDS; led++)
	{
		for(byte = 0; byte < 3; byte++)
		{
			unsigned char v = led_grb[led][byte];

			for(bit = 7; bit >= 0; bit--)
			{
				WS2812_Send_Bit((v >> bit) & 0x1);
			}
		}
	}

	__enable_irq();

	/* Reset 신호 : 최신 WS2812B(클론 포함)는 280us 이상 필요한 경우가 많아 여유있게 300us */
	start = DWT->CYCCNT;
	while((DWT->CYCCNT - start) < (96 * 300));
}