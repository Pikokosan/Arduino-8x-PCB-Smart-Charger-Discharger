# Functions to add

- add setup function for [ds12b20's](#11-ds12b20)
- Possible change current measurement method. [see bellow](#12-discharge-mosfet)





## 1.1 ds12b20

Function should run at the begining if the eeprom's first setup bit is not set.
steps:

1. allow sensors to settle.
* check to make sure 9 sensors are DETECTED
* check current temps of all sensors and hold thouse values.
* ask user to place there finger on battery 1 sensor.
* pause to allow sensor to warm up.
* recheck all sensors. if 1 sensor is higher then assign it's address to bat1Temp.
* tell user that the device was saved and there finger should be removed.
* mark address as used so we dont use it again.
* repeat steps 4-8 with remaining banks.
* assume last address is the ambient sensor.
* store settings and address on the eeprom



## 1.2 Discharge mosfet

The circuit would be set as a pwm at 60 kHz more info to follow.
This is based on the way the zanflare c4 works.

![dischage mosfet idea](images/discharge.png)
