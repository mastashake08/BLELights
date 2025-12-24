# BLELights

ESP32-C6 BLE-controlled RGB LED with Photo Slideshow Display

![Platform](https://img.shields.io/badge/platform-ESP32--C6-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-00979D)
![License](https://img.shields.io/badge/license-MIT-green)

## Overview

BLELights is a feature-rich embedded application for the Waveshare ESP32-C6-LCD-1.47 board that combines:
- **BLE Control**: Wirelessly control an RGB LED via Bluetooth Low Energy
- **Photo Slideshow**: Display JPEG images from SD card with smooth fade transitions
- **Dual Operation**: LED continues cycling colors while photos are displayed
- **LVGL UI**: Beautiful graphical interface with real-time status display

## Hardware Requirements

- **Board**: [Waveshare ESP32-C6-LCD-1.47](https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47)
  - ESP32-C6 RISC-V microcontroller
  - 1.47" ST7789 LCD (172x320 pixels)
  - Onboard RGB LED (WS2812/NeoPixel)
  - TF/SD card slot
- **Accessories**:
  - USB-C cable (data-capable)
  - MicroSD/TF card (FAT32 formatted)
  - JPEG images (max 172x320 pixels recommended)

## Features

### 🎨 BLE LED Control
- Connect via Bluetooth (device name: `ESP32C6-LED`)
- Send RGB values to control LED color
- Smooth color transitions with fade effects
- Automatic random color cycling when disconnected

### 📸 Photo Viewer
- Automatic detection of JPEG files on SD card
- Smooth fade in/out transitions between photos
- Configurable slideshow timing (default: 3 seconds)
- Supports photos up to display resolution

### 🎯 Dual-Mode Operation
- **Photo Mode**: Displays slideshow while LED cycles independently
- **LED Control Mode**: Shows RGB values and connection status on display
- Automatic fallback if no SD card detected

## Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| LCD MOSI | 6    | SPI data out |
| LCD SCLK | 7    | SPI clock |
| LCD MISO | 5    | SPI data in (for SD) |
| LCD CS   | 14   | LCD chip select |
| LCD DC   | 15   | Data/command |
| LCD RST  | 21   | Reset |
| LCD BL   | 22   | Backlight (PWM) |
| SD CS    | 11   | SD card chip select |
| RGB LED  | 8    | WS2812 data |

## Installation

### Prerequisites
- [PlatformIO](https://platformio.org/) installed
- Git (optional)

### Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/mastashake08/BLELights.git
   cd BLELights
   ```

2. **Prepare SD card**
   - Format SD card as FAT32
   - Copy JPEG images to the root directory
   - Insert card into the board's TF slot

3. **Build and upload**
   ```bash
   pio run --target upload
   ```

4. **Monitor serial output** (optional)
   ```bash
   pio device monitor
   ```

## Usage

### Photo Slideshow
1. Insert SD card with JPEG images
2. Power on the device
3. Photos will automatically cycle with fade transitions
4. LED continues cycling random colors

### BLE LED Control
1. Scan for BLE devices on your phone/computer
2. Connect to `ESP32C6-LED`
3. Send 3 bytes (RGB values) to control the LED:
   - Service UUID: `12345678-1234-1234-1234-123456789012`
   - Characteristic UUID: `87654321-4321-4321-4321-210987654321`

**Example (Python with bleak):**
```python
import asyncio
from bleak import BleakClient

async def set_color(red, green, blue):
    async with BleakClient("ESP32C6-LED") as client:
        await client.write_gatt_char(
            "87654321-4321-4321-4321-210987654321",
            bytes([red, green, blue])
        )

asyncio.run(set_color(255, 0, 128))  # Bright pink
```

### Configuration

Edit `src/main.cpp` to customize:

```cpp
// Photo slideshow interval (milliseconds)
const unsigned long PHOTO_CHANGE_INTERVAL = 3000;

// Fade transition duration (milliseconds)
const int FADE_DURATION = 500;

// LED color change interval (milliseconds)
const unsigned long COLOR_CHANGE_INTERVAL = 3000;

// LED fade speed (0.1 to 1.0)
const float FADE_SPEED = 0.5;
```

## Development

### Project Structure
```
BLELights/
├── src/
│   └── main.cpp           # Main application code
├── include/
│   ├── PhotoViewer.h      # SD card & JPEG viewer
│   └── lv_conf.h          # LVGL configuration
├── lib/                   # Local libraries
├── platformio.ini         # Build configuration
└── .github/workflows/
    └── release.yml        # Automated releases
```

### Building from Source

```bash
# Clean build
pio run --target clean

# Build only
pio run

# Upload and monitor
pio run --target upload && pio device monitor
```

### Dependencies

The following libraries are automatically installed by PlatformIO:
- [LVGL 8.3.11](https://lvgl.io/) - Graphics library
- [Adafruit NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) - RGB LED control
- [TJpg_Decoder](https://github.com/Bodmer/TJpg_Decoder) - JPEG image decoding

## Release Process

Automated releases are created via GitHub Actions when you push a version tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

This automatically:
1. Builds the firmware
2. Creates a GitHub release
3. Attaches `.bin` and `.elf` files

Download pre-built firmware from [Releases](https://github.com/mastashake08/BLELights/releases).

## Flashing Pre-built Firmware

Using esptool:
```bash
esptool.py --chip esp32c6 --port /dev/ttyUSB0 write_flash 0x0 BLELights-v1.0.0.bin
```

Or use the Arduino IDE / PlatformIO upload tools.

## Troubleshooting

### Display not working
- Verify pin connections match the hardware
- Check that backlight is enabled (GPIO 22)
- Ensure SPI speed is compatible (80MHz)

### SD card not detected
- Format card as FAT32
- Check card is fully inserted
- Verify CS pin (GPIO 11) connections

### BLE not connecting
- Ensure device is advertising (check serial output)
- Phone/computer Bluetooth must be enabled
- Try restarting the device

### Photos not displaying
- Use JPEG format only (PNG not supported)
- Keep image dimensions ≤ 172x320 pixels
- Verify files are in root directory of SD card

## Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Submit a pull request

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- [Waveshare](https://www.waveshare.com/) for excellent hardware documentation
- [LVGL](https://lvgl.io/) team for the graphics library
- ESP32 community for examples and support

## Resources

- [Waveshare ESP32-C6-LCD-1.47 Wiki](https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47)
- [ESP32-C6 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c6_datasheet_en.pdf)
- [LVGL Documentation](https://docs.lvgl.io/)
- [PlatformIO Documentation](https://docs.platformio.org/)

---

**Made with ❤️ for the ESP32 community**
