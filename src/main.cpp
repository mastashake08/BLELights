#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <lvgl.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>

// Pin definitions for Waveshare ESP32-C6-LCD-1.47
#define RGB_LED_PIN 8      // RGB LED pin
#define RGB_LED_COUNT 1    // Single RGB LED on board

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"

// Color variables
float currentRed = 0.0;
float currentGreen = 0.0;
float currentBlue = 0.0;
float targetRed = 0.0;
float targetGreen = 0.0;
float targetBlue = 0.0;
bool bleConnected = false;
bool bleColorReceived = false;
unsigned long lastColorChange = 0;
const unsigned long COLOR_CHANGE_INTERVAL = 3000; // Change target color every 3 seconds
const float FADE_SPEED = 0.5; // Speed of color transition (lower = slower)

// Display pins for Waveshare ESP32-C6-LCD-1.47
#define TFT_MOSI 7
#define TFT_SCLK 6
#define TFT_CS   10
#define TFT_DC   2
#define TFT_RST  -1
#define TFT_BL   15

// LVGL configuration
#define SCREEN_WIDTH  172
#define SCREEN_HEIGHT 320
#define LVGL_BUFFER_SIZE (SCREEN_WIDTH * 40)

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[LVGL_BUFFER_SIZE];
static lv_disp_drv_t disp_drv;

// LVGL UI elements
lv_obj_t* colorBox = nullptr;
lv_obj_t* labelR = nullptr;
lv_obj_t* labelG = nullptr;
lv_obj_t* labelB = nullptr;
lv_obj_t* labelStatus = nullptr;
lv_obj_t* labelMode = nullptr;

// Hardware objects
Adafruit_NeoPixel rgbLED(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

// BLE Server Callbacks
class ServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    bleConnected = true;
    Serial.println("BLE Client connected");
  }
  
  void onDisconnect(BLEServer* pServer) {
    bleConnected = false;
    Serial.println("BLE Client disconnected");
    // Restart advertising
    BLEDevice::startAdvertising();
  }
};

// BLE Characteristic Callbacks
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue();
    
    if (value.length() >= 3) {
      targetRed = (uint8_t)value[0];
      targetGreen = (uint8_t)value[1];
      targetBlue = (uint8_t)value[2];
      bleColorReceived = true;
      
      Serial.printf("BLE Color received: R=%d, G=%d, B=%d\n", 
                    (uint8_t)targetRed, (uint8_t)targetGreen, (uint8_t)targetBlue);
    }
  }
};

// ST7789 SPI display flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  
  // Set window
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2A); // Column address set
  digitalWrite(TFT_DC, HIGH);
  SPI.write16((area->x1 >> 8) & 0xFF);
  SPI.write16(area->x1 & 0xFF);
  SPI.write16((area->x2 >> 8) & 0xFF);
  SPI.write16(area->x2 & 0xFF);
  
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2B); // Row address set
  digitalWrite(TFT_DC, HIGH);
  SPI.write16((area->y1 >> 8) & 0xFF);
  SPI.write16(area->y1 & 0xFF);
  SPI.write16((area->y2 >> 8) & 0xFF);
  SPI.write16(area->y2 & 0xFF);
  
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2C); // Memory write
  digitalWrite(TFT_DC, HIGH);
  
  uint32_t size = w * h;
  uint16_t *p = (uint16_t *)color_p;
  while (size--) {
    SPI.write16(*p++);
  }
  
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  
  lv_disp_flush_ready(disp);
}

void initDisplay() {
  // Configure SPI pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TFT_BL, HIGH); // Turn on backlight
  
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  
  // ST7789 initialization sequence
  SPI.beginTransaction(SPISettings(40000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  
  // Software reset
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x01);
  digitalWrite(TFT_CS, HIGH);
  delay(150);
  
  digitalWrite(TFT_CS, LOW);
  
  // Sleep out
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x11);
  delay(120);
  
  // Color mode - 16bit
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x3A);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(0x55);
  
  // Memory access control (rotation)
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x36);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(0x00);
  
  // Display on
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x29);
  
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  
  delay(10);
}

void setLEDColor() {
  uint8_t r = (uint8_t)currentRed;
  uint8_t g = (uint8_t)currentGreen;
  uint8_t b = (uint8_t)currentBlue;
  rgbLED.setPixelColor(0, rgbLED.Color(r, g, b));
  rgbLED.show();
}

void updateDisplay() {
  uint8_t r = (uint8_t)currentRed;
  uint8_t g = (uint8_t)currentGreen;
  uint8_t b = (uint8_t)currentBlue;
  
  // Update color box
  if (colorBox) {
    lv_obj_set_style_bg_color(colorBox, lv_color_make(r, g, b), 0);
  }
  
  // Update RGB value labels
  if (labelR) {
    lv_label_set_text_fmt(labelR, "R: %3d", r);
  }
  if (labelG) {
    lv_label_set_text_fmt(labelG, "G: %3d", g);
  }
  if (labelB) {
    lv_label_set_text_fmt(labelB, "B: %3d", b);
  }
  
  // Update connection status
  if (labelStatus) {
    if (bleConnected) {
      lv_label_set_text(labelStatus, "BLE: Connected");
      lv_obj_set_style_text_color(labelStatus, lv_color_make(0, 255, 0), 0);
    } else {
      lv_label_set_text(labelStatus, "BLE: Waiting...");
      lv_obj_set_style_text_color(labelStatus, lv_color_make(255, 255, 0), 0);
    }
  }
  
  // Update mode
  if (labelMode) {
    if (bleColorReceived && bleConnected) {
      lv_label_set_text(labelMode, "Mode: Controlled");
    } else {
      lv_label_set_text(labelMode, "Mode: Random");
    }
  }
}

