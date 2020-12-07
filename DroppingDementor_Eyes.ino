
/* 
 *  Dropping Dementor - Eyes           By: Manny Batt  12/3/2020
 * 
 * This code runs on an ESP8266 connected to an Ultrasonic Sensor Module and 
 * an MP3 player Module (DF_Player Mini) with a speaker. This looks for anyone
 * to walk close enough to the sensor to trigger a response. When triggerd, a 
 * MQTT feed is updated and a loud sound is played to scare anyone walking by!
 * 
 */

// ***************************************
// ********** Global Variables ***********
// ***************************************

//Wifi Setup and OTA Updates
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ArduinoQueue.h>

//WiFi Credentials
#ifndef STASSID
#define STASSID "" // <- Your WiFi Name       
#define STAPSK  "" // <- Your WiFi Password   
#endif
const char* ssid = STASSID;
const char* password = STAPSK;

//Globals for MQTT
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#define MQTT_CONN_KEEPALIVE 300
#define AIO_SERVER      "" // <- Your MQTT Server           
#define AIO_SERVERPORT  0  // <- Your MQTT Server Port      
#define AIO_USERNAME    "" // <- Your MQTT Server Username  
#define AIO_KEY         "" // <- Your MQTT Server Key       

//Initialize and Subscribe to MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish dementor = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DroppingDementor"); //Your MQTT feed info

//MP3 Player
#include <DFPlayer_Mini_Mp3.h>     //Library for interfacing with the MP3 Player module
#include <SoftwareSerial.h>        //Sets up Serial for communicating with MP3 player module
SoftwareSerial mp3Serial(D3, D2);  //Pins for MP3 Player Serial (RX, TX)

//Globals for Relays
#define trigPin D7  //Pin for Ultrasonic Sensor Trigger
#define echoPin D8  //Pin for Ultrasonic Sensor Reciever
float duration, distance;

//Globals for System
int trigger = 0;  //Indicates someone in the vicinity
int lastDistance = 0;       //Previous distance recorded 
int lastLastDistance = 0;   //and the one before that
int reallyLastDistance = 0; //and even one more before the last.
int song = 1;  //The song choice for the MP3 player. Must start with 1


// ***************************************
// *************** Setup *****************
// ***************************************

void setup() {

  //WiFi
  wifiSetup();

  //Initialize Relays
  delay(1000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //Initialize Serial for MP3 Module communication
  Serial.println("Setting up software serial");
  mp3Serial.begin (9600);
  Serial.println("Setting up mp3 player");
  mp3_set_serial (mp3Serial);
  delay(1000);  //Delay is required before accessing the MP3 player
  mp3_set_volume (30);
}


// ***************************************
// ************* Da Loop *****************
// ***************************************
void loop() {

  //Network Housekeeping, keeps MQTT connected and OTA Updates ready
  ArduinoOTA.handle();
  MQTT_connect();

  //Retrieve Ultrasonic Sensor Data. 
  //This ~should~ be the distance away an obstruction is in centimeters
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = (duration * .0343) / 2;

  //This is where onjects are detected by the ultrasonic sensor.
  //Previous distances are compared with determined acceptable ranges.
  //Sensor data can react in weird ways so if the data exceeds the control 
  //ranges in either direction, the trigger will be activated.
  //The 120-80 range was discovered by lots of trial and error and may
  //need to be changed when you try at home. 
  if (distance < 200 && distance > 40) { //This range eliminates false positives
    if ((distance > 120 || distance < 80)  && 
        (lastDistance > 120 || lastDistance < 80) &&
        (lastLastDistance > 120 || lastLastDistance < 80) &&
        (reallyLastDistance > 120 || reallyLastDistance < 80)) {
            trigger = 1;
            Serial.print("flag = ");
    }
    Serial.println(distance);
  }

  //This code runs if someone is detected
  if (trigger == 1) {
    Serial.println("***********SUCKERS DETECTED*************");
    delay(350);
    dementor.publish(1); //Send MQTT trigger to the dropping mechanism to Drop

    //Loop song playlist
    if (song == 5) {
      song = 1;
    }
    mp3_play_physical(song); //Play Spooky sound
    song++;
    
    delay(8000); //This is how long the ghost stays dropped for
    dementor.publish(2); //Send MQTT trigger to the dropping mechanism to Raise
  }

  trigger = 0;
  reallyLastDistance = lastLastDistance;
  lastLastDistance = lastDistance;
  lastDistance = distance;
  delay(1);
}


// ***************************************
// ********** Backbone Methods ***********
// ***************************************

//Connects to MQTT service
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    //Serial.println("Connected");
    return;
  }
  Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
      Serial.println("Wait 10 min to reconnect");
      delay(600000);
    }
  }
  Serial.println("MQTT Connected!");
}

//Initializes the WiFi connection
void wifiSetup() {
  //Initialize Serial
  Serial.begin(115200);
  Serial.println("Booting");

  //Initialize WiFi & OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname("DroppingDementorEyes");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}
