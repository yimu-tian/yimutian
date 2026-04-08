/*********************************************************************
 * 路由节点代码（基于 SampleApp.c 修改）
 * 功能：采集温湿度、光照、MQ135、MQ7，通过 Zigbee 网络发送给协调器，
 *       同时自动转发其他节点的数据（路由功能由协议栈自动完成）。
 *********************************************************************/
#include <stdio.h>
#include <string.h>
#include "AF.h"
#include "OnBoard.h"
#include "OSAL_Tasks.h"
#include "SampleApp.h"
#include "ZDApp.h"
#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_uart.h"
#include "dht11.h"
#include "Sensor_ADC.h"

#define SAMPLE_APP_PORT     0
#define SAMPLE_APP_TX_MAX   80

// 事件定义
#define SAMPLEAPP_SEND_PERIODIC_MSG_EVT  0x0001

// Cluster ID 列表
const cId_t SampleApp_ClusterList[SAMPLE_MAX_CLUSTERS] = {
    SAMPLEAPP_P2P_CLUSTERID,
    SAMPLEAPP_PERIODIC_CLUSTERID
};

// 简单描述符
const SimpleDescriptionFormat_t SampleApp_SimpleDesc = {
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
endPointDesc_t SampleApp_epDesc = {
    SAMPLEAPP_ENDPOINT,
    &SampleApp_TaskID,
    (SimpleDescriptionFormat_t *)&SampleApp_SimpleDesc,
    noLatencyReqs
};

// 传感器数据结构（与协调器解析一致）
typedef struct {
    uint8 id;           // 节点 ID（可设为路由节点的编号）
    uint8 temperature;
    uint8 humidity;
    uint16 light;
    uint16 mq135;
    uint16 mq7;
} sensor_data_t;

devStates_t SampleApp_NwkState;
uint8 SampleApp_TaskID;

static uint8 SampleApp_MsgID;
afAddrType_t SampleApp_P2P_DstAddr;    // 目标地址（协调器）

// 函数声明
static void SampleApp_Send_P2P_Message(void);

/*********************************************************************
 * @fn      SampleApp_Init
 * @brief   路由节点初始化：ADC、DHT11、注册端点、启动入网
 *********************************************************************/
void SampleApp_Init( uint8 task_id )
{
    SampleApp_TaskID = task_id;
    SampleApp_NwkState = DEV_INIT;

    // 初始化传感器硬件
    ADC_Init();          // P0_4,P0_5,P0_6 设为 ADC 输入
    // DHT11 引脚 P0_7 已在 DHT11.h 中配置

    // 注册端点
    afRegister(&SampleApp_epDesc);

    // 配置发送目标地址（协调器短地址固定为 0x0000）
    SampleApp_P2P_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
    SampleApp_P2P_DstAddr.endPoint = SAMPLEAPP_ENDPOINT;
    SampleApp_P2P_DstAddr.addr.shortAddr = 0x0000;

    // 可选：通过串口打印路由节点启动信息
    HalUARTWrite(0, (uint8*)"Router Node Started\r\n", 21);
}

/*********************************************************************
 * @fn      SampleApp_ProcessEvent
 * @brief   事件处理：入网状态变化、定时发送数据
 *********************************************************************/
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
            case ZDO_STATE_CHANGE:
                SampleApp_NwkState = (devStates_t)(MSGpkt->hdr.status);
                if (SampleApp_NwkState == DEV_ROUTER || SampleApp_NwkState == DEV_END_DEVICE)
                {
                    // 入网成功，启动定时器（每 5 秒发送一次，避免网络拥塞）
                    osal_start_timerEx(SampleApp_TaskID,
                                       SAMPLEAPP_SEND_PERIODIC_MSG_EVT,
                                       5000);
                    HalUARTWrite(0, (uint8*)"Joined Network\r\n", 16);
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
        SampleApp_Send_P2P_Message();
        // 重新启动定时器（周期 5 秒）
        osal_start_timerEx(SampleApp_TaskID, SAMPLEAPP_SEND_PERIODIC_MSG_EVT, 5000);
        return (events ^ SAMPLEAPP_SEND_PERIODIC_MSG_EVT);
    }

    return 0;
}

/*********************************************************************
 * @fn      SampleApp_Send_P2P_Message
 * @brief   采集数据并发送给协调器
 *********************************************************************/
void SampleApp_Send_P2P_Message(void)
{
    sensor_data_t data;
    uint8 buf[sizeof(sensor_data_t)];

    // 读取 DHT11 温湿度
    DHT11();
    data.id = 2;                     // 路由节点 ID（可自定义，如 2）
    data.temperature = wendu;
    data.humidity = shidu;
    data.light = Read_Light();
    data.mq135 = Read_MQ135();
    data.mq7 = Read_MQ7();

    // 拷贝数据
    osal_memcpy(buf, &data, sizeof(data));

    // 串口打印调试信息
    char strTemp[60];
    sprintf(strTemp, "Router Send: ID=%d T=%d H=%d L=%d M135=%d M7=%d\r\n",
            data.id, data.temperature, data.humidity,
            data.light, data.mq135, data.mq7);
    HalUARTWrite(0, (uint8*)strTemp, strlen(strTemp));

    // 无线发送
    if (AF_DataRequest(&SampleApp_P2P_DstAddr, &SampleApp_epDesc,
                       SAMPLEAPP_P2P_CLUSTERID,
                       sizeof(data), buf,
                       &SampleApp_MsgID,
                       AF_DISCV_ROUTE, AF_DEFAULT_RADIUS) == afStatus_SUCCESS)
    {
        // 发送成功，可点亮 LED 指示（如 P1_0 闪烁）
        HalLedBlink(HAL_LED_1, 1, 50, 100);
    }
    else
    {
        HalUARTWrite(0, (uint8*)"Send failed\r\n", 13);
    }
}