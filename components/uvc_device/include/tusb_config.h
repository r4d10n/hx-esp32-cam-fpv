/*
 * TinyUSB Configuration for ESP32-S3 UVC Camera
 */

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// Board Configuration
//--------------------------------------------------------------------

#define BOARD_TUD_RHPORT            0
#define BOARD_TUD_MAX_SPEED         OPT_MODE_FULL_SPEED

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------

#define CFG_TUSB_MCU                OPT_MCU_ESP32S3
#define CFG_TUSB_OS                 OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG              0

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

//--------------------------------------------------------------------
// Device Mode Configuration
//--------------------------------------------------------------------

#define CFG_TUD_ENABLED             1
#define CFG_TUD_MAX_SPEED           OPT_MODE_FULL_SPEED
#define CFG_TUD_ENDPOINT0_SIZE      64

//--------------------------------------------------------------------
// Class Drivers
//--------------------------------------------------------------------

#define CFG_TUD_VIDEO               1
#define CFG_TUD_VIDEO_STREAMING     1

#define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

//--------------------------------------------------------------------
// Video Class Configuration
//--------------------------------------------------------------------

// Use bulk transfers for reliability
#define CFG_TUD_VIDEO_STREAMING_BULK    1

// Endpoint buffer size
#define CFG_TUD_VIDEO_EP_BUFSIZE        512

// Maximum payload size
#define CFG_TUD_VIDEO_STREAMING_EP_BUFSIZE  (512)

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
