/*!
 * @file Adafruit_ImageReader.cpp
 *
 * @mainpage Companion library for Adafruit_GFX to load images from SD card.
 *
 * @section intro_sec Introduction
 *
 * This is the documentation for Adafruit's ImageReader library for the
 * Arduino platform. It is designed to work in conjunction with Adafruit_GFX
 * and a display-specific library.
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * @section dependencies Dependencies
 *
 * This library depends on <a href="https://github.com/adafruit/Adafruit_GFX">
 * Adafruit_GFX</a> plus a display device-specific library such as
 * <a href="https://github.com/adafruit/Adafruit_ILI9341"> Adafruit_ILI9341</a>
 * or other subclasses of SPITFT. Please make sure you have installed the
 * latest versions before using this library.
 *
 * @section author Author
 *
 * Written by Phil "PaintYourDragon" Burgess for Adafruit Industries.
 *
 * @section license License
 *
 * BSD license, all text here must be included in any redistribution.
 */

#include <SD.h>
#include "Adafruit_ImageReader.h"

// Buffers in BMP draw function (to screen) require 5 bytes/pixel: 3 bytes
// for each BMP pixel (R+G+B), 2 bytes for each TFT pixel (565 color).
// Buffers in BMP load (to canvas) require 3 bytes/pixel (R+G+B from BMP),
// no interim 16-bit buffer as data goes straight to the canvas buffer.
// Because buffers are flushed at the end of each scanline (to allow for
// cropping, etc.), no point in any of these pixel counts being more than
// the screen width.

#ifdef __AVR__
 #define DRAWPIXELS  24 ///<  24 * 5 =  120 bytes
 #define LOADPIXELS  32 ///<  32 * 3 =   96 bytes
#else
 #define DRAWPIXELS 200 ///< 200 * 5 = 1000 bytes
 #define LOADPIXELS 320 ///< 320 * 3 =  960 bytes
#endif

/*!
    @brief   Constructor.
    @return  Adafruit_ImageReader object.
*/
Adafruit_ImageReader::Adafruit_ImageReader(void) {
}

/*!
    @brief   Destructor.
    @return  None (void).
*/
Adafruit_ImageReader::~Adafruit_ImageReader(void) {
  if(file) file.close();
}

/*!
    @brief   Loads BMP image file from SD card directly to SPITFT screen.
    @param   filename
             Name of BMP image file to load.
    @param   tft
             Adafruit_SPITFT object (e.g. one of the Adafruit TFT or OLED
             displays that subclass Adafruit_SPITFT).
    @param   x
             Horizontal offset in pixels; left edge = 0, positive = right.
             Value is signed, image will be clipped if all or part is off
             the screen edges. Screen rotation setting is observed.
    @param   y
             Vertical offset in pixels; top edge = 0, positive = down.
    @return  One of the ImageReturnCode values (IMAGE_SUCCESS on successful
             completion, other values on failure).
*/
ImageReturnCode Adafruit_ImageReader::drawBMP(
  char *filename, Adafruit_SPITFT &tft, int16_t x, int16_t y) {
  uint16_t tftbuf[DRAWPIXELS]; // Temp space for buffering TFT data
  // Call core BMP-reading function, passing address to TFT object,
  // TFT working buffer, and X & Y position of top-left corner (image
  // will be cropped on load if necessary). Last two arguments are
  // NULL when reading straight to TFT...
  return coreBMP(filename, &tft, tftbuf, x, y, NULL, NULL, NULL, NULL);
}

