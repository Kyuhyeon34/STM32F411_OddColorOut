#include "device_driver.h"
#include <stdio.h>

/*
 * 다른 색 찾기 게임 (Odd Color Out)
 *
 *  - WS2812B 4개(PA0 한 줄 직렬연결)에 색을 표시. 3개는 같은 색, 1개만 RGB 값이
 *    delta만큼 다름. 스테이지가 올라갈수록 delta가 줄어들어 점점 어려워짐 (1~7단계).
 *  - 색 선택 : 버튼 4개(PC0/PC1/PC2/PC3) 또는 TeraTerm 키보드 1~4
 *  - 재시작 : PC13 버튼 또는 Space Bar (색 선택 버튼과 분리된 전용 버튼)
 *  - 제한시간 : 가변저항(PA6, ADC1_IN6)으로 스테이지 시작 시 설정 (3~8초)
 *    - 게임 진행 중에도 가변저항을 돌리면 실시간으로 감시하다가, 3초간 값이 안정되면
 *      그 시점에 남은시간에 반영하고 TeraTerm에 출력함
 *  - 정답 : SC16IS752 GPIO(SPI1, CS1) LED 7개 중 스테이지 번호 LED 점등 후 다음 스테이지
 *  - 오답/시간초과 : 즉시 게임오버. 재시작 = PC13 버튼 또는 Space Bar
 *  - 입력 인터록 : 정답/오답 판정 직후 0.3초간 버튼/키 입력 무시(디바운스, 연타 오작동 방지)
 */

typedef enum { GAME_WAIT = 0, GAME_RUNNING, GAME_OVER, GAME_CLEAR } GameState;

#define TICK_MS			(100)		/* TIM4 주기 */
#define ADC_MAX			(4095)
#define STAGE_MAX		(7)
#define BRIGHTNESS_MAX	(20)		/* WS2812B 최대 밝기 상한 (0~255 중 40 = 눈부심 방지) */
#define TIME_MIN_MS		(5000)		/* 가변저항 최소 -> 제한시간 5초 */
#define TIME_MAX_MS		(30000)		/* 가변저항 최대 -> 제한시간 30초 */
#define WARNING_TICKS	(10)		/* 남은시간 1.0초 이하 = 경고 점멸 */

#define IDLE_STEP		(2)			/* 대기화면 색 전환 속도 (틱당 이동량, 클수록 빨리 바뀜) */

#define FADE_STEPS		(15)		/* 스테이지 전환시 부드러운 페이드 단계 수 */
#define FADE_STEP_MS	(25)		/* 15*25 = 약 0.4초에 걸쳐 전환 (TIM2_Delay 사용, TIM4 틱과 무관) */
#define FLASH_COUNT		(3)			/* 게임오버/올클리어 점멸 횟수 */
#define FLASH_HALF_MS	(150)		/* 점멸 on/off 각각 150ms = 한 사이클 0.3초 */

#define INPUT_LOCK_TICKS	(3)		/* 선택 처리 직후 0.3초간 추가입력 무시(디바운스/인터록) */
#define POT_STABLE_TICKS	(30)	/* 가변저항 값이 3초간 안정되면 적용 */
#define POT_CHANGE_THRESH	(3)		/* 이 이상 변해야 "움직였다"고 판단(노이즈 무시) */

/* 스테이지별 RGB 차이값을 BRIGHTNESS_MAX 대비 "비율(%)"로 관리.
   절대값으로 두면 BRIGHTNESS_MAX를 그보다 작게 바꿨을 때 연산이 깨짐(색 폭주).
   비율로 두면 BRIGHTNESS_MAX를 얼마로 바꾸든 항상 안전함. */
static const unsigned char stage_delta_percent[STAGE_MAX] = { 80, 58, 42, 30, 22, 16, 12 };

