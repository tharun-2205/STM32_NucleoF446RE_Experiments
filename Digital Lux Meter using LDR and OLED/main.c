#include "main.h"
#include "i2c.h"
#include "adc.h"
#include "gpio.h"
#include <string.h>
#include <stdio.h>

/* Prototypes */
void SystemClock_Config(void);
void Error_Handler(void);

#define OLED_ADDR (0x3C << 1)

/* ===== FONT (0–9 only for simplicity) ===== */
const uint8_t font5x7[][5] = {
{0,0,0,0,0}, // space
{0x3E,0x51,0x49,0x45,0x3E}, // 0
{0x00,0x42,0x7F,0x40,0x00}, // 1
{0x62,0x51,0x49,0x49,0x46}, // 2
{0x22,0x49,0x49,0x49,0x36}, // 3
{0x18,0x14,0x12,0x7F,0x10}, // 4
{0x2F,0x49,0x49,0x49,0x31}, // 5
{0x3E,0x49,0x49,0x49,0x32}, // 6
{0x01,0x71,0x09,0x05,0x03}, // 7
{0x36,0x49,0x49,0x49,0x36}, // 8
{0x26,0x49,0x49,0x49,0x3E}  // 9
};

/* ===== OLED LOW LEVEL ===== */
void OLED_Command(uint8_t cmd)
{
  uint8_t d[2] = {0x00, cmd};
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, d, 2, HAL_MAX_DELAY);
}

void OLED_Data(uint8_t *data, uint16_t size)
{
  uint8_t buf[129];
  buf[0] = 0x40;
  memcpy(&buf[1], data, size);
  HAL_I2C_Master_Transmit(&hi2c1, OLED_ADDR, buf, size+1, HAL_MAX_DELAY);
}

/* ===== INIT ===== */
void OLED_Init(void)
{
  HAL_Delay(100);

  OLED_Command(0xAE);
  OLED_Command(0x20); OLED_Command(0x02); // PAGE MODE
  OLED_Command(0xAF);
}

/* ===== CLEAR ===== */
void OLED_Clear(void)
{
  uint8_t zero[128] = {0};

  for(int i=0;i<8;i++)
  {
    OLED_Command(0xB0 + i);
    OLED_Command(0x00);
    OLED_Command(0x10);
    OLED_Data(zero,128);
  }
}

/* ===== CURSOR ===== */
void OLED_SetCursor(uint8_t x, uint8_t y)
{
  OLED_Command(0xB0 + y);
  OLED_Command(0x00 + (x & 0x0F));
  OLED_Command(0x10 + (x >> 4));
}

/* ===== PRINT CHAR ===== */
void OLED_Char(char c)
{
  uint8_t data[6];

  if(c >= '0' && c <= '9')
    memcpy(data, font5x7[c - '0' + 1], 5);
  else
    memset(data, 0, 5);

  data[5] = 0x00;
  OLED_Data(data,6);
}

/* ===== PRINT STRING ===== */
void OLED_Print(char *str)
{
  while(*str)
  {
    OLED_Char(*str++);
  }
}

/* ===== MAIN ===== */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();

  OLED_Init();
  OLED_Clear();

  char buffer[20];

  while (1)
  {
    /* Read LDR */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    uint32_t adc = HAL_ADC_GetValue(&hadc1);

    /* Convert to % */
    int percent = (adc * 100) / 4095;

    /* Display */
    OLED_Clear();

    OLED_SetCursor(0,0);
    sprintf(buffer, "%lu", adc);
    OLED_Print(buffer);

    OLED_SetCursor(0,3);
    sprintf(buffer, "%d", percent);
    OLED_Print(buffer);

    HAL_Delay(300);
  }
}

/* ===== CLOCK ===== */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;

  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void Error_Handler(void)
{
  while(1){}
}
