//Library includes
#include <Arduino.h> //Arduino libraries
#include <Wire.h> //I2C communication
#include <WiFiS3.h> //ESP32 S3 WiFi
#include <EEPROM.h> //Arduino EEPROM
#include <Adafruit_GFX.h> //OLED screen
#include <Adafruit_SH110X.h> //OLED screen
#include <SensirionI2cScd4x.h> //CO2 sensor
#include <Adafruit_LPS35HW.h> //Pressure sensor
#include <Adafruit_LTR329_LTR303.h> //Light sensor
  
//Soil moisture sensor variables
#define SOIL_PIN A0 //Soil moisture sensor analogue input
#define Moist_PIN 13 //Digital high to provide VCC voltage to soil moisture sensor
int soilRaw = 0; //Sets initial value to 0
int soilMoisture = 0; //Sets initial value to 0
const int SOIL_DRY = 638; //Dry calibrated value
const int SOIL_WET = 17; //Wet calibrated value
  
//WiFi and credential variables
#define SSID_ADDR 0 //WiFi SSID (network name) memory address in EEPROM
#define PASS_ADDR 64 //WiFi password memory address in EEPROM
char ssidBuf[32]; //Sets the SSID buffer to 32 characters
char passBuf[64]; //Sets the password buffer to 64 characters
String ssid; //Sets a string variable for the SSID
String pass; //Sets a string variable for the password
bool forceSetup = false; //Sets a boolean variable for forced Wifi setup. Used for debug.
WiFiServer server(80); //Defines the WiFi mode, server using port 80
bool showIPAtBoot = true; //Debug function to show the IP on the OLED at boot. This helps if the DCHP has shifted the IP address.
unsigned long ipScreenStartTime = 0; //Sets the IP screen start time to 0. Must be unsigned long for millis() function.

//I2C Bus definition. ESP32 has two I2C busses. Wire1 uses the I2C connector on the Arduino
TwoWire &I2CBUS = Wire1; //Sets the I2C bus to Wire1

//Sensors
SensirionI2cScd4x scd4x; //Defines the SCD41 sensor using the sensor library
Adafruit_LPS35HW lps35hw; //Defines the LPS33 sensor using the sensor library
Adafruit_LTR329 ltr; //Defines the LTR-329 sensor using the sensor library
Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire1); //Defines the OLED screen using the library 64x128 resolution using Wire1

//Watering variables
#define levelswitchin 8
#define levelswitchmode 9 
unsigned long lastwater = 0;
const unsigned long waterdosing = 5UL*60UL*1000UL; //5min delay
bool wateringmode = false;

//Relay pin variables
#define RELAY1_PIN 4
#define RELAY2_PIN 5
#define RELAY3_PIN 6
#define RELAY4_PIN 7
bool relayPresent = true; //Boolean for relay debug
bool relaystate[4]= {false, false, false, false}; //Sets an array of bool values for the relay states

//Sensor data variables
uint16_t co2 = 0; //CO2 value
float temp = 0; //Temperature value
float hum = 0; //Humidity value
float pressure = 0; //Pressure value
uint16_t visible = 0; //Visible light value
uint16_t ir = 0; //Infrared light value

//Control variables
bool controlmode = true; //Control mode flag true is auto and false is manual
float temp_min = 18; //Temperature min
float temp_max = 25; //Temperature max
float temp_db = 1; //Deadband used to control hysteresis

float hum_min = 70; //Humidity min
float hum_max = 100; //Humidity max
float hum_db = 3; //Deadband used to control hysteresis

float moist_min = 75; //Soil moisture min
float moist_max = 90; //Soil moisture max
float moist_db = 3; //Deadband used to control hysteresis

bool waterlow = false; //Sets a water low flag


