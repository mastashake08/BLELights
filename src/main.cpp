#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <lvgl.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include "PhotoViewer.h"

// Pin definitions for Waveshare ESP32-C6-LCD-1.47
#define RGB_LED_PIN 8      // RGB LED pin
#define RGB_LED_COUNT 1    // Single RGB LED on board
#define BUTTON_PIN 0       // Side button (BOOT button)

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

// Photo viewer variables
bool photoMode = false;
unsigned long lastPhotoChange = 0;
const unsigned long PHOTO_CHANGE_INTERVAL = 3000; // Change photo every 3 seconds
bool photoFading = false;
const int FADE_DURATION = 500; // Fade animation duration in ms

// Button variables
bool lastButtonState = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long DEBOUNCE_DELAY = 50; // 50ms debounce delay

// Display pins for Waveshare ESP32-C6-LCD-1.47
#define TFT_MOSI 6
#define TFT_SCLK 7
#define TFT_MISO 5
#define TFT_CS   14
#define TFT_DC   15
#define TFT_RST  21
#define TFT_BL   22

// Display offset for ST7789
#define TFT_OFFSET_X 34
#define TFT_OFFSET_Y 0

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
  
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  
  // Set column address with offset
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2A);
  digitalWrite(TFT_DC, HIGH);
  SPI.write((area->x1 + TFT_OFFSET_X) >> 8);
  SPI.write((area->x1 + TFT_OFFSET_X) & 0xFF);
  SPI.write((area->x2 + TFT_OFFSET_X) >> 8);
  SPI.write((area->x2 + TFT_OFFSET_X) & 0xFF);
  
  // Set row address with offset
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2B);
  digitalWrite(TFT_DC, HIGH);
  SPI.write((area->y1 + TFT_OFFSET_Y) >> 8);
  SPI.write((area->y1 + TFT_OFFSET_Y) & 0xFF);
  SPI.write((area->y2 + TFT_OFFSET_Y) >> 8);
  SPI.write((area->y2 + TFT_OFFSET_Y) & 0xFF);
  
  // Write memory
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2C);
  digitalWrite(TFT_DC, HIGH);
  
  // Send pixel data
  uint32_t size = w * h * 2;
  SPI.writeBytes((uint8_t*)color_p, size);
  
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  
  lv_disp_flush_ready(disp);
}

void initDisplay() {
  // Configure SPI pins
  pinMode(TFT_CS, OUTPUT);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);
  pinMode(TFT_BL, OUTPUT);
  
  digitalWrite(TFT_CS, HIGH);
  
  // Initialize backlight with PWM
  ledcAttach(TFT_BL, 1000, 10);  // 1kHz, 10-bit resolution
  ledcWrite(TFT_BL, 512);         // 50% brightness
  
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  
  // Hardware reset
  digitalWrite(TFT_RST, LOW);
  delay(50);
  digitalWrite(TFT_RST, HIGH);
  delay(50);
  
  // ST7789 initialization sequence
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  
  // Sleep out
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x11);
  digitalWrite(TFT_CS, HIGH);
  delay(120);
  
  digitalWrite(TFT_CS, LOW);
  
  // Memory access control (rotation) - horizontal mode
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x36);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(0x00);  // 0x00 for horizontal
  
  // Color mode - 16bit RGB565
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x3A);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(0x05);
  
  // Inversion on
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x21);
  
  // Display on
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x29);
  
  digitalWrite(TFT_CS, HIGH);
  SPI.endTransaction();
  
  delay(100);
  
  // Clear display to black
  SPI.beginTransaction(SPISettings(80000000, MSBFIRST, SPI_MODE0));
  digitalWrite(TFT_CS, LOW);
  
  // Set full screen window with offset
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2A);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(TFT_OFFSET_X >> 8);
  SPI.write(TFT_OFFSET_X & 0xFF);
  SPI.write((171 + TFT_OFFSET_X) >> 8);
  SPI.write((171 + TFT_OFFSET_X) & 0xFF);
  
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2B);
  digitalWrite(TFT_DC, HIGH);
  SPI.write(TFT_OFFSET_Y >> 8);
  SPI.write(TFT_OFFSET_Y & 0xFF);
  SPI.write((319 + TFT_OFFSET_Y) >> 8);
  SPI.write((319 + TFT_OFFSET_Y) & 0xFF);
  
  // Fill with black
  digitalWrite(TFT_DC, LOW);
  SPI.write(0x2C);
  digitalWrite(TFT_DC, HIGH);
  
  for(uint32_t i = 0; i < (172 * 320); i++) {
    SPI.write16(0x0000);
  }
  
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