static GameState g_game = GAME_WAIT;
static int g_stage = 1;
static int g_answer = 0;			/* 이번 스테이지의 정답 LED 인덱스(0~3) */
static unsigned int g_stage_leds = 0;	/* SC16IS752에 누적 표시할 스테이지 진행 LED 비트마스크 */
static int g_time_left_ticks = 0;
static int g_blink_on = 0;
static unsigned int g_rand_state = 0xACE1u;

static int g_idle_cur[4][3];		/* 대기화면 : 각 LED 현재 RGB (부드러운 전환용) */
static int g_idle_tgt[4][3];		/* 대기화면 : 각 LED 목표 RGB */

static int g_last_shown[4][3];		/* 마지막으로 실제 표시된 4개 LED 색 (페이드 시작점, 오답시 복귀용) */

static int g_elapsed_ticks = 0;	/* 게임 시작(스페이스바) 이후 경과 tick 수, 100ms 단위 */
static int g_best_time_ticks = -1;	/* 역대 최고기록(가장 빠른 클리어), tick 단위. -1 = 기록 없음 */

static int g_time_limit_sec = 0;	/* 현재 적용된 제한시간(초), 로그 출력용 */
static int g_last_print_sec = -1;	/* 마지막으로 출력한 남은 초 (중복 출력 방지) */

static int g_pot_last = -1;		/* 최근 확인한 가변저항 값(%), 변화 감지 기준 */
static int g_pot_stable_ticks = 0;	/* 가변저항 값이 안 변한 채 지속된 tick 수 */
static int g_pot_preview_last = -1;/* 마지막으로 미리보기 출력한 가변저항 값(%) */
static int g_pot_applied = -1;		/* 실제로 제한시간에 반영된 가변저항 값(%) */

static int g_input_lock_ticks = 0;	/* 버튼/키 인터록(디바운스) 남은 tick 수 */

/* ----- 간단 PRNG (xorshift32) ----- */

static unsigned int Rand_Next(void)
{
	g_rand_state ^= g_rand_state << 13;
	g_rand_state ^= g_rand_state >> 17;
	g_rand_state ^= g_rand_state << 5;

	return g_rand_state;
}

/* ----- 대기화면(스페이스바 누르기 전) : 4개 LED가 각자 다른 랜덤색으로 서서히 전환 ----- */

static int Idle_Rand_Channel(void)
{
	return Rand_Next() % (BRIGHTNESS_MAX + 1);
}

static void Idle_New_Target(int idx)
{
	g_idle_tgt[idx][0] = Idle_Rand_Channel();
	g_idle_tgt[idx][1] = Idle_Rand_Channel();
	g_idle_tgt[idx][2] = Idle_Rand_Channel();
}

static void Idle_Animation_Init(void)
{
	int i, c;

	for(i = 0; i < 4; i++)
	{
		for(c = 0; c < 3; c++) g_idle_cur[i][c] = Idle_Rand_Channel();
		Idle_New_Target(i);	/* 각 LED마다 독립적으로 다른 목표색 -> 서로 다르게, 따로따로 바뀜 */
	}
}

static void Idle_Animation_Tick(void)
{
	int i, c;

	for(i = 0; i < 4; i++)
	{
		int done = 1;

		for(c = 0; c < 3; c++)
		{
			int diff = g_idle_tgt[i][c] - g_idle_cur[i][c];

			if(diff > 0)      g_idle_cur[i][c] += (diff < IDLE_STEP) ? diff : IDLE_STEP;
			else if(diff < 0) g_idle_cur[i][c] += (diff > -IDLE_STEP) ? diff : -IDLE_STEP;

			if(g_idle_cur[i][c] != g_idle_tgt[i][c]) done = 0;
		}

		if(done) Idle_New_Target(i);	/* 목표에 도달하면 다음 랜덤색으로 -> 계속 사르르 바뀜 */

		WS2812_Set_Pixel(i, (unsigned char)g_idle_cur[i][0], (unsigned char)g_idle_cur[i][1], (unsigned char)g_idle_cur[i][2]);
	}

	WS2812_Show();
}