/*!
    @brief   Loads BMP image file from SD card into RAM (as one of the GFX
             canvas object types) for use with the bitmap-drawing functions.
             Not practical for most AVR microcontrollers, but some of the
             more capable 32-bit micros can afford some RAM for this.
    @param   filename
             Name of BMP image file to load.
    @param   canvas1
             A canvas object vector, which type can be determined from the
             value returned in the third argument. (Currently will return
             only NULL or a GFXcanvas16, cast to a void* pointer).
    @param   canvas2
             A canvas object vector -- currently WILL ALWAYS RETURN NULL,
             no support for this in the BMP loader yet -- plan is that a
             GFXcanvas1 type could be returned and used as a bitmask argument
             to the GFX drawRGBBitmap() function.
    @param   palette
             A uint16_t vector -- currently WILL ALWAYS RETURN NULL, no
             support for this in the BMP loader yet -- plan is that a 16-bit
             5/6/5 color palette could be returned with some image formats,
             WOULD ALSO REQUIRE A PALETTE-ENABLED drawRGBBitmap() VARIANT
             IN GFX, WHICH DOES NOT CURRENTLY EXIST.
    @param   fmt
             Pointer to an CanvasFormat variable, which will indicate the
             canvas type(s) that resulted from the load operation. Currently
             provides only CANVAS_NONE (load error, canvas1 pointer will be
             NULL) or IMAGE_CANVAS16 (success, canvas1 pointer can be cast
             to a GFXcanvas16* type, from which the buffer, width and height
             can be queried with other GFX functions).
    @return  One of the ImageReturnCode values (IMAGE_SUCCESS on successful
             completion, other values on failure).
*/
ImageReturnCode Adafruit_ImageReader::loadBMP(
  char *filename, void **canvas1, void **canvas2, void **palette,
  CanvasFormat *fmt) {
  // Call core BMP-reading function. TFT and working buffer are NULL
  // (unused and allocated in function, respectively), X & Y position are
  // always 0 because full image is loaded (RAM permitting). Last four
  // arguments allow GFX canvas object(s), palette and type to be returned
  // (palette and second canvas are NOT CURRENTLY SUPPORTED).
  return coreBMP(filename, NULL, NULL, 0, 0, canvas1, canvas2, palette, fmt);
}

