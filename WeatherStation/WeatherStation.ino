#include "SHT3x.h"
#include "DFRobot_BMP280.h"
#include  <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"
#include "PMS.h"

#define SHT3_ADDRESS (0x45)
#define SEA_LEVEL_PRESSURE    1015.0f   // sea level pressure

// Power Switches Pins
uint8_t ThreeVPowerSwitch = D5;
uint8_t FiveVPowerSwitch = D6;

// Struct for collected data
struct GoogleData {
  float temperature;
  float humidity;
  float pressure;
  float airQuality_PM_AE_UG_1_0;
  float airQuality_PM_AE_UG_2_5;
  float airQuality_PM_AE_UG_10_0;
};
GoogleData collectedData = {-1,-1,-1,-1,-1,-1};


// Sensors globals
typedef DFRobot_BMP280_IIC BMP; 
BMP bmp(&Wire, BMP::eSdoLow);
const int httpsPort = 443;
HTTPSRedirect* client = nullptr;


// External configuration
extern const char* ssid;
extern const char* password;
extern const char* host;
extern const char *GScriptId;


// Google Spreadsheet Strings
// Write to Google Spreadsheet
String google_write_url = String("/macros/s/") + GScriptId + "/exec?cal";
// Read from Google Spreadsheet 
String google_read_url = String("/macros/s/") + GScriptId + "/exec?read";

String google_payload_prefix = "{\"command\": \"appendRow\", \
                          \"sheet_name\": \"Datalogger\", \
                          \"values\": \"";
String google_payload_suffix = "\"}";
String google_payload = "";


// PMS Functions
struct PMS_DATA {
    // Standard Particles, CF=1
    uint16_t PM_SP_UG_1_0;
    uint16_t PM_SP_UG_2_5;
    uint16_t PM_SP_UG_10_0;

    // Atmospheric environment
    uint16_t PM_AE_UG_1_0;
    uint16_t PM_AE_UG_2_5;
    uint16_t PM_AE_UG_10_0;
};

void PMS_SetPassiveMode()
{
  uint8_t command[] = { 0x42, 0x4D, 0xE1, 0x00, 0x00, 0x01, 0x70 };
  Serial.write(command, sizeof(command));
}

void PMS_RequestRead(uint8_t buffer[])
{
  uint8_t command[] = { 0x42, 0x4D, 0xE2, 0x00, 0x00, 0x01, 0x71 };
  Serial.write(command, sizeof(command));
  for(int i = 0; i < 1000; ++i){
    if(Serial.available()){
      Serial.readBytes(buffer, 24);
      return;
    }
    else{
      delay(1);
    }
  }  
  return;
}

bool PMS_CheckCorrectBuffer(uint8_t buffer[])
{
  if(buffer[0] != 0x42 || buffer[1] != 0x4D){
    return false;
  }

  uint16_t calculated_checksum = 0;
  for (int i = 0; i < 22; ++i)
  {
      calculated_checksum += buffer[i];
  }
  uint16_t checksum = makeWord(buffer[22], buffer[23]);
  if (calculated_checksum != checksum){
    return false;
  }
  return true;
}

void ReadPMS(uint16_t timeout = 5000){
  uint8_t buffer[24];
  uint32_t start = millis();
  do
  {
    PMS_RequestRead(buffer);
    if(PMS_CheckCorrectBuffer(buffer)){
      // Standard Particles, CF=1.
      // PM_SP_UG_1_0 = makeWord(buffer[0], buffer[1]);
      // PM_SP_UG_2_5 = makeWord(buffer[2], buffer[3]);
      // PM_SP_UG_10_0 = makeWord(buffer[4], buffer[5]);

      // Atmospheric Environment.
      collectedData.airQuality_PM_AE_UG_1_0  = makeWord(buffer[6], buffer[7]);
      collectedData.airQuality_PM_AE_UG_2_5  = makeWord(buffer[8], buffer[9]);
      collectedData.airQuality_PM_AE_UG_10_0 = makeWord(buffer[10], buffer[11]);
    } 
  } while (millis() - start < timeout);
}

// ESP Functions
void ConnectWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}
 
void DisconnectWiFi(){
  WiFi.disconnect();
}

bool ConnectToGoogleHost(){
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  
  // Try to connect for a maximum of 5 times
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
       return true;
    }
    else
      delay(500);
  }
  return false;
}

void CollectData(){
  TurnOn5VPowerSwitch();
  TurnOn3VPowerSwitch();
  PrepareSensors();

  // Getting temperature and humidity from sensor
  sRHAndTemp_t sht3x_data = sht3x_readTemperatureAndHumidity(SHT3_ADDRESS, eRepeatability_High);
  collectedData.temperature = sht3x_data.TemperatureC;
  collectedData.humidity = sht3x_data.Humidity;

  // Getting air pressure from sensor
  // float    bmp_temp = bmp.getTemperature();
  uint32_t bmp_press = bmp.getPressure()/100;
  // float    bmp_alti = bmp.calAltitude(SEA_LEVEL_PRESSURE, bmp_press);
  collectedData.pressure = bmp_press;
  TurnOff3VPowerSwitch();

  // Function to get air quality
  //Delay for PMS sensor (30s from datasheet)
  delay(30000);
  ReadPMS();
  Serial.end();
  TurnOff5VPowerSwitch();
}

String CreatePayload(){
  String payload = (google_payload_prefix + collectedData.temperature + "," + collectedData.humidity + \
                    "," + collectedData.pressure + "," + collectedData.airQuality_PM_AE_UG_1_0 + "," + \
                    collectedData.airQuality_PM_AE_UG_2_5 + "," + collectedData.airQuality_PM_AE_UG_10_0 + google_payload_suffix);
  return payload;
}


void TurnOn5VPowerSwitch(){
  digitalWrite(FiveVPowerSwitch, LOW);
  delay(100);
}

void TurnOff5VPowerSwitch(){
  digitalWrite(FiveVPowerSwitch, HIGH);
}

void TurnOn3VPowerSwitch(){
  digitalWrite(ThreeVPowerSwitch, LOW);
  delay(100);
}

void TurnOff3VPowerSwitch(){
  digitalWrite(ThreeVPowerSwitch, HIGH);
}


void PrepareSensors(){ 
  Serial.begin(9600);
  Wire.begin();
  delay(100);
  if(!sht3x_softReset(SHT3_ADDRESS)){
     delay(100);
  }

  bmp.reset();
  for (int i = 0; i < 5; ++i){
    if(bmp.begin() != BMP::eStatusOK){
      break;
    }
    delay(100);
  }
  PMS_SetPassiveMode();
}

void setup() {
  pinMode(ThreeVPowerSwitch, OUTPUT);
  pinMode(FiveVPowerSwitch, OUTPUT);
  TurnOff3VPowerSwitch();
  TurnOff5VPowerSwitch();
}



void loop() {

    ConnectWiFi();
    if (ConnectToGoogleHost()){
      CollectData();
      String payload = CreatePayload();
      client->POST(google_write_url, host, payload, false);
    }
    DisconnectWiFi();
    delete client;

    delay(10000);
  
}
