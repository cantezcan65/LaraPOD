#include <TFT_eSPI.h>
#include <pgmspace.h>
#include <Preferences.h>
#include <DFRobotDFPlayerMini.h>
#include "DinoGifPlayer.h"
#include "button_handler.h"
#include "bitmap.h"
#include "LaraMenu.h"

// tft is already defined in bitmap.h

HardwareSerial dfSerial(2);
DFRobotDFPlayerMini dfplayer;
Preferences prefs;
DinoGifPlayer dino;
LaraMenu menu;

// =====================================================
// State machine
// =====================================================
enum PlayerState {
  STATE_NOT_READY,
  STATE_STOPPED,
  STATE_PLAYING,
  STATE_PAUSED,
  STATE_MENUOPEN
};

enum BrowserLevel {
  BROWSER_ROOT_DIRS,
  BROWSER_FILES
};

enum PlayMode {
  PLAYMODE_NONE,
  PLAYMODE_SINGLE_FILE,
  PLAYMODE_WHOLE_FOLDER,
  PLAYMODE_RANDOM_ALL
};

PlayerState gState = STATE_NOT_READY;
PlayerState gStateBeforeMenu = STATE_STOPPED;
BrowserLevel gBrowserLevel = BROWSER_ROOT_DIRS;
PlayMode gPlayMode = PLAYMODE_NONE;

// =====================================================
// Global state
// =====================================================
bool gDfInitialized = false;
bool gNeedRedraw = true;

uint8_t gVolume = 10;
uint8_t gEqMode = 0;
RepeatMode gRepeatMode = REPEAT_OFF;
bool gRandomEnabled = false;

// Root browser is purely numeric and does not probe on boot.
static const uint8_t MAX_ROOT_FOLDERS = 20;

uint8_t gSelectedFolder = 1;
uint8_t gTopFolder = 1;

uint8_t gCurrentFolder = 1;
uint16_t gKnownFileCountInCurrentFolder = 0;   // 0 means unknown
uint16_t gSelectedFileRow = 0;                 // 0 = "..", 1..N
uint16_t gTopFileRow = 0;

uint8_t gPlayingFolder = 0;
uint16_t gPlayingTrack = 0;

unsigned long gBootMillis = 0;

// =====================================================
// UI constants
// =====================================================
static const int HEADER_H = 24;
static const int LIST_Y0 = 28;
static const int LIST_ROW_H = 17;
static const int VISIBLE_ROWS = 6;

// =====================================================
// Helpers
// =====================================================
const char* stateToText(PlayerState s) {
  switch (s) {
    case STATE_NOT_READY: return "NOT READY";
    case STATE_STOPPED:   return "STOPPED";
    case STATE_PLAYING:   return "PLAYING";
    case STATE_PAUSED:    return "PAUSED";
    case STATE_MENUOPEN:  return "MENU";
    default:              return "?";
  }
}

void setState(PlayerState s) {
  if (gState != s) {
    gState = s;
    Serial.printf("STATE -> %s\n", stateToText(gState));
    gNeedRedraw = true;
  }
}

bool inStartupGuardWindow() {
  return (millis() - gBootMillis) < 4000UL;
}

void saveSettings() {
  prefs.putUChar("volume", gVolume);
  prefs.putUChar("eqmode", gEqMode);
  prefs.putUChar("repeat", (uint8_t)gRepeatMode);
  prefs.putBool("random", gRandomEnabled);
}

void loadSettings() {
  prefs.begin("larapod", false);

  gVolume = prefs.getUChar("volume", 10);
  if (gVolume > 30) gVolume = 30;

  gEqMode = prefs.getUChar("eqmode", 0);
  if (gEqMode > 5) gEqMode = 0;

  uint8_t repeatRaw = prefs.getUChar("repeat", 0);
  if (repeatRaw > 2) repeatRaw = 0;
  gRepeatMode = (RepeatMode)repeatRaw;

  gRandomEnabled = prefs.getBool("random", false);
}

String folderLabel(uint8_t folder) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02u", folder);
  return String(buf);
}

String fileLabel(uint16_t fileNo) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%03u", fileNo);
  return String(buf);
}

