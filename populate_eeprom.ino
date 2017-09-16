//#include <EEPROM.h>
//
//struct Record {
//  char name[10];
//  byte level;
//  byte len;
//};
//
//void setup() {
//  int eeAddress = 0;
//  
//  Record record1 = {"Aleksandr", 4, 10};
//  EEPROM.put(eeAddress, record1);
//  eeAddress += sizeof(Record);  
//  Record record2 = {"Vasilisa", 4, 8};
//  EEPROM.put(eeAddress, record2);
//  eeAddress += sizeof(Record);
//  Record record3 = {"Ludmila", 3, 8};
//  EEPROM.put(eeAddress, record3);
//  eeAddress += sizeof(Record);
//  Record record4 = {"Rodion", 3, 6};
//  EEPROM.put(eeAddress, record4);
//  eeAddress += sizeof(Record);
//  Record record5 = {"Ekaterina", 2, 10};
//  EEPROM.put(eeAddress, record5);
//  eeAddress += sizeof(Record);
//  Record record6 = {"Anechka", 2, 9};
//  EEPROM.put(eeAddress, record6);
//  eeAddress += sizeof(Record);
//}
//
//void loop() {
//  // put your main code here, to run repeatedly:
//
//}
