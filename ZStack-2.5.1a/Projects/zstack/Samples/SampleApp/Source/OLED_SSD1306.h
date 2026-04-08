#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include <ioCC2530.h>
#include "ZComDef.h"

// 根据实际 J2 连接修改引脚
#define OLED_SCL P1_0
#define OLED_SDA P1_1

extern void OLED_Init(void);
extern void OLED_Clear(void);
extern void OLED_ShowString(uint8 x, uint8 y, char *str);

#endif