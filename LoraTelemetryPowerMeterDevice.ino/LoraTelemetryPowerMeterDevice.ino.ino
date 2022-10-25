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

//UART
#define RX 25
#define TX 02

//TIME
struct tm timeinfo;
bool isTimeSet = false;
int lastCheck = 0;
uint8_t status = 0x00;

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
String LoRaData;


char string[100];

bool startsWith(String str, String pre){
  return !strncmp(str.c_str(), pre.c_str(), pre.length());
}

void displayText(const char *text){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(F(text));
  display.display();
}

void generateReturn(uint8_t table){
  Serial.printf("Generating return for table: %x with status: %x\n", table, status);
  uint8_t request[12];

  request[0] = 0xAA;
  request[1] = table;
  request[2] = status;
  request[3] = timeinfo.tm_mday;
  request[4] = timeinfo.tm_mon;
  request[5] = timeinfo.tm_year - 100;
  request[6] = timeinfo.tm_wday + 1;
  request[7] = timeinfo.tm_hour;
  request[8] = timeinfo.tm_min;
  request[9] = timeinfo.tm_sec;
  request[10] = 0xA1;

  uint8_t check = 0x00;
  for (int i = 1; i < 10; i++){
      check += request[i];
  }
  request[11] = check;
  Serial1.write(request, 12);
}

time_t stringToTime(const char *epoch){
  char *eptr;
  return strtol(epoch, &eptr, 10);
}

void setTime(const char *epoch){
    time_t now = stringToTime(epoch);
    timeinfo = *localtime(&now);
    isTimeSet = true;
    displayText("Awaiting Packages");
}

void handleLora(){
  LoRaData = LoRa.readString();
  displayText(("LoRa Packet Received!\n" + LoRaData).c_str());
  if(startsWith(LoRaData, "time")){
    char substr[10];
    strncpy(substr, LoRaData.c_str() + 5, 10);
    setTime(substr);
  }
  delay(5000);
}

void sendPacketToGateway(uint8_t data[]){
  sprintf(string, "Sending packet\n table: %x", data[2]);;

  LoRa.beginPacket();
  LoRa.print("pck:");
  LoRa.write(data, sizeof(data));
  LoRa.endPacket();

  displayText("Package Sent");
}

void handleUART(){
    uint8_t data[262];
    int len = Serial1.readBytes(data, 262);
    Serial.printf("received %d bytes\n", len);
    if(len == 262){
      sprintf(string, "Package received\n, table: %x", data[2]);
      displayText("Package received!");
      if(data[2] == 0x14){
        status = 0x02;
        generateReturn(data[2]);
        delay(2000);
        sendPacketToGateway(data);
        delay(2000);
      }else{
        status = 0x00;
      }
      Serial1.flush();
    }
    displayText("Awaiting Packages");
}

void getTimeInfoFromGateway(){
  displayText("Requesting Time Info");
  uint8_t buff[] = "time";
  LoRa.beginPacket();
  LoRa.write(buff, sizeof(buff));
  LoRa.endPacket();
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

void configOled(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for(;;);
  }
  display.display();
  delay(2000);
}

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600, SERIAL_8N1, RX, TX);
  configOled();
  configLora();
  getTimeInfoFromGateway();
}

void loop() {
  if(Serial1.available() && isTimeSet){
    handleUART();
  }
  int loraPacketLen = LoRa.parsePacket();
  if(loraPacketLen){
    while(LoRa.available()){
      handleLora();
    }
  }
  if(!isTimeSet){
    lastCheck++;
    if(lastCheck > 50000){
      displayText("Request Time ERROR!");
      delay(2000);
      lastCheck = 0;
      getTimeInfoFromGateway();
    }
  }
}