// =====================================================
// TFT drawing
// =====================================================
void drawHeaderBar() {
  char buf[48];
  snprintf(buf, sizeof(buf), "%s - LaraPOD - V:%u", stateToText(gState), gVolume);

  tft.fillRect(0, 0, tft.width(), HEADER_H, TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextFont(2);
  tft.drawString(buf, tft.width() / 2, HEADER_H / 2);
}

void clearListArea() {
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H, TFT_WHITE);
}

void drawListRow(int row, const String& text, bool selected) {
  int y = LIST_Y0 + row * LIST_ROW_H;
  uint16_t bg = selected ? TFT_BLACK : TFT_WHITE;
  uint16_t fg = selected ? TFT_WHITE : TFT_BLACK;

  tft.fillRect(6, y, tft.width() - 12, LIST_ROW_H - 1, bg);
  tft.drawRect(6, y, tft.width() - 12, LIST_ROW_H - 1, TFT_LIGHTGREY);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  tft.setTextFont(2);
  tft.drawString(text, 12, y + (LIST_ROW_H / 2));
}

void drawCenteredMessage(const char* line1, const char* line2 = nullptr) {
  clearListArea();

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setTextFont(2);
  tft.drawString(line1, tft.width() / 2, 55);

  if (line2) {
    tft.drawString(line2, tft.width() / 2, 80);
  }
}

void drawRootBrowser() {
  clearListArea();

  for (int row = 0; row < VISIBLE_ROWS; row++) {
    uint8_t folder = gTopFolder + row;
    if (folder > MAX_ROOT_FOLDERS) break;
    drawListRow(row, folderLabel(folder), folder == gSelectedFolder);
  }
}

void drawFileBrowser() {
  clearListArea();

  uint16_t maxRow = (gKnownFileCountInCurrentFolder > 0) ? gKnownFileCountInCurrentFolder : 20;

  for (int row = 0; row < VISIBLE_ROWS; row++) {
    uint16_t itemRow = gTopFileRow + row;
    if (itemRow > maxRow) break;

    String label = (itemRow == 0) ? ".." : fileLabel(itemRow);
    drawListRow(row, label, itemRow == gSelectedFileRow);
  }
}

void redrawUI() {
  drawHeaderBar();

  switch (gState) {
    case STATE_NOT_READY:
      drawCenteredMessage("DFPlayer not ready");
      break;

    case STATE_STOPPED:
      if (gBrowserLevel == BROWSER_ROOT_DIRS) {
        drawRootBrowser();
      } else {
        drawFileBrowser();
      }
      break;

    case STATE_PLAYING:
      clearListArea();
      break;

    case STATE_PAUSED: {
      clearListArea();
      char l1[32], l2[32];
      snprintf(l1, sizeof(l1), "Folder %02u", gPlayingFolder);
      snprintf(l2, sizeof(l2), "Track %03u", gPlayingTrack);
      drawCenteredMessage("Paused", l1);
      tft.drawString(l2, tft.width() / 2, 100);
      break;
    }

    case STATE_MENUOPEN:
      clearListArea();
      menu.draw(&tft, HEADER_H);
      break;
  }

  gNeedRedraw = false;
}

// =====================================================
// DFPlayer
// =====================================================
void printDfPlayerError(int value) {
  Serial.print("DFPlayer error: ");
  switch (value) {
    case Busy:              Serial.println("Card not found"); break;
    case Sleeping:          Serial.println("Sleeping"); break;
    case SerialWrongStack:  Serial.println("Serial wrong stack"); break;
    case CheckSumNotMatch:  Serial.println("Checksum mismatch"); break;
    case FileIndexOut:      Serial.println("File index out of range"); break;
    case FileMismatch:      Serial.println("Cannot find file"); break;
    case Advertise:         Serial.println("In advertise"); break;
    default:
      Serial.printf("Unknown error code %d\n", value);
      break;
  }
}

bool evaluateReadiness() {
  if (!gDfInitialized) {
    setState(STATE_NOT_READY);
    return false;
  }

  gBrowserLevel = BROWSER_ROOT_DIRS;
  gSelectedFolder = 1;
  gTopFolder = 1;
  gKnownFileCountInCurrentFolder = 0;
  gSelectedFileRow = 0;
  gTopFileRow = 0;

  setState(STATE_STOPPED);
  return true;
}

