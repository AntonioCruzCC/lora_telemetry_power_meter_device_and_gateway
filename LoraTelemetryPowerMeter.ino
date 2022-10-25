#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <time.h>

#define RX 16
#define TX 17

#define WIFI_SSID "SSID"
#define WIFI_PWD "WIFI PASSWORD"

#define API_KEY "FIREBASE_API_KEY"
#define FIREBASE_PROJECT_ID "firebase-project-id"
#define USER_EMAIL "email@email.com"
#define USER_PASSWORD "emailPassword"

#define POWER_METER_ID "POWERMETER_ID"
#define NTP_SERVER "pool.ntp.org"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

char timestampString[21];
struct tm timeinfo;
uint8_t status = 0x00;

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

void updateTimestamp()
{
  if(!getLocalTime(&timeinfo)){
    updateTimestamp();
    return;
  }
  Serial.print(&timeinfo);
  strftime(timestampString, sizeof(timestampString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
}

static void saveInstantValuesToFirestore(InstantValuesPackage pck){
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
    Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
  }else{
    Serial.println(fbdo.errorReason());
  }          
}

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

InstantValuesPackage parseInstantValues(uint8_t data[]){
    InstantValuesPackage package = {};

    package.VA = byteArrayToFloat(data, 12);
    package.VB = byteArrayToFloat(data, 16);
    package.VC = byteArrayToFloat(data, 20);

    package.IA = byteArrayToFloat(data, 36);
    package.IB = byteArrayToFloat(data, 40);
    package.IC = byteArrayToFloat(data, 44);

    package.WA = byteArrayToFloat(data, 52);
    package.WB = byteArrayToFloat(data, 56);
    package.WC = byteArrayToFloat(data, 60);

    package.QA = byteArrayToFloat(data, 68);
    package.QB = byteArrayToFloat(data, 72);
    package.QC = byteArrayToFloat(data, 76);

    package.FPA = byteArrayToFloat(data, 152);
    package.FPB = byteArrayToFloat(data, 156);
    package.FPC = byteArrayToFloat(data, 160);

    package.temp = byteArrayToFloat(data, 180);
    package.freq = byteArrayToFloat(data, 184);

    return package;
}

static void handleInstantValuesPackage(uint8_t data[]){
  InstantValuesPackage pck = parseInstantValues(data);
  Serial.printf("VA: %f VB: %f VC: %f\n", pck.VA, pck.VB, pck.VC);
  Serial.printf("IA: %f IB: %f IC: %f\n", pck.IA, pck.IB, pck.IC);
  Serial.printf("WA: %f WB: %f WC: %f\n", pck.WA, pck.WB, pck.WC);
  Serial.printf("QA: %f QB: %f QC: %f\n", pck.QA, pck.QB, pck.QC);
  Serial.printf("FPA: %f FPB: %f FPC: %f\n", pck.FPA, pck.FPB, pck.FPC);
  Serial.printf("TEMP: %f FREQ: %f\n", pck.temp, pck.freq);
  saveInstantValuesToFirestore(pck);
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
  Serial2.write(request, 12);
}

void configureWifi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.printf("\nConnected\n");
}

void configureFirebase(){
  config.host = FIREBASE_PROJECT_ID;
  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.token_status_callback = tokenStatusCallback;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
void handleUART(){
    uint8_t data[262];
    int len = Serial2.readBytes(data, 262);
    if(len == 262){
      Serial.printf("Package received, table: %x\n", data[2]);
      if(data[2] == 0x14){
        status = 0x02;
        generateReturn(data[2]);
        handleInstantValuesPackage(data);
      }else{
        status = 0x00;
      }
      Serial2.flush();
    }else if(len >= 3){
      Serial.printf("Package received, table: %x\n", data[2]);
    }
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RX, TX);
  configTime(0, 0, NTP_SERVER);
  configureWifi();
  configureFirebase();
  updateTimestamp();
}

void loop() {
  Firebase.ready();
  if(Serial2.available()){
    handleUART();
  }
}
