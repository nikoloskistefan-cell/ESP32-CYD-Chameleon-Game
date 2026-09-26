#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

// =====================================================
// CYD TFT
// =====================================================

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21

// =====================================================
// TOUCH
// =====================================================

#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33
#define TOUCH_IRQ  36

#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3900

// =====================================================
// BUZZER
// =====================================================

#define BUZZER_PIN 22

// =====================================================
// DISPLAY
// =====================================================

#define SCREEN_W 320
#define SCREEN_H 240
#define HUD_H 30

SPIClass tftSPI(HSPI);
SPIClass touchSPI(VSPI);

Adafruit_ILI9341 tft(
  &tftSPI,
  TFT_DC,
  TFT_CS,
  TFT_RST
);

XPT2046_Touchscreen ts(
  TOUCH_CS,
  TOUCH_IRQ
);

Preferences preferences;

// =====================================================
// COLORS
// =====================================================

#define WALL_COLOR       0xBDF7
#define FLOOR_COLOR      0xD5A9
#define FLOOR_LINE       0xA3E5

#define SHELF_COLOR      0x9A60
#define SHELF_DARK       0x61C0

#define BED_COLOR        0x7B1F
#define BED_LIGHT        0xB5BF
#define PILLOW_COLOR     0xFF9C

#define CURTAIN_COLOR    0x34BF
#define WINDOW_COLOR     0x7EFF

#define RUG_COLOR        0xEACB
#define RUG_DARK         0xB186

#define TOYBOX_COLOR     0xFBE0

#define PLANT_COLOR      0x05C0
#define PLANT_DARK       0x03A0
#define POT_COLOR        0xA3E0

#define PICTURE_COLOR    0xFFE0
#define LAMP_COLOR       0xFFE0

#define HUD_COLOR        0x001F

#define FOUND_COLOR      0x07E0
#define HINT_COLOR       0xFFE0

// =====================================================
// GAME
// =====================================================

enum GameState {
  TITLE_SCREEN,
  PLAYING,
  WIN_SCREEN,
  TIME_UP_SCREEN
};

GameState gameState = TITLE_SCREEN;

const unsigned long GAME_DURATION = 45000;

unsigned long gameStartedAt = 0;

long score = 0;
long highScore = 0;

int foundCount = 0;
int hintsLeft = 3;

#define HIDERS_PER_GAME 5
#define CANDIDATE_COUNT 12

// =====================================================
// HIDING SPOTS
// =====================================================

struct HideSpot {
  int x;
  int y;

  uint16_t bodyColor;
  uint16_t detailColor;

  bool faceRight;
};

HideSpot spots[CANDIDATE_COUNT] = {

  // BOOK SHELF
  {  45,  57, SHELF_COLOR, SHELF_DARK, true  },
  {  88,  92, SHELF_COLOR, SHELF_DARK, false },

  // PICTURE
  { 151,  65, PICTURE_COLOR, 0xD680, true },

  // CURTAINS
  { 226,  82, CURTAIN_COLOR, 0x1B59, false },
  { 278,  91, CURTAIN_COLOR, 0x1B59, true  },

  // BED / PILLOW
  {  55, 147, PILLOW_COLOR, 0xE6F7, true  },
  { 112, 162, BED_COLOR, BED_LIGHT, false },

  // LAMP
  { 167, 135, LAMP_COLOR, 0xD680, true },

  // TOY BOX
  { 225, 153, TOYBOX_COLOR, 0xCB20, false },

  // PLANT
  { 282, 125, PLANT_COLOR, PLANT_DARK, true },

  // RUG
  { 177, 196, RUG_COLOR, RUG_DARK, false },
  { 253, 197, RUG_COLOR, RUG_DARK, true }
};

// selected hiders

int selectedHiders[HIDERS_PER_GAME];

bool hiderFound[HIDERS_PER_GAME];

// =====================================================
// HINT
// =====================================================

int activeHint = -1;

unsigned long hintUntil = 0;

