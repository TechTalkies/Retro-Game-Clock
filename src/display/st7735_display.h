/* Buffered, colour ST7735 display adapter.
 *
 * Firmware draws directly at the ST7735's native 160x128 resolution.  The
 * complete RGB frame is transferred only after drawing is complete, preventing
 * the panel from showing intermediate clear/draw operations.
 */
#ifndef ST7735_DISPLAY_H
#define ST7735_DISPLAY_H

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

class ST7735Display : public GFXcanvas16 {
 public:
  static const uint16_t LOGICAL_WIDTH = 160;
  static const uint16_t LOGICAL_HEIGHT = 128;
  static const uint16_t TFT_WIDTH = 160;
  static const uint16_t TFT_HEIGHT = 128;

  static const uint16_t BACKGROUND = 0x0841;  // deep navy blue

  ST7735Display(int8_t cs, int8_t dc, int8_t rst)
      : GFXcanvas16(LOGICAL_WIDTH, LOGICAL_HEIGHT), tft(cs, dc, rst) {}

  bool begin() {
    if (!GFXcanvas16::getBuffer()) return false;
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(BACKGROUND);
    clearDisplay();
    return true;
  }

  void clearDisplay() { fillScreen(BACKGROUND); }
  void enableDisplay(bool enabled) { tft.enableDisplay(enabled); }

  void display() {
    tft.drawRGBBitmap(0, 0, GFXcanvas16::getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  }

  // Used only by the old OLED Pong framebuffer sampler.
  uint8_t *getMonochromeBuffer() { return nullptr; }
  uint16_t getPixel(int16_t x, int16_t y) {
    if (x < 0 || y < 0 || x >= LOGICAL_WIDTH || y >= LOGICAL_HEIGHT) return BACKGROUND;
    return GFXcanvas16::getBuffer()[y * LOGICAL_WIDTH + x];
  }

 private:
  Adafruit_ST7735 tft;
};

#endif  // ST7735_DISPLAY_H
