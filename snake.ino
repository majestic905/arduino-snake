#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

// TODO: debug current (there is a collision)
// TODO: pause functionality
// TODO: setting new record (username) functionality
// TODO: wifi module functionality
// TODO: buzzer functionality

// ====================

// CLK=9, DIN=10, CE=13, DC=11, RST=12
U8G2_PCD8544_84X48_1_4W_SW_SPI u8g2(U8G2_R0, 9, 10, 13, 11, 12);

const byte SCREEN_MENU = 0;
const byte SCREEN_RECORDS = 1;
const byte SCREEN_GAME = 2;
const byte SCREEN_PAUSE = 3;
const byte SCREEN_OVER = 4;
const byte SCREEN_LEVEL = 5;
byte SCREEN = 0;

const byte MENU_ITEM_GAME = 0;
const byte MENU_ITEM_RECORDS = 1;
byte MENU_ITEM = 0;

const byte PIN_NONE = 100;
const byte PIN_ANALOG_X = 0, PIN_ANALOG_Y = 1;
const byte PIN_NORTH = 2, PIN_SOUTH = 4, PIN_WEST = 5, PIN_EAST = 3;
const byte PIN_RIGHT = 6, PIN_LEFT = 7, PIN_SELECT = 8;
int X_CURRENT = 350, Y_CURRENT = 350;

byte LEVEL = 100, countdown = 100, gameSpeed = 100;
static byte levels[4][8][2] = {
   {{B0010000,B0000100}, {B0010000,B0000100}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0010000,B0000100}, {B0010000,B0000100}},
   {{B0010010,B0100100}, {B0010010,B0100100}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0000000,B0000000}, {B0010010,B0100100}, {B0010010,B0100100}},
   {{B0000000,B0000000}, {B0000110,B0110000}, {B0100001,B1000010}, {B0010001,B1000100}, {B0001001,B1001000}, {B0000100,B0010000}, {B0000010,B0100000}, {B0000000,B0000000}},
   {{B0010010,B0100100}, {B1001001,B0010010}, {B0000000,B0000000}, {B1010101,B1010101}, {B0100010,B0100010}, {B0000000,B0000000}, {B0100100,B1001001}, {B0010010,B0100100}}
};

static byte field[48*11];
byte foodRow, foodCol;
byte foodTotal, foodMax = 25;

struct Node {
  byte row, col, dir;
  Node *next;
} *head, *tail;

struct Record {
  char name[10];
  byte level;
  byte len;
} records[6];

// ====================

byte getPressedButton() {
  if (X_CURRENT + 100 <= analogRead(PIN_ANALOG_X) || X_CURRENT - 100 >= analogRead(PIN_ANALOG_X)) {
    X_CURRENT = analogRead(PIN_ANALOG_X);
    if (X_CURRENT == 0)
      return PIN_EAST;
    if (X_CURRENT >= 600)
      return PIN_WEST;
  }
  if (Y_CURRENT + 100 <= analogRead(PIN_ANALOG_Y) || Y_CURRENT - 100 >= analogRead(PIN_ANALOG_Y)) {
    Y_CURRENT = analogRead(PIN_ANALOG_Y);
    if (Y_CURRENT == 0)
      return PIN_SOUTH;
    if (Y_CURRENT >= 600)
      return PIN_NORTH;
  }
  if (digitalRead(PIN_NORTH) == LOW)
    return PIN_NORTH;
  if (digitalRead(PIN_EAST) == LOW)
    return PIN_EAST;
  if (digitalRead(PIN_SOUTH) == LOW)
    return PIN_SOUTH;
  if (digitalRead(PIN_WEST) == LOW)
    return PIN_WEST;
  if (digitalRead(PIN_RIGHT) == LOW)
    return PIN_RIGHT;
  if (digitalRead(PIN_LEFT) == LOW)
    return PIN_LEFT;
  if (digitalRead(PIN_SELECT) == LOW)
    return PIN_SELECT;
  return PIN_NONE;
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
  if (row == 0 || row == 47 || col == 0 || col == 83)
    return true;
  byte r = row/6, c = col/6;
  return (bool) (levels[LEVEL][r][c/7] & (1 << (6-(c%7))));
}

