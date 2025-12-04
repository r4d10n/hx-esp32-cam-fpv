/*
 * USB Descriptors for ESP32-S3 UVC Camera
 *
 * Defines USB Video Class descriptors for MJPEG streaming.
 */

#include "tusb.h"
#include <string.h>

//--------------------------------------------------------------------
// USB VID/PID
//--------------------------------------------------------------------

#define USB_VID     0x303A  // Espressif VID
#define USB_PID     0x8000  // UVC Camera

//--------------------------------------------------------------------
// Endpoint Configuration
//--------------------------------------------------------------------

#define EPNUM_VIDEO_IN  0x81

//--------------------------------------------------------------------
// Interface Numbers
//--------------------------------------------------------------------

#define ITF_NUM_VIDEO_CONTROL       0
#define ITF_NUM_VIDEO_STREAMING     1
#define ITF_NUM_TOTAL               2

//--------------------------------------------------------------------
// UVC Specific Defines
//--------------------------------------------------------------------

// Video Interface Subclass
#define UVC_SC_VIDEOCONTROL         0x01
#define UVC_SC_VIDEOSTREAMING       0x02
#define UVC_SC_VIDEO_INTERFACE_COLLECTION 0x03

// Video Interface Protocol
#define UVC_PC_PROTOCOL_15          0x01

// Video Class-Specific Descriptor Types
#define UVC_CS_INTERFACE            0x24
#define UVC_CS_ENDPOINT             0x25

// VC Interface Descriptor Subtypes
#define UVC_VC_HEADER               0x01
#define UVC_VC_INPUT_TERMINAL       0x02
#define UVC_VC_OUTPUT_TERMINAL      0x03
#define UVC_VC_PROCESSING_UNIT      0x05

// VS Interface Descriptor Subtypes
#define UVC_VS_INPUT_HEADER         0x01
#define UVC_VS_FORMAT_MJPEG         0x06
#define UVC_VS_FRAME_MJPEG          0x07
#define UVC_VS_COLORFORMAT          0x0D

// Terminal Types
#define UVC_ITT_CAMERA              0x0201
#define UVC_TT_STREAMING            0x0101

// Terminal/Unit IDs
#define UVC_IT_ID                   1
#define UVC_PU_ID                   2
#define UVC_OT_ID                   3

// Frame intervals (100ns units)
#define FRAME_INTERVAL_30FPS        333333
#define FRAME_INTERVAL_15FPS        666666
#define FRAME_INTERVAL_10FPS        1000000

//--------------------------------------------------------------------
// Descriptor Lengths
//--------------------------------------------------------------------

#define UVC_VC_HEADER_LEN           13
#define UVC_VC_IT_LEN               18
#define UVC_VC_PU_LEN               13
#define UVC_VC_OT_LEN               9

#define UVC_VC_TOTAL_LEN    (UVC_VC_HEADER_LEN + UVC_VC_IT_LEN + UVC_VC_PU_LEN + UVC_VC_OT_LEN)

#define UVC_VS_HEADER_LEN           14
#define UVC_VS_FORMAT_LEN           11
#define UVC_VS_FRAME_LEN            38
#define UVC_VS_COLOR_LEN            6

#define UVC_VS_TOTAL_LEN    (UVC_VS_HEADER_LEN + UVC_VS_FORMAT_LEN + 3*UVC_VS_FRAME_LEN + UVC_VS_COLOR_LEN)

// Configuration total length
#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + \
                             8 + \
                             9 + UVC_VC_TOTAL_LEN + \
                             9 + UVC_VS_TOTAL_LEN + \
                             9 + 7)

//--------------------------------------------------------------------
// Device Descriptor
//--------------------------------------------------------------------

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*)&desc_device;
}

//--------------------------------------------------------------------
// Configuration Descriptor
//--------------------------------------------------------------------

