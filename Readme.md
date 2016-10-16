# EspFeeder
### A web server and esp8266 based automated pet feeder controller.




## Requirements

### Hardware

* ESP-xx with 4 gpio pins available.  Plus an additional one if you'd like status sounds. (ESP-03, ESP-07, ESP-12x, nodemcu) (probably others)
* 2 SPST push buttons
* 2 servos
* A Pet Feeder, something like this: https://www.amazon.com/gp/product/B01IPF3N6Y

### Software
* Arduino-1.6.11
* ESP8266/Arduino :Additional Boards Manager URL: http://arduino.esp8266.com/stable/package_esp8266com_index.json
* ESP8266FS plugin, installed in tools https://github.com/esp8266/arduino-esp8266fs-plugin/releases/download/0.2.0/ESP8266FS-0.2.0.zip
* Bounce2 Library, installed in library https://github.com/thomasfredericks/Bounce2/releases/tag/V2.21
* ArduinoJson Library, install in libarry https://github.com/bblanchon/ArduinoJson/releases/tag/v5.6.7
    * (the libraries can be installed with the library manager instead)


## General Instructions

### Hardware

Wireup according to the picture.  You can change which pin you use for each function near the top of the
scetch where it refers to BUTTON1, BUTTON2, SERVO1, SERVO2 and TONE.  I use an ESP-12, so the code
is showing the pins I chose for ti.

Arrange the servos and the buttons on your pet feeder as shown, or differently if you're using some other
kind of feeder.

### Software
Don't forget to restart the Arduino IDE after installing the libraries and boards.

Set your esp settings.. the board, program method, flash size and spiffs size.

This uses the SPIFFS file system.  So we need to load that in your esp-xx first.
Upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)

Then compile and upload the .ino.