/*!
    @brief   BMP-reading function common both to the draw function (to TFT)
             and load function (to canvas object in RAM). BMP code has been
             centralized here so if/when more BMP format variants are added
             in the future, it doesn't need to be implemented, debugged and
             kept in sync in two places.
    @param   filename
             Name of BMP image file to load.
    @param   data
             A canvas object vector, which type can be determined from the
             value returned in the third argument. (Currently will return
             only NULL or a GFXcanvas16, cast to a void* pointer).
    @param   fmt
             Pointer to an CanvasFormat variable, which will indicate the
             canvas type that resulted from the load operation. Currently
             provides only CANVAS_NONE (load error, data pointer will be
             NULL) or IMAGE_CANVAS16 (success, data pointer can be cast to
             a GFXcanvas16* type, from which the buffer, width and height
             can be queried with other GFX functions).
    @return  One of the ImageReturnCode values (IMAGE_SUCCESS on successful
             completion, other values on failure).
*/
ImageReturnCode Adafruit_ImageReader::coreBMP(
  char            *filename, // SD file to load
  Adafruit_SPITFT *tft,      // Pointer to TFT object, or NULL if to canvas
  uint16_t        *dest,     // TFT working buffer, or NULL if to canvas
  int16_t          x,        // Position if loading to TFT (else ignored)
  int16_t          y,
  void           **canvas1,  // Canvas return (if loading to canvas)
  void           **canvas2,  // Bitmask canvas return (NOT YET SUPPORTED)
  void           **palette,  // Color palette return (NOT YET SUPPORTED)
  CanvasFormat    *fmt) {    // Canvas type return (if loading to canvas)

  ImageReturnCode status  = IMAGE_ERR_FORMAT; // IMAGE_SUCCESS on valid file
  uint32_t        offset;                     // Start of image data in file
  int             bmpWidth, bmpHeight;        // BMP width & height in pixels
  uint8_t         depth;                      // BMP bit depth
  uint32_t        rowSize;                    // >bmpWidth if scanline padding
  uint8_t         sdbuf[3*DRAWPIXELS];        // BMP pixel buf (R+G+B per pixel)
#if ((3*DRAWPIXELS) <= 255)
  uint8_t         srcidx  = sizeof sdbuf;     // Current position in sdbuf
#else
  uint16_t        srcidx  = sizeof sdbuf;
#endif
  uint32_t        destidx = 0;
  boolean         flip    = true;             // BMP is stored bottom-to-top
  uint32_t        bmpPos  = 0;                // Next pixel position in file
  int             loadWidth, loadHeight;      // Region being loaded (clipped)
  int             row, col;                   // Current pixel pos.
  uint8_t         r, g, b;                    // Current pixel color
  GFXcanvas16    *canvas;                     // If loading into RAM

  if(fmt)     *fmt     = CANVAS_NONE; // Nothing loaded yet
  if(canvas2) *canvas2 = NULL;        // Not yet supported
  if(palette) *palette = NULL;        // Not yet supported

  // If BMP is being drawn off the right or bottom edge of the screen,
  // nothing to do here. NOT an error, just a trivial clip operation.
  if(tft && ((x >= tft->width()) || (y >= tft->height())))
    return IMAGE_SUCCESS;

  // Open requested file on SD card
  if(!(file = SD.open(filename))) return IMAGE_ERR_FILE_NOT_FOUND;

  // Parse BMP header
  if(readLE16() == 0x4D42) { // BMP signature
    (void)readLE32();        // Read & ignore file size
    (void)readLE32();        // Read & ignore creator bytes
    offset = readLE32();     // Start of image data
    // Read DIB header
    (void)readLE32();        // Read & ignore header size
    bmpWidth  = readLE32();
    bmpHeight = readLE32();
    if(readLE16() == 1) {    // # planes -- currently must be '1'
      depth = readLE16();    // bits per pixel
      if((depth == 24) && (readLE32() == 0)) { // Uncompressed BGR only

        // BMP rows are padded (if needed) to 4-byte boundary
        rowSize = (bmpWidth * 3 + 3) & ~3;

        // If bmpHeight is negative, image is in top-down order.
        // This is not canon but has been observed in the wild.
        if(bmpHeight < 0) {
          bmpHeight = -bmpHeight;
          flip      =  false;
        }

        loadWidth  = bmpWidth;
        loadHeight = bmpHeight;

        if(tft) {
          // Crop area to be loaded (if destination is TFT)
          if(x < 0) {
            loadWidth += x;
            x          = 0;
          }
          if(y < 0) {
            loadHeight += y;
            y          = 0;
          }
          if((x + loadWidth ) > tft->width())  loadWidth  = tft->width()  - x;
          if((y + loadHeight) > tft->height()) loadHeight = tft->height() - y;
        } else {
          // Loading to RAM -- allocate GFX 16-bit canvas type
          status = IMAGE_ERR_MALLOC; // Assume won't fit to start
          if((canvas = new GFXcanvas16(bmpWidth, bmpHeight))) {
            dest = canvas->getBuffer();
          }
        }

        if(dest) { // Supported format, alloc OK, etc.
          status = IMAGE_SUCCESS;

          if((loadWidth > 0) && (loadHeight > 0)) { // Clip top/left
            if(tft) {
              tft->startWrite();         // Start new TFT SPI transaction
              tft->setAddrWindow(x, y, loadWidth, loadHeight);
            } else {
              *canvas1 = canvas;         // Canvas to be returned
              if(fmt) *fmt  = CANVAS_16; // Is a GFX 16-bit canvas type
            }

            for(row=0; row<loadHeight; row++) { // For each scanline...

              yield(); // Keep ESP8266 happy

              // Seek to start of scan line.  It might seem labor-intensive to
              // be doing this on every line, but this method covers a lot of
              // gritty details like cropping, flip and scanline padding.  Also,
              // the seek only takes place if the file position actually needs
              // to change (avoids a lot of cluster math in SD library).
              if(flip) // Bitmap is stored bottom-to-top order (normal BMP)
                bmpPos = offset + (bmpHeight - 1 - row) * rowSize;
              else     // Bitmap is stored top-to-bottom
                bmpPos = offset + row * rowSize;
              if(file.position() != bmpPos) { // Need seek?
                if(tft) tft->endWrite();      // End TFT SPI transaction
                file.seek(bmpPos);            // Seek = SD transaction
                srcidx = sizeof sdbuf;        // Force buffer reload
              }
              for(col=0; col<loadWidth; col++) { // For each pixel...
                if(srcidx >= sizeof sdbuf) {        // Time to load more data?
                  if(tft) {                         // Drawing to TFT?
                    tft->endWrite();                // End TFT SPI transaction
                    file.read(sdbuf, sizeof sdbuf); // Load from SD
                    tft->startWrite();              // Start TFT SPI transac
                    if(destidx) {                   // If any buffered TFT data
                      tft->writePixels(dest, destidx); // Write it now and
                      destidx = 0;                     // reset dest index
                    }
                  } else {                          // Canvas is much simpler,
                    file.read(sdbuf, sizeof sdbuf); // just load sdbuf
                  }                                 // (destidx never resets)
                  srcidx = 0;                       // Reset bmp buf index
                }
                // Convert each pixel from BMP to 565 format, save in dest
                b = sdbuf[srcidx++];
                g = sdbuf[srcidx++];
                r = sdbuf[srcidx++];
                dest[destidx++] = ((r & 0xF8) << 8) |
                                  ((g & 0xFC) << 3) |
                                  ((b & 0xF8) >> 3);
              } // end pixel loop
              if(tft) {                            // Drawing to TFT?
                if(destidx) {                      // Any remainders?
                  tft->writePixels(dest, destidx); // Write to screen and
                  destidx = 0;                     // reset dest index
                }
                tft->endWrite();                   // End TFT SPI transaction
              }
            } // end scanline loop
          } // end top/left clip
        } // end malloc
      } // end format
    } // end planes
  } // end signature

  file.close();
  return status;
}

