// Smart Charger 1.0.1
// ---------------------------------------------------------------------------
// Created by Brett Watt on 20/07/2018
// Copyright 2018 - Under creative commons license 3.0:
//
// This software is furnished "as is", without technical support, and with no
// warranty, express or implied, as to its usefulness for any purpose.
//
// @brief
// This is the Arduino 8x 18650 Smart Charger / Discharger Code
//
// Current implementation:
// TP4056, Rotary Encoder KY-040 Module, Temp Sensor DS18B20
// Ethernet Module W5500, Mini USB Host Shield (Barcode Scanner),
// LCD 2004 20x4 with IIC/I2C/TWI Serial, Discharge (MilliAmps and MillOhms)
//
// @author Email: info@vortexit.co.nz
//       Web: www.vortexit.co.nz





//Includes
#include <Wire.h> // Comes with Arduino IDE
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "config.h"
#include "Timer.h"
#include <Encoder.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#if defined(WIRELESS_ENABLE) && !defined(ETHERNET_ENABLE)
  #include <WiFiEsp.h>
#endif
#if !defined(WIRELESS_ENABLE) && defined(ETHERNET_ENABLE)
  #include <Ethernet.h>
#endif

#if defined(USB_ENABLE)
  #include <hidboot.h> //Barcode Scanner
  #include <usbhub.h> //Barcode Scanner
#endif
#ifdef dobogusinclude //Barcode Scanner
#include <spi4teensy3.h> //Barcode Scanner
#endif

//Objects
LiquidCrystal_I2C lcd(0x27,20,4); // set the LCD address to 0x27 for a 20 chars and 4 line display
Timer timerObject;

void(* resetFunc) (void) = 0;//declare reset function at address 0

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

struct EEPROMConfig {
  float shuntResistor;
  float referenceVoltage;
  float defaultBatteryCutOffVoltage;
  uint8_t restTimeMinutes;
  uint16_t lowMilliamps;
  uint16_t highMilliOhms;
  uint16_t offsetMilliOhms;
  uint8_t chargingTimeout;
  uint8_t tempThreshold;
  uint8_t tempMaxThreshold;
  char server[40];
  char userHash[10];
  uint8_t CDUnitID;
  char ssid[10];
  char pass[16];
};


//Serial stuff
const byte numChars = 32;
char receivedChars[numChars]; // an array to store the received data

boolean newData = false;

//--------------------------------------------------------------------------
// Constant Variables
//const byte modules = 8; // Number of Modules
//const byte dischargerID[8] = {1,2,3,4,5,6,7,8}; // Discharger ID's from PHP Database Table Chargers ------> Change to Modules Array ID + 1
// You need to run the Dallas get temp sensors sketch to get your DS serails
DeviceAddress tempSensorSerial[9]= {
  {0x28, 0xFF, 0x81, 0x90, 0x63, 0x16, 0x03, 0x4A},
  {0x28, 0xFF, 0xB8, 0xC1, 0x62, 0x16, 0x04, 0x42},
  {0x28, 0xFF, 0xBA, 0xD5, 0x63, 0x16, 0x03, 0xDF},
  {0x28, 0xFF, 0xE7, 0xBB, 0x63, 0x16, 0x03, 0x23},
  {0x28, 0xFF, 0x5D, 0xDC, 0x63, 0x16, 0x03, 0xC7},
  {0x28, 0xFF, 0xE5, 0x45, 0x63, 0x16, 0x03, 0xC4},
  {0x28, 0xFF, 0x0E, 0xBC, 0x63, 0x16, 0x03, 0x8C},
  {0x28, 0xFF, 0xA9, 0x9E, 0x63, 0x16, 0x03, 0x99}
  };
// Add in an ambient air tempSensor ---->> This will be ID 9 (8)
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // If you have multiple devices you may need to change this per CD Unit (MAC conflicts on same network???)

EEPROMConfig loadedSettings {
  3.3f,// In Ohms - Shunt resistor resistance
  4.95f,// 5V Output of Arduino
  2.8f,// Voltage that the discharge stops ---> Will be replaced with Get DB Discharge Cutoff
  1,// The time in Minutes to rest the battery after charge. 0-59 are valid
  1000,//  This is the value of Milli Amps that is considered low and does not get recharged because it is considered faulty
  500,//  This is the value of Milli Ohms that is considered high and the battery is considered faulty
  0,// Offset calibration for MilliOhms
  8,// The timeout in Hours for charging
  7,// Warning Threshold in degrees above initial Temperature
  10,//Maximum Threshold in degrees above initial Temperature - Considered Faulty
  "submit.vortexit.co.nz",// Server to connect - Add your server here
  "",// Database Hash - this is unique per user - Get this from Charger / Discharger Menu -> View
  1,// CDUnitID this is the Units ID - this is unique per user - Get this from Charger / Discharger Menu -> View -> Select your Charger / Discharger
  "yournetwork",//Your wireless networks ssid
  "yourpassword"//Your wireless networks password

};


Encoder myEnc(7,6); //Encoder knob setup
#if defined(ETHERNET_ENABLE)
  EthernetClient client;
#endif
#if defined(WIRELESS_ENABLE)
WiFiEspClient client;
#endif

//--------------------------------------------------------------------------
// Initialization
unsigned long longMilliSecondsCleared[modules];
int intSeconds[modules];
int intMinutes[modules];
int intHours[modules];
//--------------------------------------------------------------------------
// Cycle Variables
byte cycleState[modules];
byte cycleStateLast = 0;
byte rotaryOveride = 0;
boolean rotaryOverideLock = 0;
boolean buttonState = 0;
boolean lastButtonState = 0;
boolean mosfetSwitchState[modules];
byte cycleStateCycles = 0;
//int connectionAttempts[8];
float batteryVoltage[modules];
byte batteryFaultCode[modules];
byte batteryInitialTemp[modules];
byte batteryHighestTemp[modules];
byte batteryCurrentTemp[modules];
char lcdLine2[25];
char lcdLine3[25];

// readPage Variables
char serverResult[32]; // string for incoming serial data
int stringPosition = 0; // string index counter readPage()
bool startRead = false; // is reading? readPage()

// Check Battery Voltage
byte batteryDetectedCount[modules];
//bool batteryDetected[modules];

// Get Barcode
char barcodeString[25] = "";
bool barcodeStringCompleted = false;
byte pendingBarcode = 255;
char batteryBarcode[modules][25];
//bool barcodeDuplicateFound[modules];

// Cutoff Voltage / Get Barcode
bool batteryGetCompleted[modules];
bool pendingBatteryRecord[modules];

