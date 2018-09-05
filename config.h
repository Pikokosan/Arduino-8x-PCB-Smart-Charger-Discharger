
//************************PIN CONFIGURATION****************************
static const uint8_t batteryVolatgePins[] =     {A0,A2,A4,A6,A8,A10,A12,A14};
static const uint8_t batteryVolatgeDropPins[] = {A1,A3,A5,A7,A9,A11,A13,A15};
static const uint8_t chargeMosfetPins[] =       {22,25,28,31,34,37,40,43};
static const uint8_t chargeLedPins[] =          {23,26,29,32,35,38,41,44};
static const uint8_t dischargeMosfetPins[] =    {24,27,30,33,36,39,42,45};

#define ONE_WIRE_BUS 4 // Pin 2 Temperature Sensors | Pin 4 for Version 2.0


//************************END PIN CONFIGURATION************************

#define TEMPERATURE_PRECISION 9




// Constant Variables
const byte modules = 8; // Number of Modules
const float shuntResistor = 3.3; // In Ohms - Shunt resistor resistance
const float referenceVoltage = 4.95; // 5V Output of Arduino
const float defaultBatteryCutOffVoltage = 2.8; // Voltage that the discharge stops ---> Will be replaced with Get DB Discharge Cutoff
const byte restTimeMinutes = 1; // The time in Minutes to rest the battery after charge. 0-59 are valid
const int lowMilliamps = 1000; //  This is the value of Milli Amps that is considered low and does not get recharged because it is considered faulty
const int highMilliOhms = 500; //  This is the value of Milli Ohms that is considered high and the battery is considered faulty
const int offsetMilliOhms = 0; // Offset calibration for MilliOhms
const byte chargingTimeout = 8; // The timeout in Hours for charging
const byte tempThreshold = 7; // Warning Threshold in degrees above initial Temperature 
const byte tempMaxThreshold = 10; //Maximum Threshold in degrees above initial Temperature - Considered Faulty
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
const byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // If you have multiple devices you may need to change this per CD Unit (MAC conflicts on same network???)
const char server[] = "submit.vortexit.co.nz";    // Server to connect - Add your server here
const char userHash[] = "";    // Database Hash - this is unique per user - Get this from Charger / Discharger Menu -> View
const byte CDUnitID = ;    // CDUnitID this is the Units ID - this is unique per user - Get this from Charger / Discharger Menu -> View -> Select your Charger / Discharger

