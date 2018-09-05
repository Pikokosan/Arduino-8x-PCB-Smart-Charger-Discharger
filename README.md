Arduino-8x-PCB-Smart-Charger-Discharger
---------------------------------------------------------------------------
Created by Brett Watt on 20/07/2018
Copyright 2018 - Under creative commons license 3.0:
Modified by Pikoko

This software is furnished "as is", without technical support, and with no
warranty, express or implied, as to its usefulness for any purpose.

Original creator Brett Watt
Email info@vortexit.co.nz
Web www.vortexit.co.nz

This is the Arduino 8x 18650 Smart Charger / Discharger Code

## Current implementation:
- TP4056
- Rotary Encoder KY-040 Module
- Temp Sensor DS18B20
- Ethernet Module W5500
- ESP-01(hardware Serial1)
- Mini USB Host Shield (Barcode Scanner)
- LCD 2004 20x4 with IIC/I2C/TWI Serial
- Discharge (MilliAmps and MillOhms)
- Serial configuration.

## Things to fix:
- Add ESP-01(Software Serial5)?
- Fix mini USB host shield (make auto patch)

## Serial configuration commands
Good commands will respond with "ACK"
If the command is not supported or incorrectly type a "NAK" will be seen.
Please allow some time for the command to thru. during testing without internet connectivity the response time is very high.

|Command| Description|
|-------|------------|
|reset  |This resets the eeprom settings to default settings. device must be restarted|
|milliamps=1000|   This command sets the value to tell the controller to not recharge is its to low of a mAh.|
|shunt=3.3|   This is used to set the measured shunt resistor resistance.|
|refv=4.95|        This is used to set the referenceVoltage as most devices are not 5v exactly.|
|cutv=2.8|  This is used to set the max dischage voltage. this will stop the battery from discharging to low.|
|timeout=1| The time in Minutes to rest the battery after charge. 0-59 are valid.|
|millohms=500| This is used to set the max battery resistance. if its higher it will mark the battery as bad.|
|ohmoffset=0| Offset calibration for MilliOhms(you should ever need to change this)|
|chrgtimeout=8| Battery charge timeout in hours. if it take over the allotted time it marks it as bad.|
|tempthreshold=7| Warning Threshold in degrees above initial Temperature(you shouldn't need to change this)|
|tempmax=10| Maximum Threshold in degrees above initial Temperature - Considered Faulty(you shouldn't need to change this)|
|server=submit.vortexit.co.nz| Server to connect - Add your server here|
|userhash=j08asd08d| Database Hash - this is unique per user - Get this from Charger / Discharger Menu -> View|
|cduintid=1| CDUnitID this is the Units ID - this is unique per user - Get this from Charger / Discharger Menu -> View -> Select your Charger / Discharger|
|ssid=yournetwork| Sets your networks ssid(name)|
|pass=yournetwork| pass Set your network password|
|read| This dumps the currently set eeprom values to the serial port.|



## Manual USB host system patch


On line 43 of UsbCore.h in .piolibdeps/USB-Host-Shield-Library-20_ID59/UsbCore.h
Change  P10 to P8
```
//typedef MAX3421e<P10, P9> MAX3421E; // Official Arduinos (UNO, Duemilanove, Mega, 2560, Leonardo, Due etc.), Intel Edison, Intel Galileo 2 or Teensy 2.0 and 3.x - Original

typedef MAX3421e<P8, P9> MAX3421E; // Official Arduinos (UNO, Duemilanove, Mega, 2560, Leonardo, Due etc.), Intel Edison, Intel Galileo 2 or Teensy 2.0 and 3.x
```