uint8_t const desc_configuration[] = {
    // Configuration Descriptor
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 500),

    // Interface Association Descriptor (IAD)
    8,                                          // bLength
    TUSB_DESC_INTERFACE_ASSOCIATION,            // bDescriptorType
    ITF_NUM_VIDEO_CONTROL,                      // bFirstInterface
    2,                                          // bInterfaceCount
    TUSB_CLASS_VIDEO,                           // bFunctionClass
    UVC_SC_VIDEO_INTERFACE_COLLECTION,          // bFunctionSubClass
    UVC_PC_PROTOCOL_15,                         // bFunctionProtocol
    0x00,                                       // iFunction

    //---- Video Control Interface ----
    // Standard VC Interface Descriptor
    9,                                          // bLength
    TUSB_DESC_INTERFACE,                        // bDescriptorType
    ITF_NUM_VIDEO_CONTROL,                      // bInterfaceNumber
    0x00,                                       // bAlternateSetting
    0,                                          // bNumEndpoints
    TUSB_CLASS_VIDEO,                           // bInterfaceClass
    UVC_SC_VIDEOCONTROL,                        // bInterfaceSubClass
    UVC_PC_PROTOCOL_15,                         // bInterfaceProtocol
    0x00,                                       // iInterface

    // Class-specific VC Interface Header Descriptor
    UVC_VC_HEADER_LEN,                          // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VC_HEADER,                              // bDescriptorSubtype
    U16_TO_U8S_LE(0x0150),                     // bcdUVC 1.5
    U16_TO_U8S_LE(UVC_VC_TOTAL_LEN),           // wTotalLength
    U32_TO_U8S_LE(48000000),                   // dwClockFrequency (48MHz)
    1,                                          // bInCollection
    ITF_NUM_VIDEO_STREAMING,                    // baInterfaceNr

    // Input Terminal Descriptor (Camera)
    UVC_VC_IT_LEN,                              // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VC_INPUT_TERMINAL,                      // bDescriptorSubtype
    UVC_IT_ID,                                  // bTerminalID
    U16_TO_U8S_LE(UVC_ITT_CAMERA),             // wTerminalType
    0x00,                                       // bAssocTerminal
    0x00,                                       // iTerminal
    U16_TO_U8S_LE(0),                          // wObjectiveFocalLengthMin
    U16_TO_U8S_LE(0),                          // wObjectiveFocalLengthMax
    U16_TO_U8S_LE(0),                          // wOcularFocalLength
    3,                                          // bControlSize
    0x00, 0x00, 0x00,                          // bmControls

    // Processing Unit Descriptor
    UVC_VC_PU_LEN,                              // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VC_PROCESSING_UNIT,                     // bDescriptorSubtype
    UVC_PU_ID,                                  // bUnitID
    UVC_IT_ID,                                  // bSourceID
    U16_TO_U8S_LE(0),                          // wMaxMultiplier
    3,                                          // bControlSize
    0x00, 0x00, 0x00,                          // bmControls
    0x00,                                       // iProcessing

    // Output Terminal Descriptor
    UVC_VC_OT_LEN,                              // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VC_OUTPUT_TERMINAL,                     // bDescriptorSubtype
    UVC_OT_ID,                                  // bTerminalID
    U16_TO_U8S_LE(UVC_TT_STREAMING),           // wTerminalType
    0x00,                                       // bAssocTerminal
    UVC_PU_ID,                                  // bSourceID
    0x00,                                       // iTerminal

    //---- Video Streaming Interface (Alt 0 - Zero Bandwidth) ----
    9,                                          // bLength
    TUSB_DESC_INTERFACE,                        // bDescriptorType
    ITF_NUM_VIDEO_STREAMING,                    // bInterfaceNumber
    0x00,                                       // bAlternateSetting
    0,                                          // bNumEndpoints
    TUSB_CLASS_VIDEO,                           // bInterfaceClass
    UVC_SC_VIDEOSTREAMING,                      // bInterfaceSubClass
    UVC_PC_PROTOCOL_15,                         // bInterfaceProtocol
    0x00,                                       // iInterface

    // Class-specific VS Input Header Descriptor
    UVC_VS_HEADER_LEN,                          // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_INPUT_HEADER,                        // bDescriptorSubtype
    1,                                          // bNumFormats
    U16_TO_U8S_LE(UVC_VS_TOTAL_LEN),           // wTotalLength
    EPNUM_VIDEO_IN,                             // bEndpointAddress
    0x00,                                       // bmInfo
    UVC_OT_ID,                                  // bTerminalLink
    0x01,                                       // bStillCaptureMethod
    0x00,                                       // bTriggerSupport
    0x00,                                       // bTriggerUsage
    1,                                          // bControlSize
    0x00,                                       // bmaControls

    // MJPEG Format Descriptor
    UVC_VS_FORMAT_LEN,                          // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_FORMAT_MJPEG,                        // bDescriptorSubtype
    1,                                          // bFormatIndex
    3,                                          // bNumFrameDescriptors
    0x01,                                       // bmFlags (fixed samples)
    1,                                          // bDefaultFrameIndex
    0x00,                                       // bAspectRatioX
    0x00,                                       // bAspectRatioY
    0x00,                                       // bmInterlaceFlags
    0x00,                                       // bCopyProtect

    // Frame 1: 640x480 (VGA)
    UVC_VS_FRAME_LEN,                           // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_FRAME_MJPEG,                         // bDescriptorSubtype
    1,                                          // bFrameIndex
    0x00,                                       // bmCapabilities
    U16_TO_U8S_LE(640),                        // wWidth
    U16_TO_U8S_LE(480),                        // wHeight
    U32_TO_U8S_LE(640*480*16),                 // dwMinBitRate
    U32_TO_U8S_LE(640*480*16*30),              // dwMaxBitRate
    U32_TO_U8S_LE(100*1024),                   // dwMaxVideoFrameBufferSize
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // dwDefaultFrameInterval
    3,                                          // bFrameIntervalType
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // 30 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_15FPS),       // 15 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_10FPS),       // 10 fps

    // Frame 2: 800x600 (SVGA)
    UVC_VS_FRAME_LEN,                           // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_FRAME_MJPEG,                         // bDescriptorSubtype
    2,                                          // bFrameIndex
    0x00,                                       // bmCapabilities
    U16_TO_U8S_LE(800),                        // wWidth
    U16_TO_U8S_LE(600),                        // wHeight
    U32_TO_U8S_LE(800*600*16),                 // dwMinBitRate
    U32_TO_U8S_LE(800*600*16*30),              // dwMaxBitRate
    U32_TO_U8S_LE(100*1024),                   // dwMaxVideoFrameBufferSize
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // dwDefaultFrameInterval
    3,                                          // bFrameIntervalType
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // 30 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_15FPS),       // 15 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_10FPS),       // 10 fps

    // Frame 3: 1280x720 (HD)
    UVC_VS_FRAME_LEN,                           // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_FRAME_MJPEG,                         // bDescriptorSubtype
    3,                                          // bFrameIndex
    0x00,                                       // bmCapabilities
    U16_TO_U8S_LE(1280),                       // wWidth
    U16_TO_U8S_LE(720),                        // wHeight
    U32_TO_U8S_LE(1280*720*16),                // dwMinBitRate
    U32_TO_U8S_LE(1280*720*16*30),             // dwMaxBitRate
    U32_TO_U8S_LE(150*1024),                   // dwMaxVideoFrameBufferSize
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // dwDefaultFrameInterval
    3,                                          // bFrameIntervalType
    U32_TO_U8S_LE(FRAME_INTERVAL_30FPS),       // 30 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_15FPS),       // 15 fps
    U32_TO_U8S_LE(FRAME_INTERVAL_10FPS),       // 10 fps

    // Color Matching Descriptor
    UVC_VS_COLOR_LEN,                           // bLength
    UVC_CS_INTERFACE,                           // bDescriptorType
    UVC_VS_COLORFORMAT,                         // bDescriptorSubtype
    1,                                          // bColorPrimaries (BT.709)
    1,                                          // bTransferCharacteristics
    4,                                          // bMatrixCoefficients (SMPTE 170M)

    //---- Video Streaming Interface (Alt 1 - Data) ----
    9,                                          // bLength
    TUSB_DESC_INTERFACE,                        // bDescriptorType
    ITF_NUM_VIDEO_STREAMING,                    // bInterfaceNumber
    0x01,                                       // bAlternateSetting
    1,                                          // bNumEndpoints
    TUSB_CLASS_VIDEO,                           // bInterfaceClass
    UVC_SC_VIDEOSTREAMING,                      // bInterfaceSubClass
    UVC_PC_PROTOCOL_15,                         // bInterfaceProtocol
    0x00,                                       // iInterface

    // Bulk Endpoint Descriptor (more reliable than isochronous)
    7,                                          // bLength
    TUSB_DESC_ENDPOINT,                         // bDescriptorType
    EPNUM_VIDEO_IN,                             // bEndpointAddress
    TUSB_XFER_BULK,                             // bmAttributes
    U16_TO_U8S_LE(512),                        // wMaxPacketSize
    0x00,                                       // bInterval
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

//--------------------------------------------------------------------
// String Descriptors
//--------------------------------------------------------------------

static char const* string_desc_arr[] = {
    (const char[]) { 0x09, 0x04 },      // 0: Language ID (English)
    "Espressif",                        // 1: Manufacturer
    "ESP32-S3 UVC Camera",              // 2: Product
    "123456",                           // 3: Serial
};

static uint16_t _desc_str[32 + 1];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
        case 0:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        default:
            if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) {
                return NULL;
            }

            const char* str = string_desc_arr[index];
            chr_count = strlen(str);
            if (chr_count > 31) chr_count = 31;

            for (size_t i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = str[i];
            }
            break;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return _desc_str;
}