bool onSnake (byte row, byte col) {
  Node *p = tail;
  while (p != NULL) {
    if (p->row == row || p->col == col)
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

void createAndSetSnake() {
  Node *p, *q;
  tail = (Node*) malloc(sizeof(Node));
  tail->row = 33;
  tail->col = 8;
  tail->dir = PIN_EAST;
  q = tail;

  setBit(q->row, q->col, true);

  for (int i = 0; i < 4; i++) {
    p = (Node*) malloc(sizeof(Node));
    p->row = q->row;
    p->col = q->col+1;
    p->dir = q->dir;
    q->next = p;
    q = p;
    
    setBit(q->row, q->col, true);
  }
  
  p->next = NULL;
  head = p;
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

void growSnake() {
  Node *p;
  p = (Node*) malloc(sizeof(Node));
  p->row = tail->row;
  p->col = tail->col;
  p->dir = tail->dir;
  p->next = tail;
  tail = p;
}

void createAndSetFood() {
  bool notPlaced = true;
  while (notPlaced) {
    foodRow = random(48);
    foodCol = random(84);
    notPlaced = (onBorderOrBlock(foodRow, foodCol) || onSnake(foodRow, foodCol));
  }
  setBit(foodRow, foodCol, true);
}

void moveSnake() {
  Node *p = tail;

  setBit(p->row, p->col, false);

  while (p->next != NULL) {
    p->row = p->next->row;
    p->col = p->next->col;
    p->dir = p->next->dir;
    p = p->next;
  }

  Serial.println("after while");
 
  switch (head->dir) {
    case PIN_NORTH: --head->row;    break;
    case PIN_SOUTH: ++head->row;    break;
    case PIN_EAST:  ++head->col; break;
    case PIN_WEST:  --head->col; break;
    default: break;
  }

  Serial.println("after switch");

  if (onBorderOrBlock(head->row, head->col) || onSnake(head->row, head->col)) { // game over
    Serial.println("collision");
    LEVEL = 100;
    SCREEN = SCREEN_OVER;
    clearField();
    return;
  } else
    setBit(head->row, head->col, true);

  if (head->row == foodRow && head->col == foodCol) {
    Serial.println("got food");
    ++foodTotal;
    gameSpeed += 3;
    
    if (foodTotal < foodMax) { // moving forward
      growSnake();
      createAndSetFood(); // old food point will be cleared when snake moves past it
    } else { // next level
      ++LEVEL;
      SCREEN = SCREEN_LEVEL;
      startGame();
    }
  }
}

void startGame() {
  gameSpeed = 2;
  clearField();
  setBordersAndBlocks();
  createAndSetSnake();
  createAndSetFood();
}

void setup() {
  Serial.begin(9600);
  u8g2.begin();

  pinMode(PIN_NORTH, INPUT);
  pinMode(PIN_EAST, INPUT);
  pinMode(PIN_SOUTH, INPUT);
  pinMode(PIN_WEST, INPUT);
  pinMode(PIN_RIGHT, INPUT);
  pinMode(PIN_LEFT, INPUT);
  pinMode(PIN_SELECT, INPUT);
  digitalWrite(PIN_NORTH, HIGH);
  digitalWrite(PIN_EAST, HIGH);
  digitalWrite(PIN_SOUTH, HIGH);
  digitalWrite(PIN_WEST, HIGH);
  digitalWrite(PIN_RIGHT, HIGH);
  digitalWrite(PIN_LEFT, HIGH);
  digitalWrite(PIN_SELECT, HIGH);

  int eeAddress = 0;
  Record record;
  for (byte i = 0; i < 6; ++i) {
    EEPROM.get(eeAddress, records[i]);
    eeAddress += sizeof(Record);
  }
}

void loop() {
  byte buttonPressed = getPressedButton();
  
  u8g2.firstPage();
  
  switch (SCREEN) {
    case SCREEN_MENU:
      if (buttonPressed == PIN_SELECT) {
        if (MENU_ITEM == MENU_ITEM_GAME) {
          LEVEL = 0;
          SCREEN = SCREEN_GAME;
          startGame();
        } else if (MENU_ITEM == MENU_ITEM_RECORDS)
          SCREEN = SCREEN_RECORDS;
      } else if (buttonPressed == PIN_NORTH) {
        MENU_ITEM = MENU_ITEM-1 == -1 ? 1 : MENU_ITEM-1;
      } else if (buttonPressed == PIN_SOUTH) {
        MENU_ITEM = (MENU_ITEM+1) % 2; // ибо всего 2 пункта в меню
      }
    
      do {
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(10,10,"New Game");
        u8g2.drawStr(10,20,"Records Table");
        u8g2.drawStr(0, (MENU_ITEM+1)*10, "=");
      } while( u8g2.nextPage() );
      delay(80);
      break;
      
    case SCREEN_RECORDS:
      if (buttonPressed != PIN_NONE)
        SCREEN = SCREEN_MENU;
    
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
      // TODO: calculating position and setting new record
      delay(80);
      break;

    case SCREEN_LEVEL: // it is important that SCREEN_LEVEL is placed BEFORE SCREEN_GAME
      if (countdown == 100)
        countdown = 3;
      else if (countdown == 0) {
        countdown = 100;
        SCREEN = SCREEN_GAME;
      } else
        --countdown;

      char level[2]; String(LEVEL+1).toCharArray(level, 2);
      do {
        u8g2.setFont(u8g2_font_crox4t_tr);
        u8g2.drawStr(14, 18, "LEVEL");
        u8g2.drawStr(39, 38, level);
      } while ( u8g2.nextPage() );
      delay(1000);
      break;

    case SCREEN_GAME:
//      if (buttonPressed != PIN_NONE)
//        SCREEN = SCREEN_MENU;

      if      (buttonPressed == PIN_EAST  && head->dir != PIN_WEST)  head->dir = PIN_EAST;
      else if (buttonPressed == PIN_NORTH && head->dir != PIN_SOUTH) head->dir = PIN_NORTH;
      else if (buttonPressed == PIN_SOUTH && head->dir != PIN_NORTH) head->dir = PIN_SOUTH;
      else if (buttonPressed == PIN_WEST  && head->dir != PIN_EAST)  head->dir = PIN_WEST;

      moveSnake();
        
      do {
        u8g2.drawXBM(0, 0, 84 /* width */, 48 /* height */, field);
      } while ( u8g2.nextPage() );
      delay(1000/gameSpeed);
      break;

    case SCREEN_OVER:
      do {
        u8g2.setFont(u8g2_font_crox4t_tr);
        u8g2.drawStr(16, 18, "GAME");
        u8g2.drawStr(18, 38, "OVER");
      } while ( u8g2.nextPage() );
      SCREEN = SCREEN_MENU; // SCREEN_RECORDS
      delay(1000);
      break;
  }
}