const unsigned long HINT_DURATION = 1500;

// =====================================================
// TOUCH
// =====================================================

bool previousTouch = false;

// =====================================================
// HUD TIMER
// =====================================================

unsigned long lastHUDUpdate = 0;

// =====================================================
// TITLE MUSIC
// =====================================================

const int titleNotes[] = {

  523, 659, 784, 659,
  587, 698, 880, 698,

  659, 784, 988, 784,
  523, 659, 784, 1047
};

const int titleDurations[] = {

  120,120,160,120,
  120,120,160,120,

  120,120,180,150,
  120,120,160,250
};

const int TITLE_NOTE_COUNT =
  sizeof(titleNotes) /
  sizeof(titleNotes[0]);

int titleNote = 0;

unsigned long titleNoteStarted = 0;

bool titleMusicRunning = false;

// =====================================================
// FORWARD DECLARATIONS
// =====================================================

void drawScene();
void updateHUD();
void startGame();
void showWin();
void showTimeUp();

// =====================================================
// SOUND
// =====================================================

void soundOff() {

  ledcWriteTone(
    BUZZER_PIN,
    0
  );
}

// -----------------------------------------------------
// FOUND!
// -----------------------------------------------------

void soundFound() {

  ledcWriteTone(
    BUZZER_PIN,
    850
  );

  delay(35);

  ledcWriteTone(
    BUZZER_PIN,
    1200
  );

  delay(40);

  ledcWriteTone(
    BUZZER_PIN,
    1700
  );

  delay(70);

  soundOff();
}

// -----------------------------------------------------
// WRONG PLACE
// -----------------------------------------------------

void soundWrong() {

  ledcWriteTone(
    BUZZER_PIN,
    240
  );

  delay(55);

  soundOff();
}

// -----------------------------------------------------
// HINT
// -----------------------------------------------------

void soundHint() {

  ledcWriteTone(
    BUZZER_PIN,
    700
  );

  delay(40);

  ledcWriteTone(
    BUZZER_PIN,
    1000
  );

  delay(70);

  soundOff();
}

// -----------------------------------------------------
// START
// -----------------------------------------------------

void soundStart() {

  ledcWriteTone(BUZZER_PIN, 523);
  delay(55);

  ledcWriteTone(BUZZER_PIN, 659);
  delay(55);

  ledcWriteTone(BUZZER_PIN, 784);
  delay(55);

  ledcWriteTone(BUZZER_PIN, 1047);
  delay(100);

  soundOff();
}

// -----------------------------------------------------
// WIN
// -----------------------------------------------------

void soundWin() {

  int notes[] = {
    523,
    659,
    784,
    1047,
    1319,
    1568
  };

  for (
    int i = 0;
    i < 6;
    i++
  ) {

    ledcWriteTone(
      BUZZER_PIN,
      notes[i]
    );

    delay(90);
  }

  soundOff();
}

// =====================================================
// TITLE MUSIC
// =====================================================

void startTitleMusic() {

  titleNote = 0;

  titleNoteStarted =
    millis();

  titleMusicRunning =
    true;

  ledcWriteTone(
    BUZZER_PIN,
    titleNotes[0]
  );
}

void stopTitleMusic() {

  titleMusicRunning =
    false;

  soundOff();
}

void updateTitleMusic() {

  if (
    !titleMusicRunning
  ) {
    return;
  }

  unsigned long now =
    millis();

  if (
    now -
    titleNoteStarted >=
    titleDurations[titleNote]
  ) {

    titleNote++;

    if (
      titleNote >=
      TITLE_NOTE_COUNT
    ) {

      titleNote = 0;
    }

    ledcWriteTone(
      BUZZER_PIN,
      titleNotes[titleNote]
    );

    titleNoteStarted =
      now;
  }
}

// =====================================================
// CENTER TEXT
// =====================================================