// Charge Battery
float batteryInitialVoltage[modules];
byte batteryChargedCount[modules];
boolean preDischargeCompleted[modules];

// Check Battery Milli Ohms
byte batteryMilliOhmsCount[modules];
float tempMilliOhmsValue[modules];
float milliOhmsValue[modules];

// Discharge Battery
int intMilliSecondsCount[modules];
unsigned long longMilliSecondsPreviousCount[modules];
unsigned long longMilliSecondsPrevious[modules];
unsigned long longMilliSecondsPassed[modules];
float dischargeMilliamps[modules];
float dischargeVoltage[modules];
float dischargeAmps[modules];
float batteryCutOffVoltage[modules];
bool dischargeCompleted[modules];
bool dischargeUploadCompleted[modules];

int dischargeMinutes[modules];
bool pendingDischargeRecord[modules];

// Recharge Battery
byte batteryRechargedCount[modules];

// Completed
byte batteryNotDetectedCount[modules];
byte mosfetSwitchCount[modules];

// USB Host Shield - Barcode Scanner
#if defined(USB_ENABLE)
class KbdRptParser : public KeyboardReportParser
{

protected:
  virtual void OnKeyDown  (uint8_t mod, uint8_t key);
  virtual void OnKeyPressed(uint8_t key);
};

KbdRptParser Prs;
USB     Usb;
HIDBoot<USB_HID_PROTOCOL_KEYBOARD>    HidKeyboard(&Usb);
#endif

//--------------------------------------------------------------------------

void setup()
{
  char lcdStartup0[25];
  char lcdStartup1[25];
  char lcdStartup2[25];
  char lcdStartup3[25];
  char lcdStartup4[25];


  //Check to see if the first run byte is set.
  if (EEPROM.read(0) != 1)
  {
    lcd.init();
    lcd.clear();
    lcd.backlight();// Turn on backlight
    sprintf(lcdStartup4, "%-20s", "Loading Default Settings");
    //set byte 1 of eeprom to true indicating the setting were saved.
    EEPROM.put(0,1);
    EEPROMConfig defaultsettings {
      3.3f,// In Ohms - Shunt resistor resistance
      4.95f,// 5V Output of Arduino
      2.8f,// Voltage that the discharge stops ---> Will be replaced with Get DB Discharge Cutoff
      1,// The time in Minutes to rest the battery after charge. 0-59 are valid
      1000,//  This is the value of Milli Amps that is considered low and does not get recharged because it is considered faulty
      500,//  This is the value of Milli Ohms that is considered high and the battery is considered faulty
      0,// Offset calibration for MilliOhms
      8,// The timeout in Hours for charging
      7,// Warning Threshold in degrees above initial Temperature
      10,//Maximum Threshold in degrees above initial Temperature - Considered Faulty
      "submit.vortexit.co.nz",// Server to connect - Add your server here
      "",// Database Hash - this is unique per user - Get this from Charger / Discharger Menu -> View
      1,// CDUnitID this is the Units ID - this is unique per user - Get this from Charger / Discharger Menu -> View -> Select your Charger / Discharger
      "yournetwork",//Your wireless networks ssid
      "yourpassword"//Your wireless networks password
    };
    //write the default settings to the eeprom

    sprintf(lcdStartup4, "%-20s", "Restart tester");
    #if defined(SERIAL_DEBUG)
      Serial.println(lcdStartup4);
    #endif
    EEPROM.put(1,defaultsettings);

  }else{
  //load the settings from the eeprom on startup.
  EEPROM.get(1,loadedSettings);
}

  sensors.begin(); // Start up the library Dallas Temperature IC Control
  //encoder_begin(7, 6); // Start the decoder encoder Pin A (CLK) = 3 encoder Pin B (DT) = 4 | Pin 7 (CLK) and Pin 6 (DT) for Version 2.0
  pinMode(5, INPUT_PULLUP); // Pin 5 Rotary Encoder Button (SW)
  for(byte i = 0; i < modules; i++)
  {
    pinMode(chargeLedPins[i], INPUT_PULLUP);
    pinMode(chargeMosfetPins[i], OUTPUT);
    pinMode(dischargeMosfetPins[i], OUTPUT);
    digitalWrite(chargeMosfetPins[i], LOW);
    digitalWrite(dischargeMosfetPins[i], LOW);
  }

  Serial.begin(115200);
  //readconfig();

  //Startup Screen

  lcd.init();
  lcd.clear();
  lcd.backlight();// Turn on backlight
  sprintf(lcdStartup0, "%-20s", "Smart Charger 1.0.1");
  #if defined(SERIAL_DEBUG)
    Serial.println(lcdStartup0);
  #endif
  lcd.setCursor(0,0);
  lcd.print(lcdStartup0);
  sprintf(lcdStartup1, "%-20s", "Initializing TP4056");
  #if defined(SERIAL_DEBUG)
    Serial.println(lcdStartup1);
  #endif
  lcd.setCursor(0,1);
  lcd.print(lcdStartup1);
  mosfetSwitchAll();
  #if defined(ETHERNET_ENABLE)
    sprintf(lcdStartup1, "%-20s", "Initializing W5500..");
  #endif
  #if defined(WIRELESS_ENABLE)
    sprintf(lcdStartup1,"%-20s", "Initializing ESP-01..");
  #endif
    #if defined(SERIAL_DEBUG)
      Serial.println(lcdStartup1);
    #endif

    lcd.setCursor(0,1);
    lcd.print(lcdStartup1);
  #if defined(ETHERNET_ENABLE)
    if (Ethernet.begin(mac) == 0) {
      // Try to congifure using a static IP address instead of DHCP:
      #if defined(SERIAL_DEBUG)
        Serial.println("Ethernet connection set to static ");
        //Serial.println(Ethernet.begin(mac),DEC);
      #endif
      Ethernet.begin(mac, ip);
    }
    char ethIP[16];
    IPAddress ethIp = Ethernet.localIP();
    sprintf(lcdStartup1, "IP:%d.%d.%d.%d%-20s", ethIp[0], ethIp[1], ethIp[2], ethIp[3], " ");
    lcd.setCursor(0,1);
    lcd.print(lcdStartup1);
  #endif

  #if defined(WIRELESS_ENABLE)
    //Initializing ESP-01
    Serial1.begin(9600);
    WiFi.init(&Serial1);
    //attempt to connect to WiFi
    if(WiFi.status() == WL_NO_SHIELD){
      sprintf(lcdStartup1,"%-20s", "ESP-01 not found");
      #if defined(SERIAL_DEBUG)
        Serial.println(lcdStartup1);
      #endif
      status = WL_NO_SHIELD;
      lcd.setCursor(0,2);
      lcd.print(lcdStartup1);

      //uncomment so it hangs the system if it fails to find the ESP-01
      //while(true);
    }
    // attempt to connect to WiFi network
    /*
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(loadedSettings.ssid);
    // Connect to WPA/WPA2 network
    status = WiFi.begin(loadedSettings.ssid, loadedSettings.pass);
  }
  */


    #endif


  #if defined(ETHERNET_ENABLE)
    float checkConnectionResult = checkConnection();
    if(checkConnectionResult == 0){
      sprintf(lcdStartup2, "%-20s", "DB/WEB: Connected");
    } else if (checkConnectionResult == 2) {
      sprintf(lcdStartup2, "%-20s", "DB/WEB: Time Out");
    } else {
      sprintf(lcdStartup2, "%-20s", "DB/WEB: Failed");
    }
    lcd.setCursor(0,2);
    lcd.print(lcdStartup2);
    #if defined(SERIAL_DEBUG)
      Serial.println(lcdStartup2);
    #endif
  #endif



  lcd.setCursor(0,3);
  #if defined(USB_ENABLE)
  sprintf(lcdStartup3, "%-20s", Usb.Init() == -1 ? "USB: Did not start" : "USB: Started");

  lcd.setCursor(0,3);
  lcd.print(lcdStartup3);
  #if defined(SERIAL_DEBUG)
    Serial.println(lcdStartup3);
  #endif
  HidKeyboard.SetReportParser(0, &Prs);
  #endif
  delay(2000);
  // Timer Triggers
  timerObject.every(2, rotaryEncoder);
  timerObject.every(8, serialcheck);
  timerObject.every(1000, cycleStateValues);
  timerObject.every(1000, cycleStateLCD);
  #if defined(ETHERNET_ENABLE) || defined(WIRELESS_ENABLE)
  timerObject.every(60000, cycleStatePostWebData);
  timerObject.every(5000, cycleStateGetWebData);
  #endif
  lcd.clear();
}