/*!
    @brief   Query pixel dimensions of BMP image file on SD card.
    @param   filename
             Name of BMP image file to query.
    @param   width
             Pointer to int32_t; image width in pixels, returned.
    @param   height
             Pointer to int32_t; image height in pixels, returned.
    @return  One of the ImageReturnCode values (IMAGE_SUCCESS on successful
             completion, other values on failure).
*/
ImageReturnCode Adafruit_ImageReader::bmpDimensions(
  char *filename, int32_t *width, int32_t *height) {

  ImageReturnCode status = IMAGE_ERR_FILE_NOT_FOUND; // Guilty until innocent

  if((file = SD.open(filename))) { // Open requested file on SD card
    status = IMAGE_ERR_FORMAT;     // File's there, might not be BMP tho
    if(readLE16() == 0x4D42) {     // BMP signature?
      (void)readLE32();            // Read & ignore file size
      (void)readLE32();            // Read & ignore creator bytes
      (void)readLE32();            // Read & ignore position of image data
      (void)readLE32();            // Read & ignore header size
      if(width) *width = readLE32();
      if(height) {
        int32_t h = readLE32();    // Don't abs() this, may be a macro
        if(h < 0) h = -h;          // Do manually instead
        *height = h;
      }
      status = IMAGE_SUCCESS;      // YAY.
    }
  }

  file.close();
  return status;
}

/*!
    @brief   Reads a little-endian 16-bit unsigned value from currently-
             open File, converting if necessary to the microcontroller's
             native endianism. (BMP files use little-endian values.)
    @return  Unsigned 16-bit value, native endianism.
*/
uint16_t Adafruit_ImageReader::readLE16(void) {
#if !defined(ESP32) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  // Read directly into result -- BMP data and variable both little-endian.
  uint16_t result;
  file.read(&result, sizeof result);
  return result;
#else
  // Big-endian or unknown. Byte-by-byte read will perform reversal if needed.
  return file.read() | ((uint16_t)file.read() << 8);
#endif
}

/*!
    @brief   Reads a little-endian 32-bit unsigned value from currently-
             open File, converting if necessary to the microcontroller's
             native endianism. (BMP files use little-endian values.)
    @return  Unsigned 32-bit value, native endianism.
*/
uint32_t Adafruit_ImageReader::readLE32(void) {
#if !defined(ESP32) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  // Read directly into result -- BMP data and variable both little-endian.
  uint32_t result;
  file.read(&result, sizeof result);
  return result;
#else
  // Big-endian or unknown. Byte-by-byte read will perform reversal if needed.
  return       file.read()        |
    ((uint32_t)file.read() <<  8) |
    ((uint32_t)file.read() << 16) |
    ((uint32_t)file.read() << 24);
#endif
}
