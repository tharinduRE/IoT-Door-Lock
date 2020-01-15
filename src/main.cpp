#include <Arduino.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Keypad.h>

#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//const char* ssid = "IoT-Demo";
//const char* password =  "iotdemo8080";
const char* ssid = "TREP_4G";
const char* password =  "#randika2048";
//const char* mqttServer = "raspberrypi.local";
const char* mqttServer = "192.168.8.128";

const int mqttPort = 1883;
const char* mqttUser = "admin";
const char* mqttPassword = "pass";

const uint8_t debugLED = 2;

#define RST_PIN 49        // Configurable, see typical pin layout above
#define SS_PIN  53       // Configurable, see typical pin layout above
MFRC522 rfid(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::MIFARE_Key key;
String tagID = "";
byte readCard[4];  

// Mode flag  
bool flag = false;
// Lock state
bool lockState = false;

#define lockPin 22
#define ledRED 23
#define ledBLUE 25
#define buzzer 41
#define reedPin 43

#define TINY_GSM_MODEM_ESP8266
#define TINY_GSM_USE_WIFI true
#include <TinyGsmClient.h>
TinyGsm modem(Serial1);
TinyGsmClient tinyclient(modem);
PubSubClient client(tinyclient);

#define mySerial Serial3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
// Stored finger-id array
uint8_t fingerIdz[128] = {0};

// check for finger ID
int fingerID = 0;
// finger_id
uint8_t id;

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, 0);


const byte ROWS = 4; //four rows
const byte COLS = 4; //four columns
//define the cymbols on the buttons of the keypads
char hexaKeys[ROWS][COLS] = {
  {'1','2','3','A',},
  {'4','5','6','B',},
  {'7','8','9','C',},
  {'*','0','#','D',}
};
byte colPins[COLS] = {28,30,32,34}; //connect to the row pinouts of the keypad
byte rowPins[ROWS] = {29,31,33,35}; //connect to the column pinouts of the keypad

//initialize an instance of class NewKeypad
Keypad keyPad = Keypad( makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS); 
void keypadEvent(KeypadEvent key);
char pin[4];

//declrative functions
void callbackAuth(char* topic, byte* payload, unsigned int length);
void displayText(const char* text);
void getKeyPad();


void setup() {
 
  Serial.begin(115200);

  Serial1.begin(115200);

  pinMode(lockPin,OUTPUT);
  pinMode(ledRED,OUTPUT);
  pinMode(ledBLUE,OUTPUT);
  pinMode(buzzer,OUTPUT);
  digitalWrite(lockPin,LOW);

  Serial.println("Device Starting..");
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x32
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }

  displayText("Starting...");
  tone(buzzer, 2000); // Send 1KHz sound signal...
  delay(100);        // ...for 1 sec
  noTone(buzzer);     // Stop sound...

  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID reading process initiated.");   

  // set the data rate for the sensor serial port
  finger.begin(57600);
  
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) {
      displayText("Error :( ");
      delay(1); 
      }

  finger.getTemplateCount();
  Serial.print("Sensor contains "); 
  Serial.print(finger.templateCount); Serial.println(" templates");
  Serial.println("Waiting for valid finger...");
}

  Serial1.begin(115200);
  modem.restart();
  displayText("Connecting...");
  Serial.print(F("Setting SSID/password..."));
  if (!modem.networkConnect(ssid, password)) {
    Serial.println(" fail");
    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16,0);
    display.println(F("Connection"));
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16,16);
    display.println(F("FAILED"));
    display.display();
    delay(10000);
    return;
  }
  Serial.println(" success");
  displayText("Connected");
  delay(100);
 
  client.setServer(mqttServer, mqttPort);

  client.setCallback(callbackAuth);
   
}
 
void displayText(const char* text){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextWrap(false);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(text);
  display.display();
  delay(150); 
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
      client.subscribe("lock/authOUT");
      //client.subscribe("lock/authIN");
      client.subscribe("lock/register");

      DynamicJsonDocument doc(24);
      doc["status_"]   = 1 ;
      char buffer[24];
      serializeJson(doc, buffer);
      client.publish("lock/master", buffer);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      display.clearDisplay();
      display.setTextSize(2); // Draw 2X-scale text
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(16,0);
      display.println(F("Gateway"));
      display.setCursor(0,16);
      display.println(F("reconnecting..."));
      display.display(); 
      delay(2000);
    }
  }
}
void array_to_string(byte a[],unsigned int len,char buffer[])
{
  for(unsigned int i=0;i<len;i++)
  {
    byte nib1=(a[i]>>4)&0x0F;
    byte nib2=(a[i]>>0)&0x0F;
    buffer[i*2+0]=nib1 < 0x0A ? '0' + nib1 : 'A'+ nib1 - 0x0A;
    buffer[i*2+1]=nib2 < 0x0A ? '0' + nib2 : 'A'+ nib2 - 0x0A;
  }
  buffer[len*2]='\0';
}
void doorOpen(){
  Serial.println("Door Opened");
  digitalWrite(lockPin,HIGH);
  digitalWrite(ledBLUE,HIGH);
  tone(buzzer, 1000); 
  delay(100);      
  noTone(buzzer);  
  delay(1000);
  digitalWrite(ledBLUE,LOW);
  digitalWrite(buzzer,LOW);
  digitalWrite(lockPin,LOW);
  delay(200);
}

