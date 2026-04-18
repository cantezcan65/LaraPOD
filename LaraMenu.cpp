#include "LaraMenu.h"

static const int MENU_Y0 = 28;
static const int MENU_ROW_H = 17;

static const char* EQ_LABELS[] = {
  "0: Normal",
  "1: Pop",
  "2: Rock",
  "3: Jazz",
  "4: Classic",
  "5: Bass"
};

static const char* REPEAT_LABELS[] = {
  "No Repeat",
  "Single File Repeat",
  "Folder Repeat"
};

static const char* RANDOM_LABELS[] = {
  "No",
  "Yes"
};

void LaraMenu::begin() {
  mPage = MENU_PAGE_MAIN;
  mSelectedMain = 0;
  mEqMode = 0;
  mRepeatMode = REPEAT_OFF;
  mRandomEnabled = false;
  mSelectedOption = 0;
}

void LaraMenu::open(uint8_t currentEqMode, RepeatMode currentRepeatMode, bool currentRandomEnabled) {
  mEqMode = currentEqMode;
  if (mEqMode > 5) mEqMode = 0;

  mRepeatMode = currentRepeatMode;
  mRandomEnabled = currentRandomEnabled;

  mPage = MENU_PAGE_MAIN;
  mSelectedMain = 0;
  mSelectedOption = 0;
}

void LaraMenu::drawRow(TFT_eSPI* tft, int y, const String& text, bool selected) {
  uint16_t bg = selected ? TFT_BLACK : TFT_WHITE;
  uint16_t fg = selected ? TFT_WHITE : TFT_BLACK;

  tft->fillRect(6, y, tft->width() - 12, MENU_ROW_H - 1, bg);
  tft->drawRect(6, y, tft->width() - 12, MENU_ROW_H - 1, TFT_LIGHTGREY);

  tft->setTextDatum(ML_DATUM);
  tft->setTextColor(fg, bg);
  tft->setTextFont(2);
  tft->drawString(text, 12, y + (MENU_ROW_H / 2));
}

void LaraMenu::drawMainPage(TFT_eSPI* tft, int headerH) {
  tft->fillRect(0, headerH, tft->width(), tft->height() - headerH, TFT_WHITE);

  drawRow(tft, MENU_Y0 + 0 * MENU_ROW_H, "Equalizer", mSelectedMain == 0);
  drawRow(tft, MENU_Y0 + 1 * MENU_ROW_H, "Repeat",    mSelectedMain == 1);
  drawRow(tft, MENU_Y0 + 2 * MENU_ROW_H, "Random",    mSelectedMain == 2);
}

void LaraMenu::drawEqPage(TFT_eSPI* tft, int headerH) {
  tft->fillRect(0, headerH, tft->width(), tft->height() - headerH, TFT_WHITE);
  for (int i = 0; i < 6; i++) {
    drawRow(tft, MENU_Y0 + i * MENU_ROW_H, EQ_LABELS[i], mSelectedOption == i);
  }
}

void LaraMenu::drawRepeatPage(TFT_eSPI* tft, int headerH) {
  tft->fillRect(0, headerH, tft->width(), tft->height() - headerH, TFT_WHITE);
  for (int i = 0; i < 3; i++) {
    drawRow(tft, MENU_Y0 + i * MENU_ROW_H, REPEAT_LABELS[i], mSelectedOption == i);
  }
}

void LaraMenu::drawRandomPage(TFT_eSPI* tft, int headerH) {
  tft->fillRect(0, headerH, tft->width(), tft->height() - headerH, TFT_WHITE);
  for (int i = 0; i < 2; i++) {
    drawRow(tft, MENU_Y0 + i * MENU_ROW_H, RANDOM_LABELS[i], mSelectedOption == i);
  }
}

void LaraMenu::draw(TFT_eSPI* tft, int headerH) {
  switch (mPage) {
    case MENU_PAGE_MAIN:      drawMainPage(tft, headerH);   break;
    case MENU_PAGE_EQUALIZER: drawEqPage(tft, headerH);     break;
    case MENU_PAGE_REPEAT:    drawRepeatPage(tft, headerH); break;
    case MENU_PAGE_RANDOM:    drawRandomPage(tft, headerH); break;
  }
}

uint8_t LaraMenu::optionCountForPage() const {
  switch (mPage) {
    case MENU_PAGE_EQUALIZER: return 6;
    case MENU_PAGE_REPEAT:    return 3;
    case MENU_PAGE_RANDOM:    return 2;
    default:                  return 0;
  }
}

void LaraMenu::syncSelectedOptionToCurrentValue() {
  switch (mPage) {
    case MENU_PAGE_EQUALIZER:
      mSelectedOption = mEqMode;
      break;
    case MENU_PAGE_REPEAT:
      mSelectedOption = (uint8_t)mRepeatMode;
      break;
    case MENU_PAGE_RANDOM:
      mSelectedOption = mRandomEnabled ? 1 : 0;
      break;
    default:
      mSelectedOption = 0;
      break;
  }
}

LaraMenuResult LaraMenu::handleInput(bool up, bool down, bool left, bool right, bool mid, bool dbl, bool lng) {
  (void)right;

  LaraMenuResult r;
  r.eqMode = mEqMode;
  r.repeatMode = mRepeatMode;
  r.randomEnabled = mRandomEnabled;

  if (lng || dbl) {
    r.closeMenu = true;
    return r;
  }

  if (mPage == MENU_PAGE_MAIN) {
    if (up && mSelectedMain > 0) mSelectedMain--;
    if (down && mSelectedMain < 2) mSelectedMain++;

    if (mid) {
      if (mSelectedMain == 0) mPage = MENU_PAGE_EQUALIZER;
      else if (mSelectedMain == 1) mPage = MENU_PAGE_REPEAT;
      else mPage = MENU_PAGE_RANDOM;

      syncSelectedOptionToCurrentValue();
    }

    return r;
  }

  uint8_t count = optionCountForPage();

  if (left) {
    mPage = MENU_PAGE_MAIN;
    return r;
  }

  if (up && mSelectedOption > 0) {
    mSelectedOption--;
  }
  if (down && mSelectedOption + 1 < count) {
    mSelectedOption++;
  }

  if (mid) {
    if (mPage == MENU_PAGE_EQUALIZER) {
      mEqMode = mSelectedOption;
    } else if (mPage == MENU_PAGE_REPEAT) {
      mRepeatMode = (RepeatMode)mSelectedOption;
    } else if (mPage == MENU_PAGE_RANDOM) {
      mRandomEnabled = (mSelectedOption == 1);
    }

    r.eqMode = mEqMode;
    r.repeatMode = mRepeatMode;
    r.randomEnabled = mRandomEnabled;
    r.selectionCommitted = true;
    r.closeMenu = true;
  }

  return r;
}
