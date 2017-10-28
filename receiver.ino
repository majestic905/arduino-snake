#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include <nRF24L01.h>
#include <RF24.h>

//// LCD
// CLK -> 3
// DIN -> 4
// DC -> 5
// CE -> 6
// RST -> 7
// VCC -> 3.3V
// LIGHT -> GND
// GND -> GND

#define PIN_CLK 3
#define PIN_DIN 4
#define PIN_DC 5
#define PIN_CE 6
#define PIN_RST 7
U8G2_PCD8544_84X48_1_4W_SW_SPI u8g2(U8G2_R0, PIN_CLK, PIN_DIN, PIN_CE, PIN_DC, PIN_RST);

//// NRF Adapter
// VCC -> 5V
// GND -> GND
// CE -> 8
// CSN -> 9
// SCK -> 13
// MOSI (MO) -> 11
// MISO (MI) -> 12
// IRQ -> никуда, не нужен

#define PIN_CE 8
#define PIN_CSN 9
RF24 radio(PIN_CE, PIN_CSN);
const byte pipe[] = "snake"; // Needs to be the same for communicating between 2 NRF24L01 

#define SCREEN_MENU 0
#define SCREEN_RECORDS 1
#define SCREEN_GAME 2
#define SCREEN_PAUSE 3
#define SCREEN_OVER 4
#define SCREEN_LEVEL 5
#define SCREEN_OPTIONS 6
byte SCREEN = 0;

#define MENU_ITEM_GAME 0
#define MENU_ITEM_RECORDS 1
#define MENU_ITEM_OPTIONS 2
byte MENU_ITEM = 0;

#define O_MENU_ITEM_LENGTH 0
#define O_MENU_ITEM_MODE 1
#define O_MENU_ITEM_SPEED 2
#define O_MENU_ITEM_THICKNESS 3
#define O_MENU_ITEM_BACK 4
byte O_MENU_ITEM = 0;

#define PIN_NONE 100
#define PIN_NORTH 2
#define PIN_SOUTH 4
#define PIN_WEST 5
#define PIN_EAST 3
#define PIN_RIGHT 6
#define PIN_LEFT 7
#define PIN_SELECT 8
#define PIN_ANALOG_X 0
#define PIN_ANALOG_Y 1
int X_CURRENT = 350, Y_CURRENT = 350;

static byte field[48*11];
const byte defaultSpeed = 8, levelsMax = 4;
byte foodRow, foodMax = 22, foodCol, foodTotal = 0, LEVEL = 100, countdown = 100;
byte gameSpeedMode = 0, gameSpeed = 100, gameSpeedStep = 3, gameSpeedLevel = 1; // gameSpeedMode: 0 - auto (growing), 1 - const
byte snakeThickness = 1;
static byte levels[4][8][2] = {
  {{B0010000,B0000100}, {B0010000,B0000100}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0010000,B0000100}, {B0010000,B0000100}},
  {{B0010010,B0100100}, {B0010010,B0100100}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0010010,B0100100}, {B0010010,B0100100}},
  {{B0000000,B0000000}, {B0000110,B0110000}, {B0100001,B1000010}, {B0010001,B1000100}, {B0001001,B1001000}, {B0000100,B0010000}, {B0000010,B0100000}, {B0000000,B0000000}},
  {{B0010010,B0100100}, {B1001001,B0010010}, {B0000000,B0000000}, {B1010101,B1010101}, {B0100010,B0100010}, {B0000000,B0000000}, {B0100100,B1001001}, {B0010010,B0100100}}
};

struct Node {
  byte row0, row1, col0, col1, dir;
  Node *next;
} *head, *tail;

struct Record {
  char name[10];
  byte level;
  byte len;
} records[6];
byte currentSymbol = 0;
bool nameWasSet = true;

// ====================

byte getPressedButton() {
  byte button = PIN_NONE;
  while (radio.available())
    radio.read(&button, sizeof(byte)); // Read information from the NRF24L01
  return button;
}

void clearField() {
  for (int i = 0; i < 48*11; ++i)
    field[i] = 0;
}

void setBit(byte row, byte col, bool val) {
  if (val)
    field[11*row+col/8] |= 1 << (col % 8);
  else
    field[11*row+col/8] &= ~(1 << (col % 8));
}