//EEPROM save function. Saves the WiFi credentials to EEPROM. Only works with Arduino
void saveCredentials(String s, String p) { //Takes the inputs as strings and assigns them to s and p 
  memset(ssidBuf, 0, sizeof(ssidBuf)); //Sets the memory size to the size of the SSID buffer
  memset(passBuf, 0, sizeof(passBuf)); //Sets the memory size to the size of the password buffer
  
  s.toCharArray(ssidBuf, sizeof(ssidBuf)); //Converts the values to a character array the size of the buffer
  p.toCharArray(passBuf, sizeof(passBuf)); //Converts the values to a character array the size of the buffer
  
  EEPROM.put(SSID_ADDR, ssidBuf); //Saves the SSID value to EEPROM
  EEPROM.put(PASS_ADDR, passBuf); //Saves the password to EEPROM
}

//EEPROM load function. Loads the WiFi credentials from the EEPROM.
bool loadCredentials() {//Boolean function for loading the credentials
  EEPROM.get(SSID_ADDR, ssidBuf); //Retreives the SSID
  EEPROM.get(PASS_ADDR, passBuf); //Retreives the password
  ssidBuf[31] = '\0'; //Sets termination bit to zero
  passBuf[63] = '\0'; //Sets termination bit to zero
  ssid = String(ssidBuf); //Sets the ssid variable to the ssidbuf string
  pass = String(passBuf); //Sets the pass variable to the passbuf string
  ssid.trim(); //Removes the null values from the string
  pass.trim(); //Removes the null values from the string
  if (ssid.length()<2||pass.length()<2) return false; //Returns false if the ssid or password are less than 2 bits
  return true; //Returns true if details are loaded from the EEPROM
}

//Serial input
String serialline(const char* output) { //Creates a character pointer to output value
  Serial.println(output); //Prints output value
  String value = ""; //Defines value as a string and sets to blank
  while (true) { //While loop
    while (Serial.available()) { //While serial is availible
      char input = Serial.read(); //Defines input as a character and sets to output of serial read
      if (input =='\n'||input =='\r') { //If new line (enter is pressed)
        if (value.length() > 0) { //If the length of value is greater than 0
          Serial.print("."); //Prints a .
          Serial.println(value); //Prints value
          return value; //Returns value of value
        }
      } else {
        value += input; //Adds value to the input
      }
    }
    delay(10); //10 ms delay
  }
}

//WiFi setup function
void askForWiFi() { //Defines function
  Serial.println("\n WiFi Setup");//Prints WiFi setup to the serial
  ssid = serialline("Enter WiFi SSID:"); //Runs the serial line function with "Enter WiFi SSID:" prompt
  pass = serialline("Enter WiFi Password:"); //Runs the serial line function with "Enter WiFi Password:" prompt
  saveCredentials(ssid, pass); //Runs the saveCredentials function with ssid and pass variables
  Serial.println("WiFi saved!"); //Prints WiFi saved to the serial
}

//WiFi connection function using router DCHP (Dynamic Host Connection Protocol)
void connectWiFi() { //Defines the function
  Serial.print("Connecting to: "); //Prints to the serial
  Serial.println(ssid); //Prints the ssid 
  
  WiFi.begin(ssid.c_str(), pass.c_str()); //WiFi begin function from WiFi S3 to connect using ssid and pass
  
  int attempts = 0; //Debug function defines attempts as an integer
  
  while (WiFi.status() != WL_CONNECTED && attempts < 10) { //While WiFi status is not connected and attempts is lees than 10
    delay(500); //Half second delay
    Serial.print("."); //Prints "." to show the function is working
    attempts++; //Increments attempts by one
  }

  Serial.println(); //Prints a line
  
  if (WiFi.status() != WL_CONNECTED) { //If WiFi not connected after 10 attempts
    Serial.println("WiFi Connection Failed"); //Prints WiFi Connection failed
    forceSetup = true; //Debug flag to force WiFi credential input 
    return;
  }
  
  if (forceSetup != false) { //If debug function activated
    askForWiFi(); //Asks for WiFi credentials
    WiFi.begin(ssid.c_str(), pass.c_str()); //WiFi begin function from WiFi S3 to connect using ssid and pass
    forceSetup = false; //Resets debug flag to false
    return;
  }
  
  Serial.println("Waiting for DHCP..."); //Prints "Waiting for DHCP..."
  
  IPAddress IP; //Defines IP as the IP address from WiFi S3 Library
  int DCHPattempts = 0; //Debug function defines DCHPattempts as an integer
  
  while (DCHPattempts < 10) { //While the attempts are less than 10
    IP = WiFi.localIP(); //Sets IP variable to the Local IP assigned by the router
    if (IP[0] != 0 || IP[1] != 0 || IP[2] != 0 || IP[3] != 0) { //If any of the first four elements of the IP address are not 0
      break; //Breaks the while loop
    }
    delay(250); //Quarter second delay
    DCHPattempts++; //Increments the DCHPattempts by one
  }
  
  Serial.print("IP: "); //Prints the IP address to the serial
  
  if (IP[0] == 0 && IP[1] == 0 && IP[2] == 0 && IP[3] == 0) { //If first four elements of the IP are 0
    Serial.println("DHCP Failed"); //Print "DCHP Failed"
  } else { 
    Serial.println(IP); //Prints the IP address
  }
  server.begin();
}

