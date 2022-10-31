//OLED
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32 
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//WIFI
#include <WiFi.h>
#define WIFI_SSID "SSID"
#define WIFI_PWD "WIFI PASSWORD"

//Firebase
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#define API_KEY "FIREBASE_API_KEY"
#define FIREBASE_PROJECT_ID "firebase-project-id"
#define USER_EMAIL "email@email.com"
#define USER_PASSWORD "emailPassword"
#define POWER_METER_ID "POWERMETER_ID"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//TIME
#include <time.h>
#define NTP_SERVER "pool.ntp.org"
struct tm timeinfo;
char timestampString[21];
unsigned long epochTime;

//LoRa
#include <SPI.h>
#include <LoRa.h>
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 23
#define DIO0 26
#define BAND 915E6
uint8_t LoRaData[262];
int loraPacketLen;

//PowerMeter Package
uint8_t package[262];

typedef struct InstantValuesPackage
{
    float VA;
    float VB;
    float VC;
    
    float IA;
    float IB;
    float IC;

    float WA;
    float WB;
    float WC;

    float QA;
    float QB;
    float QC;

    float FPA;
    float FPB;
    float FPC;

    float temp;
    float freq;
} InstantValuesPackage;

float byteArrayToFloat(uint8_t buffer[], int start){
    float val=0;
    unsigned long result=0;
    result |= ((unsigned long)(buffer[start]) << 0x18);
    result |= ((unsigned long)(buffer[start + 1]) << 0x10);
    result |= ((unsigned long)(buffer[start + 2]) << 0x08);
    result |= ((unsigned long)(buffer[start + 3]));
    memcpy(&val,&result,4);
    return val;
}

bool startsWith(uint8_t pck[], String pre){
  bool ret = true;
  for(int i = 0; i < pre.length(); i++){
    if(pck[i] != pre[i]){
      ret = false;
    }
  }
  return ret;
}

void displayText(const char text[]){
  display.clearDisplay();
  
  display.setTextSize(1);             // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);        // Draw white text
  display.setCursor(0,0);             // Start at top-left corner
  display.println(F(text));
  display.display();
}

