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
#define WIFI_SSID "BeMaker2G"
#define WIFI_PWD "bemaker_3d_2020"

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

void handleDataPackage(){
  uint8_t data[262];
  char str[100];
  memcpy(data, &LoRaData[4], 262);
  sprintf(str, "Package received\ntable: %x", data[2]);
  displayText(str);
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
  }else if(startsWith(LoRaData, "pck")){
    delay(2000);
    handleDataPackage();
  }
  delay(2000);
  displayText("Awaiting connections");
}

void configWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  displayText("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
  }
  displayText("Connected");
  delay(2000);
}

void configOled(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  display.display();
  delay(2000);
}

void configLora(){
  displayText("Configuring LoRa");
  SPI.begin(SCK, MISO, MOSI, SS);
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(BAND)) {
    displayText("LoRa ERROR!");
    while (1);
  }
  displayText("LoRa Connected!");
  delay(2000);
}

void setup() {
  Serial.begin(9600);
  configOled();
  configWifi();
  configLora();
  displayText("Getting Time Info");
  configTime(0, 0, NTP_SERVER);
  updateTimestamp();
  delay(2000);
  displayText("Awaiting Connections");
}

void loop() {
  loraPacketLen = LoRa.parsePacket();
  if(loraPacketLen){
    while(LoRa.available()){
      handleLora();
    }
  }
}
