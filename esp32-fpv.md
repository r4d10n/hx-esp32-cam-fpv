# ESP32 FPV Ground Station - USB NCM Implementation

## Overview

This project implements a USB NCM (Network Control Model) network interface for the ESP32-S3 FPV ground station, allowing the web interface to be accessed over USB in addition to WiFi AP.

## Components

### Main Project: `esp32-gs/`
The full FPV ground station with:
- Promiscuous WiFi packet capture (always enabled)
- Optional WiFi AP for wireless access
- Optional USB NCM for wired USB network access
- Web server with WebSocket MJPEG streaming

### Test Project: `esp32-gs-usb-test/`
Standalone USB NCM + CDC composite test for validating USB functionality before integration.

## USB NCM Architecture

### Dependencies
```yaml
# idf_component.yml
dependencies:
  chegewara/usb-netif: "*"      # USB NCM + esp_netif + DHCP server integration
  espressif/esp_tinyusb: "^1.4.0"
  idf: "^5.0"
```

### Key Component: chegewara/usb-netif
This component provides proper integration between:
- TinyUSB NCM device class
- ESP-IDF esp_netif (lwIP network stack)
- DHCP server (assigns IP to connected host)

Source: https://github.com/esp32-open-source/usb-components/tree/master/usb-netif

### Default IP Configuration
- Device IP: `192.168.4.1`
- DHCP range: `192.168.4.2` - `192.168.4.254`
- Configured via usb-netif component's Kconfig options

## USB Configuration

### Critical: GPIO19/20 Conflict
ESP32-S3 shares GPIO19/20 between USB Serial/JTAG and USB OTG. **USB Serial/JTAG must be disabled** for USB OTG to work:

```
# sdkconfig.defaults
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=n
CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
```

### TinyUSB Configuration
```
CONFIG_TINYUSB_ENABLED=y
CONFIG_TINYUSB_NET_MODE_NCM=y
CONFIG_TINYUSB_NET_ENABLED=y
CONFIG_TINYUSB_DESC_CUSTOM_VID=0x303A
CONFIG_TINYUSB_DESC_CUSTOM_PID=0x4000
```

## CDC + NCM Composite Device

To have both USB serial console AND USB network on the same USB connection:

### The Problem
`CONFIG_ESP_CONSOLE_USB_CDC=y` initializes TinyUSB early (before app_main) with CDC-only configuration, preventing NCM from being added later.

### The Solution
1. **Disable** `CONFIG_ESP_CONSOLE_USB_CDC`
2. Use UART for boot messages
3. Initialize TinyUSB ourselves in app_main with CDC+NCM composite
4. Redirect stdout to CDC after initialization

### Configuration for CDC+NCM
```
# sdkconfig.defaults
CONFIG_ESP_CONSOLE_UART_DEFAULT=y
CONFIG_ESP_CONSOLE_USB_CDC=n

CONFIG_TINYUSB_ENABLED=y
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_CDC_COUNT=1
CONFIG_TINYUSB_NET_MODE_NCM=y
CONFIG_TINYUSB_NET_ENABLED=y
CONFIG_TINYUSB_DESC_CUSTOM_PID=0x4001  # Different PID for composite
```

### Code for CDC+NCM Composite
```c
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tusb_console.h"
#include "usb_netif.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize TinyUSB with CDC+NCM composite
    ESP_ERROR_CHECK(init_tinyusb(NULL));

    // Initialize CDC ACM and redirect console
    tinyusb_config_cdcacm_t cdc_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&cdc_cfg));
    ESP_ERROR_CHECK(esp_tusb_init_console(TINYUSB_CDC_ACM_0));

    // Initialize NCM network with DHCP server
    ESP_ERROR_CHECK(usb_net_create(NULL));
    esp_netif_t *netif = netif_create(NULL, NULL, NULL);
    esp_netif_action_start(netif, 0, 0, 0);

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_ETH);
    esp_netif_set_mac(netif, mac);
}
```

## NCM-Only Mode (Simpler)

For NCM network without CDC console:

```c
#include "usb_netif.h"

void app_main(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // Single call handles everything: TinyUSB, NCM, esp_netif, DHCP
    esp_netif_t *netif = usb_ip_init_default_config();
}
```

## Host Setup (Linux)

After connecting USB-OTG port:

```bash
# Check USB enumeration
sudo dmesg -w

# Expected output for NCM:
# cdc_ncm 5-2:1.0: MAC-Address: xx:xx:xx:xx:xx:xx
# cdc_ncm 5-2:1.0 usb0: register 'cdc_ncm'

# Get IP from DHCP
sudo dhclient <interface>  # e.g., enp6s0f4u2 or usb0

# Access web interface
curl http://192.168.4.1/

# For CDC+NCM composite, also see:
# /dev/ttyACM0 for serial console
picocom /dev/ttyACM0 -b 115200
```

## File Structure

```
esp32-gs/
├── main/
│   ├── idf_component.yml    # chegewara/usb-netif + esp_tinyusb
│   ├── Kconfig.projbuild    # FPV_GS_ENABLE_USB_NET option
│   ├── usb_network.c        # USB NCM wrapper using usb-netif
│   ├── usb_network.h
│   └── main.c               # Calls usb_network_init() when enabled
├── sdkconfig.defaults       # USB config, TinyUSB NCM enabled

esp32-gs-usb-test/
├── main/
│   ├── idf_component.yml    # chegewara/usb-netif + esp_tinyusb
│   └── main.c               # CDC+NCM composite test
├── sdkconfig.defaults       # CDC+NCM configuration
```

## Troubleshooting

### Error -71 during USB enumeration
- USB Serial/JTAG is still enabled, conflicting with USB OTG
- Fix: Ensure `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=n`

### NCM not appearing (only CDC)
- `CONFIG_ESP_CONSOLE_USB_CDC` is enabled, initializing TinyUSB too early
- Fix: Disable it and initialize TinyUSB manually in app_main

### DHCP not assigning IP
- DHCP server not started
- Fix: Ensure `esp_netif_action_start()` is called after netif creation

## References

- [usb-netif component](https://components.espressif.com/components/chegewara/usb-netif)
- [usb-netif-example](https://github.com/esp32-open-source/usb-netif-example)
- [ESP-IDF tusb_ncm example](https://github.com/espressif/esp-idf/tree/v5.5.1/examples/peripherals/usb/device/tusb_ncm)
- [esp_tinyusb component](https://components.espressif.com/components/espressif/esp_tinyusb)