bool openFolder(uint8_t folderNo) {
  if (!gDfInitialized) return false;

  gCurrentFolder = folderNo;
  gKnownFileCountInCurrentFolder = 0;
  gSelectedFileRow = 0;
  gTopFileRow = 0;
  gBrowserLevel = BROWSER_FILES;
  gNeedRedraw = true;

  Serial.printf("Entered folder %02u\n", folderNo);
  return true;
}

// =====================================================
// Playback helpers
// =====================================================
void applyVolume() {
  dfplayer.volume(gVolume);
}

void applyEqSetting() {
  switch (gEqMode) {
    default:
    case 0: dfplayer.EQ(DFPLAYER_EQ_NORMAL);  break;
    case 1: dfplayer.EQ(DFPLAYER_EQ_POP);     break;
    case 2: dfplayer.EQ(DFPLAYER_EQ_ROCK);    break;
    case 3: dfplayer.EQ(DFPLAYER_EQ_JAZZ);    break;
    case 4: dfplayer.EQ(DFPLAYER_EQ_CLASSIC); break;
    case 5: dfplayer.EQ(DFPLAYER_EQ_BASS);    break;
  }
}

void setVolume(int v) {
  if (v < 0) v = 0;
  if (v > 30) v = 30;
  if ((uint8_t)v == gVolume) return;

  gVolume = (uint8_t)v;
  applyVolume();
  saveSettings();
  Serial.printf("Volume -> %u\n", gVolume);
  gNeedRedraw = true;
}

void enterMenu() {
  gStateBeforeMenu = gState;
  menu.open(gEqMode, gRepeatMode, gRandomEnabled);
  setState(STATE_MENUOPEN);
}

void leaveMenu() {
  setState(gStateBeforeMenu);
}

bool startRandomAll() {
  if (!gDfInitialized) return false;

  Serial.println("Start random all");
  dfplayer.randomAll();

  gPlayMode = PLAYMODE_RANDOM_ALL;
  gPlayingFolder = 0;
  gPlayingTrack = 0;
  setState(STATE_PLAYING);
  return true;
}

void applyMenuSelection(uint8_t eqMode, RepeatMode repeatMode, bool randomEnabled) {
  bool randomWasEnabled = gRandomEnabled;

  gEqMode = eqMode;
  gRepeatMode = repeatMode;
  gRandomEnabled = randomEnabled;

  applyEqSetting();
  saveSettings();

  if (!randomWasEnabled && gRandomEnabled &&
      (gStateBeforeMenu == STATE_PLAYING || gStateBeforeMenu == STATE_PAUSED)) {
    startRandomAll();
    gStateBeforeMenu = STATE_PLAYING;
  }
}

// =====================================================
// Playback
// =====================================================
bool playTrack(uint8_t folder, uint16_t fileNo, PlayMode mode) {
  if (!gDfInitialized) return false;
  if (folder < 1) return false;
  if (fileNo < 1) return false;

  if (gRandomEnabled) {
    return startRandomAll();
  }

  Serial.printf("Play folder %02u file %03u\n", folder, fileNo);

  // Keep playback start simple and robust
  dfplayer.playFolder(folder, fileNo);

  gPlayingFolder = folder;
  gPlayingTrack = fileNo;
  gPlayMode = mode;

  setState(STATE_PLAYING);
  return true;
}

void stopPlayback() {
  if (!gDfInitialized) return;
  dfplayer.stop();
  gPlayMode = PLAYMODE_NONE;
  setState(STATE_STOPPED);
}

void pausePlayback() {
  if (!gDfInitialized) return;
  dfplayer.pause();
  setState(STATE_PAUSED);
}

void resumePlayback() {
  if (!gDfInitialized) return;
  dfplayer.start();
  setState(STATE_PLAYING);
}

void playSelectedFileFromBrowser() {
  if (gBrowserLevel != BROWSER_FILES) return;
  if (gSelectedFileRow == 0) return;

  if (gRandomEnabled) {
    startRandomAll();
    return;
  }

  playTrack(gCurrentFolder, gSelectedFileRow, PLAYMODE_SINGLE_FILE);
}

