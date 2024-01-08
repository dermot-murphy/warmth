#include <ThingSpeak.h>
#include <ArduinoHttpClient.h>
#include <b64.h>
#include <HttpClient.h>
#include <URLEncoder.h>
#include <WebSocketClient.h>

#include <EasyNTPClient.h>
#include <TimeLib.h>
#include <ArduinoMqttClient.h>
#include <MqttClient.h>
//#include <ArduinoBLE.h>
#include <rgb_lcd.h>
#include <math.h>
#include <SimpleTimer.h>
#include <WiFiNINA.h>

/* Hardware Configuration ---------------------------------------------------*/

int ledPin = LED_BUILTIN;

/* Serial Configuration -----------------------------------------------------*/

int SERIAL_BAUD_RATE = 19200;

/* LCD Display Configuration ------------------------------------------------*/

const int colorR_not_connected = 0;
const int colorG_not_connected = 128;
const int colorB_not_connected = 128;

const int colorR_ok = 0;
const int colorG_ok = 128;
const int colorB_ok = 0;

const int colorR_hot = 128;
const int colorG_hot = 0;
const int colorB_hot = 0;

const int colorR_cold = 0;
const int colorG_cold = 0;
const int colorB_cold = 128;

rgb_lcd lcd; 

/* Temperature Sensor -------------------------------------------------------*/

const int B = 4275;               // B value of the thermistor
const int R0 = 100000;            // R0 = 100k
const int pinTempSensor = A0;     // Grove - Temperature Sensor connect to A0

/* Temperature Thresholds ---------------------------------------------------*/

const int TEMPERATURE_HIGH = 25.0 ;
const int TEMPERATURE_LOW = 24.0 ;

/* Temperature State --------------------------------------------------------*/

typedef enum {

  TEMPERATURE_STATE_OK,
  TEMPERATURE_STATE_TOO_HOT,
  TEMPERATURE_STATE_TOO_COLD

} temperature_state_t ;

temperature_state_t temperature_state;
float temperature ;
char temperature_time [9] ;

/* WiFi ---------------------------------------------------------------------*/

const char WIFI_SSID[] = "BT-QPAF2N";
const char WIFI_PASS[] = "pqbMycYM637eRb";

int wifi_status = WL_IDLE_STATUS;

/* Web Server ---------------------------------------------------------------*/

const int WEB_SERVER_PORT=80 ;

WiFiServer web_server(WEB_SERVER_PORT);
WiFiClient web_client = web_server.available();

/* NTP ----------------------------------------------------------------------*/

const int NTP_INTERVAL = 60 ;
const int NTP_PORT= 123;

WiFiUDP ntp_udp;
EasyNTPClient ntp_client(ntp_udp, "pool.ntp.org");

/* MQTT ---------------------------------------------------------------------*/

const bool MQTT_PUBLISH_ENABLE = true ;
const char MQTT_BROKER[] = "test.mosquitto.org";
const int  MQTT_PORT     = 1883;
const char MQTT_TOPIC[]  = "warmth-checker";

WiFiClient mqtt_wifi_client;
MqttClient mqtt_client(mqtt_wifi_client);

/* HTTP ---------------------------------------------------------------------*/
/*
const bool HTTP_POST_ENABLE = false ;
char HTTP_SERVER_ADDRESS[] = "192.168.0.3"; 
int HTTP_SERVER_PORT = 8080;

WiFiClient http_wifi;
HttpClient http_client = HttpClient(http_wifi, HTTP_SERVER_ADDRESS, HTTP_SERVER_PORT);
int http_status = WL_IDLE_STATUS;
*/
/* ThingSpeak ---------------------------------------------------------------*/

const bool THINGSPEAK_POST_ENABLE = true ;
const unsigned long THINGSPEAK_CHANNEL_ID = 2396838;
const char * THINGSPEAK_WRITEAPIKEY = "K4Z6QTJXCYYG9IM2";

WiFiClient  thingspeak_client;
SimpleTimer thingspeak_timer ;

/* Timers -------------------------------------------------------------------*/

SimpleTimer sample_timer;

/* Hardware State -----------------------------------------------------------*/

/* Setup --------------------------------------------------------------------*/

void setup() {

  // Initiaise the serial port
  Serial.begin(SERIAL_BAUD_RATE);

  // Initiaise the MPU hardware
  pinMode(ledPin, OUTPUT);

  // Wait until the serial port is ready
  while (!Serial);
  
  // Announcement
  Serial.println("WiFi Demo");
  Serial.println("V1.00.00 12/12/2023");

  // LCD Screen
  enable_LCD() ;

  // Enable WiFI
  enable_WiFi();

  // Enable temperature sampling
  samplingStart();

  // Connect to a network
  connect_to_WiFi();

  // NTP
  enable_NTP() ;

  // Time
  enable_Time();

  // Enable MQTT
  enable_MQTT() ;

  // Enable ThingSpeak
  enable_ThingSpeak() ;

  // Start the web server
  enable_WebServer() ;
  printWifiStatus();
}

