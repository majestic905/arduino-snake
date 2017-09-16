#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>

// CLK=9, DIN=10, CE=13, DC=11, RST=12
U8G2_PCD8544_84X48_1_4W_SW_SPI u8g2(U8G2_R0, 9, 10, 13, 11, 12);

const byte SCREEN_MENU = 0;
const byte SCREEN_ABOUT = 1;
const byte SCREEN_RECORDS = 2;
const byte SCREEN_GAME = 3;
const byte SCREEN_PAUSE = 4;
byte SCREEN_CURRENT = 0;

const byte MENU_ITEM_GAME = 0;
const byte MENU_ITEM_RECORDS = 1;
const byte MENU_ITEM_ABOUT = 2;
byte MENU_ITEM_CURRENT = 0;

const byte PIN_BUTTON_NONE = -1;
const byte PIN_BUTTON_NORTH = 2;
const byte PIN_BUTTON_EAST = 3;
const byte PIN_BUTTON_SOUTH = 4;
const byte PIN_BUTTON_WEST = 5;
const byte PIN_BUTTON_RIGHT = 6;
const byte PIN_BUTTON_LEFT = 7;
const byte PIN_BUTTON_SELECT = 8;
const byte PIN_ANALOG_X = 0;
const byte PIN_ANALOG_Y = 1;
int X_CURRENT = 350;
int Y_CURRENT = 350;

struct Record {
  char name[10];
  byte level;
  byte len;
};

struct Node {
  byte x, y, dir; 
  struct Node* next;
};

static unsigned char field[48*84];
Node *snakeHead, *snakeTail;
bool gameIsGoing = false;

void clearField() {
  for (int i = 0; i < 48; ++i)
    for (int j = 0; j < 84; ++j)
      field[i*48+j] = 0;
}

void drawBorders() {
  for (byte i = 0; i < 84; ++i)
    field[i] = field[47*84+i] = 0xff;
  for (byte i = 0; i < 48; ++i)
    field[i] = field[i*48+83] = 0xff;
}

void createSnake() {
  Node *p, *q;
  snakeTail = (Node*)malloc(sizeof(Node));
  snakeTail->y = 24;
  snakeTail->x = 5;
  snakeTail->dir = PIN_BUTTON_EAST;
  q = snakeTail;

  for (int i = 0; i < 4; i++) {
    p = (Node*)malloc(sizeof(Node));
    p->y = q->y;
    p->x = q->x+1;
    p->dir = q->dir;
    q->next = p;
    q = p;
  }
  
  p->next = NULL;
  snakeHead = p;
}

void drawSnake() {
  Node* p = snakeTail;
  while (p->next != NULL) {
    field[(p->y)*48 + p->x] = 0xff;
    p = p->next;
  }
}

void setup() {
  Serial.begin(9600);
  u8g2.begin();

  pinMode(PIN_BUTTON_NORTH, INPUT);
  pinMode(PIN_BUTTON_EAST, INPUT);
  pinMode(PIN_BUTTON_SOUTH, INPUT);
  pinMode(PIN_BUTTON_WEST, INPUT);
  pinMode(PIN_BUTTON_RIGHT, INPUT);
  pinMode(PIN_BUTTON_LEFT, INPUT);
  pinMode(PIN_BUTTON_SELECT, INPUT);
  digitalWrite(PIN_BUTTON_NORTH, HIGH);
  digitalWrite(PIN_BUTTON_EAST, HIGH);
  digitalWrite(PIN_BUTTON_SOUTH, HIGH);
  digitalWrite(PIN_BUTTON_WEST, HIGH);
  digitalWrite(PIN_BUTTON_RIGHT, HIGH);
  digitalWrite(PIN_BUTTON_LEFT, HIGH);
  digitalWrite(PIN_BUTTON_SELECT, HIGH);
}

