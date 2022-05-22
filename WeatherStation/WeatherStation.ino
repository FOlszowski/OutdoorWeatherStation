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
PMS pms(Serial);
PMS::DATA data;
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


// Functions
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

 //Delay for PMS sensor (try to lower value to save energy)
  delay(30000);

  // Function to get air quality
  pms.requestRead();
  if (pms.readUntil(data, 10000))
  {
    collectedData.airQuality_PM_AE_UG_1_0  = data.PM_AE_UG_1_0;
    collectedData.airQuality_PM_AE_UG_2_5  = data.PM_AE_UG_2_5;
    collectedData.airQuality_PM_AE_UG_10_0 = data.PM_AE_UG_10_0;
  }
  pms.sleep();
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
  TurnOn5VPowerSwitch();
  TurnOn3VPowerSwitch();
  Serial.begin(9600);
  pms.passiveMode();
  pms.wakeUp();
  Serial.println("PMS sensor prepared"); 

  Wire.begin();
  if(!sht3x_softReset(SHT3_ADDRESS)){
     delay(100);
  }
  Serial.println("Sht3x chip prepared");   //Wrzucić w for 

  bmp.reset();
  for (int i = 0; i < 5; ++i){
    if(bmp.begin() != BMP::eStatusOK){
      break;
    }
    delay(100);
  }
  Serial.println("Bmp chip prepared");   // Dołożyć komunikat o błędzie
}


void PrintCollectedData(){
    Serial.println("======================================");
    Serial.println("Collected data:");
    Serial.print("temperature[C]: ");
    Serial.println(collectedData.temperature);
    Serial.print("humidity[%]: ");
    Serial.println(collectedData.humidity);
    Serial.print("pressure[hPa]: ");
    Serial.println(collectedData.pressure);
    Serial.println("air quality[ug/m3]: ");
    Serial.print("PM 1.0: ");
    Serial.println(collectedData.airQuality_PM_AE_UG_1_0);
    Serial.print("PM 2.5: ");
    Serial.println(collectedData.airQuality_PM_AE_UG_2_5);
    Serial.print("PM 10.0: ");
    Serial.println(collectedData.airQuality_PM_AE_UG_10_0);
    Serial.println("======================================");
    Serial.flush();
}



void setup() {
  pinMode(ThreeVPowerSwitch, OUTPUT);
  pinMode(FiveVPowerSwitch, OUTPUT);
  TurnOff3VPowerSwitch();
  TurnOff5VPowerSwitch();
  Serial.begin(9600);
}



void loop() {

    ConnectWiFi();
    bool google_connected = ConnectToGoogleHost();
    if (google_connected){
      CollectData();
      // PrintCollectedData();
      String payload = CreatePayload();
      client->POST(google_write_url, host, payload, false);
    }
    DisconnectWiFi();
    delete client;

    // CollectData();
    // PrintCollectedData();

    delay(60000);
    

}
