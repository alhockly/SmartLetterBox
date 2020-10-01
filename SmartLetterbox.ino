#include <ESP8266WiFi.h>        // Library to connect to Wi-Fi
#include <ESP8266HTTPClient.h>  // Library to send http
#include <ESP8266WebServer.h>   // esp8266 Arduino webserver lib
#include <ESP8266mDNS.h>        // Include the mDNS library
#include <EEPROM.h>             // use the 512kb of non-volitile memory
#include <Wire.h>               // communicate with I2C devices.

// Replace with your network credentials
const char* ssid = "TrilliumKillinEm";
const char* password = "Despacito2";
const char* mDnsName = "Letterbox";

String ip;

String ifttt_url = "http://maker.ifttt.com/trigger/postbox/with/key/"; //KEY INTENTIONALLY LEFT OUT

String html_index_head = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Smart letterbox</title></head>";

ESP8266WebServer server(80);

int16_t gyroY_thresh = 1000;

const int gyroY_thresh_ADDR = 0x20;

const int MPU_ADDR = 0x68; // I2C address of the MPU-6050. If AD0 pin is set to HIGH, the I2C address will be 0x69.
int16_t accelerometer_x, accelerometer_y, accelerometer_z; // variables for accelerometer raw data
int16_t last_accelerometer_y;
int16_t gyro_y_diff;
int16_t gyro_x, gyro_y, gyro_z; // variables for gyro raw data
int16_t temperature; // variables for temperature data
char tmp_str[7]; // temporary variable used in convert function

char* convert_int16_to_str(int16_t i) { // converts int16 to string. Moreover, resulting strings will have the same length in the debug monitor.
  sprintf(tmp_str, "%6d", i);
  return tmp_str;
}

void setup() {
  Serial.begin(9600);
  EEPROM.begin(512);  //Initialize EEPROM
  
  Wire.begin();
  Wire.beginTransmission(MPU_ADDR); // Begins a transmission to the I2C slave (GY-521 board)
  Wire.write(0x6B); // PWR_MGMT_1 register
  Wire.write(0); // set to zero (wakes up the MPU-6050)
  Wire.endTransmission(true);
  readMPU();
  gyroY_thresh = eepromRead(gyroY_thresh_ADDR);
  
  wifiSetup();
  server.on("/", handleRoot);
  server.begin();
  ip = (String)WiFi.localIP();
  Serial.print("got closed value of "); Serial.println(accelerometer_y);
}

void loop() {
  readMPU();
  // print some data
  Serial.print(" | aY = ");
  Serial.print(convert_int16_to_str(accelerometer_y));
  Serial.print(" | gYdiff = "); 
  Serial.print(convert_int16_to_str(gyro_y_diff));
  Serial.println();

  //if device is moving more than threshold
  if(gyro_y_diff > gyroY_thresh){
    sendNotification();
  }
            
  MDNS.update();
  server.handleClient();
  delay(100);
}

void eepromWrite(int address, int16_t data){
  uint8_t xlow = data & 0xff;
  uint8_t xhigh = (data >> 8);
  
  EEPROM.write(address, xlow); 
  EEPROM.write(address + 0x01, xhigh);
  EEPROM.commit();
}

//each memory address is only 8 bits (max int is 255)
int16_t eepromRead(int address){
    //Reading a blank eeprom returns 51
    return EEPROM.read(address);
  }


template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
   const byte* p = (const byte*)(const void*)&value;
   int i(0);
   for (; i < sizeof(value); ++i)
       EEPROM.write(ee++, *p++);
   return i;
}


template <class T> int EEPROM_readAnything(int ee, T& value)
{
   byte* p = (byte*)(void*)&value;
   int i (0);
   for (; i < sizeof(value); ++i)
       *p++ = EEPROM.read(ee++);
   return i;
}

void sendNotification(){
    Serial.print("sending GET to ");
    Serial.println(ifttt_url);
    HTTPClient http;
    http.begin(ifttt_url);
    int httpResponseCode = http.GET();
    http.end();
    
    Serial.println("response was: " + (String) httpResponseCode);
}

void handleRoot() {

  if(server.argName(0) == "gyroY_thresh"){
    gyroY_thresh = server.arg(0).toInt();
    Serial.print("SETTING GYRO Y THRESH TO: ");
    Serial.println(gyroY_thresh);
    Serial.println();
    eepromWrite(gyroY_thresh_ADDR, gyroY_thresh);
  }
  
  String message = html_index_head;
  message += "<body>";
  message += "<body><h1>Letter box</h1>";
  message += "accelerometer Y: " + (String)convert_int16_to_str(accelerometer_y);
  message += "<br>";
  message += "gyro Y threshold:  " + (String)convert_int16_to_str(gyroY_thresh);
  message += "<br>";
  message += "gyro Y diff:  " + (String)convert_int16_to_str(gyro_y_diff);
  message += "<form><input name=\"gyroY_thresh\" type=\"text\"  />";
  message += "<button id=\"gyro_thresh_input\" type=\"submit\" value=\"Submit\">set gyro threshold</button></form>";
  message += "<br><br><br>";
  message += WiFi.macAddress();
  
  server.send(200, "text/html", message);
}



void readMPU(){
  last_accelerometer_y = accelerometer_y;
  gyro_y_diff = gyro_y;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B); // starting with register 0x3B (ACCEL_XOUT_H) [MPU-6000 and MPU-6050 Register Map and Descriptions Revision 4.2, p.40]
  Wire.endTransmission(false); // the parameter indicates that the Arduino will send a restart. As a result, the connection is kept active.
  Wire.requestFrom(MPU_ADDR, 7*2, true); // request a total of 7*2=14 registers
  
  // "Wire.read()<<8 | Wire.read();" means two registers are read and stored in the same variable
  accelerometer_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x3B (ACCEL_XOUT_H) and 0x3C (ACCEL_XOUT_L)
  accelerometer_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x3D (ACCEL_YOUT_H) and 0x3E (ACCEL_YOUT_L)
  accelerometer_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x3F (ACCEL_ZOUT_H) and 0x40 (ACCEL_ZOUT_L)
  temperature = Wire.read()<<8 | Wire.read(); // reading registers: 0x41 (TEMP_OUT_H) and 0x42 (TEMP_OUT_L)
  gyro_x = Wire.read()<<8 | Wire.read(); // reading registers: 0x43 (GYRO_XOUT_H) and 0x44 (GYRO_XOUT_L)
  gyro_y = Wire.read()<<8 | Wire.read(); // reading registers: 0x45 (GYRO_YOUT_H) and 0x46 (GYRO_YOUT_L)
  gyro_z = Wire.read()<<8 | Wire.read(); // reading registers: 0x47 (GYRO_ZOUT_H) and 0x48 (GYRO_ZOUT_L)

  accelerometer_y = map(accelerometer_y, -10000,10000,0,1000);

  accelerometer_y = (last_accelerometer_y + accelerometer_y)/2;
  gyro_y_diff = abs(gyro_y_diff - gyro_y);
 }

void wifiSetup(){
   // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  
  if (MDNS.begin(mDnsName)) {             // Start the mDNS responder for .local
    Serial.println("MDNS responder started");
  }
  MDNS.addService("http", "tcp", 80);
}