/* Main loop ----------------------------------------------------------------*/

void loop() {

  web_client = web_server.available();

  if (web_client) {
      printWebPage();
  }

  if (sample_timer.isReady()) {
    sample_timer.reset();
    samplingTask() ;
    if (MQTT_PUBLISH_ENABLE) {
      publish_MQTT() ;
    }
  }
  if (thingspeak_timer.isReady()) {
    thingspeak_timer.reset();
    if (THINGSPEAK_POST_ENABLE) {
      post_ThingSpeak() ;
    }
  }
}

/* Temperature Sampling -----------------------------------------------------*/

void samplingStart() {
  sample_timer.setInterval(5000) ;
}

void samplingTask() {

  int adc_value ;
  float temperature_resistance ;
  char buff[16];
  int val_int;
  int val_fra;
  char text[10];

  print_Time() ;
  Serial.print("Sampling temperature: ");

  store_Time() ;
  adc_value = analogRead(pinTempSensor);

  temperature_resistance = 1023.0/adc_value-1.0;
  temperature_resistance = R0*temperature_resistance;

  temperature = 1.0/(log(temperature_resistance/R0)/B+1/298.15)-273.15; // convert to temperature via datasheet

  // Print a message to the LCD on the second line.
  val_int = (int) temperature;   // compute the integer part of the float 
  val_fra = (int) ((temperature - (float)val_int) * 100);   // compute 1 decimal places (and convert it to int)
  snprintf (buff, sizeof(buff), "%d.%d %cC       ", val_int, val_fra, 0xDF); 
  lcd.setCursor(0, 1);
  lcd.print(buff) ;

  if (temperature > TEMPERATURE_HIGH) {
    temperature_state = TEMPERATURE_STATE_TOO_HOT ;
  } else if (temperature < TEMPERATURE_LOW) {
    temperature_state = TEMPERATURE_STATE_TOO_COLD ;
  } else {
    temperature_state = TEMPERATURE_STATE_OK;
  }

  switch (temperature_state) {
    case TEMPERATURE_STATE_OK:
      lcd.setRGB(colorR_ok, colorG_ok, colorB_ok);
      strcpy(text, " OK  ");
      break ;
    case TEMPERATURE_STATE_TOO_HOT:
      lcd.setRGB(colorR_hot, colorG_hot, colorB_hot);
      strcpy(text, " HOT ");
      break ;
    case TEMPERATURE_STATE_TOO_COLD:
      lcd.setRGB(colorR_cold, colorG_cold, colorB_cold);
      strcpy(text, " COLD");
      break ;
    default:
      break ;
  }

  lcd.setCursor(10, 1);
  lcd.print(text) ;

  Serial.print(temperature);
  Serial.println(text) ;
}

/* LCD Screen ---------------------------------------------------------------*/

void enable_LCD() {
  // set up the LCD's number of columns and rows
  lcd.begin(16, 2);

  lcd.setRGB(colorR_not_connected, colorG_not_connected, colorB_not_connected);

  // Print a message to the LCD on the first line.
  lcd.setCursor(0, 0);
  lcd.print("Warmth Checker");

  // Print a message to the LCD on the second line.
  lcd.setCursor(0, 1);
  lcd.print("Not connected");
}

/* WiFi ---------------------------------------------------------------------*/

void printWifiStatus() {
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");

  // Print a message to the LCD on the second line
  lcd.setRGB(colorR_ok, colorG_ok, colorB_ok);
  lcd.setCursor(0, 1);
  lcd.print("Connected     ");

  Serial.print("Web server started at http://");
  Serial.println(ip);
}

void enable_WiFi() {
  Serial.println("Starting WiFi module");
  // check for the WiFi module
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < "1.0.0") {
    Serial.println("Please upgrade the firmware");
  } else {
    Serial.println("WiFi module started");
  }
}

void connect_to_WiFi() {
  // attempt to connect to Wifi network
  while (wifi_status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WiFI network: ");
    Serial.println(WIFI_SSID);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network
    wifi_status = WiFi.begin(WIFI_SSID, WIFI_PASS);

    // wait 10 seconds for connection
    delay(10000);
  }
  Serial.println("Connected to WiFi network");
}

/* MQTT ---------------------------------------------------------------------*/

void enable_MQTT() {

  Serial.print("Attempting to connect to the MQTT broker: ");
  Serial.println(MQTT_BROKER);

  if (!mqtt_client.connect(MQTT_BROKER, MQTT_PORT)) {
      Serial.print("MQTT connection failed! Error code = ");
      Serial.println(mqtt_client.connectError());
  }

  Serial.println("Connected to the MQTT broker");
  Serial.println();

}

