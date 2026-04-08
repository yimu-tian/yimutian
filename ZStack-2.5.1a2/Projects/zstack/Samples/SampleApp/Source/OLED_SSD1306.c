#include "OLED_SSD1306.h"
#include <string.h>

#define I2C_Delay() asm("NOP"); asm("NOP")

// 8x16 字库（此处仅为示例，请替换为完整字库）
static const uint8 F8x16[][16] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // 空格
    // ... 实际使用时需补充完整字库
};

static void I2C_Start(void)
{
    OLED_SDA = 1; OLED_SCL = 1; I2C_Delay();
    OLED_SDA = 0; I2C_Delay(); OLED_SCL = 0;
}

static void I2C_Stop(void)
{
    OLED_SDA = 0; OLED_SCL = 1; I2C_Delay();
    OLED_SDA = 1;
}

static uint8 I2C_WriteByte(uint8 dat)
{
    uint8 i, ack;
    for(i=0; i<8; i++)
    {
        OLED_SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;
        OLED_SCL = 1; I2C_Delay(); OLED_SCL = 0;
    }
    OLED_SDA = 1; OLED_SCL = 1; I2C_Delay();
    ack = OLED_SDA; OLED_SCL = 0;
    return ack;
}

static void OLED_WriteCmd(uint8 cmd)
{
    I2C_Start();
    I2C_WriteByte(0x78);
    I2C_WriteByte(0x00);
    I2C_WriteByte(cmd);
    I2C_Stop();
}

static void OLED_WriteData(uint8 dat)
{
    I2C_Start();
    I2C_WriteByte(0x78);
    I2C_WriteByte(0x40);
    I2C_WriteByte(dat);
    I2C_Stop();
}

static void OLED_SetPos(uint8 x, uint8 y)
{
    OLED_WriteCmd(0xB0 + y);
    OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10);
    OLED_WriteCmd(x & 0x0F);
}

void OLED_Init(void)
{
    P1DIR |= 0x03; // P1_0,P1_1 输出
    OLED_SCL = 0; OLED_SDA = 0;

    OLED_WriteCmd(0xAE); OLED_WriteCmd(0xD5); OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8); OLED_WriteCmd(0x3F); OLED_WriteCmd(0xD3);
    OLED_WriteCmd(0x00); OLED_WriteCmd(0x40); OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x14); OLED_WriteCmd(0x20); OLED_WriteCmd(0x00);
    OLED_WriteCmd(0xA1); OLED_WriteCmd(0xC8); OLED_WriteCmd(0xDA);
    OLED_WriteCmd(0x12); OLED_WriteCmd(0x81); OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9); OLED_WriteCmd(0xF1); OLED_WriteCmd(0xDB);
    OLED_WriteCmd(0x40); OLED_WriteCmd(0xA4); OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0x2E); OLED_WriteCmd(0xAF);
    OLED_Clear();
}

void OLED_Clear(void)
{
    for(uint8 i=0; i<8; i++)
    {
        OLED_WriteCmd(0xB0 + i);
        OLED_WriteCmd(0x00);
        OLED_WriteCmd(0x10);
        for(uint8 j=0; j<128; j++) OLED_WriteData(0x00);
    }
}

void OLED_ShowString(uint8 x, uint8 y, char *str)
{
    while(*str)
    {
        uint8 c = (*str) - 32;
        if(c > 95) c = 0;
        OLED_SetPos(x, y);
        for(uint8 j=0; j<8; j++) OLED_WriteData(F8x16[c][j]);
        OLED_SetPos(x, y+1);
        for(uint8 j=8; j<16; j++) OLED_WriteData(F8x16[c][j]);
        x += 8; str++;
        if(x > 120) break;
    }
}