#pragma once

#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h>
#include <AnimatedGIF.h>

class DinoGifPlayer {
public:
  DinoGifPlayer()
  : _tft(nullptr),
    _path(nullptr),
    _x(0),
    _y(0),
    _bgColor(TFT_BLACK),
    _started(false),
    _isOpen(false),
    _canvasW(0),
    _canvasH(0) {
    _instance = this;
  }

  bool begin(TFT_eSPI* tft,
             const char* gifPath,
             int16_t x,
             int16_t y,
             uint16_t bgColor = TFT_BLACK) {
    _tft = tft;
    _path = gifPath;
    _x = x;
    _y = y;
    _bgColor = bgColor;
    _started = false;
    _isOpen = false;

    if (!LittleFS.begin(false)) {
      Serial.println("LittleFS mount FAILED");
      return false;
    }

    Serial.println("LittleFS mount OK");

    if (!LittleFS.exists(_path)) {
      Serial.print("GIF missing: ");
      Serial.println(_path);
      return false;
    }

    Serial.print("GIF exists: ");
    Serial.println(_path);

    fs::File f = LittleFS.open(_path, "r");
    if (!f) {
      Serial.println("GIF open FAILED");
      return false;
    }

    Serial.print("GIF size: ");
    Serial.println((uint32_t)f.size());
    f.close();

    _gif.begin(BIG_ENDIAN_PIXELS);

    if (!openGif()) {
      Serial.println("gif.open() FAILED");
      return false;
    }

    Serial.print("GIF canvas: ");
    Serial.print(_canvasW);
    Serial.print(" x ");
    Serial.println(_canvasH);

    _started = true;
    return true;
  }

  void end() {
    closeGif();
    _started = false;
  }

  bool update() {
    if (!_started || _tft == nullptr) return false;

    if (!_isOpen) {
      if (!openGif()) return false;
    }

    if (!_gif.playFrame(true, nullptr)) {
      _gif.reset();
    }

    return true;
  }

  bool isRunning() const {
    return _started;
  }

private:
  TFT_eSPI* _tft;
  const char* _path;
  int16_t _x;
  int16_t _y;
  uint16_t _bgColor;

  AnimatedGIF _gif;
  fs::File _file;
  uint16_t _lineBuffer[240];

  bool _started;
  bool _isOpen;
  int16_t _canvasW;
  int16_t _canvasH;

  inline static DinoGifPlayer* _instance = nullptr;

  bool openGif() {
    closeGif();

    if (_path == nullptr || !LittleFS.exists(_path)) {
      return false;
    }

    if (!_gif.open(_path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
      return false;
    }

    _canvasW = _gif.getCanvasWidth();
    _canvasH = _gif.getCanvasHeight();
    _isOpen = true;
    return true;
  }

  void closeGif() {
    if (_isOpen) {
      _gif.close();
      _isOpen = false;
    }
    if (_file) {
      _file.close();
    }
  }

  static void* GIFOpenFile(const char* fname, int32_t* pSize) {
    if (_instance == nullptr) {
      *pSize = 0;
      return nullptr;
    }

    _instance->_file = LittleFS.open(fname, "r");
    if (!_instance->_file || _instance->_file.isDirectory()) {
      *pSize = 0;
      return nullptr;
    }

    *pSize = _instance->_file.size();
    return (void*)&_instance->_file;
  }

  static void GIFCloseFile(void* pHandle) {
    fs::File* f = static_cast<fs::File*>(pHandle);
    if (f) f->close();
  }

  static int32_t GIFReadFile(GIFFILE* pFile, uint8_t* pBuf, int32_t iLen) {
    fs::File* f = static_cast<fs::File*>(pFile->fHandle);
    if (!f) return 0;

    int32_t bytesRead = f->read(pBuf, iLen);
    pFile->iPos = f->position();
    return bytesRead;
  }

  static int32_t GIFSeekFile(GIFFILE* pFile, int32_t iPosition) {
    fs::File* f = static_cast<fs::File*>(pFile->fHandle);
    if (!f) return 0;

    f->seek(iPosition);
    pFile->iPos = f->position();
    return pFile->iPos;
  }

  static void GIFDraw(GIFDRAW* pDraw) {
    if (_instance == nullptr || _instance->_tft == nullptr) return;

    DinoGifPlayer* self = _instance;
    TFT_eSPI* tft = self->_tft;

    int16_t x = self->_x + pDraw->iX;
    int16_t y = self->_y + pDraw->iY + pDraw->y;
    int16_t width = pDraw->iWidth;

    if (y < 0 || y >= tft->height()) return;
    if (x >= tft->width()) return;
    if ((x + width) <= 0) return;

    int16_t srcOffset = 0;

    if (x < 0) {
      srcOffset = -x;
      width += x;
      x = 0;
    }

    if ((x + width) > tft->width()) {
      width = tft->width() - x;
    }

    if (width <= 0) return;

    uint8_t* src = pDraw->pPixels + srcOffset;
    uint16_t* palette = pDraw->pPalette;

    if (pDraw->ucHasTransparency) {
      uint8_t transparent = pDraw->ucTransparent;

      for (int16_t i = 0; i < width; i++) {
        self->_lineBuffer[i] = self->_bgColor;
      }

      for (int16_t i = 0; i < width; i++) {
        uint8_t c = src[i];
        if (c != transparent) {
          self->_lineBuffer[i] = palette[c];
        }
      }

      tft->pushImage(x, y, width, 1, self->_lineBuffer);
    } else {
      for (int16_t i = 0; i < width; i++) {
        self->_lineBuffer[i] = palette[src[i]];
      }

      tft->pushImage(x, y, width, 1, self->_lineBuffer);
    }
  }
};