void createUI() {
  // Create color preview box
  colorBox = lv_obj_create(lv_scr_act());
  lv_obj_set_size(colorBox, 100, 100);
  lv_obj_align(colorBox, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_set_style_border_color(colorBox, lv_color_white(), 0);
  lv_obj_set_style_border_width(colorBox, 2, 0);
  
  // Create RGB value labels
  labelR = lv_label_create(lv_scr_act());
  lv_label_set_text(labelR, "R: 0");
  lv_obj_align(labelR, LV_ALIGN_TOP_LEFT, 20, 170);
  lv_obj_set_style_text_color(labelR, lv_color_white(), 0);
  
  labelG = lv_label_create(lv_scr_act());
  lv_label_set_text(labelG, "G: 0");
  lv_obj_align(labelG, LV_ALIGN_TOP_LEFT, 20, 195);
  lv_obj_set_style_text_color(labelG, lv_color_white(), 0);
  
  labelB = lv_label_create(lv_scr_act());
  lv_label_set_text(labelB, "B: 0");
  lv_obj_align(labelB, LV_ALIGN_TOP_LEFT, 20, 220);
  lv_obj_set_style_text_color(labelB, lv_color_white(), 0);
  
  // Create status label
  labelStatus = lv_label_create(lv_scr_act());
  lv_label_set_text(labelStatus, "BLE: Waiting...");
  lv_obj_align(labelStatus, LV_ALIGN_BOTTOM_LEFT, 10, -30);
  
  // Create mode label
  labelMode = lv_label_create(lv_scr_act());
  lv_label_set_text(labelMode, "Mode: Random");
  lv_obj_align(labelMode, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_text_color(labelMode, lv_color_make(0, 255, 255), 0);
}

void generateRandomColor() {
  targetRed = random(0, 256);
  targetGreen = random(0, 256);
  targetBlue = random(0, 256);
}

void fadeToTarget() {
  // Smoothly interpolate current color toward target color
  if (abs(currentRed - targetRed) > 0.5) {
    currentRed += (targetRed - currentRed) * FADE_SPEED * 0.1;
  } else {
    currentRed = targetRed;
  }
  
  if (abs(currentGreen - targetGreen) > 0.5) {
    currentGreen += (targetGreen - currentGreen) * FADE_SPEED * 0.1;
  } else {
    currentGreen = targetGreen;
  }
  
  if (abs(currentBlue - targetBlue) > 0.5) {
    currentBlue += (targetBlue - currentBlue) * FADE_SPEED * 0.1;
  } else {
    currentBlue = targetBlue;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Waveshare ESP32-C6-LCD-1.47 BLE LED Controller");
  
  // Initialize RGB LED
  rgbLED.begin();
  rgbLED.setBrightness(50); // Set brightness to 50/255
  rgbLED.show();
  
  // Initialize display hardware
  initDisplay();
  
  // Initialize LVGL
  lv_init();
  
  // Initialize display buffer
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, LVGL_BUFFER_SIZE);
  
  // Initialize display driver
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);
  
  // Create UI
  createUI();
  
  // Display startup message
  lv_obj_t* startupLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(startupLabel, "Starting BLE...");
  lv_obj_align(startupLabel, LV_ALIGN_CENTER, 0, -50);
  lv_obj_set_style_text_color(startupLabel, lv_color_white(), 0);
  
  lv_timer_handler();
  delay(1000);
  
  lv_obj_del(startupLabel);
  
  // Initialize BLE
  BLEDevice::init("ESP32C6-LED");
  
  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());
  
  // Create BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);
  
  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(new CharacteristicCallbacks());
  
  // Start the service
  pService->start();
  
  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE Server started!");
  Serial.println("Service UUID: " + String(SERVICE_UUID));
  Serial.println("Characteristic UUID: " + String(CHARACTERISTIC_UUID));
  Serial.println("Send 3 bytes (R, G, B) to control LED color");
  
  // Generate initial random color and set as both current and target
  generateRandomColor();
  currentRed = targetRed;
  currentGreen = targetGreen;
  currentBlue = targetBlue;
  setLEDColor();
  updateDisplay();
  
  lastColorChange = millis();
}

void loop() {
  lv_timer_handler(); // Handle LVGL tasks
  
  unsigned long currentTime = millis();
  
  // If not connected or no BLE color, generate new target colors periodically
  if (!bleColorReceived || !bleConnected) {
    if (currentTime - lastColorChange >= COLOR_CHANGE_INTERVAL) {
      generateRandomColor();
      lastColorChange = currentTime;
    }
  }
  
  // Smoothly fade toward target color
  fadeToTarget();
  setLEDColor();
  updateDisplay();
  
  delay(20); // ~50 FPS for smooth fading
}