//JSON Definitions for sensor data to be sent to react page
String json() { //Defines function json as a string
  String data = "{"; //Defines data as a string and sets the required JSON delimiter for an array 
  data += "\"co2\":" + String(co2) + ","; //Adds the CO2 data to data string, setting the next variable using ","
  data += "\"temp\":" + String(temp) + ","; //Adds the temperature data to data string, setting the next variable using ","
  data += "\"hum\":" + String(hum) + ","; //Adds the humidity data to data string, setting the next variable using ","
  data += "\"pressure\":" + String(pressure) + ","; //Adds the pressure data to data string, setting the next variable using ","
  data += "\"visible\":" + String(visible) + ","; //Adds the visible data to data string, setting the next variable using ","
  data += "\"ir\":" + String(ir) + ","; //Adds the infrared data to data string, setting the next variable using ","
  data += "\"soil\":" + String(soilMoisture)+ ","; //Adds the soil moisture data to data string, setting the next variable using ","
  data += "\"controlmode\":"; //Sends the control mode state
  data += controlmode ? "true" : "false"; //Sends the bool state for control mode
  data += ",\"relays\":["; //Relay state date for each relay
  data += relaystate[0] ? "true" : "false";
  data += ",";
  data += relaystate[1] ? "true" : "false";
  data += ",";
  data += relaystate[2] ? "true" : "false";
  data += ",";
  data += relaystate[3] ? "true" : "false";
  data += "]";
  data += ",";
  data += "\"waterlow\":"; //Sends the control mode state
  data += waterlow ? "true" : "false";
  data += "}"; //Sets the JSON delimiter for the array end
  return data; //Returns the whole data array string
}

//Relay activation function
void relayOn(int channel) { //Takes the input value and assigns to channel
  switch (channel) { //Switch statement using the channel value
    case 1: digitalWrite(RELAY1_PIN, LOW); 
    relaystate[0] = true; //Sets the relay state flag for JSON
    break;
    case 2: digitalWrite(RELAY2_PIN, LOW);
    relaystate[1] = true; //Sets the relay state flag for JSON
    break;
    case 3: digitalWrite(RELAY3_PIN, LOW);
    relaystate[2] = true; //Sets the relay state flag for JSON
    break;
    case 4: digitalWrite(RELAY4_PIN, LOW); 
    relaystate[3] = true; //Sets the relay state flag for JSON
    break;
  }
}

//Relay off function
void relayOff(int channel) { //Takes the input value and assigns to channel
  switch (channel) { //Switch statement using the channel value
    case 1: digitalWrite(RELAY1_PIN, HIGH); 
    relaystate[0] = false; //Sets the relay state flag for JSON
    break;
    case 2: digitalWrite(RELAY2_PIN, HIGH); 
    relaystate[1] = false; //Sets the relay state flag for JSON
    break;
    case 3: digitalWrite(RELAY3_PIN, HIGH); 
    relaystate[2] = false; //Sets the relay state flag for JSON
    break;
    case 4: digitalWrite(RELAY4_PIN, HIGH); 
    relaystate[3] = false; //Sets the relay state flag for JSON
    break;
  }
}

