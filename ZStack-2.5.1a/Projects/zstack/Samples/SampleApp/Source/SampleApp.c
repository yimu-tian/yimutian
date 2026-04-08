/*********************************************************************
 * INCLUDES
 */
#include <stdio.h>
#include <string.h>
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SampleApp.h"
#include "ZDApp.h"
#include "hal_drivers.h"
#include "hal_key.h"
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_uart.h"
#include "MT_UART.h"        // 新增，解决 MT_UartInit 等隐式声明
#include "dht11.h"
#include "Sensor_ADC.h"
#include "OLED_SSD1306.h"


/*********************************************************************
 * CONSTANTS
 */
#define SAMPLE_APP_PORT     0
#define SAMPLE_APP_BAUD     HAL_UART_BR_115200
#define SAMPLE_APP_TX_MAX   80

// 事件定义已在 SampleApp.h 中
#ifndef SAMPLEAPP_SEND_PERIODIC_MSG_EVT
#define SAMPLEAPP_SEND_PERIODIC_MSG_EVT  0x0001
#endif

// Cluster ID 列表
const cId_t SampleApp_ClusterList[SAMPLE_MAX_CLUSTERS] =
{
    SAMPLEAPP_P2P_CLUSTERID,
    SAMPLEAPP_PERIODIC_CLUSTERID
};

// 简单描述符
const SimpleDescriptionFormat_t SampleApp_SimpleDesc =
{
    SAMPLEAPP_ENDPOINT,
    SAMPLEAPP_PROFID,
    SAMPLEAPP_DEVICEID,
    SAMPLEAPP_DEVICE_VERSION,
    SAMPLEAPP_FLAGS,
    SAMPLE_MAX_CLUSTERS,
    (cId_t *)SampleApp_ClusterList,
    SAMPLE_MAX_CLUSTERS,
    (cId_t *)SampleApp_ClusterList
};

// 端点描述符
endPointDesc_t SampleApp_epDesc =
{
    SAMPLEAPP_ENDPOINT,
    &SampleApp_TaskID,
    (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc,
    noLatencyReqs
};

/*********************************************************************
 * TYPEDEFS
 */
// 传感器数据结构（用于无线传输）
typedef struct {
    uint8 id;           // 终端 ID
    uint8 temperature;  // 温度
    uint8 humidity;     // 湿度
    uint16 light;       // 光敏 ADC
    uint16 mq135;       // MQ-135 ADC
    uint16 mq7;         // MQ-7 ADC
} sensor_data_t;

/*********************************************************************
 * GLOBAL VARIABLES
 */
devStates_t SampleApp_NwkState;
uint8 SampleApp_TaskID;

/*********************************************************************
 * LOCAL VARIABLES
 */
static uint8 SampleApp_MsgID;
afAddrType_t SampleApp_Periodic_DstAddr;
afAddrType_t SampleApp_Flash_DstAddr;
afAddrType_t SampleApp_P2P_DstAddr;
static uint8 SampleApp_TxBuf[SAMPLE_APP_TX_MAX+1];
static uint8 SampleApp_TxLen;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void SampleApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt );
static void SampleApp_Send_P2P_Message(void);
static void packDataAndSend(uint8 fc, uint8* data, uint8 len);

/*********************************************************************
 * @fn      SampleApp_Init
 */
void SampleApp_Init( uint8 task_id )
{
    SampleApp_TaskID = task_id;
    SampleApp_NwkState = DEV_INIT;

    MT_UartInit();
    MT_UartRegisterTaskID(task_id);
    afRegister(&SampleApp_epDesc);
    RegisterForKeys(task_id);

#ifdef ZDO_COORDINATOR
    // 协调器初始化
    // 蜂鸣器 P0_7 输出，默认高电平（不响）
    P0SEL &= ~0x80;
    P0DIR |= 0x80;
    P0_7 = 1;

    // OLED 初始化（取代原有 LCD）
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, "Coordinator");
#else
    // 终端初始化
    ADC_Init();   // 初始化 ADC 传感器引脚
    // DHT11 引脚已在 DHT11.h 中定义为 P0_7，无需额外配置
#endif

    // 配置通信地址
    SampleApp_Periodic_DstAddr.addrMode = (afAddrMode_t)AddrBroadcast;
    SampleApp_Periodic_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
    SampleApp_Periodic_DstAddr.addr.shortAddr = 0xFFFF;

    SampleApp_Flash_DstAddr.addrMode = (afAddrMode_t)afAddrGroup;
    SampleApp_Flash_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
    SampleApp_Flash_DstAddr.addr.shortAddr = SAMPLEAPP_FLASH_GROUP;

    SampleApp_P2P_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    SampleApp_P2P_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
    SampleApp_P2P_DstAddr.addr.shortAddr = 0x0000;  // 发给协调器
}

/*********************************************************************
 * @fn      SampleApp_ProcessEvent
 */
UINT16 SampleApp_ProcessEvent( uint8 task_id, UINT16 events )
{
    (void)task_id;

    if ( events & SYS_EVENT_MSG )
    {
        afIncomingMSGPacket_t *MSGpkt;
        while ((MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive(SampleApp_TaskID)))
        {
            switch (MSGpkt->hdr.event)
            {
            case AF_INCOMING_MSG_CMD:
                SampleApp_ProcessMSGCmd(MSGpkt);
                break;
            case ZDO_STATE_CHANGE:
                SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
                if (SampleApp_NwkState == DEV_ROUTER || SampleApp_NwkState == DEV_END_DEVICE)
                {
                    // 入网成功，启动定时器发送数据
                    osal_start_timerEx(SampleApp_TaskID,
                                       SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                                       3000); // 3 秒
                }
                break;
            default:
                break;
            }
            osal_msg_deallocate((uint8 *)MSGpkt);
        }
        return (events ^ SYS_EVENT_MSG);
    }

    if (events & SAMPLEAPP_SEND_PERIODIC_MSG_EVT)
    {
        // 终端发送数据
        SampleApp_Send_P2P_Message();

        // 重新启动定时器（加随机抖动）
        osal_start_timerEx(SampleApp_TaskID, SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                           3000 + (osal_rand() & 0x00FF));
        return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
    }

    return 0;
}

