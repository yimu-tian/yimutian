#ifndef SENSOR_ADC_H
#define SENSOR_ADC_H

#include <ioCC2530.h>
#include "ZComDef.h"   // 提供 uint8, uint16 定义

#define LIGHT_CHANNEL   6   // P0_6
#define MQ135_CHANNEL   5   // P0_5
#define MQ7_CHANNEL     4   // P0_4

extern void ADC_Init(void);
extern uint16 ADC_Read(uint8 channel);

static inline uint16 Read_Light(void) { return ADC_Read(LIGHT_CHANNEL); }
static inline uint16 Read_MQ135(void) { return ADC_Read(MQ135_CHANNEL); }
static inline uint16 Read_MQ7(void)   { return ADC_Read(MQ7_CHANNEL); }

#endif