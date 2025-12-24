/**
 * PhotoViewer.h
 * SD Card photo viewer for Waveshare ESP32-C6-LCD-1.47
 */

#ifndef PHOTOVIEWER_H
#define PHOTOVIEWER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <lvgl.h>
#include "SD_Card.h"
#include "LCD_Image.h"

// External references to global variables from LCD_Image module
extern uint16_t Image_CNT;
extern char SD_Image_Name[][100];

// Photo viewer state
namespace PhotoViewer {
    bool initialized = false;
    int currentImageIndex = 0;
    const char* imageDirectory = "/";
    const char* imageExtension = ".png";

    // Initialize SD card using SD_Card module
    bool initSD() {
        SD_Init();
        
        // Check if SD card was successfully initialized
        uint8_t cardType = SD.cardType();
        if (cardType == CARD_NONE) {
            Serial.println("No SD card attached - photo mode unavailable");
            initialized = false;
            return false;
        }
        
        initialized = true;
        return true;
    }

    // Load all images from SD card using LCD_Image module
    bool loadImageList() {
        if (!initialized) {
            Serial.println("SD card not initialized!");
            return false;
        }
        
        Serial.println("Scanning for PNG images on SD card...");
        Search_Image(imageDirectory, imageExtension);
        
        Serial.printf("Found %d images\n", ::Image_CNT);
        return ::Image_CNT > 0;
    }

    // Display a PNG image using LCD_Image module
    bool displayImage(int imageIndex, lv_obj_t* parent = nullptr) {
        if (!initialized) {
            Serial.println("SD card not initialized!");
            return false;
        }
        
        if (::Image_CNT == 0) {
            Serial.println("No images available");
            return false;
        }
        
        // Use LCD_Image module to display the PNG
        Display_Image(imageDirectory, imageExtension, imageIndex);
        
        return true;
    }

    // Display next image in list
    bool showNextImage(lv_obj_t* parent = nullptr) {
        if (::Image_CNT == 0) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex = (currentImageIndex + 1) % ::Image_CNT;
        return displayImage(currentImageIndex, parent);
    }

    // Display previous image in list
    bool showPreviousImage(lv_obj_t* parent = nullptr) {
        if (::Image_CNT == 0) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex--;
        if (currentImageIndex < 0) {
            currentImageIndex = ::Image_CNT - 1;
        }
        return displayImage(currentImageIndex, parent);
    }

    // Display first image
    bool showFirstImage(lv_obj_t* parent = nullptr) {
        if (::Image_CNT == 0) {
            Serial.println("No images to display");
            return false;
        }
        
        currentImageIndex = 0;
        return displayImage(0, parent);
    }

    // Check if images are available
    bool hasImages() {
        return ::Image_CNT > 0;
    }

    // Clean up resources
    void cleanup() {
        // LCD_Image module handles its own cleanup
        currentImageIndex = 0;
    }
}

#endif // PHOTOVIEWER_H
