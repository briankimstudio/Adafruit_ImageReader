# Display BMP file from SPIFFS on TFT display using ESP32

This is a library for drawing BMP file from SPIFFS on TFT display. It is a modified version of https://github.com/adafruit/Adafruit_ImageReader.  

If you have installed Adafruit_ImageReader library already, just overwrite two files to exisiting library folder.

```
Adafruit_ImageReader.cpp
Adafruit_ImageReader.h
```

# How to use

First of all, upload BMP file to SPIFFS using https://github.com/me-no-dev/arduino-esp32fs-plugin

Then in your sketch, include header file, initialize SPIFFS. That's it.

1. Include header file

```
  #include <Adafruit_ImageReader.h>
```

2. Initialize SPIFFS

```
  if (!SPIFFS.begin(true)) {
    print("An Error has occurred while mounting SPIFFS",1);
    delay(1000);
  }
```  

3. Load image and draw on display

```
  Adafruit_ST7735 display = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);
  Adafruit_Image img;
  ImageReturnCode stat;
  stat = reader.loadBMP("/cat.bmp", img);
  img.draw(display, 0, 0); 
```  

Then, you will see something like this.

![On ESP32](https://github.com/briankimstudio/Adafruit_ImageReader/blob/master/bmp_spiffs.jpg)
I hope you enjoy it~

Thanks,

Brian

# Adafruit_ImageReader ![Build Status](https://github.com/adafruit/Adafruit_ImageReader/workflows/Arduino%20Library%20CI/badge.svg)

Companion library for Adafruit_GFX to load images from SD card or SPI Flash

Requires Adafruit_GFX library and one of the SPI color graphic display libraries, e.g. Adafruit_ILI9341.

**IMPORTANT NOTE: version 2.0 is a "breaking change"** from the 1.X releases of this library. Existing code WILL NOT COMPILE without revision. Adafruit_ImageReader now relies on the Adafruit_SPIFlash and SdFat libraries, and the Adafruit_ImageReader constructor call has changed (other functions remain the same). See the examples for reference. Very sorry about that but it brings some helpful speed and feature benefits (like loading from SPI/QSPI flash).