void loop()
{
  //Nothing else should be in this loop. Use timerObject and Usb.Task only .

    //Serial.println("byte available");
    //String test = Serial.readString();

#if defined(USB_ENABLE)
  Usb.Task(); //Barcode Scanner
#endif
  timerObject.update();
}

void processBarcode(int keyInput)
{
  //Barcode Scanner
  if (keyInput == 19) //Return Carriage ASCII Numeric 19
  {
    barcodeStringCompleted = true;
    cycleStateValues();
    cycleStateLCD();
  } else {
    sprintf(barcodeString, "%s%c", barcodeString, (char)keyInput);
    barcodeStringCompleted = false;
  }
}
#if defined(USB_ENABLE)
void KbdRptParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  //Barcode Scanner
  uint8_t c = OemToAscii(mod, key);
  if (c) OnKeyPressed(c);
}

void KbdRptParser::OnKeyPressed(uint8_t key)
{
  //Barcode Scanner
  processBarcode(key);
};
#endif



void readconfig()
{
  EEPROM.get(1,loadedSettings);
  Serial.print("loadedSettings.shuntResistor= ");
  Serial.println(loadedSettings.shuntResistor,2);
  Serial.print("loadedSettings.referenceVoltage= ");
  Serial.println(loadedSettings.referenceVoltage,2);
  Serial.print("loadedSettings.defaultBatteryCutOffVoltage= " );
  Serial.println(loadedSettings.defaultBatteryCutOffVoltage,2);
  Serial.print("loadedSettings.restTimeMinutes= ");
  Serial.println(loadedSettings.restTimeMinutes);
  Serial.print("loadedSettings.lowMilliamps= ");
  Serial.println(loadedSettings.lowMilliamps);
  Serial.print("loadedSettings.highMilliOhms= ");
  Serial.println(loadedSettings.highMilliOhms);
  Serial.print("loadedSettings.offsetMilliOhms= ");
  Serial.println(loadedSettings.offsetMilliOhms);
  Serial.print("loadedSettings.chargingTimeout= ");
  Serial.println(loadedSettings.chargingTimeout);
  Serial.print("loadedSettings.tempThreshold= ");
  Serial.println(loadedSettings.tempThreshold);
  Serial.print("loadedSettings.tempMaxThreshold= ");
  Serial.println(loadedSettings.tempMaxThreshold);
  Serial.print("loadedSettings.server= ");
  Serial.println(loadedSettings.server);
  Serial.print("loadedSettings.userHash= ");
  Serial.println(loadedSettings.userHash);
  Serial.print("loadedSettings.CDUnitID= ");
  Serial.println(loadedSettings.CDUnitID);
  Serial.print("loadedSettings.ssid= ");
  Serial.println(loadedSettings.ssid);
  Serial.print("loadedSettings.pass= ");
  Serial.println(loadedSettings.pass);

}
//************************serial configuration functions*********************
void recvWithEndMarker() {
 static byte ndx = 0;
 char endMarker = '\n';
 char rc;

 // if (Serial.available() > 0) {
           while (Serial.available() > 0 && newData == false) {
 rc = Serial.read();

 if (rc != endMarker) {
 receivedChars[ndx] = rc;
 ndx++;
 if (ndx >= numChars) {
 ndx = numChars - 1;
 }
 }
 else {
 receivedChars[ndx] = '\0'; // terminate the string
 ndx = 0;
 newData = true;
 }
 }
}