byte getPressedButton() {
  if (X_CURRENT + 100 <= analogRead(PIN_ANALOG_X) || X_CURRENT - 100 >= analogRead(PIN_ANALOG_X)) {
    X_CURRENT = analogRead(PIN_ANALOG_X);
    if (X_CURRENT == 0)
      return PIN_BUTTON_EAST;
    if (X_CURRENT >= 600)
      return PIN_BUTTON_WEST;
  }
  if (Y_CURRENT + 100 <= analogRead(PIN_ANALOG_Y) || Y_CURRENT - 100 >= analogRead(PIN_ANALOG_Y)) {
    Y_CURRENT = analogRead(PIN_ANALOG_Y);
    if (Y_CURRENT == 0)
      return PIN_BUTTON_SOUTH;
    if (Y_CURRENT >= 600)
      return PIN_BUTTON_NORTH;
  }
  if (digitalRead(PIN_BUTTON_NORTH) == LOW)
    return PIN_BUTTON_NORTH;
  if (digitalRead(PIN_BUTTON_EAST) == LOW)
    return PIN_BUTTON_EAST;
  if (digitalRead(PIN_BUTTON_SOUTH) == LOW)
    return PIN_BUTTON_SOUTH;
  if (digitalRead(PIN_BUTTON_WEST) == LOW)
    return PIN_BUTTON_WEST;
  if (digitalRead(PIN_BUTTON_RIGHT) == LOW)
    return PIN_BUTTON_RIGHT;
  if (digitalRead(PIN_BUTTON_LEFT) == LOW)
    return PIN_BUTTON_LEFT;
  if (digitalRead(PIN_BUTTON_SELECT) == LOW)
    return PIN_BUTTON_SELECT;
  return PIN_BUTTON_NONE;
}

void loop() {
  byte buttonPressed = getPressedButton();
  
  if (SCREEN_CURRENT == SCREEN_MENU) {
    if (buttonPressed == PIN_BUTTON_SELECT) {
      if (MENU_ITEM_CURRENT == MENU_ITEM_GAME)
        SCREEN_CURRENT = SCREEN_GAME;
      else if (MENU_ITEM_CURRENT == MENU_ITEM_RECORDS)
        SCREEN_CURRENT = SCREEN_RECORDS;
      else if (MENU_ITEM_CURRENT == MENU_ITEM_ABOUT)
        SCREEN_CURRENT = SCREEN_ABOUT;
    } else if (buttonPressed == PIN_BUTTON_NORTH) {
      MENU_ITEM_CURRENT = (MENU_ITEM_CURRENT-1 == -1 ? 2 : MENU_ITEM_CURRENT-1) % 3;  // ибо всего 3 пункта в меню
    } else if (buttonPressed == PIN_BUTTON_SOUTH) {
      MENU_ITEM_CURRENT = (MENU_ITEM_CURRENT+1) % 3;
    }
    
  } else if (SCREEN_CURRENT == SCREEN_RECORDS) {
    if (buttonPressed != PIN_BUTTON_NONE)
      SCREEN_CURRENT = SCREEN_MENU;
      
  } else if (SCREEN_CURRENT == SCREEN_ABOUT) {
    if (buttonPressed != PIN_BUTTON_NONE)
      SCREEN_CURRENT = SCREEN_MENU;
      
  } else if (SCREEN_CURRENT == SCREEN_GAME) {
    if (buttonPressed != PIN_BUTTON_NONE)
      SCREEN_CURRENT = SCREEN_MENU;
  }
  
  u8g2.firstPage();
  
  switch (SCREEN_CURRENT) {
    case SCREEN_MENU:
      do {
        u8g2.setFont(u8g2_font_profont10_mr);
        u8g2.drawStr(10,10,"New Game");
        u8g2.drawStr(10,20,"Records Table");
        u8g2.drawStr(10,30,"About");
        u8g2.drawStr(0, (MENU_ITEM_CURRENT+1)*10, "=");
      } while( u8g2.nextPage() );
      break;
      
    case SCREEN_RECORDS:
      u8g2.setFont(u8g2_font_profont10_mr);
      do {
        int eeAddress = 0;
        Record record;
        for (byte i = 0; i < 6; ++i) {
          EEPROM.get(eeAddress, record);
          eeAddress += sizeof(Record);

          char level[3], len[3];
          sprintf(level, "%d", record.level);
          sprintf(len, "%d", record.len);
          Serial.println(record.name);
          Serial.println(record.level);
          Serial.println(record.len);
          u8g2.drawStr(0,  i*8+8, record.name);
          u8g2.drawStr(62, i*8+8, level);
          u8g2.drawStr(74, i*8+8, len);
        }
      } while( u8g2.nextPage() );
      break;
      
    case SCREEN_ABOUT:
      do {
        u8g2.setFont(u8g2_font_timR10_tr);
        u8g2.drawStr(13,12,"APMATH");
        u8g2.drawStr(22,24,"SPBSU");
        u8g2.setFont(u8g2_font_helvR08_tr);
        u8g2.drawStr(0,36,"kein Russisch in");
        u8g2.drawStr(0,46,"der Bibliothek :(");
      } while( u8g2.nextPage() );
      break;

    case SCREEN_GAME:
      if (!gameIsGoing) {
        clearField();
        createSnake();
        drawBorders();
        drawSnake();
        gameIsGoing = true;
      } else {
        do {
          u8g2.drawXBM(0, 0, 48, 84, field);
        } while ( u8g2.nextPage() );
      }
  }

  delay(75);
}