bool onBorderOrBlock(byte row, byte col) {
  if (row == 0 || row >= 47 || col == 0 || col >= 83 || row >= 240 || col >= 240)
    return true;
  byte r = row/6, c = col/6;
  return (bool) (levels[LEVEL][r][c/7] & (1 << (6-(c%7))));
}

bool onSnake(byte row, byte col) {
  Node *p = tail;
  while (p->next != NULL) { // excluding snake's head
    for (byte i = p->row0; i <= p->row1; ++i)
      for (byte j = p->col0; j <= p->col1; ++j)
        if (i == row && j == col)
          return true;
    p = p->next;
  }
  return false;
}

void setBordersAndBlocks() {
  for (int row = 0; row < 48; ++row)
    for (int col = 0; col < 84; ++col)
      if (onBorderOrBlock(row, col))
        setBit(row, col, true);
}

void destroySnake() {
  Node *p, *q;
  p = tail;
  while (p != NULL) {
    q = p;
    p = p->next;
    free(q);
  }
  head = tail = NULL;
}

void createAndSetSnake() {
  destroySnake();
 
  Node *p, *q;
  tail = (Node*) malloc(sizeof(Node));
  tail->row0 = 32/snakeThickness*snakeThickness;
  tail->row1 = 32+snakeThickness-1;
  tail->col0 = 8/snakeThickness*snakeThickness;
  tail->col1 = 8+snakeThickness-1;
  tail->dir = PIN_EAST;
  q = tail;

  for (byte i = q->row0; i <= q->row1; ++i)
    for (byte j = q->col0; j <= q->col1; ++j)
      setBit(i, j, true);

  for (int i = 0; i < 3; i++) {
    p = (Node*) malloc(sizeof(Node));
    p->row0 = q->row0;
    p->row1 = q->row1;
    p->col0 = q->col1+1;
    p->col1 = q->col1+1+snakeThickness-1;
    p->dir = q->dir;
    q->next = p;
    q = p;

  for (byte i = q->row0; i <= q->row1; ++i)
    for (byte j = q->col0; j <= q->col1; ++j)
      setBit(i, j, true);
  }

  p->next = NULL;
  head = p;
}

void growSnake() {
  Node *p;
  p = (Node*) malloc(sizeof(Node));
  p->row0 = tail->row0;
  p->row1 = tail->row1;
  p->col0 = tail->col0;
  p->col1 = tail->col1;
  p->dir = tail->dir;
  p->next = tail;
  tail = p;
}

void createAndSetFood() {
  bool notPlaced = true;
  while (notPlaced) {
    foodRow = random(48+1-snakeThickness);
    foodCol = random(84+1-snakeThickness);
    if (foodRow % snakeThickness != 0 || foodCol % snakeThickness != 0)
      continue;
   
  notPlaced = false;
  for (byte i = foodRow; i < foodRow+snakeThickness; ++i)
    for (byte j = foodCol; j < foodCol+snakeThickness; ++j)
      notPlaced = notPlaced || (onBorderOrBlock(i, j) || onSnake(i, j));
  }
  for (byte i = foodRow; i < foodRow+snakeThickness; ++i)
    for (byte j = foodCol; j < foodCol+snakeThickness; ++j)
      setBit(i, j, true);
}