static int Read_Pot_Percent(void)
{
	int raw;

	ADC1_Start();
	while(!ADC1_Get_Status());
	raw = ADC1_Get_Data();

	return (raw * 100) / ADC_MAX;
}

static int abs_int(int x) { return (x < 0) ? -x : x; }

static int Pot_To_Ms(int pot)
{
	return TIME_MIN_MS + (pot * (TIME_MAX_MS - TIME_MIN_MS)) / 100;
}

#define BEST_TIME_MAGIC		(0xB3577A1Eu)	/* 백업레지스터에 유효한 기록이 저장돼있는지 표시하는 매직값 */

/* 베스트기록을 RTC 백업레지스터에 저장 -> 리셋(NRST) 눌러도 유지됨 (완전 전원차단 시엔 사라짐, 코인셀 없는 보드 기준) */

static void Best_Time_Save(int ticks)
{
	Macro_Set_Bit(RCC->APB1ENR, 28);	/* PWR Clock On */
	Macro_Set_Bit(PWR->CR, 8);			/* DBP : 백업도메인 쓰기 허용 */

	RTC->BKP1R = (unsigned int)ticks;
	RTC->BKP0R = BEST_TIME_MAGIC;
}

static void Best_Time_Load(void)
{
	Macro_Set_Bit(RCC->APB1ENR, 28);
	Macro_Set_Bit(PWR->CR, 8);

	if(RTC->BKP0R == BEST_TIME_MAGIC) g_best_time_ticks = (int)RTC->BKP1R;
	else                              g_best_time_ticks = -1;
}

/* 매 tick 가변저항을 감시. 값이 3초 이상 안정되면 그때 실제 제한시간에 반영 */

static void Poll_Pot_Monitor(void)
{
	int pot = Read_Pot_Percent();

	if(g_pot_last < 0) g_pot_last = pot;

	if(abs_int(pot - g_pot_last) >= POT_CHANGE_THRESH)
	{
		/* 유의미하게 움직임 -> 안정성 카운트 리셋 */
		g_pot_last = pot;
		g_pot_stable_ticks = 0;

		if(abs_int(pot - g_pot_applied) >= POT_CHANGE_THRESH &&
		   (g_pot_preview_last < 0 || abs_int(pot - g_pot_preview_last) >= POT_CHANGE_THRESH))
		{
			printf("[가변저항] 현재값 기준 제한시간 %d초 (3초 유지시 적용)\n", Pot_To_Ms(pot) / 1000);
			g_pot_preview_last = pot;
		}
	}
	else if(g_pot_stable_ticks < POT_STABLE_TICKS)
	{
		g_pot_stable_ticks++;	/* POT_STABLE_TICKS에서 더 이상 증가 안 함(상한) -> 아래 if가 딱 한 번만 실행됨 */

		if(g_pot_stable_ticks >= POT_STABLE_TICKS && abs_int(g_pot_last - g_pot_applied) >= POT_CHANGE_THRESH)
		{
			int new_ms = Pot_To_Ms(g_pot_last);

			g_time_left_ticks = new_ms / TICK_MS;
			g_time_limit_sec  = new_ms / 1000;
			g_last_print_sec  = g_time_limit_sec;
			g_pot_applied = g_pot_last;

			printf("[가변저항 적용] 제한시간 %d초로 변경됨\n", g_time_limit_sec);
		}
	}
}

/* 현재(g_last_shown)에서 목표색(tr/tg/tb)으로 부드럽게 전환 후, g_last_shown 갱신 */