//Automatic stepoint control based on parameters
void autocontrol (float t, uint8_t m, uint16_t v, uint8_t h, bool ls) {
  if (t > (temp_max - temp_db) | m > (moist_max)){ //Checks the max limit minus the deadband
    relayOn(4); //Activates the relay
  } else if (t < (temp_min + temp_db)){ //Checks the lower limt plus the deadband
    relayOff(4); //Turns off the relay
  }

  if (h > (hum_max - hum_db)){ //Checks the max limit minus the deadband
    relayOn(4); //Activates the relay
  } else if (h < (hum_min + hum_db)){ //Checks the lower limt plus the deadband
    relayOff(4); //Turns off the relay
  }

  if (m < (moist_min + moist_db) && ls != true){ //Checks the soil moistrue is above the min and level switch is not active
    unsigned long currentTime = millis();
    
    if (!wateringmode || (currentTime - lastwater >= waterdosing)){
    relayOn(3); //Activates the relay
    delay (2000); //Small delay
    relayOff(3); //Deactivates the relay to stop water from overflowing
    lastwater = millis(); //Sets last water to the elapsed time
    wateringmode = true; //Sets watering mode flag to true
    } else {
    relayOff(3); //Else turns off the pump
    }
  }
  if (ls == true){ //Checks the level switch flag
    relayOff(3); //If low level active turn off the pump
  }

  if (v > 100){ //Checks the visible light level
    relayOn(2); //If low level active turn on the light
  } else{
    relayOff(2);
  }
}

//Function to update the sensor values
void updateSensors() { //Function definition
  bool ready = false; //Sets ready flag for SCD41 using library 

  scd4x.getDataReadyStatus(ready); //Takes ready flag for SCD41 using library and returns true if sensor is ready
  if (ready) { //If ready is returned true
    scd4x.readMeasurement(co2, temp, hum); //Read values from the SCD41 using library and return to variables 
  }
  
  pressure = lps35hw.readPressure(); //Reads pressure value using library
    
  if (ltr.newDataAvailable()) { //If data is availible using library
    ltr.readBothChannels(visible, ir); //Read both visible and ir values using library 
  }

  digitalWrite(Moist_PIN, HIGH); //Sets the soil moisture VCC pin high
  soilRaw = analogRead(SOIL_PIN); //Analog read the soil moisture using the Arduino library
  soilMoisture = map(soilRaw, SOIL_DRY, SOIL_WET, 0, 100); //Takes the raw value and maps it to 0-100% with the calibration values using the Arduino library
  soilMoisture = constrain(soilMoisture, 0, 100); //Constrains the value to the 0-100% range using the Arduino library
  
  digitalWrite (levelswitchmode, LOW);
  waterlow = digitalRead(levelswitchin);

  if (controlmode == true) { //Checks control mode flag
    autocontrol(temp, soilMoisture, visible, hum, waterlow); //Automatic control function
  }
}

