// =========================
// Button pin mapping
// =========================
const int PIN_UP     = 32;
const int PIN_RIGHT  = 33;
const int PIN_LEFT   = 25;
const int PIN_MIDDLE = 26;
const int PIN_DOWN   = 27;

// =========================
// Public event flags
// =========================
volatile bool upClicked         = false;
volatile bool rightClicked      = false;
volatile bool leftClicked       = false;
volatile bool downClicked       = false;

volatile bool middleClicked     = false;
volatile bool middleDblClicked  = false;
volatile bool middleLongClicked = false;

// =========================
// Timing
// =========================
static const uint32_t DEBOUNCE_MS        = 60;
static const uint32_t MIDDLE_DEBOUNCE_MS = 40;
static const uint32_t LONG_PRESS_MS      = 700;
static const uint32_t DOUBLE_CLICK_MS    = 450;

// =========================
// Internal raw IRQ flags
// ISR only sets these
// =========================
volatile bool upIrq     = false;
volatile bool rightIrq  = false;
volatile bool leftIrq   = false;
volatile bool middleIrq = false;
volatile bool downIrq   = false;

// =========================
// Internal state for simple buttons
// =========================
bool upPressed     = false;
bool rightPressed  = false;
bool leftPressed   = false;
bool downPressed   = false;

uint32_t upDownMs     = 0;
uint32_t rightDownMs  = 0;
uint32_t leftDownMs   = 0;
uint32_t downDownMs   = 0;

// =========================
// Internal state for middle button
// =========================
bool middlePressed            = false;
bool middleLongSent           = false;
bool middleWaitingSecondClick = false;

uint32_t middleDownMs         = 0;
uint32_t middleReleaseMs      = 0;

// =========================
// ISR prototypes
// =========================
void IRAM_ATTR isrUp();
void IRAM_ATTR isrRight();
void IRAM_ATTR isrLeft();
void IRAM_ATTR isrMiddle();
void IRAM_ATTR isrDown();


// =========================
// Setup
// =========================
void setupButtons() {
  pinMode(PIN_UP,     INPUT_PULLUP);
  pinMode(PIN_RIGHT,  INPUT_PULLUP);
  pinMode(PIN_LEFT,   INPUT_PULLUP);
  pinMode(PIN_MIDDLE, INPUT_PULLUP);
  pinMode(PIN_DOWN,   INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(PIN_UP),     isrUp,     FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_RIGHT),  isrRight,  FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_LEFT),   isrLeft,   FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_MIDDLE), isrMiddle, FALLING);
  attachInterrupt(digitalPinToInterrupt(PIN_DOWN),   isrDown,   FALLING);
}

// =========================
// Handle all buttons in loop()
// =========================
void handleButtons() {
  uint32_t now = millis();

  // -------------------------
  // Transfer IRQ flags safely
  // -------------------------
  noInterrupts();
  bool upEvent     = upIrq;     upIrq = false;
  bool rightEvent  = rightIrq;  rightIrq = false;
  bool leftEvent   = leftIrq;   leftIrq = false;
  bool middleEvent = middleIrq; middleIrq = false;
  bool downEvent   = downIrq;   downIrq = false;
  interrupts();

  // -------------------------
  // Start press tracking on IRQ
  // -------------------------
  if (upEvent && !upPressed) {
    upPressed = true;
    upDownMs = now;
  }

  if (rightEvent && !rightPressed) {
    rightPressed = true;
    rightDownMs = now;
  }

  if (leftEvent && !leftPressed) {
    leftPressed = true;
    leftDownMs = now;
  }

  if (downEvent && !downPressed) {
    downPressed = true;
    downDownMs = now;
  }

  if (middleEvent && !middlePressed) {
    middlePressed = true;
    middleLongSent = false;
    middleDownMs = now;
  }

  // -------------------------
  // Simple buttons:
  // generate click only after release + debounce
  // -------------------------
  if (upPressed) {
    if (digitalRead(PIN_UP) == HIGH) {
      if (now - upDownMs >= DEBOUNCE_MS) {
        upClicked = true;
      }
      upPressed = false;
    }
  }

  if (rightPressed) {
    if (digitalRead(PIN_RIGHT) == HIGH) {
      if (now - rightDownMs >= DEBOUNCE_MS) {
        rightClicked = true;
      }
      rightPressed = false;
    }
  }

  if (leftPressed) {
    if (digitalRead(PIN_LEFT) == HIGH) {
      if (now - leftDownMs >= DEBOUNCE_MS) {
        leftClicked = true;
      }
      leftPressed = false;
    }
  }

  if (downPressed) {
    if (digitalRead(PIN_DOWN) == HIGH) {
      if (now - downDownMs >= DEBOUNCE_MS) {
        downClicked = true;
      }
      downPressed = false;
    }
  }

  // -------------------------
  // Middle button long press
  // -------------------------
  if (middlePressed) {
    if (digitalRead(PIN_MIDDLE) == LOW) {
      if (!middleLongSent && (now - middleDownMs >= LONG_PRESS_MS)) {
        middleLongClicked = true;
        middleLongSent = true;
        middleWaitingSecondClick = false;
      }
    } else {
      // released
      uint32_t pressDuration = now - middleDownMs;

      if (!middleLongSent && pressDuration >= MIDDLE_DEBOUNCE_MS) {
        if (middleWaitingSecondClick) {
          if (now - middleReleaseMs <= DOUBLE_CLICK_MS) {
            middleDblClicked = true;
            middleWaitingSecondClick = false;
          } else {
            middleWaitingSecondClick = true;
            middleReleaseMs = now;
          }
        } else {
          middleWaitingSecondClick = true;
          middleReleaseMs = now;
        }
      }

      middlePressed = false;
    }
  }

  // -------------------------
  // Middle single click timeout
  // -------------------------
  if (middleWaitingSecondClick && !middlePressed) {
    if (now - middleReleaseMs > DOUBLE_CLICK_MS) {
      middleClicked = true;
      middleWaitingSecondClick = false;
    }
  }
}

// =========================
// ISRs
// Keep them minimal
// =========================
void IRAM_ATTR isrUp() {
  upIrq = true;
}

void IRAM_ATTR isrRight() {
  rightIrq = true;
}

void IRAM_ATTR isrLeft() {
  leftIrq = true;
}

void IRAM_ATTR isrMiddle() {
  middleIrq = true;
}

void IRAM_ATTR isrDown() {
  downIrq = true;
}

