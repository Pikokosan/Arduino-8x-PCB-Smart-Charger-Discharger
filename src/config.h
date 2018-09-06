#include <Arduino.h>
#include <WiFiEsp.h>

//uncomment for serial debug output.
#define SERIAL_DEBUG
//#define ETHERNET_ENABLE
//#define USB_ENABLE
#define WIRELESS_ENABLE

#define TEMPERATURE_PRECISION 9

const byte modules = 8; // Number of Modules


//************************PIN CONFIGURATION****************************
static const uint8_t batteryVolatgePins[] =     {A0,A2,A4,A6,A8,A10,A12,A14};
static const uint8_t batteryVolatgeDropPins[] = {A1,A3,A5,A7,A9,A11,A13,A15};
static const uint8_t chargeMosfetPins[] =       {22,25,28,31,34,37,40,43};
static const uint8_t chargeLedPins[] =          {23,26,29,32,35,38,41,44};
static const uint8_t dischargeMosfetPins[] =    {24,27,30,33,36,39,42,45};

#define ONE_WIRE_BUS 4 // Pin 2 Temperature Sensors | Pin 4 for Version 2.0


//************************END PIN CONFIGURATION************************



//*************************NETWORK CONFIG******************************
#if defined(ETHERNET_ENABLE)
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // If you have multiple devices you may need to change this per CD Unit (MAC conflicts on same network???)
  IPAddress ip(192, 168, 0, 177); // Set the static IP address to use if the DHCP fails to get assign
#endif
#if defined(WIRELESS_ENABLE)
  int status = WL_IDLE_STATUS;     // the Wifi radio's status
#endif
#define NAK 0x01
#define ACK 0x02
//*************************NETWORK CONFIG END******************************