void configResponce(uint8_t type)
{
  if(type==NAK) Serial.println("NAK");
  if(type==ACK) Serial.println("ACK");
  if(type!=ACK && type!=NAK) Serial.println("ERR");
}
void serialcheck()
{
  //Serial.println("serialcheck called");
  recvWithEndMarker();

  if (newData == true)
  {
    char command[32] {0};
    int intvalue;
    float floatvalue;

    char * strtokIndx; // this is used by strtok() as an index


  strtokIndx = strtok(receivedChars,"=");      // get the first part - the string
  strcpy(command, strtokIndx); // copy it to messageFromPC
  strtokIndx = strtok(NULL, "="); // this continues where the previous call left off



    if(strcmp(command,"milliamps")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to an integer
      loadedSettings.lowMilliamps = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }

    else if(strcmp(command,"shunt")==0)
    {
      floatvalue = atof(strtokIndx);     // convert this part to a float
      loadedSettings.shuntResistor = floatvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"refv")==0)
    {
      floatvalue = atof(strtokIndx);     // convert this part to a float
      loadedSettings.referenceVoltage = floatvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"cutv")==0)
    {
      floatvalue = atof(strtokIndx);     // convert this part to a float
      loadedSettings.defaultBatteryCutOffVoltage = floatvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"timeout")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.restTimeMinutes = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"milliohms")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.highMilliOhms = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"ohmoffset")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.offsetMilliOhms = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"chrgtimeout")==0)
    {
      floatvalue = atof(strtokIndx);     // convert this part to a float
      loadedSettings.chargingTimeout = floatvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"tempthreshold")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.tempThreshold = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"tempmax")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.tempMaxThreshold = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"server")==0)
    {
      strcpy( loadedSettings.server,strtokIndx);
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"userhash")==0)
    {
      strcpy( loadedSettings.userHash,strtokIndx);
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"cduintid")==0)
    {
      intvalue = atoi(strtokIndx);     // convert this part to a float
      loadedSettings.CDUnitID = intvalue;
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"ssid")==0)
    {
      strcpy( loadedSettings.ssid,strtokIndx);
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"pass")==0)
    {
      strcpy( loadedSettings.pass,strtokIndx);
      EEPROM.put(1,loadedSettings);
      configResponce(ACK);
    }
    else if(strcmp(command,"reset")==0)
    {
      configResponce(ACK);
      EEPROM.write(0,0);
    }
    else if(strcmp(command,"read")==0)
    {
      configResponce(ACK);
      readconfig();


    }
    else
    {
      //NAK responce
      configResponce(NAK);
    }
    newData = false;

  }



}
//**************************END SERIAL CONFIURATION FUNCTIONS*******************
void rotaryEncoder()
{
  int rotaryEncoderDirection = myEnc.read();//encoder_data(); // Check for rotation
  if(rotaryEncoderDirection != 0)              // Rotary Encoder
  {
    if (cycleStateLast - rotaryEncoderDirection >= 0 && cycleStateLast - rotaryEncoderDirection < modules)
    {
      cycleStateLast = cycleStateLast - rotaryEncoderDirection;
    } else if (cycleStateLast - rotaryEncoderDirection == modules) {
      cycleStateLast = 0;
    } else if (cycleStateLast - rotaryEncoderDirection < 0) {
      cycleStateLast = modules - 1;
    }
    rotaryOveride = 60; // x (1 min) cycles of cycleStateLCD() decrements - 1 each run until 0
    lcd.clear();
    cycleStateLCD();
  } else { // Button
    buttonState = digitalRead(5);
    if (buttonState != lastButtonState)
    {
      if (buttonState == LOW)
      {
        if (rotaryOverideLock == 1)
        {
          rotaryOverideLock = 0;
          rotaryOveride = 0;
        } else {
          if (rotaryOveride > 0) rotaryOverideLock = 1;
        }
      }
    }
    lastButtonState = buttonState;
  }
}

#if defined(ETHERNET_ENABLE) || defined(WIRELESS_ENABLE)
void cycleStatePostWebData()
{
  bool postData;
  for(byte i = 0; i < modules; i++) // Check if any data needs posting
  {
    if(pendingDischargeRecord[i] == true)
    {
      postData = true;
      break;
    }
  }
  if (postData == true)
  {
    byte connectionResult = checkConnection();
    if (connectionResult == 0) // Check Connection
    {
      for(byte i = 0; i < modules; i++)
      {
        if(pendingDischargeRecord[i] == true)
        {
          if (addDischargeRecord(i) == 0) pendingDischargeRecord[i] = false;
        }
      }
    } else if (connectionResult == 1){
      // Error Connection

    } else if (connectionResult == 2){
      // Time out Connection

    }
  }
}

void cycleStateGetWebData()
{
  bool postData;
  for(byte i = 0; i < modules; i++) // Check if any data needs posting
  {
    if(pendingBatteryRecord[i] == true)
    {
      postData = true;
      break;
    }
  }
  if (postData == true)
  {
    byte connectionResult = checkConnection();
    if (connectionResult == 0) // Check Connection
    {
      for(byte i = 0; i < modules; i++)
      {
        if(pendingBatteryRecord[i] == true)
        {
          float cutOffVoltageResult = getCutOffVoltage(i);
          if (cutOffVoltageResult > 100)
          {
            batteryCutOffVoltage[i] = cutOffVoltageResult - 100;
            pendingBatteryRecord[i] = false;
            break;
          } else {
            switch ((int) cutOffVoltageResult)
            {
            case 0:
              //SUCCESSFUL = 0
              batteryCutOffVoltage[i] = loadedSettings.defaultBatteryCutOffVoltage; // No DB in Table tblbatterytype for BatteryCutOffVoltage (NULL record)
              pendingBatteryRecord[i] = false;
              break;
            case 1:
              //FAILED = 1
              //Need to retry
              pendingBatteryRecord[i] = true;
              break;
            case 2:
              //TIMEOUT = 2
              //Need to retry
              pendingBatteryRecord[i] = true;
              break;
            case 5:
              //ERROR_NO_BARCODE_DB = 5
              batteryCutOffVoltage[i] = loadedSettings.defaultBatteryCutOffVoltage;
              pendingBatteryRecord[i] = false;
              // What happens next as not in DB???
              break;
            }
          }
        }
      }
    } else if (connectionResult == 1){
      // Error Connection

    } else if (connectionResult == 2){
      // Time out Connection

    }
  }
}
#endif

void cycleStateLCD()
{
  if (rotaryOveride > 0)
  {
    cycleStateLCDOutput(cycleStateLast ,0 ,1);
    if (rotaryOverideLock == 0) rotaryOveride--;
  } else {
    cycleStateLCDOutput(cycleStateLast ,0 ,1);
    if (cycleStateLast == modules - 1)
    {
      cycleStateLCDOutput(0 ,2 ,3);
    } else {
      cycleStateLCDOutput(cycleStateLast + 1 ,2 ,3);
    }
    if(cycleStateCycles == 4) // 4 = 4x 1000ms (4 seconds) on 1 screen -> Maybe have const byte for 4 -> Menu Option?
    {
      if (cycleStateLast >= modules - 2)
      {
        cycleStateLast = 0;
      } else {
        cycleStateLast += 2;
      }
      cycleStateCycles = 0;
    } else {
      cycleStateCycles++;
    }
  }
}