void centerText(
  const char* text,
  int y,
  int size,
  uint16_t color
) {

  tft.setTextSize(size);

  tft.setTextColor(
    color
  );

  int16_t x1;
  int16_t y1;

  uint16_t w;
  uint16_t h;

  tft.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &w,
    &h
  );

  tft.setCursor(
    (SCREEN_W - w) / 2,
    y
  );

  tft.print(text);
}

// =====================================================
// CHAMELEON
// =====================================================

void drawChameleon(
  int x,
  int y,
  uint16_t bodyColor,
  uint16_t detailColor,
  bool faceRight,
  bool found
) {

  int dir =
    faceRight ?
    1 :
    -1;

  if (
    found
  ) {

    bodyColor =
      FOUND_COLOR;

    detailColor =
      ILI9341_YELLOW;
  }

  // ===================================================
  // CURLED TAIL
  // ===================================================

  tft.drawCircle(
    x - dir * 12,
    y + 1,
    6,
    bodyColor
  );

  tft.drawCircle(
    x - dir * 12,
    y + 1,
    4,
    bodyColor
  );

  // tail connection

  tft.drawLine(
    x - dir * 7,
    y + 1,

    x - dir * 3,
    y,

    bodyColor
  );

  // ===================================================
  // BODY
  // ===================================================

  tft.fillRoundRect(
    x - 8,
    y - 5,
    17,
    11,
    5,
    bodyColor
  );

  // subtle stripe

  tft.drawLine(
    x - 4,
    y - 4,

    x - 4,
    y + 4,

    detailColor
  );

  tft.drawLine(
    x + 1,
    y - 4,

    x + 1,
    y + 4,

    detailColor
  );

  // ===================================================
  // HEAD
  // ===================================================

  tft.fillCircle(
    x + dir * 10,
    y - 1,
    6,
    bodyColor
  );

  // eye bump

  tft.fillCircle(
    x + dir * 11,
    y - 5,
    3,
    bodyColor
  );

  // eye

  tft.fillCircle(
    x + dir * 12,
    y - 5,
    1,
    ILI9341_BLACK
  );

  // ===================================================
  // LEGS
  // ===================================================

  tft.drawLine(
    x - 4,
    y + 4,

    x - 7,
    y + 9,

    detailColor
  );

  tft.drawLine(
    x + 4,
    y + 4,

    x + 7,
    y + 9,

    detailColor
  );

  // found indicator

  if (
    found
  ) {

    tft.drawCircle(
      x,
      y,
      17,
      ILI9341_WHITE
    );

    tft.drawCircle(
      x,
      y,
      18,
      FOUND_COLOR
    );
  }
}

// =====================================================
// CHILD ROOM BACKGROUND
// =====================================================