void moveSnake() {
  Node *p = tail;

  for (byte i = p->row0; i <= p->row1; ++i)
    for (byte j = p->col0; j <= p->col1; ++j)
      setBit(i, j, false);

  while (p->next != NULL) {
    p->row0 = p->next->row0;
    p->row1 = p->next->row1;
    p->col0 = p->next->col0;
    p->col1 = p->next->col1;
    p->dir = p->next->dir;
    p = p->next;
  }

  switch (head->dir) {
    case PIN_NORTH: head->row0 = head->row0-snakeThickness; head->row1 = head->row1-snakeThickness; break;
    case PIN_SOUTH: head->row0 = head->row0+snakeThickness; head->row1 = head->row1+snakeThickness; break;
    case PIN_EAST:  head->col0 = head->col0+snakeThickness; head->col1 = head->col1+snakeThickness; break;
    case PIN_WEST:  head->col0 = head->col0-snakeThickness; head->col1 = head->col1-snakeThickness; break;
    default: break;
  }

  if (onBorderOrBlock(head->row0, head->col0) || onSnake(head->row0, head->col0)) { // game over
    SCREEN = SCREEN_OVER;
    return;
  } else {
    for (byte i = head->row0; i <= head->row1; ++i)
      for (byte j = head->col0; j <= head->col1; ++j)
        setBit(i, j, true);
  }

  if (head->row0 == foodRow && head->col0 == foodCol) { // got food
    ++foodTotal;
    if (gameSpeedMode == 0 /* auto */)
      gameSpeed += gameSpeedStep; // else nothing
   
    if (foodTotal < foodMax) { // moving forward
      growSnake();
      createAndSetFood(); // old food point will be cleared when snake moves past it
    } else { // next level or game over
      ++LEVEL;
      if (LEVEL == 4) { // zero-based
        --LEVEL; // иначе в таблице будет пятый уровень, которого нет
        SCREEN = SCREEN_OVER;
      } else
        SCREEN = SCREEN_LEVEL;
    }
  }
}

void startGame() {
  nameWasSet = false;
  currentSymbol = 0;

  gameSpeed = defaultSpeed;
  if (gameSpeedMode == 1 /* const */)
    gameSpeed += gameSpeedLevel*3;
  foodTotal = 0;
 
  clearField();
  setBordersAndBlocks();
  createAndSetSnake();
  createAndSetFood();
}

void shiftRecords() {
  for (int i = 5; i > 0; --i) {
    for (int j = 0; j < 9; ++j)
      records[i].name[j] = records[i-1].name[j];
    records[i].level = records[i-1].level;
    records[i].len = records[i-1].len;
  }
  for (int i = 0; i < 9; ++i)
    records[0].name[i] = '_';
  records[0].level = LEVEL+1;
  records[0].len = foodTotal;
}

void writeToEEPROM() {
  int eeAddress = 0;
  for (byte i = 0; i < 6; ++i) {
    EEPROM.put(eeAddress, records[i]);
    eeAddress += sizeof(Record);
  }
}

void setup() {
  u8g2.begin();
 
  radio.begin();
  radio.openReadingPipe(1, pipe);
  radio.startListening();

  int eeAddress = 0;
  Record record;
  for (byte i = 0; i < 6; ++i) {
    EEPROM.get(eeAddress, records[i]);
    eeAddress += sizeof(Record);
  }

  randomSeed(analogRead(0));
}

