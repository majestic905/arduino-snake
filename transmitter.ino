#include <nRF24L01.h>
#include <RF24.h>
#include <SPI.h>

const byte PIN_NONE = 100;
const byte PIN_ANALOG_X = 0, PIN_ANALOG_Y = 1;
const byte PIN_NORTH = 2, PIN_SOUTH = 4, PIN_WEST = 5, PIN_EAST = 3;
const byte PIN_RIGHT = 6, PIN_LEFT = 7, PIN_SELECT = 8;
int X_CURRENT = 350, Y_CURRENT = 350;

const byte PIN_CE = 9;
const byte PIN_CSN = 10;
RF24 radio(PIN_CE, PIN_CSN);
const byte pipe[] = "snake"; // Needs to be the same for communicating between 2 NRF24L01 

unsigned long last = 0;
const byte thresh = 250;

byte getPressedButton() {
  if (X_CURRENT + 100 <= analogRead(PIN_ANALOG_X) || X_CURRENT - 100 >= analogRead(PIN_ANALOG_X)) {
    X_CURRENT = analogRead(PIN_ANALOG_X);
    if (X_CURRENT == 0)
      return PIN_WEST;
    if (X_CURRENT >= 600)
      return PIN_EAST;
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

void setup(void) {
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
  
  radio.begin(); // Start the NRF24L01
  radio.setPALevel(RF24_PA_LOW);
  radio.openWritingPipe(pipe); // Get NRF24L01 ready to transmit
  radio.stopListening();
}

void loop(void) {
  byte button = getPressedButton();
  if ((button != PIN_NONE) && (millis()-last > thresh)) {
    radio.write(&button, sizeof(byte));
    last = millis();
  }
  delay(15);
}