void playWholeSelectedFolder() {
  if (gBrowserLevel != BROWSER_ROOT_DIRS) return;

  if (gRandomEnabled) {
    startRandomAll();
    return;
  }

  playTrack(gSelectedFolder, 1, PLAYMODE_WHOLE_FOLDER);
}

void playNextTrack() {
  if (gPlayingFolder == 0) return;
  playTrack(gPlayingFolder, gPlayingTrack + 1, gPlayMode);
}

void playPreviousTrack() {
  if (gPlayingFolder == 0) return;
  if (gPlayingTrack > 1) {
    playTrack(gPlayingFolder, gPlayingTrack - 1, gPlayMode);
  }
}

void onTrackFinished() {
  Serial.printf("Track finished: folder=%02u track=%03u\n", gPlayingFolder, gPlayingTrack);

  if (gRandomEnabled || gPlayMode == PLAYMODE_RANDOM_ALL) {
    startRandomAll();
    return;
  }

  if (gRepeatMode == REPEAT_SINGLE_FILE) {
    playTrack(gPlayingFolder, gPlayingTrack, PLAYMODE_SINGLE_FILE);
    return;
  }

  if (gRepeatMode == REPEAT_FOLDER) {
    if (gPlayingFolder > 0) {
      playTrack(gPlayingFolder, gPlayingTrack + 1, PLAYMODE_WHOLE_FOLDER);
      return;
    }
  }

  if (gPlayMode == PLAYMODE_WHOLE_FOLDER) {
    playTrack(gPlayingFolder, gPlayingTrack + 1, PLAYMODE_WHOLE_FOLDER);
    return;
  }

  gPlayMode = PLAYMODE_NONE;
  setState(STATE_STOPPED);
}

// =====================================================
// Browser navigation
// =====================================================
void browseUp() {
  if (gBrowserLevel == BROWSER_ROOT_DIRS) {
    if (gSelectedFolder > 1) {
      gSelectedFolder--;
      if (gSelectedFolder < gTopFolder) gTopFolder = gSelectedFolder;
      gNeedRedraw = true;
    }
  } else {
    if (gSelectedFileRow > 0) {
      gSelectedFileRow--;
      if (gSelectedFileRow < gTopFileRow) gTopFileRow = gSelectedFileRow;
      gNeedRedraw = true;
    }
  }
}

void browseDown() {
  if (gBrowserLevel == BROWSER_ROOT_DIRS) {
    if (gSelectedFolder < MAX_ROOT_FOLDERS) {
      gSelectedFolder++;
      if (gSelectedFolder >= gTopFolder + VISIBLE_ROWS) {
        gTopFolder = gSelectedFolder - VISIBLE_ROWS + 1;
      }
      gNeedRedraw = true;
    }
  } else {
    uint16_t maxRow = (gKnownFileCountInCurrentFolder > 0) ? gKnownFileCountInCurrentFolder : 20;
    if (gSelectedFileRow < maxRow) {
      gSelectedFileRow++;
      if (gSelectedFileRow >= gTopFileRow + VISIBLE_ROWS) {
        gTopFileRow = gSelectedFileRow - VISIBLE_ROWS + 1;
      }
      gNeedRedraw = true;
    }
  }
}

void browserSelect() {
  if (gBrowserLevel == BROWSER_ROOT_DIRS) {
    openFolder(gSelectedFolder);
    return;
  }

  if (gSelectedFileRow == 0) {
    gBrowserLevel = BROWSER_ROOT_DIRS;
    gNeedRedraw = true;
    return;
  }

  playSelectedFileFromBrowser();
}

// =====================================================
// DFPlayer event polling
// =====================================================
void pollDfPlayerMessages() {
  if (!gDfInitialized) return;

  while (dfplayer.available()) {
    int type = dfplayer.readType();
    int value = dfplayer.read();

    Serial.printf("DF msg type=%d value=%d\n", type, value);

    switch (type) {
      case DFPlayerCardInserted:
      case DFPlayerCardOnline:
        if (inStartupGuardWindow()) {
          Serial.println("Ignoring startup card-online event");
        }
        break;

      case DFPlayerCardRemoved:
        if (inStartupGuardWindow()) {
          Serial.println("Ignoring startup card-removed event");
        } else {
          Serial.println("Card removed");
          gPlayMode = PLAYMODE_NONE;
          setState(STATE_NOT_READY);
        }
        break;

      case DFPlayerPlayFinished:
        onTrackFinished();
        break;

      case DFPlayerError:
        printDfPlayerError(value);
        break;

      default:
        break;
    }
  }
}

