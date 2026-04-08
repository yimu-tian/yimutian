#ifndef __DHT11_H__
#define __DHT11_H__

#define uchar unsigned char

// 数据引脚定义（终端使用 P0_7）
#define DATA_PIN P0_7
#define DATA_PIN_INPUT  (P0DIR &= ~0x80)
#define DATA_PIN_OUTPUT (P0DIR |= 0x80)

extern void Delay_ms(unsigned int xms);
extern void COM(void);
extern void DHT11(void);

extern uchar shidu, wendu;

#endif