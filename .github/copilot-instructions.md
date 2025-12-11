# BLELights - AI Coding Agent Instructions

## Project Overview
PlatformIO-based embedded project targeting the **Adafruit Feather ESP32-C6** board. The project is intended for BLE-controlled lighting applications (currently in early development stages with boilerplate Arduino code).

## Platform & Hardware
- **Board**: Adafruit Feather ESP32-C6
- **Framework**: Arduino
- **Platform**: ESP32 via custom platform package from pioarduino
  - Uses: `https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip`
  - This is NOT the standard ESP32 platform - it's a fork supporting the ESP32-C6 variant

## Build System (PlatformIO)

### Essential Commands
```bash
# Build the project
pio run

# Upload to device (ensure device is connected via USB)
pio run --target upload

# Clean build artifacts
pio run --target clean

# Open serial monitor
pio device monitor

# Build + Upload + Monitor in one command
pio run --target upload && pio device monitor
```

### Project Structure
- `src/main.cpp` - Main application code (Arduino setup/loop pattern)
- `include/` - Header files for project-specific code
- `lib/` - Local libraries (private, not in PlatformIO registry)
- `platformio.ini` - Build configuration, board definition, dependencies
- `.pio/` - Build artifacts (gitignored, do not edit)

## Code Conventions

### Arduino Framework Patterns
- Use `setup()` for initialization (runs once on boot)
- Use `loop()` for continuous execution (runs repeatedly)
- Prefer Arduino libraries (e.g., `Serial`, `pinMode`) for portability
- All Arduino code should `#include <Arduino.h>`

### ESP32-C6 Specific Notes
- This is a RISC-V based chip (not Xtensa like older ESP32 variants)
- Has native USB support - serial communication works over USB-C directly
- BLE 5.0 capable with extended advertising features
- When adding BLE functionality, use ESP32 BLE Arduino libraries compatible with C6 variant

## Adding Dependencies

Add libraries to `platformio.ini`:
```ini
[env:adafruit_feather_esp32c6]
lib_deps =
    adafruit/Adafruit NeoPixel @ ^1.11.0
    h2zero/NimBLE-Arduino @ ^1.4.1
```

Or install via CLI:
```bash
pio pkg install --library "adafruit/Adafruit NeoPixel"
```

## Development Workflow

1. **Make code changes** in `src/` or create headers in `include/`
2. **Build locally** with `pio run` to catch compilation errors
3. **Upload to device** with `pio run --target upload`
4. **Debug via serial** with `pio device monitor` (baudrate defaults to 115200)

### Debugging
- Use `Serial.begin(115200)` in `setup()` and `Serial.println()` for debugging
- Monitor output: `pio device monitor` or `pio device monitor --baud 115200`
- Press `Ctrl+C` to exit monitor

## Common Pitfalls

- **Upload failures**: Ensure only one serial monitor is open, USB cable supports data (not charge-only)
- **Platform not found**: The custom ESP32 platform URL must be exactly as specified in platformio.ini
- **Library compatibility**: Verify libraries support ESP32-C6/RISC-V (some older ESP32 libraries may not work)
- **GPIO conflicts**: ESP32-C6 has different GPIO assignments than ESP32/ESP32-S3 - consult [Adafruit Feather ESP32-C6 pinout](https://learn.adafruit.com/adafruit-feather-esp32-c6)

## References
- [PlatformIO ESP32 Platform](https://docs.platformio.org/en/latest/platforms/espressif32.html)
- [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32)
- [Adafruit Feather ESP32-C6 Guide](https://learn.adafruit.com/adafruit-feather-esp32-c6)
