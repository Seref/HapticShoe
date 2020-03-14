//https://forum.arduino.cc/index.php?topic=658775.msg4443610#msg4443610

#include <EEPROM.h>
#include "eeprom_helper.h"

//This function will write an integer to the eeprom at the specified address
void EepromWriteInt(uint16_t StartAddress, uint16_t value) {
  const unsigned char high = ((value >> 7) & 0x7f) | 0x80;
  const unsigned char low  = (value & 0x7f);

  EEPROM.write(StartAddress, high);
  EEPROM.write(StartAddress+1, low);
 
  // Commit changes to EEPROM
  EEPROM.commit();
}

//This function will read an integer from the eeprom at the specified address
uint16_t EepromReadInt(uint16_t StartAddress) {
  const unsigned char high = EEPROM.read(StartAddress);
  const unsigned char low = EEPROM.read(StartAddress+1);

  return (((high & 0x7f) << 7) | low);
}