//General setup function
void setup() { //Setup function definition
  Serial.begin(115200); //Serial begin using 115200 BAUD rate
  delay(2000); //2 second delay
  Wire1.begin(); //Wire1 I2C bus begin function
  Wire1.setClock(100000); //Sets I2C clock value to 100000
  
  //OLED Initialisation
  if (!display.begin(0x3C, true)) { //If the display fails to begin
    Serial.println("OLED Fail"); //Print "OLED Fail"
    } else {
      Serial.println("OLED Ok"); //Else Print "OLED Ok"
  }

  display.setRotation(1); //Sets roation value to 1 using the Adafruit library
  display.clearDisplay(); //Clears display using the Adafruit library
  display.setTextSize(1); //Sets text size value to 1 using the Adafruit library
  display.setTextColor(SH110X_WHITE); //Sets text colour to white using the Adafruit library
  display.setCursor(0, 0); //Sets cursor position to 0,0 using the Adafruit library
  display.println("Booting..."); //Prints "Booting..." while the program intitialises
  display.display(); //Displays "Booting..."
  Serial.println("Booting..."); //Prints "Booting..." to serial

  //WiFi setup
  bool valid = loadCredentials(); //Checks credentials flag
  if (!valid) { //If the load credentials boolean returns false ask for credentials
    askForWiFi(); //Calls askForWiFI function
  }
  
  connectWiFi(); //WiFi connection function using Arduino library
  ipScreenStartTime = millis(); //Sets the ipScreenStartTime to the output from millis using the Arduino library

  //SCD41 initialisation using the library
  scd4x.begin(Wire1, SCD41_I2C_ADDR_62); //Begins measuring using Wire1 and the I2C address
  scd4x.wakeUp(); //Wakes up the sensor from idle mode
  scd4x.stopPeriodicMeasurement(); //Stops periodic measurement (idle mode)
  scd4x.reinit(); //Reinitialises the sensor loading from EEPROM, must be idle to use
  scd4x.startPeriodicMeasurement(); //Starts measurement 

  //Pressure sensor initialisation using the library
  lps35hw.begin_I2C(0x5D, &Wire1); //Begins measuring using Wire1 and the I2C address
  
  //Light sensor initialisation using the library
  ltr.begin(&Wire1); //Begins measuring using Wire1
  ltr.setGain(LTR3XX_GAIN_2); //Sets gain value
  ltr.setIntegrationTime(LTR3XX_INTEGTIME_100); //Sets integral value to 100 (PID)
  ltr.setMeasurementRate(LTR3XX_MEASRATE_200); //Sets measurement rate to 200
  
  //Relays set to output
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);
  pinMode(RELAY4_PIN, OUTPUT);
  //Sets relay pins High, XC4441 is active low
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(RELAY3_PIN, HIGH);
  digitalWrite(RELAY4_PIN, HIGH);
  
  pinMode(levelswitchin, INPUT); //Sets the level switch to input
  pinMode(levelswitchmode, OUTPUT); //Sets the level switch to input
  digitalWrite (levelswitchmode, LOW);
  Serial.println("System ready"); //Prints "System ready" when function is complete
}

//Function to display the network IP address on boot. Using DCHP, sometimes the IP is different. The IP is used for the react page.
void IPboot() { //Defines the function
  display.clearDisplay(); //Clears the display
  display.setCursor(0, 0); //Sets cursor at 0,0
  display.println("Network Info"); //Displays "Network Info" on the OLED
  display.print("SSID:"); //Prints "SSID:" on the screen
  display.println(ssid); //Prints the ssid value
  display.println(); //Prints a blank line
  display.print("IP:"); //Prints "IP:"
  display.println(WiFi.localIP()); //Prints the local IP address using the WiFi S3 library
  display.display(); //Prints a blank line
}

float fetchparam(String req, String param, float val) { //Function to fetch the setpoint values when in manual mode
  uint8_t start = req.indexOf(param + "="); //Sets start to the req as an index of param
  if (start == -1){
    return val; //Returns the value
  }
  start += param.length()+1; //Increments to the start of the value
  uint8_t end = req.indexOf("&", start); //Finds the next & value in start
  if (end == -1){
    end = req.indexOf(" ", start); //Finds the next space in start
  }
  String value = req.substring(start, end); //Sets the value to the extracted data
  
  return value.toFloat(); //Returns the value to a floating point number

}

//OLED update function used to render new sensor values
void updateOLED() { //Function definition
  display.clearDisplay(); //Clears display
  display.setCursor(0, 0); //Sets cursor position to 0,0
  display.println("Environment Condition"); //Prints "Environment Condition"
  
  //CO2
  display.print("CO2: ");
  display.print(co2);
  display.println(" ppm");

  //Temperature
  display.print("Temp: ");
  display.print(temp, 1);
  display.println(" C");

  //Humidity
  display.print("Humid: ");
  display.print(hum, 1);
  display.println(" %");

  //Pressure
  display.print("Press: ");
  display.print(pressure, 1);
  display.println(" hPa");

  //Soil Moisture
  display.print("Soil Moisture: ");
  display.print(soilMoisture);
  display.println(" %");

  //Light
  display.print("Vis/IR: ");
  display.print(visible);
  display.print("/");
  display.println(ir);
  display.display();
}

