#include <ioCC2530.h>
#include "OnBoard.h"
#include "DHT11.h"

// 全局变量
uchar ucharFLAG,uchartemp,shidu,wendu;
uchar ucharT_data_H,ucharT_data_L,ucharRH_data_H,ucharRH_data_L,ucharcheckdata;
uchar ucharT_data_H_temp,ucharT_data_L_temp,ucharRH_data_H_temp,ucharRH_data_L_temp,ucharcheckdata_temp;
uchar ucharcomdata;

void Delay_us(void) { MicroWait(1); }
void Delay_10us(void) { MicroWait(10); }

void Delay_ms(unsigned int Time)
{
    unsigned char i;
    while(Time--)
        for(i=0;i<100;i++) Delay_10us();
}

void COM(void)
{
    uchar i;
    for(i=0;i<8;i++)
    {
        ucharFLAG=2;
        while((!DATA_PIN)&&ucharFLAG++);
        Delay_10us(); Delay_10us(); Delay_10us();
        uchartemp=0;
        if(DATA_PIN) uchartemp=1;
        ucharFLAG=2;
        while((DATA_PIN)&&ucharFLAG++);
        if(ucharFLAG==1) break;
        ucharcomdata<<=1;
        ucharcomdata|=uchartemp;
    }
}

void DHT11(void)
{
    DATA_PIN=0;
    Delay_ms(19);
    DATA_PIN=1;
    DATA_PIN_INPUT;
    Delay_10us(); Delay_10us(); Delay_10us(); Delay_10us();
    if(!DATA_PIN)
    {
        ucharFLAG=2;
        while((!DATA_PIN)&&ucharFLAG++);
        ucharFLAG=2;
        while((DATA_PIN)&&ucharFLAG++);
        COM(); ucharRH_data_H_temp=ucharcomdata;
        COM(); ucharRH_data_L_temp=ucharcomdata;
        COM(); ucharT_data_H_temp=ucharcomdata;
        COM(); ucharT_data_L_temp=ucharcomdata;
        COM(); ucharcheckdata_temp=ucharcomdata;
        DATA_PIN=1;
        uchartemp=(ucharT_data_H_temp+ucharT_data_L_temp+ucharRH_data_H_temp+ucharRH_data_L_temp);
        if(uchartemp==ucharcheckdata_temp)
        {
            ucharRH_data_H=ucharRH_data_H_temp;
            ucharRH_data_L=ucharRH_data_L_temp;
            ucharT_data_H=ucharT_data_H_temp;
            ucharT_data_L=ucharT_data_L_temp;
        }
        wendu=ucharT_data_H;
        shidu=ucharRH_data_H;
    }
    else
    {
        shidu=0;
        wendu=0;
    }
    DATA_PIN_OUTPUT;
}