static void LED_Fade_To(const int tr[4], const int tg[4], const int tb[4])
{
	int step, i;
	int from[4][3];

	for(i = 0; i < 4; i++)
	{
		from[i][0] = g_last_shown[i][0];
		from[i][1] = g_last_shown[i][1];
		from[i][2] = g_last_shown[i][2];
	}

	for(step = 1; step <= FADE_STEPS; step++)
	{
		for(i = 0; i < 4; i++)
		{
			int r = from[i][0] + (tr[i] - from[i][0]) * step / FADE_STEPS;
			int g = from[i][1] + (tg[i] - from[i][1]) * step / FADE_STEPS;
			int b = from[i][2] + (tb[i] - from[i][2]) * step / FADE_STEPS;

			WS2812_Set_Pixel(i, (unsigned char)r, (unsigned char)g, (unsigned char)b);
		}

		WS2812_Show();
		TIM2_Delay(FADE_STEP_MS);
	}

	for(i = 0; i < 4; i++)
	{
		g_last_shown[i][0] = tr[i];
		g_last_shown[i][1] = tg[i];
		g_last_shown[i][2] = tb[i];
	}
}

/* 4개 LED 전체를 지정색<->꺼짐으로 times번 점멸 (블로킹). g_last_shown은 갱신하지 않음(호출부가 이후 처리) */

static void LED_Flash(unsigned char r, unsigned char g, unsigned char b, int times)
{
	int i, n;

	for(n = 0; n < times; n++)
	{
		for(i = 0; i < 4; i++) WS2812_Set_Pixel(i, r, g, b);
		WS2812_Show();
		TIM2_Delay(FLASH_HALF_MS);

		for(i = 0; i < 4; i++) WS2812_Set_Pixel(i, 0, 0, 0);
		WS2812_Show();
		TIM2_Delay(FLASH_HALF_MS);
	}
}

/* ----- 스테이지 생성 : 4색 중 1개만 delta만큼 다르게 ----- */

static void Spawn_Stage(void)
{
	int i, pot, limit_ms, delta, odd_ch;
	int r, g, b;

	pot = Read_Pot_Percent();
	limit_ms = Pot_To_Ms(pot);
	g_time_left_ticks = limit_ms / TICK_MS;
	g_time_limit_sec  = limit_ms / 1000;
	g_last_print_sec  = g_time_limit_sec;

	g_pot_last = pot;			/* 새 스테이지 시작 시 감시 기준 재설정 */
	g_pot_preview_last = pot;
	g_pot_applied = pot;
	g_pot_stable_ticks = 0;

	delta = (BRIGHTNESS_MAX * stage_delta_percent[g_stage - 1]) / 100;
	if(delta < 1) delta = 1;	/* BRIGHTNESS_MAX를 아주 작게 설정해도 최소한의 차이는 보장 */

	r = delta + (Rand_Next() % (BRIGHTNESS_MAX - delta + 1));
	g = delta + (Rand_Next() % (BRIGHTNESS_MAX - delta + 1));
	b = delta + (Rand_Next() % (BRIGHTNESS_MAX - delta + 1));

	odd_ch   = Rand_Next() % 3;		/* 0=R, 1=G, 2=B 중 다르게 만들 채널 */
	g_answer = Rand_Next() & 0x3;		/* 정답 LED 인덱스 0~3 */

	{
		int tr[4], tg[4], tb[4];

		for(i = 0; i < 4; i++)
		{
			int pr = r, pg = g, pb = b;

			if(i == g_answer)
			{
				if(odd_ch == 0)      pr -= delta;
				else if(odd_ch == 1) pg -= delta;
				else                 pb -= delta;
			}

			tr[i] = pr; tg[i] = pg; tb[i] = pb;
		}

		LED_Fade_To(tr, tg, tb);	/* 이전 화면(마지막 오답/이전 스테이지 색)에서 부드럽게 전환 */
	}

	printf("[STAGE %d/%d] delta=%d  제한시간=%d초  (1~4 또는 버튼으로 다른 색 선택)\n",
			g_stage, STAGE_MAX, delta, g_time_limit_sec);
}

