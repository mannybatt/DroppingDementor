



// ***************************************
// ********** Global Variables ***********
// ***************************************


//Globals for Wifi Setup and OTA
#include <credentials.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

//WiFi Credentials
#ifndef STASSID
#define STASSID "your_ssid"
#endif
#ifndef STAPSK
#define STAPSK  "your_password"
#endif
const char* ssid = STASSID;
const char* password = STAPSK;

//MQTT
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#ifndef AIO_SERVER
#define AIO_SERVER      "your_MQTT_server_address"
#endif
#ifndef AIO_SERVERPORT
#define AIO_SERVERPORT  0000 //Your MQTT port
#endif
#ifndef AIO_USERNAME
#define AIO_USERNAME    "your_MQTT_username"
#endif
#ifndef AIO_KEY
#define AIO_KEY         "your_MQTT_key"
#endif
#define MQTT_KEEP_ALIVE 150
unsigned long previousTime;

//Initialize and Subscribe to MQTT
WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish dementor = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/DroppingDementor"); //Your MQTT feed info

//MP3 Player
#include "DFRobotDFPlayerMini.h"
#include <SoftwareSerial.h>
DFRobotDFPlayerMini myDFPlayer;
SoftwareSerial mySoftwareSerial(D3, D2);  //Pins for MP3 Player Serial (RX, TX)

//Globals for Relays
#define trigPin D7  //Pin for Ultrasonic Sensor Trigger
#define echoPin D8  //Pin for Ultrasonic Sensor Reciever
float duration, distance;

//Globals for System
//int difference = 0;
int greaterDifference = 0;
int lastSensorData = 0;
int reallyLastSensorData = 0;
int lastGreaterDifference = 0;
int readings[10];
int warmingUpFlag = 0;
int trigger = 0;

int lastDistance = 0;
int lastLastDistance = 0;
int reallyLastDistance = 0;
int s = 1;




// ***************************************
// *************** Setup *****************
// ***************************************


void setup() {

  //Wifi
  wifiSetup();

  //Initialize Relays
  delay(1000);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  //MP3
  //Serial.println("Setting up software serial");
  mySoftwareSerial.begin (9600);
  if (!myDFPlayer.begin(mySoftwareSerial)) //Is DfPlayer ready?
  {
    Serial.println(F("Not initialized:"));
    Serial.println(F("1. Check the DFPlayer Mini connections"));
    Serial.println(F("2. Insert an SD card"));
    while (true);
  }  
  Serial.println();
  Serial.println("DFPlayer Mini module initialized!");
  myDFPlayer.setTimeOut(500); //Timeout serial 500ms
  myDFPlayer.volume(30); //Volume 0-30
  myDFPlayer.EQ(0); //Equalizacao normal

  //Sensor calibration
  for (int i = 0; i < 10; i++) {
    readings[i] = 100;
  }
}




// ***************************************
// ************* Da Loop *****************
// ***************************************


void loop() {

  //Network Housekeeping
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
/*
  int hold = 0;
  int hold2 = 0;
  for (int i = 0; i < 10; i++) {
    if (i == 0) {
      hold = readings[i];
      readings[i] = greaterDifference;
    }
    else {
      hold2 = readings[i];
      readings[i] = hold;
      hold = hold2;
    }
  }
*/
  if (distance < 200 && distance > 40) {
    if ((distance > 120 || distance < 80)  &&
        (lastDistance > 120 || lastDistance < 80) &&
        (lastLastDistance > 120 || lastLastDistance < 80) &&
        (reallyLastDistance > 120 || reallyLastDistance < 80)) {
      warmingUpFlag++;
      trigger = 1;
      //Serial.print("flag = ");
      //Serial.println(warmingUpFlag);
    }
    //Serial.println(distance);
  }

  if (trigger == 1) {
    //Serial.println("***********SUCKERS DETECTED*************");
    delay(350);

    /* Meme Operation (Survivor Yell) */
    char q = 1;
    myDFPlayer.play(q);

    /* Normal Operation
    dementor.publish(1);
    if (s == 5) {
      s = 1;
    }    
    mp3_play_physical(s);
    s++;
    delay(8000);
    dementor.publish(2);
    */
  }

  trigger = 0;
  lastGreaterDifference = greaterDifference;
  delay(1);
  reallyLastDistance = lastLastDistance;
  lastLastDistance = lastDistance;
  lastDistance = distance;
}




// ***************************************
// ********** Backbone Methods ***********
// ***************************************


void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    //Serial.println("Connected");
    return;
  }
  //Serial.print("Connecting to MQTT... ");
  uint8_t retries = 3;

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    //Serial.println(mqtt.connectErrorString(ret));
    //Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
      //Serial.println("Wait 10 min to reconnect");
      delay(600000);
    }
  }
  //Serial.println("MQTT Connected!");
}

void wifiSetup() {
  //Initialize Serial
  //Serial.begin(115200);
  //Serial.println("Booting");

  //Initialize WiFi & OTA
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
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
    //Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      //Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      //Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      //Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      //Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      //Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  //Serial.println("Ready");
  //Serial.print("IP address: ");
  //Serial.println(WiFi.localIP());
}
