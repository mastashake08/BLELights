/**
 * PhotoViewer.h
 * SD Card photo viewer for Waveshare ESP32-C6-LCD-1.47
 */

#ifndef PHOTOVIEWER_H
#define PHOTOVIEWER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <TJpg_Decoder.h>
#include <lvgl.h>

// SD Card pin for Waveshare ESP32-C6-LCD-1.47
// The board typically uses the same SPI bus as display
#define SD_CS_PIN 11  // Chip select for SD card

// Photo viewer state
namespace PhotoViewer {
    bool initialized = false;
    std::vector<String> imageFiles;
    int currentImageIndex = 0;
    lv_obj_t* imageContainer = nullptr;
    lv_img_dsc_t currentImage;
    uint16_t* imageBuffer = nullptr;
    int imageWidth = 0;
    int imageHeight = 0;

    // TJpg_Decoder callback to output decoded image to buffer
    bool tjpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
        if (!imageBuffer) return false;
        
        // Copy decoded block to our image buffer
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                int destX = x + col;
                int destY = y + row;
                if (destX < imageWidth && destY < imageHeight) {
                    imageBuffer[destY * imageWidth + destX] = bitmap[row * w + col];
                }
            }
        }
        return true;
    }

    // Initialize SD card
    bool initSD() {
        Serial.println("Initializing SD card...");
        
        // Try to initialize SD card
        if (!SD.begin(SD_CS_PIN)) {
            Serial.println("SD card initialization failed!");
            return false;
        }
        
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            Serial.println("No SD card attached");
            return false;
        }
        
        Serial.print("SD Card Type: ");
        if (cardType == CARD_MMC) {
            Serial.println("MMC");
        } else if (cardType == CARD_SD) {
            Serial.println("SDSC");
        } else if (cardType == CARD_SDHC) {
            Serial.println("SDHC");
        } else {
            Serial.println("UNKNOWN");
        }
        
        uint64_t cardSize = SD.cardSize() / (1024 * 1024);
        Serial.printf("SD Card Size: %lluMB\n", cardSize);
        
        initialized = true;
        return true;
    }

    // Scan SD card for image files
    void scanForImages(File dir, String path = "/") {
        while (true) {
            File entry = dir.openNextFile();
            if (!entry) {
                break;
            }
            
            String filename = String(entry.name());
            filename.toLowerCase();
            
            if (!entry.isDirectory()) {
                // Check for supported image formats
                if (filename.endsWith(".jpg") || filename.endsWith(".jpeg")) {
                    String fullPath = path + entry.name();
                    imageFiles.push_back(fullPath);
                    Serial.println("Found image: " + fullPath);
                }
            }
            
            entry.close();
        }
    }

    // Load all images from SD card
    bool loadImageList() {
        if (!initialized) {
            Serial.println("SD card not initialized!");
            return false;
        }
        
        imageFiles.clear();
        
        File root = SD.open("/");
        if (!root) {
            Serial.println("Failed to open root directory");
            return false;
        }
        
        Serial.println("Scanning for images on SD card...");
        scanForImages(root, "/");
        root.close();
        
        Serial.printf("Found %d images\n", imageFiles.size());
        return imageFiles.size() > 0;
    }

    // Display a JPEG image
    bool displayImage(String filename, lv_obj_t* parent) {
        if (!initialized) {
            Serial.println("SD card not initialized!");
            return false;
        }
        
        File imageFile = SD.open(filename);
        if (!imageFile) {
            Serial.println("Failed to open image: " + filename);
            return false;
        }
        
        Serial.println("Loading image: " + filename);
        
        // Set up TJpg_Decoder
        TJpgDec.setCallback(tjpgOutput);
        
        // Get image dimensions first
        uint16_t w, h;
        TJpgDec.getFsJpgSize(&w, &h, imageFile);
        imageFile.seek(0); // Reset file position
        
        Serial.printf("Image dimensions: %dx%d\n", w, h);
        
        // Allocate buffer for decoded image
        imageWidth = w;
        imageHeight = h;
        
        // Free previous buffer if exists
        if (imageBuffer) {
            free(imageBuffer);
        }
        
        imageBuffer = (uint16_t*)malloc(w * h * sizeof(uint16_t));
        if (!imageBuffer) {
            Serial.println("Failed to allocate image buffer!");
            imageFile.close();
            return false;
        }
        
        // Decode JPEG to buffer
        TJpgDec.drawFsJpg(0, 0, imageFile);
        imageFile.close();
        
        // Create LVGL image from buffer
        if (!imageContainer) {
            imageContainer = lv_img_create(parent);
        }
        
        currentImage.header.cf = LV_IMG_CF_TRUE_COLOR;
        currentImage.header.always_zero = 0;
        currentImage.header.reserved = 0;
        currentImage.header.w = w;
        currentImage.header.h = h;
        currentImage.data_size = w * h * sizeof(uint16_t);
        currentImage.data = (uint8_t*)imageBuffer;
        
        lv_img_set_src(imageContainer, &currentImage);
        lv_obj_align(imageContainer, LV_ALIGN_CENTER, 0, 0);
        
        Serial.println("Image displayed successfully!");
        return true;
    }

    // Display next image in list
    bool showNextImage(lv_obj_t* parent) {
        if (imageFiles.empty()) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex = (currentImageIndex + 1) % imageFiles.size();
        return displayImage(imageFiles[currentImageIndex], parent);
    }

    // Display previous image in list
    bool showPreviousImage(lv_obj_t* parent) {
        if (imageFiles.empty()) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex--;
        if (currentImageIndex < 0) {
            currentImageIndex = imageFiles.size() - 1;
        }
        return displayImage(imageFiles[currentImageIndex], parent);
    }

    // Display first image
    bool showFirstImage(lv_obj_t* parent) {
        if (imageFiles.empty()) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex = 0;
        return displayImage(imageFiles[0], parent);
    }

    // Clean up resources
    void cleanup() {
        if (imageBuffer) {
            free(imageBuffer);
            imageBuffer = nullptr;
        }
        if (imageContainer) {
            lv_obj_del(imageContainer);
            imageContainer = nullptr;
        }
    }
}

#endif // PHOTOVIEWER_H