/*********************************************************************
 * @fn      SampleApp_ProcessMSGCmd
 */
void SampleApp_ProcessMSGCmd( afIncomingMSGPacket_t *pkt )
{
    switch (pkt->clusterId)
    {
    case SAMPLEAPP_P2P_CLUSTERID:
#ifdef ZDO_COORDINATOR
        {
            sensor_data_t *pData = (sensor_data_t *)pkt->cmd.Data;

            uint8 id = pData->id;
            uint8 t = pData->temperature;
            uint8 h = pData->humidity;
            uint16 light = pData->light;
            uint16 mq135 = pData->mq135;
            uint16 mq7 = pData->mq7;

            // OLED 显示（保持原有代码）
            char line1[20], line2[20], line3[20], line4[20];
            sprintf(line1, "ID:%d T:%dC H:%d%%", id, t, h);
            sprintf(line2, "Light:%d", light);
            sprintf(line3, "MQ135:%d", mq135);
            sprintf(line4, "MQ7:%d", mq7);
            OLED_Clear();
            OLED_ShowString(0, 0, line1);
            OLED_ShowString(0, 2, line2);
            OLED_ShowString(0, 4, line3);
            OLED_ShowString(0, 6, line4);

            // ========== 新增：通过串口输出数据 ==========
            char uartBuf[80];
            sprintf(uartBuf, "ID:%d T:%d H:%d L:%d M135:%d M7:%d\r\n",
                    id, t, h, light, mq135, mq7);
            HalUARTWrite(0, (uint8*)uartBuf, strlen(uartBuf));
            // =======================================

            // 蜂鸣器报警逻辑
            if (t > 35 || h < 30 || light < 100 || mq135 > 2000 || mq7 > 2000)
                P0_7 = 0;
            else
                P0_7 = 1;
        }
#endif
        break;

    case SAMPLEAPP_PERIODIC_CLUSTERID:
        // 处理其他消息（可根据需要添加）
        break;

    default:
        break;
    }
}

/*********************************************************************
 * @fn      SampleApp_Send_P2P_Message
 */
void SampleApp_Send_P2P_Message(void)
{
    sensor_data_t data;
    uint8 buf[sizeof(sensor_data_t)];

    // 1. 采集数据
    DHT11();  // 获取温湿度，结果存入 wendu, shidu
    data.id = 1;  // 终端 ID（若有多个终端可分别设置）
    data.temperature = wendu;
    data.humidity = shidu;
    data.light = Read_Light();
    data.mq135 = Read_MQ135();
    data.mq7 = Read_MQ7();

    // 2. 复制到发送缓冲区
    osal_memcpy(buf, &data, sizeof(data));

    // 3. 串口打印采集值（可选）
    char strTemp[60];
    sprintf(strTemp, "Sending: ID:%d T:%d H:%d L:%d M135:%d M7:%d\r\n",
            data.id, data.temperature, data.humidity,
            data.light, data.mq135, data.mq7);
    HalUARTWrite(0, (uint8 *)strTemp, strlen(strTemp));

    // 4. 无线发送
    if (AF_DataRequest(&SampleApp_P2P_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_P2P_CLUSTERID,
                       sizeof(data), buf,
                       &SampleApp_MsgID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS) == afStatus_SUCCESS)
    {
        // 发送成功
    }
    else
    {
        // 发送失败
    }
}


/*********************************************************************
 * @fn      CheckSum
 */
uint8 CheckSum(uint8 *pdata, uint8 len)
{
    uint8 i, sum = 0;
    for(i=0; i<len; i++) sum += pdata[i];
    return sum;
}

/*********************************************************************
 * @fn      packDataAndSend
 */
void packDataAndSend(uint8 fc, uint8* data, uint8 len)
{
    osal_memset(SampleApp_TxBuf, 0, SAMPLE_APP_TX_MAX+1);
    SampleApp_TxBuf[0] = 3 + len;
    SampleApp_TxBuf[2] = fc;
    if(len > 0)
        osal_memcpy(SampleApp_TxBuf+3, data, len);
    SampleApp_TxBuf[1] = CheckSum(SampleApp_TxBuf+2, len+1);
    SampleApp_TxBuf[3+len] = '$';
    SampleApp_TxBuf[4+len] = '@';
    SampleApp_TxLen = 5 + len;
    HalUARTWrite(0, SampleApp_TxBuf, SampleApp_TxLen);
}

/*********************************************************************
 * @fn      SampleApp_CallBack
 *
 * @brief   UART callback function.
 *
 * @param   port - UART port number
 * @param   event - UART event
 *
 * @return  none
 */
void SampleApp_CallBack(uint8 port, uint8 event)
{
    (void)port;  // 防止未使用参数警告

    if ((event & (HAL_UART_RX_FULL | HAL_UART_RX_ABOUT_FULL | HAL_UART_RX_TIMEOUT)) &&
#if SAMPLE_APP_LOOPBACK
        (SampleApp_TxLen < SAMPLE_APP_TX_MAX))
#else
        !SampleApp_TxLen)
#endif
    {
        SampleApp_TxLen += HalUARTRead(SAMPLE_APP_PORT,
                                       SampleApp_TxBuf + SampleApp_TxLen + 1,
                                       SAMPLE_APP_TX_MAX - SampleApp_TxLen);
    }
}