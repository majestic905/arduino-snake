#include <EEPROM.h>

struct Record {
  char name[10];
  byte level;
  byte len;
};

void setup() {
  int eeAddress = 0;
  
  Record record1 = {"Aleksandr", 0, 6};
  EEPROM.put(eeAddress, record1);
  eeAddress += sizeof(Record);  
  Record record2 = {"Aleksandr", 0, 5};
  EEPROM.put(eeAddress, record2);
  eeAddress += sizeof(Record);
  Record record3 = {"Aleksandr", 0, 4};
  EEPROM.put(eeAddress, record3);
  eeAddress += sizeof(Record);
  Record record4 = {"Aleksandr", 0, 3};
  EEPROM.put(eeAddress, record4);
  eeAddress += sizeof(Record);
  Record record5 = {"Aleksandr", 0, 2};
  EEPROM.put(eeAddress, record5);
  eeAddress += sizeof(Record);
  Record record6 = {"Aleksandr", 0, 1};
  EEPROM.put(eeAddress, record6);
  eeAddress += sizeof(Record);
}

void loop() {
  // put your main code here, to run repeatedly:

}
