#include "Arduino.h"
#include <Wire.h>

//#define ENABLE_DBG
#ifdef ENABLE_DBG
#define DBG(...) {Serial.print("[");Serial.print(__FUNCTION__); Serial.print("(): "); Serial.print(__LINE__); Serial.print(" ] "); Serial.println(__VA_ARGS__);}
#else
#define DBG(...)
#endif

#define SHT3X_CMD_GETDATA_POLLING_H (0x2400) // measurement: polling, high repeatability
#define SHT3X_CMD_GETDATA_POLLING_M (0x240B) // measurement: polling, medium repeatability
#define SHT3X_CMD_GETDATA_POLLING_L (0x2416) // measurement: polling, low repeatability

#define SHT3X_CMD_READ_SERIAL_NUMBER             (0x3780)///<  Read the chip serial number
#define SHT3X_CMD_GETDATA_H_CLOCKENBLED          (0x2C06)///<  Measurement:high repeatability
#define SHT3X_CMD_GETDATA_M_CLOCKENBLED          (0x2C0D)///<  Measurement: medium repeatability
#define SHT3X_CMD_GETDATA_L_CLOCKENBLED          (0x2C10)///<  Measurement: low repeatability
#define SHT3X_CMD_SETMODE_H_FREQUENCY_HALF_HZ    (0x2032)///<  Measurement: periodic 0.5 mps, high repeatability
#define SHT3X_CMD_SETMODE_M_FREQUENCY_HALF_HZ    (0x2024)///<  Measurement: periodic 0.5 mps, medium
#define SHT3X_CMD_SETMODE_L_FREQUENCY_HALF_HZ    (0x202F)///<  Measurement: periodic 0.5 mps, low repeatability
#define SHT3X_CMD_SETMODE_H_FREQUENCY_1_HZ       (0x2130)///<  Measurement: periodic 1 mps, high repeatability
#define SHT3X_CMD_SETMODE_M_FREQUENCY_1_HZ       (0x2126)///<  Measurement: periodic 1 mps, medium repeatability
#define SHT3X_CMD_SETMODE_L_FREQUENCY_1_HZ       (0x212D)///<  Measurement: periodic 1 mps, low repeatability
#define SHT3X_CMD_SETMODE_H_FREQUENCY_2_HZ       (0x2236)///<  Measurement: periodic 2 mps, high repeatability
#define SHT3X_CMD_SETMODE_M_FREQUENCY_2_HZ       (0x2220)///<  Measurement: periodic 2 mps, medium repeatability
#define SHT3X_CMD_SETMODE_L_FREQUENCY_2_HZ       (0x222B)///<  Measurement: periodic 2 mps, low repeatability
#define SHT3X_CMD_SETMODE_H_FREQUENCY_4_HZ       (0x2334)///<  Measurement: periodic 4 mps, high repeatability
#define SHT3X_CMD_SETMODE_M_FREQUENCY_4_HZ       (0x2322)///<  Measurement: periodic 4 mps, medium repeatability
#define SHT3X_CMD_SETMODE_L_FREQUENCY_4_HZ       (0x2329)///<  Measurement: periodic 4 mps, low repeatability
#define SHT3X_CMD_SETMODE_H_FREQUENCY_10_HZ      (0x2737)///<  Measurement: periodic 10 mps, high repeatability
#define SHT3X_CMD_SETMODE_M_FREQUENCY_10_HZ      (0x2721)///<  Measurement: periodic 10 mps, medium
#define SHT3X_CMD_SETMODE_L_FREQUENCY_10_HZ      (0x272A)///<  Measurement: periodic 10 mps, low repeatability
#define SHT3X_CMD_GETDATA                        (0xE000)///<  Readout measurements for periodic mode

#define SHT3X_CMD_STOP_PERIODIC_ACQUISITION_MODE (0x3093)///< 
#define SHT3X_CMD_SOFT_RESET                     (0x30A2)///<  Soft reset
#define SHT3X_CMD_HEATER_ENABLE                  (0x306D)///<  Enabled heater
#define SHT3X_CMD_HEATER_DISABLE                 (0x3066)///<  Disable heater
#define SHT3X_CMD_READ_STATUS_REG                (0xF32D)///<  Read status register
#define SHT3X_CMD_CLEAR_STATUS_REG               (0x3041)///<  Clear status register

#define SHT3X_CMD_READ_HIGH_ALERT_LIMIT_SET      (0xE11F)///<  Read alert limits, high set
#define SHT3X_CMD_READ_HIGH_ALERT_LIMIT_CLEAR    (0xE114)///<  Read alert limits, high clear
#define SHT3X_CMD_READ_LOW_ALERT_LIMIT_CLEAR     (0xE109)///<  Read alert limits, low clear
#define SHT3X_CMD_READ_LOW_ALERT_LIMIT_SET       (0xE102)///<  Read alert limits, low set
#define SHT3X_CMD_WRITE_HIGH_ALERT_LIMIT_SET     (0x611D)///<  Write alert limits, high set
#define SHT3X_CMD_WRITE_HIGH_ALERT_LIMIT_CLEAR   (0x6116)///<  Write alert limits, high clear
#define SHT3X_CMD_WRITE_LOW_ALERT_LIMIT_CLEAR    (0x610B)///<  Write alert limits, low clear
#define SHT3X_CMD_WRITE_LOW_ALERT_LIMIT_SET      (0x6100)///<  Write alert limits, low set


typedef struct {
    uint16_t writeDataChecksumStatus : 1;
    uint16_t commandStatus : 1;
    uint16_t reserved0 : 2;
    uint16_t systemResetDeteced : 1;
    uint16_t reserved1 : 5;
    uint16_t temperatureAlert : 1;
    uint16_t humidityAlert : 1;
    uint16_t reserved2 : 1;
    uint16_t heaterStaus :1;
    uint16_t reserved3 :1;
    uint16_t alertPendingStatus :1;
}__attribute__ ((packed)) sStatusRegister_t;


typedef struct{
    float TemperatureC;
    float Humidity;
    float TemperatureF;
    int ERR;
}sRHAndTemp_t;


typedef enum {
    ePeriodic,/**<Cycle measurement mode*/
    eOneShot,/**<Single measurement mode*/
} eMod;

typedef enum{
    eRepeatability_High = 0,/**<In high repeatability mode, the humidity repeatability is 0.10%RH, the temperature repeatability is 0.06°C*/
    eRepeatability_Medium = 1,/**<In medium repeatability mode, the humidity repeatability is 0.15%RH, the temperature repeatability is 0.12°C*/
    eRepeatability_Low = 2,/**<In low repeatability mode, the humidity repeatability is0.25%RH, the temperature repeatability is 0.24°C*/
} eRepeatability_t;


// Functions
void sht3x_write(uint8_t address, const void* pBuf, size_t size);
void sht3x_writeCommand(uint8_t address, uint16_t cmd, size_t size);
uint8_t sht3x_readData(uint8_t address, void *pBuf, size_t size);
uint8_t sht3x_checkCrc(uint8_t data[]);
uint32_t sht3x_readSerialNumber(uint8_t address);
sStatusRegister_t sht3x_readStatusRegister(uint8_t address);
bool sht3x_softReset(uint8_t address);
float sht3x_convertTemperature(uint8_t rawTemperature[]);
sRHAndTemp_t sht3x_readTemperatureAndHumidity(uint8_t address, eRepeatability_t repeatability);
float sht3x_convertHumidity(uint8_t rawHumidity[]);
