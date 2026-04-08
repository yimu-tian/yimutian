#ifndef SAMPLEPP_H
#define SAMPLEPP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ZComDef.h"

#define SAMPLEAPP_ENDPOINT           11
#define SAMPLEAPP_PROFID             0x0F05
#define SAMPLEAPP_DEVICEID           0x0001
#define SAMPLEAPP_DEVICE_VERSION     0
#define SAMPLEAPP_FLAGS              0
#define SAMPLE_MAX_CLUSTERS          2
#define SAMPLEAPP_P2P_CLUSTERID      1
#define SAMPLEAPP_PERIODIC_CLUSTERID 2

#define SAMPLEAPP_SEND_PERIODIC_MSG_TIMEOUT   3000
#define SAMPLEAPP_SEND_PERIODIC_MSG_EVT       0x0001

#define SAMPLEAPP_FLASH_GROUP                 0x0001

#define FUN_CODE_SET_DATA          0x01
#define FUN_CODE_UPDATA_DATA       0x02

extern byte SampleApp_TaskID;

extern void SampleApp_Init( byte task_id );
extern UINT16 SampleApp_ProcessEvent( byte task_id, UINT16 events );

#ifdef __cplusplus
}
#endif

#endif /* SERIALAPP_H */