void cycleStateLCDOutput(byte j, byte LCDRow0 ,byte LCDRow1)
{
  char lcdLine0[25];
  char lcdLine1[25];
  //if (rotaryOveride > 0 && strcmp(lcdLine2, "                    ") != 0 && strcmp(lcdLine3, "                    ") != 0)
  if (rotaryOveride > 0)
  {
    sprintf(lcdLine2, "%-20s", " ");
    sprintf(lcdLine3, "%-18s%02d", " ", rotaryOveride);
    lcd.setCursor(0,2);
    lcd.print(lcdLine2);
    lcd.setCursor(0,3);
    lcd.print(lcdLine3);
  }
  if (rotaryOveride > 0 && rotaryOverideLock == 1)
  {
    sprintf(lcdLine3, "%-20s", "   SCREEN LOCKED");
    lcd.setCursor(0,3);
    lcd.print(lcdLine3);
  }

  switch (cycleState[j])
  {
  case 0: // Check Battery Voltage
    sprintf(lcdLine0, "%02d%-18s", j + 1, "-BATTERY CHECK");
    sprintf(lcdLine1, "%-15s%d.%02dV", batteryDetectedCount[j] > 0 ? "DETECTED....." : "NOT DETECTED!", (int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100);
    break;
  case 1: // Get Battery Barcode
    sprintf(lcdLine0, "%02d%-18s", j + 1, "-BARCODE");
    sprintf(lcdLine1, "%-20s", strcmp(batteryBarcode[j], "") == 0 ? "NO BARCODE SCANNED!" : batteryBarcode[j]);
    break;
  case 2: // Charge Battery
    sprintf(lcdLine0, "%02d%-10s%02d:%02d:%02d", j + 1, "-CHARGING ", intHours[j], intMinutes[j], intSeconds[j]);
    sprintf(lcdLine1, "%d.%02dV %02d%c D%02d%c %d.%02dV", (int)batteryInitialVoltage[j], (int)(batteryInitialVoltage[j]*100)%100, batteryInitialTemp[j], 223, (batteryCurrentTemp[j] - batteryInitialTemp[j]) > 0 ? (batteryCurrentTemp[j] - batteryInitialTemp[j]) : 0, 223,(int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100);
    break;
  case 3: // Check Battery Milli Ohms
    sprintf(lcdLine0, "%02d%-18s", j + 1, "-RESISTANCE CHECK");
    sprintf(lcdLine1, "%-14s%04dm%c", "MILLIOHMS", (int) milliOhmsValue[j], 244);
    break;
  case 4: // Rest Battery
    sprintf(lcdLine0, "%02d%-10s%02d:%02d:%02d", j + 1, "-RESTING", intHours[j], intMinutes[j], intSeconds[j]);
    if (strlen(batteryBarcode[j]) >= 15)
    {
      sprintf(lcdLine1, "%-20s", batteryBarcode[j]);
    } else if (strlen(batteryBarcode[j]) >= 10) {
      sprintf(lcdLine1, "%-15s%d.%02dV", batteryBarcode[j], (int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100);
    } else {
      sprintf(lcdLine1, "%-10s %02d%c %d.%02dV", batteryBarcode[j], batteryCurrentTemp[j], 223, (int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100);
    }
    break;
  case 5: // Discharge Battery
    sprintf(lcdLine0, "%02d%-7s%d.%02dA %d.%02dV", j + 1, "-DCHG", (int)dischargeAmps[j], (int)(dischargeAmps[j]*100)%100, (int)dischargeVoltage[j], (int)(dischargeVoltage[j]*100)%100);
    sprintf(lcdLine1, "%02d:%02d:%02d %02d%c %04dmAh", intHours[j], intMinutes[j], intSeconds[j], batteryCurrentTemp[j], 223, (int) dischargeMilliamps[j]);
    break;
  case 6: // Recharge Battery
    sprintf(lcdLine0, "%02d%-10s%02d:%02d:%02d", j + 1, "-RECHARGE ", intHours[j], intMinutes[j], intSeconds[j]);
    sprintf(lcdLine1, "%d.%02dV %02d%c D%02d%c %d.%02dV", (int)batteryInitialVoltage[j], (int)(batteryInitialVoltage[j]*100)%100, batteryInitialTemp[j], 223, (batteryCurrentTemp[j] - batteryInitialTemp[j]) > 0 ? (batteryCurrentTemp[j] - batteryInitialTemp[j]) : 0, 223, (int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100);
    break;
  case 7: // Completed
    switch (batteryFaultCode[j])
    {
    case 0: // Finished
      sprintf(lcdLine0, "%02d%-14sH%02d%c", j + 1, "-FINISHED", batteryHighestTemp[j], 223);
      break;
    case 3: // High Milli Ohms
      sprintf(lcdLine0, "%02d%-11s %c H%02d%c", j + 1, "-FAULT HIGH", 244, batteryHighestTemp[j], 223);
      break;
    case 5: // Low Milliamps
      sprintf(lcdLine0, "%02d%-14sH%02d%c", j + 1, "-FAULT LOW Ah", batteryHighestTemp[j], 223);
      break;
    case 7: // High Temperature
      sprintf(lcdLine0, "%02d%-15s%02d%c", j + 1, "-FAULT HIGH TMP", batteryHighestTemp[j], 223);
      break;
    case 9: // Charge Timeout
      sprintf(lcdLine0, "%02d%-14sH%02d%c", j + 1, "-FAULT C TIME", batteryHighestTemp[j], 223);
      break;
    }
    sprintf(lcdLine1, "%04dm%c %d.%02dV %04dmAh",(int) milliOhmsValue[j], 244,(int)batteryVoltage[j], (int)(batteryVoltage[j]*100)%100, (int) dischargeMilliamps[j]);
    break;
  }
  lcd.setCursor(0,LCDRow0);
  lcd.print(lcdLine0);
  lcd.setCursor(0,LCDRow1);
  lcd.print(lcdLine1);
}

void cycleStateValues()
{
  byte tempProcessed = 0;
  for(byte i = 0; i < modules; i++)
  {
    switch (cycleState[i])
    {
    case 0: // Check Battery Voltage
      if(batteryCheck(i)) batteryDetectedCount[i]++;
      if (batteryDetectedCount[i] == 5)
      {
        batteryCurrentTemp[i] = getTemperature(i);
        batteryInitialTemp[i] = batteryCurrentTemp[i];
        batteryHighestTemp[i] = batteryCurrentTemp[i];
        // Re-Initialization
        batteryDetectedCount[i] = 0;
        preDischargeCompleted[i] = false;
        batteryNotDetectedCount[i] = 0;
        strcpy(batteryBarcode[i], "");
        batteryChargedCount[i] = 0;
        batteryMilliOhmsCount[i] = 0;
        tempMilliOhmsValue[i] = 0;
        milliOhmsValue[i] = 0;
        intMilliSecondsCount[i] = 0;
        longMilliSecondsPreviousCount[i] = 0;
        longMilliSecondsPrevious[i] = 0;
        longMilliSecondsPassed[i] = 0;
        dischargeMilliamps[i] = 0.0;
        dischargeVoltage[i] = 0.00;
        dischargeAmps[i] = 0.00;
        dischargeCompleted[i] = false;
        batteryFaultCode[i] = 0;
        mosfetSwitchCount[i] = 0;
        clearSecondsTimer(i);
        getBatteryVoltage(i); // Get battery voltage for Charge Cycle
        batteryInitialVoltage[i] = batteryVoltage[i];
        cycleState[i] = 1; // Check Battery Voltage Completed set cycleState to Get Battery Barcode
      }
      break;

    case 1: // Get Battery Barcode
      if(strcmp(batteryBarcode[i], "") == 0)
      {
        if (pendingBarcode == 255) pendingBarcode = i;
        // HTTP Check if the Battery Barcode is in the DB
        if (barcodeStringCompleted == true && pendingBarcode == i)
        {
          //byte connectionResult = checkBatteryExists(i);
          //if (connectionResult == 0 || connectionAttempts[i] >= 10)
          //{
          //connectionAttempts[i] = 0;
          strcpy(batteryBarcode[i], barcodeString);
          /*
      for(byte k = 0; k < modules; k++)
      {
      if (strcmp(batteryBarcode[i], batteryBarcode[k]) == 0 && i != k)
      {
      barcodeDuplicateFound[i] = true;
      //Barcode Duplicate exists in batteryBarcode ARRAY - (Same barcode scanned twice??)
      }
      }
      */
          strcpy(barcodeString, "");
          barcodeStringCompleted = false;
          rotaryOveride = 0;
          pendingBarcode = 255;
          cycleState[i] = 2; // Get Battery Barcode Completed set cycleState to Charge Battery
          clearSecondsTimer(i);
          //} else {
          //  connectionAttempts[i]++;
          // Not in DB or connection error
          //}
        } else if (pendingBarcode == i) {
          cycleStateLast = i; // Gets priority and locks cycleLCD Display
          rotaryOveride = 1; // LCD Cycle on this until barcode is scanned
        }
      }
      //Check if battery has been removed
      if(!batteryCheck(i)) batteryNotDetectedCount[i]++;
      if (batteryNotDetectedCount[i] == 5) cycleState[i] = 0; // Completed and Battery Removed set cycleState to Check Battery Voltage
      break;

    case 2: // Charge Battery
      //if (batteryVoltage[i] >= 3.95 && batteryVoltage[i] < 4.18 && intMinutes[i] < 15 && preDischargeCompleted == false)
      getBatteryVoltage(i);
      batteryCurrentTemp[i] = getTemperature(i);
      tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
      if (tempProcessed == 2)
      {
        //Battery Temperature is >= MAX Threshold considered faulty
        digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
        batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
        cycleState[i] = 7; // Temperature is to high. Battery is considered faulty set cycleState to Completed
      } else {
        digitalWrite(chargeMosfetPins[i], HIGH); // Turn on TP4056
        batteryChargedCount[i] = batteryChargedCount[i] + chargeCycle(i);
        if (batteryChargedCount[i] == 5)
        {
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          //clearSecondsTimer(i);
          cycleState[i] = 3; // Charge Battery Completed set cycleState to Check Battery Milli Ohms
        }
      }
      if (intHours[i] == loadedSettings.chargingTimeout) // Charging has reached Timeout period. Either battery will not hold charge, has high capacity or the TP4056 is faulty
      {
        digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
        batteryFaultCode[i] = 9; // Set the Battery Fault Code to 7 Charging Timeout
        cycleState[i] = 7; // Charging Timeout. Battery is considered faulty set cycleState to Completed
      }
      break;

    case 3: // Check Battery Milli Ohms
      batteryMilliOhmsCount[i] = batteryMilliOhmsCount[i] + milliOhms(i);
      tempMilliOhmsValue[i] = tempMilliOhmsValue[i] + milliOhmsValue[i];
      if (batteryMilliOhmsCount[i] == 16)
      {
        milliOhmsValue[i] = tempMilliOhmsValue[i] / 16;
        if (milliOhmsValue[i] > loadedSettings.highMilliOhms) // Check if Milli Ohms is greater than the set high Milli Ohms value
        {
          batteryFaultCode[i] = 3; // Set the Battery Fault Code to 3 High Milli Ohms
          cycleState[i] = 7; // Milli Ohms is high battery is considered faulty set cycleState to Completed
        } else {
          if (intMinutes[i] <= 1) // No need to rest the battery if it is already charged
          {
            //cycleState[i] = 5; // Check Battery Milli Ohms Completed set cycleState to Discharge Battery
            cycleState[i] = 4; // Check Battery Milli Ohms Completed set cycleState to Discharge Battery
          } else {
            cycleState[i] = 4; // Check Battery Milli Ohms Completed set cycleState to Rest Battery
          }
          clearSecondsTimer(i);
        }
      }
      break;

    case 4: // Rest Battery
      getBatteryVoltage(i);
      batteryCurrentTemp[i] = getTemperature(i);
      if (intMinutes[i] == loadedSettings.restTimeMinutes) // Rest time
      {
        clearSecondsTimer(i);
        intMilliSecondsCount[i] = 0;
        longMilliSecondsPreviousCount[i] = 0;
        cycleState[i] = 5; // Rest Battery Completed set cycleState to Discharge Battery
      }
      break;

    case 5: // Discharge Battery
      batteryCurrentTemp[i] = getTemperature(i);
      tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
      if (tempProcessed == 2)
      {
        //Battery Temperature is >= MAX Threshold considered faulty
        digitalWrite(dischargeMosfetPins[i], LOW);
        batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
        cycleState[i] = 7; // Temperature is high. Battery is considered faulty set cycleState to Completed
      } else {
        if (dischargeCompleted[i] == true)
        {
          //Set Variables for Web Post
          dischargeMinutes[i] = intMinutes[i] + (intHours[i] * 60);
          pendingDischargeRecord[i] = true;
          if (dischargeMilliamps[i] < loadedSettings.lowMilliamps) // No need to recharge the battery if it has low Milliamps
          {
            batteryFaultCode[i] = 5; // Set the Battery Fault Code to 5 Low Milliamps
            cycleState[i] = 7; // Discharge Battery Completed set cycleState to Completed
          } else {
            getBatteryVoltage(i); // Get battery voltage for Recharge Cycle
            batteryInitialVoltage[i] = batteryVoltage[i]; // Reset Initial voltage
            cycleState[i] = 6; // Discharge Battery Completed set cycleState to Recharge Battery
          }
          clearSecondsTimer(i);
        } else {
          if(dischargeCycle(i)) dischargeCompleted[i] = true;
        }
      }
      break;

    case 6: // Recharge Battery
      digitalWrite(chargeMosfetPins[i], HIGH); // Turn on TP4056
      batteryCurrentTemp[i] = getTemperature(i);
      tempProcessed = processTemperature(i, batteryCurrentTemp[i]);
      if (tempProcessed == 2)
      {
        //Battery Temperature is >= MAX Threshold considered faulty
        digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
        batteryFaultCode[i] = 7; // Set the Battery Fault Code to 7 High Temperature
        cycleState[i] = 7; // Temperature is high. Battery is considered faulty set cycleState to Completed
      } else {
        batteryRechargedCount[i] = batteryRechargedCount[i] + chargeCycle(i);
        if (batteryRechargedCount[i] == 2)
        {
          digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
          batteryRechargedCount[i] = 0; // ????
          clearSecondsTimer(i);
          cycleState[i] = 7; // Recharge Battery Completed set cycleState to Completed
        }
      }
      if (intHours[i] == loadedSettings.chargingTimeout) // Charging has reached Timeout period. Either battery will not hold charge, has high capacity or the TP4056 is faulty
      {
        digitalWrite(chargeMosfetPins[i], LOW); // Turn off TP4056
        batteryFaultCode[i] = 9; // Set the Battery Fault Code to 9 Charging Timeout
        cycleState[i] = 7; // Charging Timeout. Battery is considered faulty set cycleState to Completed
      }
      getBatteryVoltage(i);
      break;

    case 7: // Completed
      if (mosfetSwitchCount[i] <= 9)
      {
        mosfetSwitchCount[i]++;
      }
      else if (mosfetSwitchCount[i] == 10)
      {
        mosfetSwitch(i);
        mosfetSwitchCount[i]++;
      } else if (mosfetSwitchCount[i] >= 11)
      {
        mosfetSwitch(i);
        mosfetSwitchCount[i] = 0;
      }
      if (!batteryCheck(i)) batteryNotDetectedCount[i]++;
      if (batteryNotDetectedCount[i] == 5) cycleState[i] = 0; // Completed and Battery Removed set cycleState to Check Battery Voltage
      break;
    }
    secondsTimer(i);
  }
}

void mosfetSwitch(byte j)
{
  if (mosfetSwitchState[j])
  {
    digitalWrite(chargeMosfetPins[j], LOW);
    digitalWrite(dischargeMosfetPins[j], LOW);
    mosfetSwitchState[j] = false;
  } else {
    digitalWrite(chargeMosfetPins[j], HIGH);
    digitalWrite(dischargeMosfetPins[j], HIGH);
    mosfetSwitchState[j] = true;
  }
}

void mosfetSwitchAll()
{
  for(byte i = 0; i < modules; i++)
  {
    for(byte j = 0; j < 3; j++)
    {
      digitalWrite(chargeMosfetPins[i], HIGH);
      digitalWrite(dischargeMosfetPins[i], HIGH);
      delay(100);
      digitalWrite(chargeMosfetPins[i], LOW);
      digitalWrite(dischargeMosfetPins[i], LOW);
      delay(100);
    }
  }
}

int getTemperature(byte j)
{
  sensors.requestTemperaturesByAddress(tempSensorSerial[j]);
  float tempC = sensors.getTempC(tempSensorSerial[j]);
  return (int) tempC;
}

byte processTemperature(byte j, byte currentTemperature)
{
  if (currentTemperature > batteryHighestTemp[j]) batteryHighestTemp[j] = currentTemperature; // Set highest temperature if current value is higher
  if ((currentTemperature - batteryInitialTemp[j]) > loadedSettings.tempThreshold)
  {

    if ((currentTemperature - batteryInitialTemp[j]) > loadedSettings.tempMaxThreshold)
    {
      //Temp higher than Maximum Threshold
      return 2;
    } else {
      //Temp higher than Threshold <- Does nothing yet need some flag / warning
      return 1;
    }
  } else {
    //Temp lower than Threshold
    return 0;
  }
}

void secondsTimer(byte j)
{
  unsigned long longMilliSecondsCount = millis() - longMilliSecondsCleared[j];
  intHours[j] = longMilliSecondsCount / (1000L * 60L * 60L);
  intMinutes[j] = (longMilliSecondsCount % (1000L * 60L * 60L)) / (1000L * 60L);
  intSeconds[j] = (longMilliSecondsCount % (1000L * 60L * 60L) % (1000L * 60L)) / 1000;
}

void clearSecondsTimer(byte j)
{
  longMilliSecondsCleared[j] = millis();
  intSeconds[j] = 0;
  intMinutes[j] = 0;
  intHours[j] = 0;
}

byte chargeCycle(byte j)
{
  digitalWrite(chargeMosfetPins[j], HIGH); // Turn on TP4056
  if (digitalRead(chargeLedPins[j]) == HIGH)
  {
    return 1;
  } else {
    return 0;
  }
}

byte milliOhms(byte j)
{
  float resistanceAmps = 0.00;
  float voltageDrop = 0.00;
  float batteryVoltageInput = 0.00;
  float batteryShuntVoltage = 0.00;
  digitalWrite(dischargeMosfetPins[j], LOW);
  getBatteryVoltage(j);
  batteryVoltageInput = batteryVoltage[j];
  digitalWrite(dischargeMosfetPins[j], HIGH);
  getBatteryVoltage(j);
  batteryShuntVoltage = batteryVoltage[j];
  digitalWrite(dischargeMosfetPins[j], LOW);
  resistanceAmps = batteryShuntVoltage / loadedSettings.shuntResistor;
  voltageDrop = batteryVoltageInput - batteryShuntVoltage;
  milliOhmsValue[j] = ((voltageDrop / resistanceAmps) * 1000) + loadedSettings.offsetMilliOhms; // The Drain-Source On-State Resistance of the IRF504's
  if (milliOhmsValue[j] > 9999) milliOhmsValue[j] = 9999;
  return 1;
}

bool dischargeCycle(byte j)
{
  float batteryShuntVoltage = 0.00;
  intMilliSecondsCount[j] = intMilliSecondsCount[j] + (millis() - longMilliSecondsPreviousCount[j]);
  longMilliSecondsPreviousCount[j] = millis();
  if (intMilliSecondsCount[j] >= 5000 || dischargeAmps[j] == 0) // Get reading every 5+ seconds or if dischargeAmps = 0 then it is first run
  {
    dischargeVoltage[j] = analogRead(batteryVolatgePins[j]) * loadedSettings.referenceVoltage / 1023.0;
    batteryShuntVoltage = analogRead(batteryVolatgeDropPins[j]) * loadedSettings.referenceVoltage / 1023.0;
    if(dischargeVoltage[j] >= loadedSettings.defaultBatteryCutOffVoltage)
    {
      digitalWrite(dischargeMosfetPins[j], HIGH);
      dischargeAmps[j] = (dischargeVoltage[j] - batteryShuntVoltage) / loadedSettings.shuntResistor;
      longMilliSecondsPassed[j] = millis() - longMilliSecondsPrevious[j];
      dischargeMilliamps[j] = dischargeMilliamps[j] + (dischargeAmps[j] * 1000.0) * (longMilliSecondsPassed[j] / 3600000.0);
      longMilliSecondsPrevious[j] = millis();
    }
    intMilliSecondsCount[j] = 0;
    if(dischargeVoltage[j] < loadedSettings.defaultBatteryCutOffVoltage)
    {
      digitalWrite(dischargeMosfetPins[j], LOW);
      return true;
    }
  }
  return false;
}

bool batteryCheck(byte j)
{
  getBatteryVoltage(j);
  if (batteryVoltage[j] <= 2.1)
  {
    return false;
  } else {
    return true;
  }
}

void getBatteryVoltage(byte j)
{
  float batterySampleVoltage = 0.00;
  for(byte i = 0; i < 100; i++)
  {
    batterySampleVoltage = batterySampleVoltage + analogRead(batteryVolatgePins[j]);
  }
  batterySampleVoltage = batterySampleVoltage / 100;
  batteryVoltage[j] = batterySampleVoltage * loadedSettings.referenceVoltage / 1023.0;
}

byte checkConnection()
{
  if (client.connect(loadedSettings.server, 80))
  {
    client.print("GET /check_connection.php?");
	client.print("UserHash=");
    client.print(loadedSettings.userHash);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(loadedSettings.server);
    client.println("Connection: close");
    client.println();
    client.println();
    return readPage();
  } else {
    return 1;
  }
}

byte checkBatteryExists(byte j)
{
  if (client.connect(loadedSettings.server, 80))
  {
    client.print("GET /check_battery_barcode.php?");
	client.print("UserHash=");
    client.print(loadedSettings.userHash);
    client.print("&");
    client.print("BatteryBarcode=");
    client.print(batteryBarcode[j]);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(loadedSettings.server);
    client.println("Connection: close");
    client.println();
    client.println();
    return readPage();
  } else {
    return 1;
  }
}

float getCutOffVoltage(byte j)
{
  if (client.connect(loadedSettings.server, 80))
  {
    client.print("GET /get_cutoff_voltage.php?");
	client.print("UserHash=");
    client.print(loadedSettings.userHash);
    client.print("&");
    client.print("BatteryBarcode=");
    client.print(batteryBarcode[j]);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(loadedSettings.server);
    client.println("Connection: close");
    client.println();
    client.println();
    return readPage();
  } else {
    return 1;
  }
}

byte addDischargeRecord(byte j)
{
  if (client.connect(loadedSettings.server, 80))
  {
    client.print("GET /battery_discharge_post.php?");
	client.print("UserHash=");
    client.print(loadedSettings.userHash);
    client.print("&");
	client.print("CDUnitID=");
    client.print(loadedSettings.CDUnitID);
    client.print("&");
    client.print("BatteryBarcode=");
    client.print(batteryBarcode[j]);
    client.print("&");
    client.print("CDModuleNumber=");
    client.print(j + 1);
    client.print("&");
    client.print("MilliOhms=");
    client.print((int) milliOhmsValue[j]);
    client.print("&");
    client.print("Capacity=");
    client.print((int) dischargeMilliamps[j]);
    client.print("&");
    client.print("BatteryInitialTemp=");
    client.print(batteryInitialTemp[j]);
    client.print("&");
    client.print("BatteryHighestTemp=");
    client.print(batteryHighestTemp[j]);
    client.print("&");
    client.print("DischargeMinutes=");
    client.print(dischargeMinutes[j]);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.println(loadedSettings.server);
    client.println("Connection: close");
    client.println();
    client.println();
    return readPage();
  } else {
    return 1;
  }
}

float readPage()
{
  stringPosition = 0;
  unsigned long startTime = millis();
  memset( &serverResult, 0, 32 ); //Clear serverResult memory
  while(true)
  {
    if (millis() < startTime + 10000) // Time out of 10000 milliseconds (Possible cause of crash on Ethernet)
    {
      if (client.available())
      {
        char c = client.read();
        if (c == '<' )  //'<' Start character
        {
          startRead = true; //Ready to start reading the part
        } else if (startRead)
        {
          if(c != '>') //'>' End character
          {
            serverResult[stringPosition] = c;
            stringPosition ++;
          } else {
            startRead = false;
            client.stop();
            client.flush();
            if(strstr(serverResult, "CUT_OFF_VOLTAGE_") != 0)
            {
              char tempCutOffVoltage[10];
              strncpy(tempCutOffVoltage, serverResult + 16, strlen(serverResult) - 15);
              return (atof(tempCutOffVoltage) + 100); //Battery Cut Off Voltage (+ 100 to avoid conflicts with return codes)
            }
            if(strcmp(serverResult, "SUCCESSFUL") == 0) return 0; //SUCCESSFUL
            if(strcmp(serverResult, "ERROR_DATABASE") == 0) return 3; //ERROR_DATABASE
            if(strcmp(serverResult, "ERROR_MISSING_DATA") == 0) return 4; //ERROR_MISSING_DATA
            if(strcmp(serverResult, "ERROR_NO_BARCODE_DB") == 0) return 5; //ERROR_NO_BARCODE_DB
            if(strcmp(serverResult, "ERROR_NO_BARCODE_INPUT") == 0) return 6; //ERROR_NO_BARCODE_INPUT
            if(strcmp(serverResult, "ERROR_DATABASE_HASH_INPUT") == 0) return 7; //ERROR_DATABASE_HASH_INPUT
            if(strcmp(serverResult, "ERROR_HASH_INPUT") == 0) return 8; //ERROR_HASH_INPUT
           // if(strcmp(serverResult, "ERROR_BATTERY_NOT_FOUND") == 0) return 9; //ERROR_BATTERY_NOT_FOUND
          }
        }
      }
    } else {
      client.stop();
      client.flush();
      return 2; //TIMEOUT
    }
  }
}
