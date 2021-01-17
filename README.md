# TCodeESP32
This is an ESP32 fork of Tempests TCode sketch (repository doesnt exists yet). It adds Network capabilities and bluetooth.

## INSTALLATION:
  ### Binaries
  See the pdf in the latest release zip https://github.com/jcfain/TCodeESP32/releases
  ###S ource
  First You need to install the dev environment for the ESP32 and arduino.
  A good place to start https://github.com/espressif/arduino-esp32#installation-instructions
  #### LIBRARIES 
    You will need a few additional libraries to build this sketch.
    - ESP32Servo
    - ESP Async Webserver
    - EAsyncTCP
    - ArduinoJson
    
  #### TOOLS
    I used Visual studio code and PlatformIO for this project.
    I believe the sketch could be built in the Arduino IDE but I have not tested.
    
  #### FILE SYSTEM
    Make sure modify /data/userSettings.json with your wifi ssid and password.
    Upload the filesystem image and monitor the COM port to se the ip address.
    
    If you only want bluetooth you should be able to just pair with the board 
    and connect to the COM port created.