void updateTimestamp()
{
  if(!getLocalTime(&timeinfo)){
    updateTimestamp();
    return;
  }
  Serial.print(&timeinfo);
  strftime(timestampString, sizeof(timestampString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

unsigned long getEpoch() {
  time_t now;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

static void saveInstantValuesToFirestore(InstantValuesPackage pck){
  displayText("Saving data\nto firebase");
  updateTimestamp();
  FirebaseJson content;
  String documentPath = "PowerMeters/" + String(POWER_METER_ID);
  
  content.set("fields/VA/doubleValue", pck.VA);
  content.set("fields/VB/doubleValue", pck.VB);
  content.set("fields/VC/doubleValue", pck.VC);

  content.set("fields/IA/doubleValue", pck.IA);
  content.set("fields/IB/doubleValue", pck.IB);
  content.set("fields/IC/doubleValue", pck.IC);

  content.set("fields/WA/doubleValue", pck.WA);
  content.set("fields/WB/doubleValue", pck.WB);
  content.set("fields/WC/doubleValue", pck.WC);

  content.set("fields/QA/doubleValue", pck.QA);
  content.set("fields/QB/doubleValue", pck.QB);
  content.set("fields/QC/doubleValue", pck.QC);

  content.set("fields/FPA/doubleValue", pck.FPA);
  content.set("fields/FPB/doubleValue", pck.FPB);
  content.set("fields/FPC/doubleValue", pck.FPC);

  content.set("fields/Temp/doubleValue", pck.temp);
  content.set("fields/Freq/doubleValue", pck.freq);
  content.set("fields/LastUpdate/timestampValue", String(timestampString));

  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), content.raw(), "VA,VB,VC,IA,IB,IC,WA,WB,WC,QA,QB,QC,FPA,FPB,FPC,Temp,Freq,LastUpdate")){
    delay(1000);
    displayText("Data stored\non firebase!");
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  }else{
    delay(1000);
    displayText("Failed to store\ndata on firebase!");
    Serial.println(fbdo.errorReason());
  }
  delay(2000);
}

InstantValuesPackage parseInstantValues(){
    InstantValuesPackage pck = {};

    pck.VA = byteArrayToFloat(package, 12);
    pck.VB = byteArrayToFloat(package, 16);
    pck.VC = byteArrayToFloat(package, 20);

    pck.IA = byteArrayToFloat(package, 36);
    pck.IB = byteArrayToFloat(package, 40);
    pck.IC = byteArrayToFloat(package, 44);

    pck.WA = byteArrayToFloat(package, 52);
    pck.WB = byteArrayToFloat(package, 56);
    pck.WC = byteArrayToFloat(package, 60);

    pck.QA = byteArrayToFloat(package, 68);
    pck.QB = byteArrayToFloat(package, 72);
    pck.QC = byteArrayToFloat(package, 76);

    pck.FPA = byteArrayToFloat(package, 152);
    pck.FPB = byteArrayToFloat(package, 156);
    pck.FPC = byteArrayToFloat(package, 160);

    pck.temp = byteArrayToFloat(package, 180);
    pck.freq = byteArrayToFloat(package, 184);

    return pck;
}

static void handleInstantValuesPackage(){
  InstantValuesPackage pck = parseInstantValues();
  Serial.printf("VA: %f VB: %f VC: %f\n", pck.VA, pck.VB, pck.VC);
  Serial.printf("IA: %f IB: %f IC: %f\n", pck.IA, pck.IB, pck.IC);
  Serial.printf("WA: %f WB: %f WC: %f\n", pck.WA, pck.WB, pck.WC);
  Serial.printf("QA: %f QB: %f QC: %f\n", pck.QA, pck.QB, pck.QC);
  Serial.printf("FPA: %f FPB: %f FPC: %f\n", pck.FPA, pck.FPB, pck.FPC);
  Serial.printf("TEMP: %f FREQ: %f\n", pck.temp, pck.freq);
  saveInstantValuesToFirestore(pck);
}

void handleDataPackage(){
  //Join 2 LoRa packets in one power meter package
  if(LoRaData[0] == 0x01){
    memcpy(package, &LoRaData[1], 131);
  }else if(LoRaData[0] == 0x02){
    memcpy(package + 131, &LoRaData[1], 131);
    if(package[2] == 0x14){
      handleInstantValuesPackage();
    }
    memset(package, 0, sizeof(package));
  }
}

void sendTimeInfo(){
  epochTime = getEpoch();
  displayText("Sending Time Info");
  LoRa.beginPacket();
  LoRa.print("time:");
  LoRa.print(epochTime);
  LoRa.endPacket();
}

void handleLora(){
  displayText("LoRa Packet Received!");
  LoRa.readBytes(LoRaData, loraPacketLen);
  if(startsWith(LoRaData,"time")){
    delay(2000);
    sendTimeInfo();
  }else{
    delay(2000);
    handleDataPackage();
  }

  delay(2000);
  displayText("Awaiting connections");
}

void configureFirebase(){
  displayText("Configuring Firebase");
  config.host = FIREBASE_PROJECT_ID;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  delay(2000);
  displayText("Firebase Connected");
  delay(1000);
}

void configLora(){
  displayText("Configuring LoRa");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(BAND)) {
    displayText("LoRa ERROR!");
    while (1);
  }
  delay(2000);
  displayText("LoRa Connected!");
  delay(1000);
}

void configWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  displayText("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  delay(2000);
  displayText("Connected");
  delay(1000);
}

void configOled(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  display.display();
  delay(2000);
}

void setup() {
  Serial.begin(9600);
  configOled();
  configWifi();
  configLora();
  configureFirebase();
  displayText("Getting Time Info");
  configTime(0, 0, NTP_SERVER);
  updateTimestamp();
  delay(1000);
  displayText("Awaiting Connections");
}

void loop() {
  Firebase.ready();
  loraPacketLen = LoRa.parsePacket();
  if(loraPacketLen){
    while(LoRa.available()){
      handleLora();
    }
  }
}
