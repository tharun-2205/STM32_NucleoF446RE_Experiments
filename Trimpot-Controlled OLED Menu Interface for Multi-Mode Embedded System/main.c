#include "main.h"
#include "i2c.h"
#include "adc.h"
#include <string.h>

/* Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

#define OLED_ADDR (0x3C << 1)

/* ===== FONT (A-Z + 0-9 + '>') ===== */
const uint8_t font5x7[][5] = {
  {0,0,0,0,0}, // space

  // A-Z (same as before)
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
  {0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},
  {0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},

  // 0-9
  {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
  {0x62,0x51,0x49,0x49,0x46},{0x22,0x49,0x49,0x49,0x36},
  {0x18,0x14,0x12,0x7F,0x10},{0x2F,0x49,0x49,0x49,0x31},
  {0x3E,0x49,0x49,0x49,0x32},{0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},{0x26,0x49,0x49,0x49,0x3E},

  // '>' symbol
  {0x00,0x41,0x22,0x14,0x08}
};

/* ===== LOW LEVEL ===== */
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
  OLED_Command(0x20); OLED_Command(0x10);
  OLED_Command(0xB0);
  OLED_Command(0xC8);
  OLED_Command(0x00);
  OLED_Command(0x10);
  OLED_Command(0x40);
  OLED_Command(0x81); OLED_Command(0xFF);
  OLED_Command(0xA1);
  OLED_Command(0xA6);
  OLED_Command(0xA8); OLED_Command(0x3F);
  OLED_Command(0xAF);
}

/* ===== CURSOR ===== */
void OLED_SetCursor(uint8_t x, uint8_t y)
{
  OLED_Command(0xB0 + y);
  OLED_Command(0x00 + (x & 0x0F));
  OLED_Command(0x10 + (x >> 4));
}

/* ===== CHAR ===== */
void OLED_Char(char c)
{
  uint8_t data[6];

  if(c >= 'A' && c <= 'Z')
    memcpy(data, font5x7[c - 'A' + 1], 5);
  else if(c >= '0' && c <= '9')
    memcpy(data, font5x7[c - '0' + 27], 5);
  else if(c == '>')
    memcpy(data, font5x7[37], 5);
  else
    memset(data, 0, 5);

  data[5] = 0;
  OLED_Data(data,6);
}

/* ===== STRING ===== */
void OLED_Print(char *str)
{
  while(*str) OLED_Char(*str++);
}

/* ===== MENU (NO CLEAR INSIDE) ===== */
void OLED_Menu(uint8_t selected)
{
  OLED_SetCursor(0,0);
  OLED_Print(selected == 0 ? ">MODE1" : " MODE1");

  OLED_SetCursor(0,2);
  OLED_Print(selected == 1 ? ">MODE2" : " MODE2");

  OLED_SetCursor(0,4);
  OLED_Print(selected == 2 ? ">MODE3" : " MODE3");
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

  uint32_t adc;
  uint8_t selected = 0;
  uint8_t last_selected = 255;

  while (1)
  {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    adc = HAL_ADC_GetValue(&hadc1);

    if(adc < 1365)       selected = 0;
    else if(adc < 2730)  selected = 1;
    else                 selected = 2;

    /* Update only when changed */
    if(selected != last_selected)
    {
      OLED_Menu(selected);
      last_selected = selected;
    }

    HAL_Delay(50);
  }
}

/* ===== GPIO ===== */
static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void Error_Handler(void)
{
  while(1){}
}