void accessDenied(){
  digitalWrite(ledRED,HIGH);
  tone(buzzer, 5000); 
  delay(500);      
  noTone(buzzer);  
  digitalWrite(ledRED,LOW);
  digitalWrite(buzzer,LOW);
}

void callbackAuth(char* topic, byte* payload, unsigned int length) {

  /* for (int i = 0; i < length; i++) {
    Serial.println((char)payload[i]);
  } */
  //Serial.println(topic);

  if((char)payload[0] == '1'){
    Serial.println("Access Granted");
    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16,0);
    display.println(F("ACCESS"));
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16,16);
    display.println(F("GRANTED"));
    display.display();      // Show initial text
    doorOpen();
    delay(1000);
  }
  else if ((char)payload[0] == '0')
  {
    Serial.println("Access Denied");
    display.clearDisplay();
    display.setTextSize(2); // Draw 2X-scale text
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16, 0);
    display.println(F("ACCESS"));
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(16, 16);
    display.println(F("DENIED"));
    display.display();
    accessDenied();      // Show initial text
    delay(1000);
  }
  else if ((char)payload[0] == '9')
  {
    Serial.println("Register Mode");
    flag = true;
  }
  else if ((char)payload[0] == '6')
  {
    Serial.println("Register Mode exited");
    flag = false;
  }
}
uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      //Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      //Serial.println("No finger detected");
      return 0;
    case FINGERPRINT_PACKETRECIEVEERR:
      //Serial.println("Communication error");
      return -2;
    case FINGERPRINT_IMAGEFAIL:
      //Serial.println("Imaging error");
      return -2;
    default:
      //Serial.println("Unknown error");
      return -2;
  }
  // OK success!
  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      //Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      //Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      //Serial.println("Communication error");
      return -2;
    case FINGERPRINT_FEATUREFAIL:
      //Serial.println("Could not find fingerprint features");
      return -2;
    case FINGERPRINT_INVALIDIMAGE:
      //Serial.println("Could not find fingerprint features");
      return -2;
    default:
      //Serial.println("Unknown error");
      return -2;
  }
  // OK converted!
  p = finger.fingerFastSearch();
  if (p == FINGERPRINT_OK) {
    //Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    //Serial.println("Communication error");
    return -2;
  } else if (p == FINGERPRINT_NOTFOUND) {
    //Serial.println("Did not find a match");
    return -1;
  } else {
    //Serial.println("Unknown error");
    return -2;
  }   
  // found a match!
  //Serial.print("Found ID #"); Serial.print(finger.fingerID); 
  //Serial.print(" with confidence of "); Serial.println(finger.confidence); 

  return finger.fingerID;
}

uint8_t getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #"); Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  //Serial.println("Remove finger");
  displayText("Remove Finger");
  delay(1000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID "); Serial.println(id);
  p = -1;
  //Serial.println("Place same finger again");
   display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("Place the  "));
  display.setCursor(10,16);
  display.println(F("Finger Again"));
  display.display();
  delay(1000);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.print(".");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      break;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      break;
    default:
      Serial.println("Unknown error");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }
  
  // OK converted!
  Serial.print("Creating model for #");  Serial.println(id);
  
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
  
  Serial.print("ID "); Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    return 1;
    
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }   
}

void getRfID() {

   if ( ! rfid.PICC_IsNewCardPresent()){ return ;}
   if ( ! rfid.PICC_ReadCardSerial()) {return ;}

  //tagID = "";
  displayText("RFID...");
  Serial.print(F("Scanned PICC's UID: "));
  for (int i = 0; i < rfid.uid.size; i++) {  
    readCard[i] = rfid.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    //tagID.concat(String(rfid.uid.uidByte[i], HEX)); 
  }
  Serial.println();
  char msg[50];
  msg[0]='\0';
  array_to_string(readCard,4,msg);
  DynamicJsonDocument doc(24);
  doc["rfid_"]   = (int) msg ;
  char buffer[24];
  serializeJson(doc, buffer);
  client.publish("lock/authIN", buffer);
  client.setCallback(callbackAuth);
  rfid.PICC_HaltA();
}