void loop() {
  byte buttonPressed = getPressedButton();
  
  u8g2.firstPage();
  
  switch (SCREEN) {
    case SCREEN_MENU:
      if (buttonPressed == PIN_SELECT) {
        if (MENU_ITEM == MENU_ITEM_GAME) {
          LEVEL = 0;
          SCREEN = SCREEN_LEVEL;
        } else if (MENU_ITEM == MENU_ITEM_RECORDS)
          SCREEN = SCREEN_RECORDS;
        else if (MENU_ITEM == MENU_ITEM_OPTIONS)
          SCREEN = SCREEN_OPTIONS;
      } else if (buttonPressed == PIN_NORTH) {
        MENU_ITEM = (MENU_ITEM == 0 ? 2 : MENU_ITEM-1);
      } else if (buttonPressed == PIN_SOUTH) {
        MENU_ITEM = (MENU_ITEM+1) % 3; // ибо всего 2 пункта в меню
      }
   
      do {
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(10,10,"New Game");
        u8g2.drawStr(10,20,"Records Table");
        u8g2.drawStr(10,30,"Options");
        u8g2.drawStr(0, (MENU_ITEM+1)*10, "=");
      } while( u8g2.nextPage() );
      delay(80);
      break;
  
    case SCREEN_GAME:
      if (buttonPressed == PIN_LEFT || buttonPressed == PIN_RIGHT)
        SCREEN = SCREEN_PAUSE;
      else {
        if      (buttonPressed == PIN_EAST  && head->dir != PIN_WEST)  head->dir = PIN_EAST;
        else if (buttonPressed == PIN_NORTH && head->dir != PIN_SOUTH) head->dir = PIN_NORTH;
        else if (buttonPressed == PIN_SOUTH && head->dir != PIN_NORTH) head->dir = PIN_SOUTH;
        else if (buttonPressed == PIN_WEST  && head->dir != PIN_EAST)  head->dir = PIN_WEST;
  
        moveSnake();
         
        do {
          u8g2.drawXBM(0, 0, 84 /* width */, 48 /* height */, field);
        } while ( u8g2.nextPage() );
      }
      delay(1000/gameSpeed);
      break;
  
    case SCREEN_OVER:
      clearField();
     
      do {
        u8g2.setFont(u8g2_font_crox4t_tr);
        u8g2.drawStr(16, 18, "GAME");
        u8g2.drawStr(18, 38, "OVER");
      } while ( u8g2.nextPage() );
      shiftRecords();
      LEVEL = 100; // assigned here because LEVEL is used in shiftRecords();
      SCREEN = SCREEN_RECORDS;
      delay(1000);
      break;
  
    case SCREEN_LEVEL:
      if (countdown == 100)
        countdown = 3;
      else if (countdown == 0) {
        countdown = 100;
        startGame();
        SCREEN = SCREEN_GAME;
      } else {
        char level[2]; String(LEVEL+1).toCharArray(level, 2);
        char count[2]; String(countdown).toCharArray(count, 2);
        do {
          u8g2.setFont(u8g2_font_crox4t_tr);
          u8g2.drawStr(14, 18, "LEVEL");
          u8g2.drawStr(39, 38, level);
          u8g2.setFont(u8g2_font_t0_14_tn); // u8g2_font_9x15_tr
          u8g2.drawStr(75, 45, count);
        } while ( u8g2.nextPage() );
        --countdown;
        delay(850);  
      }
      break;
  
    case SCREEN_RECORDS:
      if (nameWasSet && (buttonPressed != PIN_NONE)) {
        SCREEN = SCREEN_MENU;
      } else {
        if (buttonPressed == PIN_EAST)
          currentSymbol = (currentSymbol + 1) % 9;
        else if (buttonPressed == PIN_NORTH) {
          byte nextSym = records[0].name[currentSymbol] + 1;
          if (nextSym == 127)
            nextSym = 48;
          records[0].name[currentSymbol] = nextSym;
        } else if (buttonPressed == PIN_SOUTH) {
          byte prevSym = records[0].name[currentSymbol] - 1;
          if (prevSym == 47)
            prevSym = 126;
          records[0].name[currentSymbol] = prevSym;
        } else if (buttonPressed == PIN_WEST) {
          currentSymbol = (currentSymbol == 0 ? 8 : (currentSymbol-1)); 
        } else if (buttonPressed == PIN_SELECT) {
          nameWasSet = true;
          writeToEEPROM();
        }
      }
   
      u8g2.setFont(u8g2_font_profont10_mr);
      do {
        for (byte i = 0; i < 6; ++i) {
          char level[3], len[3];
          sprintf(level, "%d", records[i].level);
          sprintf(len, "%d", records[i].len);
          u8g2.drawStr(0,  i*8+8, records[i].name);
          u8g2.drawStr(62, i*8+8, level);
          u8g2.drawStr(74, i*8+8, len);
        }
      } while( u8g2.nextPage() );
      delay(100);
      break;
  
    case SCREEN_PAUSE:
      if (buttonPressed != PIN_NONE)
        SCREEN = SCREEN_GAME;
  
      do {
        u8g2.setFont(u8g2_font_crox4t_tr);
        u8g2.drawStr(14, 18, "PAUSE");
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(4, 35, "press any button");
        u8g2.drawStr(16, 45, "to continue");
      } while( u8g2.nextPage() );
      delay(50);
      break;
  
    case SCREEN_OPTIONS:
      if (buttonPressed == PIN_NORTH)
        O_MENU_ITEM = (O_MENU_ITEM == 0 ? 4 : O_MENU_ITEM-1);
      else if (buttonPressed == PIN_SOUTH)
        O_MENU_ITEM = (O_MENU_ITEM+1) % 5; // ибо всего 5 пункта в меню
      else if (buttonPressed == PIN_SELECT && O_MENU_ITEM == O_MENU_ITEM_BACK) {
        O_MENU_ITEM = 0; // первый пункт меню, каким бы он ни был
        SCREEN = SCREEN_MENU;
      } else if (O_MENU_ITEM == O_MENU_ITEM_LENGTH) {
        if (buttonPressed == PIN_WEST)
          foodMax = (foodMax == 2 ? 2 : foodMax-1);
        else if (buttonPressed == PIN_EAST)
          foodMax = (foodMax == 25 ? 25 : foodMax+1);
      } else if (O_MENU_ITEM == O_MENU_ITEM_MODE) {
        if (buttonPressed == PIN_WEST || buttonPressed == PIN_EAST)
          gameSpeedMode = 1-gameSpeedMode;
      } else if (O_MENU_ITEM == O_MENU_ITEM_SPEED) {
        if (gameSpeedMode == 0 /* auto */) {
          if (buttonPressed == PIN_WEST)
            gameSpeedStep = (gameSpeedStep == 1 ? 1 : gameSpeedStep-1);
          else if (buttonPressed == PIN_EAST)
            gameSpeedStep = (gameSpeedStep == 3 ? 3 : gameSpeedStep+1);
        } else if (gameSpeedMode == 1 /* const */) {
          if (buttonPressed == PIN_WEST)
            gameSpeedLevel = (gameSpeedLevel == 1 ? 1 : gameSpeedLevel-1);
          else if (buttonPressed == PIN_EAST)
            gameSpeedLevel = (gameSpeedLevel == 12 ? 12 : gameSpeedLevel+1);
        }
      } else if (O_MENU_ITEM == O_MENU_ITEM_THICKNESS) {
        if (buttonPressed == PIN_WEST)
          snakeThickness = 1;
        else if (buttonPressed == PIN_EAST)
          snakeThickness = 2;
      }
  
      byte modeAuto = (gameSpeedMode == 0);
      do {
        u8g2.setFont(u8g2_font_profont10_mr);
       
        u8g2.drawStr(10, 8, "Max. len:");
        char len[3]; sprintf(len, "%d", foodMax+3);
        u8g2.drawStr(60, 8, len);
       
        u8g2.drawStr(10, 16, "Sp. mode:");
        u8g2.drawStr(60, 16, (modeAuto ? "auto" : "const"));
       
        u8g2.drawStr(10, 24, (modeAuto ? "Step:" : "Level:"));
        char spid[3]; sprintf(spid, "%d", (modeAuto ? gameSpeedStep : gameSpeedLevel));
        u8g2.drawStr(60, 24, spid);
  
        u8g2.drawStr(10, 32, "Thickn.:");
        char thick[3]; sprintf(thick, "%d", snakeThickness);
        u8g2.drawStr(60, 32, thick);
        
        u8g2.drawStr(10, 40, "Back to menu");
  
        u8g2.drawStr(0, (O_MENU_ITEM+1)*8, "=");
      } while( u8g2.nextPage() );
      delay(50);
      break;
  }
}

// делим поле 48x84 на прямоугольники по 6 пикселей, получаем поле 8x14
// поле 8х14 в свою очереь можно представить массивом 7-битовых чисел 8x2, где каждый установленный бит будет символизировать о наличии блока-препятствия размером 6х6

// static byte levels[4][8][2] = {
//    {{B0010000,B0000100},
//     {B0010000,B0000100},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0010000,B0000100},
//     {B0010000,B0000100}},

//    {{B0010010,B0100100},
//     {B0010010,B0100100},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0000000,B0000000},
//     {B0010010,B0100100},
//     {B0010010,B0100100}},

//    {{B0000000,B0000000},
//     {B0000110,B0110000},
//     {B0100001,B1000010},
//     {B0010001,B1000100},
//     {B0001001,B1001000},
//     {B0000100,B0010000},
//     {B0000010,B0100000},
//     {B0000000,B0000000}},

//    {{B0010010,B0100100},
//     {B1001001,B0010010},
//     {B0000000,B0000000},
//     {B1010101,B1010101},
//     {B0100010,B0100010},
//     {B0000000,B0000000},
//     {B0100100,B1001001},
//     {B0010010,B0100100}}
// };