// Animation callback for photo fade-in after fade-out completes
void fadeInNewPhoto(lv_anim_t* a) {
  // Load next photo
  PhotoViewer::showNextImage(lv_scr_act());
  
  // Fade in the new photo
  if (PhotoViewer::imageContainer) {
    lv_obj_set_style_opa(PhotoViewer::imageContainer, LV_OPA_TRANSP, 0);
    
    lv_anim_t anim_in;
    lv_anim_init(&anim_in);
    lv_anim_set_var(&anim_in, PhotoViewer::imageContainer);
    lv_anim_set_values(&anim_in, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&anim_in, FADE_DURATION);
    lv_anim_set_exec_cb(&anim_in, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_path_cb(&anim_in, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&anim_in, [](lv_anim_t* a) {
      photoFading = false;
    });
    lv_anim_start(&anim_in);
  } else {
    photoFading = false;
  }
}

void fadePhotoTransition() {
  if (photoFading || !PhotoViewer::imageContainer) return;
  
  photoFading = true;
  
  // Fade out current photo
  lv_anim_t anim_out;
  lv_anim_init(&anim_out);
  lv_anim_set_var(&anim_out, PhotoViewer::imageContainer);
  lv_anim_set_values(&anim_out, LV_OPA_COVER, LV_OPA_TRANSP);
  lv_anim_set_time(&anim_out, FADE_DURATION);
  lv_anim_set_exec_cb(&anim_out, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
  lv_anim_set_path_cb(&anim_out, lv_anim_path_ease_in_out);
  lv_anim_set_ready_cb(&anim_out, fadeInNewPhoto);
  lv_anim_start(&anim_out);
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

void togglePhotoMode() {
  photoMode = !photoMode;
  
  Serial.print("Photo mode toggled: ");
  Serial.println(photoMode ? "ON" : "OFF");
  
  if (photoMode) {
    // Switching to photo mode - clear screen and show first photo
    lv_obj_clean(lv_scr_act());
    
    if (PhotoViewer::hasImages() && PhotoViewer::showFirstImage(lv_scr_act())) {
      Serial.println("Photo slideshow activated!");
      lastPhotoChange = millis();
    } else {
      Serial.println("No photos available, reverting to LED mode");
      photoMode = false;
      createUI();
      updateDisplay();
    }
  } else {
    // Switching to LED control mode - clear screen and rebuild UI
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    
    createUI();
    updateDisplay();
    Serial.println("LED Control Mode activated!");
  }
}

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);
  
  // If button state changed, reset debounce timer
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  
  // If enough time has passed since last state change
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    // If button state has actually changed
    if (reading != buttonState) {
      buttonState = reading;
      
      // Button was pressed (LOW on ESP32-C6 boot button)
      if (buttonState == LOW) {
        togglePhotoMode();
      }
    }
  }
  
  lastButtonState = reading;
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
  
  // Initialize button with internal pull-up
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
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
  
  // Clear screen with black background
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
  
  // Show welcome message
  lv_obj_t* loadingLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(loadingLabel, "BLELights\n\nESP32-C6\nPhoto Viewer\n\nInitializing...");
  lv_obj_align(loadingLabel, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_text_color(loadingLabel, lv_color_white(), 0);
  lv_obj_set_style_text_align(loadingLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_timer_handler();
  delay(2000);
  
  lv_label_set_text(loadingLabel, "Checking SD Card...");
  lv_timer_handler();
  delay(500);
  
  if (PhotoViewer::initSD()) {
    if (PhotoViewer::loadImageList()) {
      lv_label_set_text(loadingLabel, "Photos found!\nStarting slideshow...");
      lv_timer_handler();
      delay(1000);
      
      // Clear screen and display first photo
      lv_obj_del(loadingLabel);
      
      if (PhotoViewer::showFirstImage(lv_scr_act())) {
        Serial.println("Photo slideshow mode activated!");
        photoMode = true;
        lastPhotoChange = millis();
      }
    } else {
      lv_label_set_text(loadingLabel, "No photos found\nSwitching to\nLED Control Mode");
      lv_timer_handler();
      delay(2000);
      lv_obj_del(loadingLabel);
    }
  } else {
    lv_label_set_text(loadingLabel, "No SD card detected\nSwitching to\nLED Control Mode");
    lv_timer_handler();
    delay(2000);
    lv_obj_del(loadingLabel);
  }
  
  // Only create LED control UI if not in photo mode
  if (!photoMode) {
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
  }
  
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
  
  // Only update display if not in photo mode
  if (!photoMode) {
    updateDisplay();
  }
  
  lastColorChange = millis();
}

void loop() {
  lv_timer_handler(); // Handle LVGL tasks
  
  // Check button state for photo mode toggle
  checkButton();
  
  unsigned long currentTime = millis();
  
  // Photo slideshow mode - cycle through photos with fade effect
  if (photoMode) {
    if (currentTime - lastPhotoChange >= PHOTO_CHANGE_INTERVAL && !photoFading) {
      fadePhotoTransition();
      lastPhotoChange = currentTime;
    }
  }
  
  // LED control - works in both photo mode and LED control mode
  // If not connected or no BLE color, generate new target colors periodically
  if (!bleColorReceived || !bleConnected) {
    if (currentTime - lastColorChange >= COLOR_CHANGE_INTERVAL) {
      generateRandomColor();
      lastColorChange = currentTime;
    }
  }
  
  // Smoothly fade toward target color and update LED
  fadeToTarget();
  setLEDColor();
  
  // Only update display UI if not in photo mode
  if (!photoMode) {
    updateDisplay();
  }
  
  delay(20); // ~50 FPS for smooth fading
}