static void Game_Over(void)
{
	int i;

	g_game = GAME_OVER;

	LED_Flash((unsigned char)BRIGHTNESS_MAX, 0, 0, FLASH_COUNT);	/* 빨강 3회 점멸 */

	for(i = 0; i < 4; i++)	/* 마지막(오답 당시) 색 조합으로 복귀 */
	{
		WS2812_Set_Pixel(i, (unsigned char)g_last_shown[i][0], (unsigned char)g_last_shown[i][1], (unsigned char)g_last_shown[i][2]);
	}
	WS2812_Show();

	printf("정답은 %d번이었습니다^^\n", g_answer + 1);
	printf("[GAME OVER] stage=%d/%d", g_stage, STAGE_MAX);
	if(g_best_time_ticks >= 0) printf("  (역대 베스트 : %d.%d초)", g_best_time_ticks / 10, g_best_time_ticks % 10);
	printf("  (재시작 : PC13 버튼 또는 Space)\n");
}

static void Game_Clear(void)
{
	g_game = GAME_CLEAR;

	LED_Flash(0, (unsigned char)BRIGHTNESS_MAX, 0, FLASH_COUNT);	/* 초록 3회 점멸 */

	printf("[ALL STAGE CLEAR!]\n");
	printf("기록 : %d.%d초\n", g_elapsed_ticks / 10, g_elapsed_ticks % 10);

	if(g_best_time_ticks < 0 || g_elapsed_ticks < g_best_time_ticks)
	{
		g_best_time_ticks = g_elapsed_ticks;
		Best_Time_Save(g_best_time_ticks);
		printf("역대 베스트 : %d.%d초  (신기록!)\n", g_best_time_ticks / 10, g_best_time_ticks % 10);
	}
	else
	{
		printf("역대 베스트 : %d.%d초\n", g_best_time_ticks / 10, g_best_time_ticks % 10);
	}

	g_game = GAME_WAIT;			/* 처음 실행화면으로 리셋 */
	Idle_Animation_Init();

	printf("Space Bar 또는 PC13 버튼으로 다시 시작 (총 %d 스테이지)\n", STAGE_MAX);
}

static void Handle_Selection(int sel)
{
	g_input_lock_ticks = INPUT_LOCK_TICKS;	/* 처리 직후 0.3초간 추가 입력 무시(디바운스) */

	if(sel == g_answer)
	{
		g_stage_leds |= (0x1u << (g_stage - 1));
		SPI1_SC16IS752_Write_GPIO(g_stage_leds);

		printf("[CORRECT] stage %d clear\n", g_stage);

		g_stage++;

		if(g_stage > STAGE_MAX) Game_Clear();
		else                     Spawn_Stage();
	}
	else
	{
		Game_Over();
	}
}

static void Restart_Game(void)
{
	int i;

	g_game = GAME_RUNNING;
	g_stage = 1;
	g_stage_leds = 0;
	g_elapsed_ticks = 0;

	for(i = 0; i < 4; i++)		/* 이전 게임의 잔여색과 안 섞이게 검정으로 리셋 */
	{
		g_last_shown[i][0] = 0;
		g_last_shown[i][1] = 0;
		g_last_shown[i][2] = 0;
		WS2812_Set_Pixel(i, 0, 0, 0);
	}
	WS2812_Show();

	SPI1_SC16IS752_Write_GPIO(0);

	printf("\n[GAME START]");
	if(g_best_time_ticks >= 0) printf("  (역대 베스트 : %d.%d초)", g_best_time_ticks / 10, g_best_time_ticks % 10);
	printf("\n");
	Spawn_Stage();
}

