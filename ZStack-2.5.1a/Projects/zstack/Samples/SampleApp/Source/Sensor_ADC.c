#include "Sensor_ADC.h"

void ADC_Init(void)
{
    P0SEL |= 0x70;   // P0_4,P0_5,P0_6 设为外设功能
    P0DIR &= ~0x70;  // 输入
}

uint16 ADC_Read(uint8 channel)
{
    uint16 value;
    while (ADCCON1 & 0x80);               // 等待空闲
    ADCCON3 = (0x30) | (channel & 0x07); // 启动转换
    while (!(ADCCON1 & 0x80));            // 等待完成
    value = ADCL >> 4;
    value |= (((uint16)ADCH) << 4);
    return value;
}