void publish_MQTT() {
    // Publish message
    Serial.print("Pubishing to the MQTT broker: ");
    Serial.print("Topic ") ;
    Serial.print(MQTT_TOPIC);
    Serial.print("Value ") ;
    Serial.println(temperature);
    mqtt_client.beginMessage(MQTT_TOPIC);
    mqtt_client.print(temperature_time);
    mqtt_client.print(" ");
    mqtt_client.print(temperature);
    mqtt_client.endMessage();
}

/* Time ---------------------------------------------------------------------*/

void enable_Time(){
  setSyncProvider(getNtpTime);
  setSyncInterval(0);
  Serial.println("Getting time") ;
  while (timeSet != timeStatus()) {
    Serial.print(".");
    delay(5) ;
  }
  setSyncInterval(5);
  Serial.println();
  Serial.print("Time set: ");
  print_Time() ;
  setSyncInterval(NTP_INTERVAL);
}

void print_Time() {
  char text[5];
  sprintf(text, "%02d:", hour());
  Serial.print(text);

  sprintf(text, "%02d:", minute());
  Serial.print(text);

  sprintf(text, "%02d ", second());
  Serial.print(text);
}

void store_Time() {
  sprintf(temperature_time, "%02d:%02d:%02d", hour(),  minute(), second());
}

/* NTP ---------------------------------------------------------------------*/

void enable_NTP(){
  Serial.println("\nConnecting to NTP server...");
  ntp_udp.begin(NTP_PORT);
}

unsigned long getNtpTime()
{
  Serial.println("Getting time from NTP server ");
  return ntp_client.getUnixTime();
}

/* Web Server ---------------------------------------------------------------*/

void enable_WebServer() {
  web_server.begin();
}

void printWebPage() {

  if (web_client) {                             // if you get a client,
    Serial.println("new client");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (web_client.connected()) {            // loop while the client's connected
      if (web_client.available()) {             // if there's bytes to read from the client,
        char c = web_client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response
          if (currentLine.length() == 0) {

            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line
            web_client.println("HTTP/1.1 200 OK");
            web_client.println("Content-type:text/html");
            web_client.println();
           
            //create the buttons
            web_client.print("Click <a href=\"/H\">here</a> turn the LED on<br>");
            web_client.print("Click <a href=\"/L\">here</a> turn the LED off<br><br>");
            
            int randomReading = analogRead(A1);
            web_client.print("Temperature: ");
            web_client.print(temperature);
           
            // The HTTP response ends with another blank line
            web_client.println();
            // break out of the while loop
            break;
          }
          else {      // if you got a newline, then clear currentLine
            currentLine = "";
          }
        }
        else if (c != '\r') {    // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        if (currentLine.endsWith("GET /H")) {
          digitalWrite(ledPin, HIGH);        
        }
        if (currentLine.endsWith("GET /L")) {
          digitalWrite(ledPin, LOW);       
        }

      }
    }
    // close the connection
    web_client.stop();
    Serial.println("client disconnected");
  }
}

/* ThingSpeak ---------------------------------------------------------------*/

void enable_ThingSpeak() {

 thingspeak_timer.setInterval(60000) ;
 ThingSpeak.begin(thingspeak_client);

}

void post_ThingSpeak() {

  Serial.println("Updating ThingSpeak Channel ...");

  ThingSpeak.setField(1, temperature_time);
  ThingSpeak.setField(2, temperature);
  ThingSpeak.setField(3, temperature_state);

  // set the status
 // ThingSpeak.setStatus(myStatus);

  int x = ThingSpeak.writeFields(THINGSPEAK_CHANNEL_ID, THINGSPEAK_WRITEAPIKEY);

  if(x == 200){
    Serial.println("ThingSpeak Channel update successful.");
  }
  else{
    Serial.println("ThinkSpeak Problem updating channel. HTTP error code " + String(x));
  }

}

/* Serial -------------------------------------------------------------------*/

void SerialTask() {

  // if there's any serial available, read it
  while (Serial.available() > 0) {

    // look for the next valid integer in the incoming serial stream
    int red = Serial.parseInt();
    // do it again:
    int green = Serial.parseInt();
    // do it again:
    int blue = Serial.parseInt();

    // look for the newline. That's the end of your sentence
    if (Serial.read() == '\n') {
      // constrain the values to 0 - 255 and invert
      // if you're using a common-cathode LED, just use "constrain(color, 0, 255);"
      red = 255 - constrain(red, 0, 255);
      green = 255 - constrain(green, 0, 255);
      blue = 255 - constrain(blue, 0, 255);

      // print the three numbers in one string as hexadecimal
      Serial.print(red, HEX);
      Serial.print(green, HEX);
      Serial.println(blue, HEX);
      Serial.println("hello");
    }
  }
}