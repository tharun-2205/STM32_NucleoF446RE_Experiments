#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"
#include <string.h>

/* Prototypes */
void SystemClock_Config(void);
void Error_Handler(void);

#define OLED_ADDR (0x3C << 1)

/* ===== SERVO FIXED ===== */
void Servo_SetAngle(uint8_t angle)
{
  uint16_t pulse = 600 + ((angle * 1800) / 180);
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, pulse);
}

/* ===== FONT ===== */
const uint8_t font5x7[][5] = {
  {0,0,0,0,0},
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
  {0x00,0x41,0x22,0x14,0x08}
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

/* ===== OLED INIT (FIXED ONLY THIS) ===== */
void OLED_Init(void)
{
  HAL_Delay(100);

  OLED_Command(0xAE);

  OLED_Command(0xD5); OLED_Command(0x80);
  OLED_Command(0xA8); OLED_Command(0x3F);
  OLED_Command(0xD3); OLED_Command(0x00);
  OLED_Command(0x40);

  OLED_Command(0x8D); OLED_Command(0x14);

  OLED_Command(0x20); OLED_Command(0x02); // 🔥 PAGE MODE FIX

  OLED_Command(0xA1);
  OLED_Command(0xC8);

  OLED_Command(0xDA); OLED_Command(0x12);
  OLED_Command(0x81); OLED_Command(0xCF);
  OLED_Command(0xD9); OLED_Command(0xF1);
  OLED_Command(0xDB); OLED_Command(0x40);

  OLED_Command(0xA4);
  OLED_Command(0xA6);

  OLED_Command(0xAF);
}

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
  else if(c == '>')
    memcpy(data, font5x7[27], 5);
  else
    memset(data, 0, 5);

  data[5] = 0x00;
  OLED_Data(data, 6);
}

void OLED_Print(const char *s)
{
  while(*s) OLED_Char(*s++);
}

/* ===== MENU ===== */
void OLED_Menu(uint8_t sel)
{
  OLED_SetCursor(0,0);
  OLED_Print(sel==0 ? "> LEFT " : "  LEFT ");

  OLED_SetCursor(0,2);
  OLED_Print(sel==1 ? "> CENTER" : "  CENTER");

  OLED_SetCursor(0,4);
  OLED_Print(sel==2 ? "> RIGHT " : "  RIGHT ");
}

/* ===== MAIN ===== */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();

  OLED_Init();

  // 🔥 CLEAR SCREEN ONCE (ADDED)
  uint8_t zero[128] = {0};
  for(int i=0;i<8;i++)
  {
    OLED_Command(0xB0 + i);
    OLED_Command(0x00);
    OLED_Command(0x10);
    OLED_Data(zero,128);
  }

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);

  uint8_t sel = 0;
  uint8_t last_btn = 1;

  OLED_Menu(sel);
  Servo_SetAngle(0);

  while (1)
  {
    uint8_t btn = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

    if(btn == 0 && last_btn == 1)
    {
      HAL_Delay(30);
      if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == 0)
      {
        sel = (sel + 1) % 3;
        OLED_Menu(sel);
      }
    }

    last_btn = btn;

    if(sel == 0) Servo_SetAngle(0);
    else if(sel == 1) Servo_SetAngle(90);
    else Servo_SetAngle(180);

    HAL_Delay(10);
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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK |
                               RCC_CLOCKTYPE_HCLK |
                               RCC_CLOCKTYPE_PCLK1 |
                               RCC_CLOCKTYPE_PCLK2;

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

void Error_Handler(void)
{
  while(1){}
}