void drawRoom() {

  // ===================================================
  // WALL
  // ===================================================

  tft.fillRect(
    0,
    HUD_H,
    SCREEN_W,
    150,
    WALL_COLOR
  );

  // ===================================================
  // FLOOR
  // ===================================================

  tft.fillRect(
    0,
    180,
    SCREEN_W,
    60,
    FLOOR_COLOR
  );

  for (
    int y = 190;
    y < SCREEN_H;
    y += 15
  ) {

    tft.drawLine(
      0,
      y,
      SCREEN_W,
      y,
      FLOOR_LINE
    );
  }

  // ===================================================
  // BOOK SHELF
  // ===================================================

  tft.fillRoundRect(
    10,
    42,
    100,
    78,
    5,
    SHELF_COLOR
  );

  tft.drawRect(
    15,
    48,
    90,
    28,
    SHELF_DARK
  );

  tft.drawRect(
    15,
    80,
    90,
    32,
    SHELF_DARK
  );

  // books

  uint16_t bookColors[] = {
    ILI9341_RED,
    ILI9341_BLUE,
    ILI9341_GREEN,
    ILI9341_YELLOW,
    ILI9341_MAGENTA
  };

  for (
    int i = 0;
    i < 5;
    i++
  ) {

    tft.fillRect(
      20 + i * 15,
      54,
      9,
      20,
      bookColors[i]
    );
  }

  // toys

  tft.fillCircle(
    31,
    94,
    9,
    ILI9341_CYAN
  );

  tft.fillRect(
    68,
    87,
    22,
    19,
    ILI9341_RED
  );

  // ===================================================
  // PICTURE
  // ===================================================

  tft.fillRect(
    122,
    43,
    60,
    47,
    SHELF_DARK
  );

  tft.fillRect(
    127,
    48,
    50,
    37,
    PICTURE_COLOR
  );

  // simple sun picture

  tft.fillCircle(
    151,
    64,
    8,
    ILI9341_ORANGE
  );

  tft.fillTriangle(
    130, 82,
    145, 65,
    157, 82,
    PLANT_COLOR
  );

  // ===================================================
  // WINDOW
  // ===================================================

  tft.fillRect(
    213,
    42,
    78,
    68,
    ILI9341_WHITE
  );

  tft.fillRect(
    218,
    47,
    68,
    58,
    WINDOW_COLOR
  );

  tft.drawLine(
    252,
    47,
    252,
    105,
    ILI9341_WHITE
  );

  tft.drawLine(
    218,
    76,
    286,
    76,
    ILI9341_WHITE
  );

  // curtains

  tft.fillRoundRect(
    205,
    40,
    20,
    80,
    7,
    CURTAIN_COLOR
  );

  tft.fillRoundRect(
    280,
    40,
    20,
    80,
    7,
    CURTAIN_COLOR
  );

  // ===================================================
  // BED
  // ===================================================

  tft.fillRoundRect(
    15,
    128,
    135,
    52,
    8,
    BED_COLOR
  );

  // mattress

  tft.fillRect(
    20,
    134,
    125,
    15,
    BED_LIGHT
  );

  // pillow

  tft.fillRoundRect(
    27,
    134,
    55,
    24,
    10,
    PILLOW_COLOR
  );

  // blanket stripe

  tft.fillRect(
    85,
    150,
    58,
    25,
    BED_LIGHT
  );

  // ===================================================
  // LAMP
  // ===================================================

  tft.drawLine(
    167,
    123,
    167,
    167,
    SHELF_DARK
  );

  tft.fillTriangle(
    150,
    122,

    184,
    122,

    176,
    105,

    LAMP_COLOR
  );

  tft.fillRect(
    157,
    166,
    21,
    5,
    SHELF_DARK
  );

  // ===================================================
  // TOY BOX
  // ===================================================

  tft.fillRoundRect(
    200,
    137,
    55,
    38,
    6,
    TOYBOX_COLOR
  );

  tft.drawRect(
    204,
    143,
    47,
    27,
    SHELF_DARK
  );

  // ===================================================
  // PLANT
  // ===================================================

  tft.fillRect(
    269,
    145,
    28,
    28,
    POT_COLOR
  );

  tft.fillTriangle(
    282, 145,
    266, 119,
    279, 129,
    PLANT_COLOR
  );

  tft.fillTriangle(
    282, 145,
    295, 112,
    291, 134,
    PLANT_DARK
  );

  tft.fillTriangle(
    282, 145,
    303, 128,
    290, 138,
    PLANT_COLOR
  );

  // ===================================================
  // RUG
  // ===================================================

  tft.fillRoundRect(
    135,
    184,
    135,
    38,
    16,
    RUG_COLOR
  );

  tft.drawRoundRect(
    140,
    189,
    125,
    28,
    12,
    RUG_DARK
  );
}

// =====================================================
// DRAW HIDERS
// =====================================================

void drawAllHiders() {

  for (
    int i = 0;
    i < HIDERS_PER_GAME;
    i++
  ) {

    int spotIndex =
      selectedHiders[i];

    HideSpot &s =
      spots[spotIndex];

    drawChameleon(
      s.x,
      s.y,
      s.bodyColor,
      s.detailColor,
      s.faceRight,
      hiderFound[i]
    );
  }
}