void regRfID() {
   if ( ! rfid.PICC_IsNewCardPresent()){ return ;}
   if ( ! rfid.PICC_ReadCardSerial()) {return ;}

  //tagID = "";
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("Register : "));
  display.setCursor(10,16);
  display.println(F("Scan RFID"));
  display.display();      // Show initial text
  delay(100);
  Serial.print(F("Scanned PICC's UID: "));
  for (int i = 0; i < rfid.uid.size; i++) {  
    readCard[i] = rfid.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
    //tagID.concat(String(rfid.uid.uidByte[i], HEX)); 
  }
  Serial.println();
  char msg[50];
  msg[0]='\0';
  array_to_string(readCard,4,msg);
  DynamicJsonDocument doc(24);
  doc["rfid_"]   = (int) msg ;
  char buffer[24];
  serializeJson(doc, buffer);
  client.publish("lock/registerIN", buffer);
  rfid.PICC_HaltA();
  //RESET FLAG ** important
  flag = false;
}

void checkfingerID(){

  id = (rand() % (128 - 0 + 1)) + 0;
  /* for (size_t i = 1; i < 127; i++)
  {
    if(fingerIdz[i] != 0){
      id = i + 1;Serial.println("i+1"); return;
    }
    else{
      id = i;Serial.println("i"); return;
    }
  } */
  
}

void regfingerPrint(){
  int FingerID = getFingerprintID();
  if (FingerID > 0){
    checkfingerID();
    getFingerprintEnroll();
    displayText("Registering ...");
    DynamicJsonDocument doc(24);
    doc["finger_id"]   = id ;
    char buffer[24];
    serializeJson(doc, buffer);
    client.publish("lock/registerIN", buffer);
    //RESET FLAG ** important
    flag = false;   
  }
}
int string_to_int(const char* number, int size){
        char stackbuf[size+1];
        memcpy(stackbuf, number, size);
        stackbuf[size] = '\0'; //or you could use strncpy
        return atoi(stackbuf);
}
    
void getPin(char key){
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("Enter Pin : "));
  pin[0] = key;
  Serial.print(pin[0]);
  display.setCursor(10,16);
  display.println(F("*"));
  display.display(); 
  for (size_t i = 1; i < 4; i++){ 
    pin[i] = keyPad.waitForKey();
    Serial.print(pin[i]);
    display.setCursor(10+i*16,16);
    display.println(F("*"));
    display.display(); 
  }
  Serial.println();
  DynamicJsonDocument doc(32);
  doc["pin_"]   = string_to_int(pin,4);
  char buffer[32];
  serializeJson(doc, buffer);
  client.publish("lock/authIN", buffer);
  client.setCallback(callbackAuth);
}

void regPin(char key){
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_BLACK,SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F(" Reg: Pin"));
  display.setTextColor(SSD1306_WHITE);
  delay(100);
  pin[0] = key;
  Serial.print(pin[0]);
  display.setCursor(10,16);
  display.println(F("*"));
  display.display(); 
  for (size_t i = 1; i < 4; i++){ 
    pin[i] = keyPad.waitForKey();
    Serial.print(pin[i]);
    display.setCursor(10+i*16,16);
    display.println(F("*"));
    display.display(); 
  }
  Serial.println();
  DynamicJsonDocument doc(32);
  doc["pin_"]   = string_to_int(pin,4) ;
  char buffer[32];
  serializeJson(doc, buffer);
  client.publish("lock/registerIN", buffer);
  client.setCallback(callbackAuth);
  //RESET FLAG ** important
  flag = false; 

}

void registerMode(){
  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F(":REGISTER:"));
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 16);
  display.println(F("Mode"));
  display.display();      // Show initial text
  delay(100);

  char key = keyPad.getKey();
  if(keyPad.getState() == PRESSED){
    regPin(key);
  }
  regRfID();
  regfingerPrint();
}

void eventMode(){

  char key = keyPad.getKey();
  if(keyPad.getState() == PRESSED){
    getPin(key);
  }

  display.clearDisplay();
  display.setTextSize(2); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("Scan or"));
  display.setCursor(10,16);
  display.println(F("Enter PIN"));
  display.display();      // Show initial text
  delay(100);

  //getKeyPad();
  getRfID();

  int FingerID = getFingerprintID();
  
  if (FingerID > 0){
    displayText("FingerPrint");
    DynamicJsonDocument doc(24);
    doc["finger_id"]   = FingerID ;
    char buffer[24];
    serializeJson(doc, buffer);
    client.publish("lock/authIN", buffer);
    client.setCallback(callbackAuth);    
  }
  //---------------------------------------------
  //No finger detected
  else if (FingerID == 0){
    //displayText("No finger");
    //DO nothing
  }
  //---------------------------------------------
  //Didn't find a match
  else if (FingerID == -1){
    displayText("No Match");
    delay(1000);
  }
  //---------------------------------------------
  //Didn't find the scanner or there an error
  else if (FingerID == -2){
    displayText("Error");
    delay(1000);
  }


}

//LOOP
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if(flag == false)
     eventMode();
  else
     registerMode();
}