// =====================================================
// Input handling
// =====================================================
void processButtons() {
  bool up    = upClicked;         upClicked = false;
  bool down  = downClicked;       downClicked = false;
  bool left  = leftClicked;       leftClicked = false;
  bool right = rightClicked;      rightClicked = false;
  bool mid   = middleClicked;     middleClicked = false;
  bool dbl   = middleDblClicked;  middleDblClicked = false;
  bool lng   = middleLongClicked; middleLongClicked = false;

  switch (gState) {
    case STATE_NOT_READY:
      (void)up; (void)down; (void)left; (void)right; (void)mid; (void)dbl; (void)lng;
      break;

    case STATE_STOPPED:
      if (up)   browseUp();
      if (down) browseDown();
      if (left)  setVolume(gVolume - 1);
      if (right) setVolume(gVolume + 1);

      if (dbl) {
        if (gBrowserLevel == BROWSER_ROOT_DIRS) {
          playWholeSelectedFolder();
        }
      } else if (mid) {
        browserSelect();
      }

      if (lng) {
        enterMenu();
      }
      break;

    case STATE_PLAYING:
      if (mid)   pausePlayback();
      if (dbl)   stopPlayback();
      if (up)    playPreviousTrack();
      if (down)  playNextTrack();
      if (left)  setVolume(gVolume - 1);
      if (right) setVolume(gVolume + 1);

      if (lng) {
        enterMenu();
      }
      break;

    case STATE_PAUSED:
      if (mid)   resumePlayback();
      if (dbl)   stopPlayback();
      if (left)  setVolume(gVolume - 1);
      if (right) setVolume(gVolume + 1);

      if (lng) {
        enterMenu();
      }
      break;

    case STATE_MENUOPEN: {
      bool anyInput = up || down || left || right || mid || dbl || lng;

      LaraMenuResult mr = menu.handleInput(up, down, left, right, mid, dbl, lng);

      if (mr.selectionCommitted) {
        applyMenuSelection(mr.eqMode, mr.repeatMode, mr.randomEnabled);
      }

      if (mr.closeMenu) {
        leaveMenu();
      } else if (anyInput) {
        gNeedRedraw = true;
      }
      break;
    }
  }
}

// =====================================================
// Setup
// =====================================================
void setupTFT() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLUE);
  drawBitmapCentered(TFT_WHITE, TFT_BLUE);
  delay(5000);
  tft.fillScreen(TFT_WHITE);
}

bool setupDFPlayer() {
  Serial.println("Init DFPlayer...");

  dfSerial.begin(9600, SERIAL_8N1, 22, 21);
  delay(500);

  if (!dfplayer.begin(dfSerial, true, false)) {
    Serial.println("DFPlayer begin FAILED");
    gDfInitialized = false;
    return false;
  }

  gDfInitialized = true;
  Serial.println("DFPlayer begin OK");

  applyVolume();
  applyEqSetting();
  delay(200);

  return true;
}

void setup() {
  Serial.begin(115200);
  gBootMillis = millis();
  setupTFT();
  loadSettings();
  setupButtons();
  menu.begin();
  drawHeaderBar();
  drawCenteredMessage("Initializing...", "DFPlayer");
  delay(200);

  if (setupDFPlayer()) {
    delay(1000);
    evaluateReadiness();
  } else {
    setState(STATE_NOT_READY);
  }

  bool ok = dino.begin(&tft, "/dino_100x100.gif", 70, 29, TFT_BLACK);
  Serial.printf("Dino begin: %s\n", ok ? "OK" : "FAILED");
  gNeedRedraw = true;
}

void loop() {
  handleButtons();
  pollDfPlayerMessages();
  processButtons();

  if (gNeedRedraw) {
    redrawUI();
  }

  if (gState == STATE_PLAYING) {
    dino.update();
  }
}
