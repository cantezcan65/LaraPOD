#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

enum LaraMenuPage {
  MENU_PAGE_MAIN,
  MENU_PAGE_EQUALIZER,
  MENU_PAGE_REPEAT,
  MENU_PAGE_RANDOM
};

enum RepeatMode {
  REPEAT_OFF,
  REPEAT_SINGLE_FILE,
  REPEAT_FOLDER
};

struct LaraMenuResult {
  bool closeMenu = false;
  bool selectionCommitted = false;

  uint8_t eqMode = 0;
  RepeatMode repeatMode = REPEAT_OFF;
  bool randomEnabled = false;
};

class LaraMenu {
public:
  void begin();
  void open(uint8_t currentEqMode, RepeatMode currentRepeatMode, bool currentRandomEnabled);
  void draw(TFT_eSPI* tft, int headerH);
  LaraMenuResult handleInput(bool up, bool down, bool left, bool right, bool mid, bool dbl, bool lng);

private:
  LaraMenuPage mPage = MENU_PAGE_MAIN;
  uint8_t mSelectedMain = 0;

  uint8_t mEqMode = 0;
  RepeatMode mRepeatMode = REPEAT_OFF;
  bool mRandomEnabled = false;

  uint8_t mSelectedOption = 0;

  void drawRow(TFT_eSPI* tft, int y, const String& text, bool selected);
  void drawMainPage(TFT_eSPI* tft, int headerH);
  void drawEqPage(TFT_eSPI* tft, int headerH);
  void drawRepeatPage(TFT_eSPI* tft, int headerH);
  void drawRandomPage(TFT_eSPI* tft, int headerH);

  uint8_t optionCountForPage() const;
  void syncSelectedOptionToCurrentValue();
};
