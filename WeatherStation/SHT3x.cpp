#include "SHT3x.h"


void sht3x_write(uint8_t address, const void* pBuf, size_t size)
{
 if (pBuf == NULL) {
   DBG("pBuf ERROR!! : null pointer");
 }
  uint8_t * _pBuf = (uint8_t *)pBuf;
  Wire.beginTransmission(address);
  for (uint8_t i = 0; i < size; i++) {
    Wire.write(_pBuf[i]);
  }
  Wire.endTransmission();
}


void sht3x_writeCommand(uint8_t address, uint16_t cmd, size_t size)
{
    uint8_t _pBuf[2];
    _pBuf[0] = cmd >> 8;
    _pBuf[1] = cmd & 0xFF;
    delay(1);
    sht3x_write(address, _pBuf,2);
}

uint8_t sht3x_readData(uint8_t address, void *pBuf, size_t size) 
{
 if (pBuf == NULL) {
   DBG("pBuf ERROR!! : null pointer");
 }
  uint8_t * _pBuf = (uint8_t *)pBuf;

  Wire.requestFrom(address, size);
  uint8_t len = 0;
  for (uint8_t i = 0 ; i < size; i++) {
    _pBuf[i] = Wire.read();
    len++;
  }

  return len;
}

uint8_t sht3x_checkCrc(uint8_t data[])
{
    uint8_t bit;
    uint8_t crc = 0xFF;

    for (uint8_t dataCounter = 0; dataCounter < 2; dataCounter++)
    {
        crc ^= (data[dataCounter]);
        for (bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x131;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}

uint32_t sht3x_readSerialNumber(uint8_t address)
{
  uint32_t result = 0 ;
  uint8_t serialNumber1[3];
  uint8_t serialNumber2[3];
  uint8_t rawData[6];
  sht3x_writeCommand(address, SHT3X_CMD_READ_SERIAL_NUMBER , 2);
  delay(1);
  sht3x_readData(address, rawData, 6);
  memcpy(serialNumber1,rawData,3);
  memcpy(serialNumber2,rawData+3,3);
  if((sht3x_checkCrc(serialNumber1) == serialNumber1[2]) && (sht3x_checkCrc(serialNumber2) == serialNumber2[2])){
    result = serialNumber1[0];
    result = (result << 8) | serialNumber1[1];
    result = (result << 8) | serialNumber2[0];
    result = (result << 8) | serialNumber2[1];
  }
  return result;
}


sStatusRegister_t sht3x_readStatusRegister(uint8_t address)
{
  uint8_t register1[3];
  uint16_t data;
  sStatusRegister_t registerRaw;
  uint8_t retry = 10;
  while(retry--){
    sht3x_writeCommand(address, SHT3X_CMD_READ_STATUS_REG, 2);
    delay(1);
    sht3x_readData(address, register1, 3);
    if(sht3x_checkCrc(register1) == register1[2]){
      break;
     }
    }
  data = (register1[0]<<8) | register1[1];
  memcpy(&registerRaw,&data,2);
  return registerRaw;
}


bool sht3x_softReset(uint8_t address)
{
  sStatusRegister_t registerRaw;
  sht3x_writeCommand(address, SHT3X_CMD_SOFT_RESET, 2);
  delay(1);
  registerRaw = sht3x_readStatusRegister(address);
  if(registerRaw.commandStatus == 0)
    return true;
  else 
    return false;
}

float sht3x_convertTemperature(uint8_t rawTemperature[])
{
  uint16_t rawValue ;
  rawValue = rawTemperature[0];
  rawValue = (rawValue << 8) | rawTemperature[1];
  return 175.0f * (float)rawValue / 65535.0f - 45.0f;
}

float sht3x_convertHumidity(uint8_t rawHumidity[])
{
  uint16_t rawValue ;
  rawValue = rawHumidity[0];
  rawValue = (rawValue << 8) | rawHumidity[1];
  return 100.0f * (float)rawValue / 65535.0f;
}

sRHAndTemp_t sht3x_readTemperatureAndHumidity(uint8_t address, eRepeatability_t repeatability)
{
  sRHAndTemp_t tempRH;
  uint8_t rawData[6];
  uint8_t rawTemperature[3];
  uint8_t rawHumidity[3];
  tempRH.ERR = 0;
      switch(repeatability){
        case eRepeatability_High:sht3x_writeCommand(address, SHT3X_CMD_GETDATA_POLLING_H,2);break;
        case eRepeatability_Medium:sht3x_writeCommand(address, SHT3X_CMD_GETDATA_POLLING_M,2);break;
        case eRepeatability_Low:sht3x_writeCommand(address, SHT3X_CMD_GETDATA_POLLING_L,2);break;
    }
  delay(15);
  sht3x_readData(address, rawData, 6);
  memcpy(rawTemperature,rawData, 3);
  memcpy(rawHumidity,rawData+3, 3);
  if((sht3x_checkCrc(rawTemperature) != rawTemperature[2]) || (sht3x_checkCrc(rawHumidity) != rawHumidity[2])){
    tempRH.ERR = -1;
    return tempRH;
  }
  tempRH.TemperatureC = sht3x_convertTemperature(rawTemperature);
  tempRH.TemperatureF = 1.8*tempRH.TemperatureC+32;
  tempRH.Humidity = sht3x_convertHumidity(rawHumidity);
  return tempRH;
}