//Program loop function
void loop() { //Function definition
  if (showIPAtBoot) { //Check the showIPAtBoot flag is true
    IPboot(); //If true, call IPboot function
    if (millis() - ipScreenStartTime >= 15000) { //Displays the IPboot function for 15 seconds
      showIPAtBoot = false; //Sets the flag to false
    }
  } else { //If the flag is flase then
  updateSensors(); //Update sensor values
  updateOLED(); //Update the screen display
  //Serial.println(digitalRead(levelswitchin)); //debug
  }

  WiFiClient client = server.available(); //Enables wifi server connection for transfer of JSON data to the react page. Non-blocking function

  //HMI control functions 
  if (client) { //If client is availible
    String req = client.readStringUntil('\n'); //String is read until new line
    while (client.available()) { //Removes headers and blank lines
      client.read();
    }
    
    if (req.indexOf("GET /mode") >= 0) { //Searches for /mode in the request
      if (req.indexOf("mode=auto")>= 0) { //If auto mode is selected
        controlmode = true; //Turns on auto mode
        } else { //Turn off if false
          controlmode = false; //Turns on manual mode
        }
      
      client.println("HTTP/1.1 200 OK"); //Responds OK to the webpage server, acknowledging the command
      client.println("Access-Control-Allow-Origin: *"); //Allows the API call from the react webpage
      client.println("Content-Type: text/plain"); //Sets the content type to plain text for the message
      client.println();
      client.println("OK");
      client.stop();
      return;
    }

    if (req.indexOf("GET /setpoints") >= 0) { //Searches for /setpoints in the request
      temp_min = fetchparam(req, "temp_min", temp_min); //Fetches the min temperature value
      temp_max = fetchparam(req, "temp_max", temp_max); //Fetches the max temperature value
      temp_db = fetchparam(req, "temp_db", temp_db); //Fetches the temperature deadband value

      hum_min= fetchparam(req, "hum_min", hum_min); //Fetches the min humidity value
      hum_max = fetchparam(req, "hum_max", hum_max); //Fetches the max humidity value
      hum_db = fetchparam(req, "hum_db", hum_db); //Fetches the humidity deadband value

      moist_min= fetchparam(req, "moist_min", moist_min); //Fetches the min soil moisture value
      moist_max = fetchparam(req, "moist_max", moist_max); //Fetches the max soil moisture value
      moist_db = fetchparam(req, "moist_db", moist_db); //Fetches the temperature deadband value
      
      client.println("HTTP/1.1 200 OK"); //Responds OK to the webpage server, acknowledging the command
      client.println("Access-Control-Allow-Origin: *"); //Allows the API call from the react webpage
      client.println("Content-Type: text/plain"); //Sets the content type to plain text for the message
      client.println();
      client.println("OK");
      client.stop();
      return;
    }
    
    if (req.indexOf("GET /relay") >= 0) { //Searches for /relay in the request
      
      if (controlmode == true){
        client.println("HTTP/1.1 404 Page not found");
        client.stop();
        return;
      } 

      int ch = req.substring(req.indexOf("ch=") + 3).toInt(); //Extracts the channel value "ch"
      bool turnOn = req.indexOf("state=on") >= 0; //Extracts the boolean value for state 
      if (ch >= 1 && ch <= 4) { //If the value is between 1 and 4
        if (turnOn) { //Turn on if true
          relayOn(ch); //Turns on using the ch value
        } else { //Turn off if false
          relayOff(ch); //Turns off using the ch value
        }
      }
      client.println("HTTP/1.1 200 OK"); //Responds OK to the webpage server, acknowledging the command
      client.println("Access-Control-Allow-Origin: *"); //Allows the API call from the react webpage
      client.println();
      client.println("OK");
      client.stop();
      return;
    }
    client.println("HTTP/1.1 200 OK"); //Responds OK to the webpage server, acknowledging the command
    client.println("Content-Type: application/json"); //Sets the content type for the message to JSON
    client.println("Access-Control-Allow-Origin: *"); //Allows the API call from the react webpage
    client.println();
    client.println();
    client.print(json()); //Sends the sensor data to the server
    client.stop();
  }
  delay(50); //Small delay to prevent network stalls
}