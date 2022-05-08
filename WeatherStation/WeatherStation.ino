#include "SHT3x.h"
#include "DFRobot_BMP280.h"
#include  <ESP8266WiFi.h>
#include "HTTPSRedirect.h"
#include "DebugMacros.h"

#define SHT3_ADDRESS (0x45)
#define SEA_LEVEL_PRESSURE    1015.0f   // sea level pressure

typedef DFRobot_BMP280_IIC    BMP; 

struct GoogleData {
  float temperature;
  float humidity;
  float pressure;
  float airQuality;
};

BMP   bmp(&Wire, BMP::eSdoLow);

extern const char* ssid;
extern const char* password;
extern const char* host;
extern const char *GScriptId;

const int httpsPort = 443;

// Write to Google Spreadsheet
String google_write_url = String("/macros/s/") + GScriptId + "/exec?cal";
// Read from Google Spreadsheet 
String google_read_url = String("/macros/s/") + GScriptId + "/exec?read";

String google_payload_prefix = "{\"command\": \"appendRow\", \
                          \"sheet_name\": \"Sheet1\", \
                          \"values\": \"";
String google_payload_suffix = "\"}";
String google_payload = "";


HTTPSRedirect* client = nullptr;
GoogleData collectedData = {0,0,0,0};

void ConnectWiFi(){
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("WiFi connected");
}
 
void DisconnectWiFi(){
  WiFi.disconnect();
  Serial.println("WiFi disconnected");
}

bool ConnectToGoogleHost(){
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  
  Serial.print("Connecting to ");
  Serial.println(host);

  // Try to connect for a maximum of 5 times
  for (int i=0; i<5; i++){
    int retval = client->connect(host, httpsPort);
    if (retval == 1) {
       return true;
    }
    else
      Serial.println("Connection failed. Retrying...");
      delay(500);
  }

  Serial.print("Could not connect to server: ");
  Serial.println(host);
  Serial.println("Exiting...");
  return false;
}

void CollectData(){
  // Getting temperature and humidity from sensor
  sRHAndTemp_t sht3x_data = sht3x_readTemperatureAndHumidity(SHT3_ADDRESS, eRepeatability_High);
  collectedData.temperature = sht3x_data.TemperatureC;
  collectedData.humidity = sht3x_data.Humidity;

  // Getting air pressure from sensor
  // float    bmp_temp = bmp.getTemperature();
  uint32_t bmp_press = bmp.getPressure()/100;
  // float    bmp_alti = bmp.calAltitude(SEA_LEVEL_PRESSURE, bmp_press);
  collectedData.pressure = bmp_press;

  // Function to get air quality
  collectedData.airQuality = 0;

}

String CreatePayload(){
  String payload = google_payload_prefix + collectedData.temperature + "," + collectedData.humidity + "," + collectedData.pressure + "," + collectedData.airQuality + google_payload_suffix;
  return payload;
}

void PrepareSensors(){
  Wire.begin();
  if(!sht3x_softReset(SHT3_ADDRESS)){
     delay(100);
  }
  Serial.println("Sht3x chip prepared");

  bmp.reset();
  for (int i = 0; i < 5; ++i){
    if(bmp.begin() != BMP::eStatusOK){
      break;
    }
    delay(100);
  }
  Serial.println("Bmp chip prepared");
}

void PrintCollectedData(){
    Serial.print("Collected data: temperature [C]: ");
    Serial.println(collectedData.temperature);
    Serial.print("humidity [%]: ");
    Serial.println(collectedData.humidity);
    Serial.print("pressure [hPa]: ");
    Serial.println(collectedData.pressure);
    Serial.print("air quality[?]: ");
    Serial.println(collectedData.airQuality);
}



void setup() {

  Serial.begin(115200);
  Serial.flush();
  PrepareSensors();  
}



void loop() {

    bool google_connected = ConnectToGoogleHost();
    if (google_connected){
      CollectData();
      PrintCollectedData();
      String payload = CreatePayload();
      client->POST(google_write_url, host, payload, false);
    }

    delete client;
    delay(20000);
    

}