int Main(void)
{
	Clock_Init();

	Uart2_Init(115200);			/* TeraTerm : 상태 출력 + 1~4/Space 입력 */
	Uart2_RX_Interrupt_Enable(1);

	Key_ISR_Enable(1);				/* PC13 : 재시작 전용 버튼 */
	Buttons0123_ISR_Enable(1);		/* PC0/PC1/PC2/PC3 : 색 선택 버튼 1~4번 */

	ADC1_IN6_Init();				/* 제한시간 설정용 가변저항 */

	SPI1_SC16IS752_Init(32);		/* SPI1 + nCS1(PA8), div 값은 필요시 조정 */
	SPI1_SC16IS752_Config_GPIO(0x00);	/* 스테이지 LED 7개(0~6번 핀) Output */

	WS2812_Init();					/* WS2812B x4 : PA0 (GPIO 비트뱅) */

	g_rand_state ^= (unsigned int)Read_Pot_Percent() + 1u;	/* 부팅 시 PRNG 시드 보강 */

	TIM4_Repeat_Interrupt_Enable(1, TICK_MS);
	Best_Time_Load();		/* 리셋 이전 베스트기록 복원 */
	Idle_Animation_Init();
	Idle_Animation_Init();

	printf("\n=== Odd Color Out : 다른 색을 찾아라! ===\n");
	printf("Space Bar 또는 PC13 버튼으로 시작 (총 %d 스테이지)\n", STAGE_MAX);

	for( ; ; )
	{
		if(g_game == GAME_WAIT)
		{
			Btn0_Flag = Btn1_Flag = Btn2_Flag = Btn3_Flag = 0;	/* 대기중엔 색선택 버튼 무시 */

			if(Key_Pressed_Flag) { Key_Pressed_Flag = 0; Restart_Game(); }

			if(Uart2_RX_Flag)
			{
				char c = Uart2_RX_Data;
				Uart2_RX_Flag = 0;

				if(c == ' ') Restart_Game();
			}
		}
		else if(g_game == GAME_RUNNING)
		{
			if(g_input_lock_ticks > 0)
			{
				/* 인터록 구간 : 바운스/연타 무시 */
				Btn0_Flag = Btn1_Flag = Btn2_Flag = Btn3_Flag = 0;
				Uart2_RX_Flag = 0;
			}
			else
			{
				int sel = -1;

				if(Btn0_Flag) { Btn0_Flag = 0; sel = 0; }
				if(Btn1_Flag) { Btn1_Flag = 0; sel = 1; }
				if(Btn2_Flag) { Btn2_Flag = 0; sel = 2; }
				if(Btn3_Flag) { Btn3_Flag = 0; sel = 3; }

				if(Uart2_RX_Flag)
				{
					char c = Uart2_RX_Data;
					Uart2_RX_Flag = 0;

					if(c >= '1' && c <= '4') sel = c - '1';
				}

				if(sel >= 0) Handle_Selection(sel);
			}
		}
		else	/* GAME_OVER / GAME_CLEAR : 재시작 대기 */
		{
			Btn0_Flag = Btn1_Flag = Btn2_Flag = Btn3_Flag = 0;

			if(Key_Pressed_Flag) { Key_Pressed_Flag = 0; Restart_Game(); }

			if(Uart2_RX_Flag)
			{
				char c = Uart2_RX_Data;
				Uart2_RX_Flag = 0;

				if(c == ' ') Restart_Game();
			}
		}

		/* ----- 100ms Tick : 제한시간 카운트다운 + 가변저항 감시 ----- */
		if(TIM4_Expired)
		{
			TIM4_Expired = 0;

			if(g_input_lock_ticks > 0) g_input_lock_ticks--;

			if(g_game == GAME_WAIT)
			{
				Idle_Animation_Tick();
			}

			if(g_game == GAME_RUNNING)
			{
				int sec_left;

				g_time_left_ticks--;
				g_elapsed_ticks++;
				sec_left = (g_time_left_ticks + 9) / 10;	/* TICK_MS=100 기준 올림 */

				if(sec_left != g_last_print_sec && sec_left >= 0)
				{
					printf("[남은시간] %d초\n", sec_left);
					g_last_print_sec = sec_left;
				}

				if(g_time_left_ticks <= WARNING_TICKS)
				{
					g_blink_on = !g_blink_on;
				}

				if(g_time_left_ticks <= 0)
				{
					printf("[TIME OUT]\n");
					Handle_Selection(-1);	/* -1은 어떤 g_answer(0~3)와도 같을 수 없어 오답 처리됨 */
				}

				Poll_Pot_Monitor();
			}
		}
	}
}