// =====================================================
// HUD BASE
// =====================================================

void drawHUDBase() {

  tft.fillRect(
    0,
    0,
    SCREEN_W,
    HUD_H,
    HUD_COLOR
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setCursor(
    5,
    3
  );

  tft.print("FOUND");

  tft.setCursor(
    72,
    3
  );

  tft.print("SCORE");

  tft.setCursor(
    150,
    3
  );

  tft.print("TIME");

  // HINT button

  tft.fillRoundRect(
    225,
    4,
    90,
    22,
    6,
    0x7BE0
  );

  tft.drawRoundRect(
    225,
    4,
    90,
    22,
    6,
    ILI9341_WHITE
  );
}

// =====================================================
// HUD UPDATE
// =====================================================

void updateHUD() {

  if (
    gameState !=
    PLAYING
  ) {
    return;
  }

  unsigned long elapsed =
    millis() -
    gameStartedAt;

  int timeLeft =
    45 -
    elapsed / 1000;

  if (
    timeLeft < 0
  ) {

    timeLeft = 0;
  }

  // FOUND

  tft.fillRect(
    4,
    14,
    60,
    15,
    HUD_COLOR
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setCursor(
    5,
    17
  );

  tft.print(foundCount);
  tft.print("/");
  tft.print(HIDERS_PER_GAME);

  // SCORE

  tft.fillRect(
    70,
    14,
    70,
    15,
    HUD_COLOR
  );

  tft.setCursor(
    72,
    17
  );

  tft.print(score);

  // TIME

  tft.fillRect(
    148,
    14,
    60,
    15,
    HUD_COLOR
  );

  tft.setCursor(
    150,
    17
  );

  tft.print(timeLeft);

  // HINT BUTTON TEXT

  tft.fillRoundRect(
    225,
    4,
    90,
    22,
    6,
    hintsLeft > 0 ?
    0x7BE0 :
    ILI9341_DARKGREY
  );

  tft.drawRoundRect(
    225,
    4,
    90,
    22,
    6,
    ILI9341_WHITE
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setCursor(
    239,
    11
  );

  tft.print("HINT ");

  tft.print(
    hintsLeft
  );
}

// =====================================================
// DRAW COMPLETE SCENE
// =====================================================

void drawScene() {

  drawRoom();

  drawAllHiders();

  // active hint ring

  if (
    activeHint >= 0 &&
    activeHint < HIDERS_PER_GAME
  ) {

    int spotIndex =
      selectedHiders[activeHint];

    HideSpot &s =
      spots[spotIndex];

    tft.drawCircle(
      s.x,
      s.y,
      23,
      HINT_COLOR
    );

    tft.drawCircle(
      s.x,
      s.y,
      24,
      HINT_COLOR
    );
  }

  drawHUDBase();

  updateHUD();
}

// =====================================================
// RANDOM HIDERS
// =====================================================

bool spotAlreadySelected(
  int candidate,
  int count
) {

  for (
    int i = 0;
    i < count;
    i++
  ) {

    if (
      selectedHiders[i] ==
      candidate
    ) {

      return true;
    }
  }

  return false;
}

void selectRandomHiders() {

  for (
    int i = 0;
    i < HIDERS_PER_GAME;
    i++
  ) {

    int candidate;

    do {

      candidate =
        random(
          0,
          CANDIDATE_COUNT
        );

    } while (
      spotAlreadySelected(
        candidate,
        i
      )
    );

    selectedHiders[i] =
      candidate;

    hiderFound[i] =
      false;
  }
}

// =====================================================
// HIT TEST
// =====================================================

int findHiderAt(
  int x,
  int y
) {

  for (
    int i = 0;
    i < HIDERS_PER_GAME;
    i++
  ) {

    if (
      hiderFound[i]
    ) {
      continue;
    }

    HideSpot &s =
      spots[
        selectedHiders[i]
      ];

    int dx =
      abs(
        x - s.x
      );

    int dy =
      abs(
        y - s.y
      );

    // generous touch hitbox

    if (
      dx <= 20 &&
      dy <= 18
    ) {

      return i;
    }
  }

  return -1;
}

// =====================================================
// WRONG TAP X
// =====================================================

void drawWrongX(
  int x,
  int y
) {

  tft.drawLine(
    x - 7,
    y - 7,
    x + 7,
    y + 7,
    ILI9341_RED
  );

  tft.drawLine(
    x + 7,
    y - 7,
    x - 7,
    y + 7,
    ILI9341_RED
  );

  delay(120);

  // static game -> safe to redraw once

  drawScene();
}

// =====================================================
// FOUND HIDER
// =====================================================

void foundHider(
  int index
) {

  hiderFound[index] =
    true;

  foundCount++;

  unsigned long elapsed =
    millis() -
    gameStartedAt;

  int timeLeft =
    45 -
    elapsed / 1000;

  if (
    timeLeft < 0
  ) {

    timeLeft = 0;
  }

  // 100 base + small speed bonus

  score +=
    100 +
    timeLeft * 2;

  if (
    score >
    highScore
  ) {

    highScore =
      score;
  }

  soundFound();

  activeHint =
    -1;

  drawScene();

  // FOUND text

  tft.fillRoundRect(
    110,
    105,
    100,
    32,
    8,
    ILI9341_BLACK
  );

  tft.drawRoundRect(
    110,
    105,
    100,
    32,
    8,
    FOUND_COLOR
  );

  centerText(
    "FOUND!",
    115,
    2,
    FOUND_COLOR
  );

  delay(300);

  drawScene();

  if (
    foundCount >=
    HIDERS_PER_GAME
  ) {

    showWin();
  }
}

// =====================================================
// HINT
// =====================================================

void useHint() {

  if (
    hintsLeft <= 0
  ) {
    return;
  }

  if (
    activeHint >= 0
  ) {
    return;
  }

  int available[HIDERS_PER_GAME];

  int availableCount = 0;

  for (
    int i = 0;
    i < HIDERS_PER_GAME;
    i++
  ) {

    if (
      !hiderFound[i]
    ) {

      available[
        availableCount
      ] = i;

      availableCount++;
    }
  }

  if (
    availableCount <= 0
  ) {

    return;
  }

  int r =
    random(
      0,
      availableCount
    );

  activeHint =
    available[r];

  hintUntil =
    millis() +
    HINT_DURATION;

  hintsLeft--;

  soundHint();

  drawScene();
}

// =====================================================
// TOUCH
// =====================================================

bool getNewTap(
  int &screenX,
  int &screenY
) {

  bool touched =
    ts.touched();

  bool tap =
    touched &&
    !previousTouch;

  previousTouch =
    touched;

  if (
    !tap
  ) {

    return false;
  }

  TS_Point p =
    ts.getPoint();

  screenX =
    map(
      p.x,
      TOUCH_MIN_X,
      TOUCH_MAX_X,
      0,
      SCREEN_W
    );

  screenY =
    map(
      p.y,
      TOUCH_MIN_Y,
      TOUCH_MAX_Y,
      0,
      SCREEN_H
    );

  screenX =
    constrain(
      screenX,
      0,
      SCREEN_W - 1
    );

  screenY =
    constrain(
      screenY,
      0,
      SCREEN_H - 1
    );

  Serial.print("Tap X=");
  Serial.print(screenX);
  Serial.print(" Y=");
  Serial.println(screenY);

  return true;
}

// =====================================================
// TITLE CHAMELEON
// =====================================================

void drawBigChameleon(
  int x,
  int y
) {

  // tail

  tft.drawCircle(
    x - 35,
    y + 7,
    15,
    ILI9341_GREEN
  );

  tft.drawCircle(
    x - 35,
    y + 7,
    11,
    ILI9341_GREEN
  );

  // body

  tft.fillRoundRect(
    x - 28,
    y - 15,
    58,
    30,
    14,
    ILI9341_GREEN
  );

  // stripes

  tft.drawLine(
    x - 10,
    y - 12,
    x - 10,
    y + 12,
    ILI9341_YELLOW
  );

  tft.drawLine(
    x + 5,
    y - 12,
    x + 5,
    y + 12,
    ILI9341_YELLOW
  );

  // head

  tft.fillCircle(
    x + 37,
    y - 3,
    19,
    ILI9341_GREEN
  );

  // eye

  tft.fillCircle(
    x + 42,
    y - 12,
    6,
    ILI9341_WHITE
  );

  tft.fillCircle(
    x + 44,
    y - 12,
    2,
    ILI9341_BLACK
  );

  // mouth

  tft.drawLine(
    x + 35,
    y + 5,
    x + 47,
    y + 5,
    ILI9341_BLACK
  );

  // feet

  tft.drawLine(
    x - 10,
    y + 13,
    x - 18,
    y + 23,
    ILI9341_GREEN
  );

  tft.drawLine(
    x + 15,
    y + 13,
    x + 23,
    y + 23,
    ILI9341_GREEN
  );
}

// =====================================================
// TITLE
// =====================================================

void drawTitleScreen() {

  tft.fillScreen(
    WALL_COLOR
  );

  tft.fillRect(
    0,
    190,
    SCREEN_W,
    50,
    FLOOR_COLOR
  );

  tft.fillRoundRect(
    20,
    18,
    280,
    50,
    10,
    ILI9341_BLACK
  );

  centerText(
    "CHAMELEON SEEK",
    32,
    2,
    ILI9341_YELLOW
  );

  drawBigChameleon(
    145,
    125
  );

  centerText(
    "Find all 5 hiders!",
    82,
    1,
    ILI9341_BLACK
  );

  centerText(
    "TAP TO START",
    176,
    2,
    ILI9341_BLUE
  );

  tft.setTextSize(1);

  tft.setTextColor(
    ILI9341_BLACK
  );

  tft.setCursor(
    112,
    218
  );

  tft.print("HIGH SCORE ");

  tft.print(
    highScore
  );
}

// =====================================================
// START GAME
// =====================================================

void startGame() {

  stopTitleMusic();

  soundStart();

  score = 0;

  foundCount = 0;

  hintsLeft = 3;

  activeHint = -1;

  selectRandomHiders();

  gameStartedAt =
    millis();

  lastHUDUpdate =
    millis();

  gameState =
    PLAYING;

  drawScene();
}

// =====================================================
// WIN
// =====================================================

void showWin() {

  gameState =
    WIN_SCREEN;

  if (
    score >
    highScore
  ) {

    highScore =
      score;
  }

  preferences.putLong(
    "high",
    highScore
  );

  soundWin();

  tft.fillScreen(
    0x5DDF
  );

  centerText(
    "ALL FOUND!",
    28,
    3,
    ILI9341_YELLOW
  );

  drawBigChameleon(
    145,
    125
  );

  tft.setTextSize(
    2
  );

  tft.setTextColor(
    ILI9341_WHITE
  );

  tft.setCursor(
    91,
    80
  );

  tft.print("SCORE ");

  tft.print(
    score
  );

  centerText(
    "5 / 5 HIDERS",
    170,
    2,
    ILI9341_GREEN
  );

  centerText(
    "TAP TO PLAY AGAIN",
    211,
    1,
    ILI9341_WHITE
  );
}

// =====================================================
// TIME UP
// =====================================================

void showTimeUp() {

  gameState =
    TIME_UP_SCREEN;

  if (
    score >
    highScore
  ) {

    highScore =
      score;

    preferences.putLong(
      "high",
      highScore
    );
  }

  soundOff();

  tft.fillScreen(
    WALL_COLOR
  );

  centerText(
    "TIME!",
    28,
    3,
    ILI9341_BLUE
  );

  drawBigChameleon(
    145,
    125
  );

  tft.setTextSize(
    2
  );

  tft.setTextColor(
    ILI9341_BLACK
  );

  tft.setCursor(
    75,
    75
  );

  tft.print("FOUND ");

  tft.print(
    foundCount
  );

  tft.print("/5");

  centerText(
    "NICE TRY!",
    170,
    2,
    ILI9341_GREEN
  );

  centerText(
    "TAP TO TRY AGAIN",
    211,
    1,
    ILI9341_BLACK
  );
}

// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(
    115200
  );

  // BUZZER

  ledcAttach(
    BUZZER_PIN,
    2000,
    8
  );

  soundOff();

  // BACKLIGHT

  pinMode(
    TFT_BL,
    OUTPUT
  );

  digitalWrite(
    TFT_BL,
    HIGH
  );

  // TFT

  tftSPI.begin(
    TFT_SCLK,
    TFT_MISO,
    TFT_MOSI,
    TFT_CS
  );

  tft.begin();

  tft.setRotation(
    1
  );

  tft.setTextWrap(
    false
  );

  // TOUCH

  touchSPI.begin(
    TOUCH_CLK,
    TOUCH_MISO,
    TOUCH_MOSI,
    TOUCH_CS
  );

  ts.begin(
    touchSPI
  );

  ts.setRotation(
    1
  );

  // HIGH SCORE

  preferences.begin(
    "chamseek",
    false
  );

  highScore =
    preferences.getLong(
      "high",
      0
    );

  randomSeed(
    micros()
  );

  drawTitleScreen();

  startTitleMusic();

  Serial.println(
    "CHAMELEON SEEK READY!"
  );
}

// =====================================================
// LOOP
// =====================================================

void loop() {

  unsigned long now =
    millis();

  // ===================================================
  // TITLE MUSIC
  // ===================================================

  if (
    gameState ==
    TITLE_SCREEN
  ) {

    updateTitleMusic();
  }

  // ===================================================
  // TOUCH
  // ===================================================

  int tapX;
  int tapY;

  if (
    getNewTap(
      tapX,
      tapY
    )
  ) {

    // TITLE

    if (
      gameState ==
      TITLE_SCREEN
    ) {

      startGame();

      return;
    }

    // WIN / TIME UP

    if (
      gameState ==
      WIN_SCREEN ||
      gameState ==
      TIME_UP_SCREEN
    ) {

      startGame();

      return;
    }

    // PLAYING

    if (
      gameState ==
      PLAYING
    ) {

      // -----------------------------------------------
      // HINT BUTTON
      // -----------------------------------------------

      if (
        tapX >= 225 &&
        tapX <= 319 &&
        tapY <= 30
      ) {

        useHint();

        return;
      }

      // don't search inside HUD

      if (
        tapY < HUD_H
      ) {

        return;
      }

      // -----------------------------------------------
      // FIND HIDER
      // -----------------------------------------------

      int hider =
        findHiderAt(
          tapX,
          tapY
        );

      if (
        hider >= 0
      ) {

        foundHider(
          hider
        );

      } else {

        soundWrong();

        drawWrongX(
          tapX,
          tapY
        );
      }
    }
  }

  // ===================================================
  // PLAYING
  // ===================================================

  if (
    gameState ==
    PLAYING
  ) {

    // -----------------------------------------------
    // TIMER
    // -----------------------------------------------

    if (
      now -
      gameStartedAt >=
      GAME_DURATION
    ) {

      showTimeUp();

      return;
    }

    // -----------------------------------------------
    // REMOVE HINT RING
    // -----------------------------------------------

    if (
      activeHint >= 0 &&
      now >= hintUntil
    ) {

      activeHint = -1;

      drawScene();
    }

    // -----------------------------------------------
    // HUD UPDATE
    // -----------------------------------------------

    if (
      now -
      lastHUDUpdate >=
      250
    ) {

      lastHUDUpdate =
        now;

      updateHUD();
    }
  }

  delay(2);
}
