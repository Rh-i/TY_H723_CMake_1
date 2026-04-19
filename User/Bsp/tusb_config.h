#ifndef __TUSB_CONFIG_H__
#define __TUSB_CONFIG_H__

#ifdef __cplusplus
extern "C" 
{
#endif

#include "tusb_option.h"

#define CFG_TUD_ENABLED             1 
#define CFG_TUH_ENABLED             0

#define CFG_TUD_MAX_SPEED           TUSB_SPEED_FULL
#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE)

#define CFG_TUD_ENDPOINT0_SIZE      64

#define CFG_TUD_CDC                 1
#define CFG_TUD_CDC_RX_BUFSIZE      256
#define CFG_TUD_CDC_TX_BUFSIZE      256
#define CFG_TUD_CDC_EP_BUFSIZE      64

#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

#define CFG_TUD_DWC2_SLAVE_ENABLE   1
#define CFG_TUD_DWC2_DMA_ENABLE     0

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

#ifdef __cplusplus
}
#endif

#endif // __TUSB_CONFIG_H__
