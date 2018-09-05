#include <Arduino.h>
//************************PIN CONFIGURATION****************************
static const uint8_t batteryVolatgePins[] =     {A0,A2,A4,A6,A8,A10,A12,A14};
static const uint8_t batteryVolatgeDropPins[] = {A1,A3,A5,A7,A9,A11,A13,A15};
static const uint8_t chargeMosfetPins[] =       {22,25,28,31,34,37,40,43};
static const uint8_t chargeLedPins[] =          {23,26,29,32,35,38,41,44};
static const uint8_t dischargeMosfetPins[] =    {24,27,30,33,36,39,42,45};

#define ONE_WIRE_BUS 4 // Pin 2 Temperature Sensors | Pin 4 for Version 2.0


//************************END PIN CONFIGURATION************************

#define TEMPERATURE_PRECISION 9



/*
eeprom layout
first startup[0]
shuntResistor[1] = float

const byte modules = 8; // Number of Modules
float shuntResistor = 3.3; // In Ohms - Shunt resistor resistance
float referenceVoltage = 4.95; // 5V Output of Arduino
const float defaultBatteryCutOffVoltage = 2.8; // Voltage that the discharge stops ---> Will be replaced with Get DB Discharge Cutoff
const byte restTimeMinutes = 1; // The time in Minutes to rest the battery after charge. 0-59 are valid
const int lowMilliamps = 1000; //  This is the value of Milli Amps that is considered low and does not get recharged because it is considered faulty
const int highMilliOhms = 500; //  This is the value of Milli Ohms that is considered high and the battery is considered faulty
const int offsetMilliOhms = 0; // Offset calibration for MilliOhms
const byte chargingTimeout = 8; // The timeout in Hours for charging
const byte tempThreshold = 7; // Warning Threshold in degrees above initial Temperature
const byte tempMaxThreshold = 10; //Maximum Threshold in degrees above initial Temperature - Considered Faulty
char server[] = "submit.vortexit.co.nz";    // Server to connect - Add your server here
char userHash[] = "";    // Database Hash - this is unique per user - Get this from Charger / Discharger Menu -> View
byte CDUnitID = 1;    // CDUnitID this is the Units ID - this is unique per user - Get this from Charger / Discharger Menu -> View -> Select your Charger / Discharger


*/
