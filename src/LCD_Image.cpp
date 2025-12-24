#include "LCD_Image.h"
  
PNG png;
File Image_file;

uint16_t Image_CNT;   
char SD_Image_Name[100][100] ;    
char File_Image_Name[100][100] ;  

int16_t xpos = 0;
int16_t ypos = 0;
void * pngOpen(const char *filePath, int32_t *size) {
  Image_file = SD.open(filePath);
  *size = Image_file.size();
  return &Image_file;
}

void pngClose(void *handle) {
  File Image_file = *((File*)handle);
  if (Image_file) Image_file.close();
}

int32_t pngRead(PNGFILE *page, uint8_t *buffer, int32_t length) {
  if (!Image_file) return 0;
  page = page; // Avoid warning
  return Image_file.read(buffer, length);
}

int32_t pngSeek(PNGFILE *page, int32_t position) {
  if (!Image_file) return 0;
  page = page; // Avoid warning
  return Image_file.seek(position);
}
//=========================================v==========================================
//                                      pngDraw
//====================================================================================
// This next function will be called during decoding of the png file to
// render each image line to the TFT.  If you use a different TFT library
// you will need to adapt this function to suit.
// Callback function to draw pixels to the display
static uint16_t lineBuffer[MAX_IMAGE_WIDTH];
int pngDraw(PNGDRAW *pDraw) {
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  
  // Calculate how much of the line we can actually draw
  uint32_t size = pDraw->iWidth;
  if (size > MAX_IMAGE_WIDTH) {
    size = MAX_IMAGE_WIDTH; // Clip to buffer size
  }
  
  // Swap byte order for all pixels
  for (size_t i = 0; i < size; i++) {
    lineBuffer[i] = (((lineBuffer[i] >> 8) & 0xFF) | ((lineBuffer[i] << 8) & 0xFF00));
  }
  
  // Only draw if within display bounds
  int16_t displayY = ypos + pDraw->y;
  if (displayY >= 0 && displayY < LCD_HEIGHT && xpos >= 0 && xpos < LCD_WIDTH) {
    // Calculate visible width
    int16_t visibleWidth = size;
    if (xpos + size > LCD_WIDTH) {
      visibleWidth = LCD_WIDTH - xpos;
    }
    
    if (visibleWidth > 0) {
      LCD_addWindow(xpos, displayY, xpos + visibleWidth, displayY + 1, lineBuffer);
    }
  }
  
  return 1; // Return 1 to continue drawing
}
/////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////

void Search_Image(const char* directory, const char* fileExtension) {        
  Image_CNT = Folder_retrieval(directory,fileExtension,SD_Image_Name,100);
  if(Image_CNT) {  
    for (int i = 0; i < Image_CNT; i++) {
      strcpy(File_Image_Name[i], SD_Image_Name[i]);
      remove_file_extension(File_Image_Name[i]); 
    }                  
  }                                                             
}
void Show_Image(const char * filePath)
{
  printf("Currently display picture %s\r\n",filePath);
  int16_t ret = png.open(filePath, pngOpen, pngClose, pngRead, pngSeek, pngDraw);                 
  if (ret == PNG_SUCCESS) {                                                                          
    printf("image specs: (%d x %d), %d bpp, pixel type: %d\r\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType()); 
    
    uint32_t dt = millis();
    
    // Center the image on the display
    // If image is larger than display, it will be clipped
    int16_t imageWidth = png.getWidth();
    int16_t imageHeight = png.getHeight();
    
    xpos = (LCD_WIDTH - imageWidth) / 2;
    ypos = (LCD_HEIGHT - imageHeight) / 2;
    
    // Ensure we don't have negative positions
    if (xpos < 0) xpos = 0;
    if (ypos < 0) ypos = 0;
    
    if (imageWidth > MAX_IMAGE_WIDTH) {                                                 
      printf("Warning: Image width (%d) exceeds buffer size (%d), image will be clipped\r\n", imageWidth, MAX_IMAGE_WIDTH);                          
    }
    
    ret = png.decode(NULL, 0);                                                             
    png.close();                                                                        
    printf("%d ms\r\n",millis()-dt);              
  }  
}

void Display_Image(const char* directory, const char* fileExtension, uint16_t ID)
{
  Search_Image(directory,fileExtension);
  if(Image_CNT) {
    String FilePath;
    if (String(directory) == "/") {                               // Handle the case when the directory is the root
      FilePath = String(directory) + SD_Image_Name[ID];
    } else {
      FilePath = String(directory) + "/" + SD_Image_Name[ID];
    }
    const char* filePathCStr = FilePath.c_str();          // Convert String to c_str() for Show_Image function
    printf("Show  : %s \r\n", filePathCStr);              // Print file path for debugging
    Show_Image(filePathCStr);                             // Show the image using the file path
  }
  else
    printf("No files with extension '%s' found in directory: %s\r\n", fileExtension, directory);     

}
uint16_t Now_Image = 0;
void Image_Next(const char* directory, const char* fileExtension)
{
  if(!digitalRead(BOOT_KEY_PIN)){ 
    while(!digitalRead(BOOT_KEY_PIN));
    Now_Image ++;
    if(Now_Image == Image_CNT)
      Now_Image = 0;
    Display_Image(directory,fileExtension,Now_Image);
  }
}
void Image_Next_Loop(const char* directory, const char* fileExtension,uint32_t NextTime)
{
  static uint32_t NextTime_Now=0;
  NextTime_Now++;
  if(NextTime_Now == NextTime)
  {
    NextTime_Now = 0;
    Now_Image ++;
    if(Now_Image == Image_CNT)
      Now_Image = 0;
    Display_Image(directory,fileExtension,Now_Image);
  }
}