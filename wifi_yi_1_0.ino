/**************************************************************
   xiaomi_yi_rc_switch is for the ESP8266/Arduino platform
   it contains the WifiManger from: AlexT https://github.com/tzapu (thx)
   You can controll an Xiaomi Yi with an radio controlled receiver.
   You can switch between Foto and Video Mode during flight (drive ...)
   Licensed under MIT license
 **************************************************************/


#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager

const int pin_gipo2 = 14;
const int pin_gipo0 = 0;
const int BUILTIN_LED1 = 2; //GPIO2


#define RC_FILTER 100 // Toleranz to find the command
#define WIFI_TX_POWER 0 // TX power of ESP module (0 -> 0.25dBm) (0...85)
#define TIMEOUT_AP 120 //sets timeout until configuration portal gets turned off

//#define DEBUG
#define DEBUG_SMALL
#define DEBUG_CAMERA_OUTPUT

int switch_signal;
int record_status = 0;
String old_token = "0";
int old_duration = 0;
int counter_while = 0;
//WiFiManager
//Local intialization. Once its business is done, there is no need to keep it around
WiFiManager wifiManager;
//ESP8266WebServer server(80);
WiFiClient client;

void setup() {

#ifdef ESP8266
  system_phy_set_max_tpw(WIFI_TX_POWER); //set as lower TX power as possible
#endif

  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(BUILTIN_LED1, OUTPUT); // Initialize the BUILTIN_LED1 pin as an output
  

  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);

  //reset saved settings
  //wifiManager.resetSettings();

  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  digitalWrite(BUILTIN_LED1, LOW);
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(TIMEOUT_AP);
  
  digitalWrite(BUILTIN_LED1, LOW);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("Xiaomi Yi RC Switch")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    //ESP.reset();
    ESP.restart();
    delay(5000);
  }


  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  // wifiManager.autoConnect("Xiaomi Yi RC Switch");
  //or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  digitalWrite(BUILTIN_LED1, HIGH);

  //ab hier neu:
  pinMode(pin_gipo2, INPUT);
  pinMode(pin_gipo0, OUTPUT);
  digitalWrite(pin_gipo0, LOW);
  //record_status = 0;


}


bool connectToServerRC() {
  bool result = true;
  const int jsonPort = 7878;

  // Open Tcp connection
  if (!client.connect("192.168.42.1", jsonPort)) {
    result = false;
  }

  return result;
}



String requestToken() {
  String token = "" ;
  String INcamera = "";
  //Serial.println("stark token");
  // This will send the request token msg to the server
  digitalWrite(BUILTIN_LED1, !digitalRead(BUILTIN_LED1)); // blink
  //digitalWrite(BUILTIN_LED1, LOW);
  while (1) {

    if (INcamera.substring(29, 34) == "param") {
      //param finden und string davor abschneiden
      int startid = INcamera.indexOf("param");
      String reststring = INcamera.substring(startid);
      // 7 weil param": wegschneiden und dann starten, solange kein leer, Klammer oder , gefunden wird
      for (int i = 8; i < reststring.length(); ++i) {
        //wenn leer, klammer Ende, oder , dann ist der param bereich zuende
        if ((reststring.charAt(i) == ' ') || (reststring.charAt(i) == '}') || (reststring.charAt(i) == ',')) {
          //Serial.println("Ende gefunden");
          break;
        }
        token.concat(reststring.charAt(i));
      }
      break;
    } else {
      //Serial.println("token request done: ");
      client.print("{\"msg_id\":257,\"token\":0}\n\r");
    }

    INcamera = "";

    while (!client.available()) {}
    //counter_while_client++;
    //yield();

    while (client.available()) {
      char k = client.read();
      INcamera += k;
      if (k == '}') {
        //Serial.print(INcamera);
      }
    }
    //Serial.println(INcamera);
  }
  //digitalWrite(BUILTIN_LED1, HIGH);
  //Serial.println("While ende");
  //Serial.println(token);
  return token;
}


void shoot(String token) {

  String INcamera = "";
  int conn = 0, state = 0;
  client.flush();
  client.print("{\"msg_id\":769,\"token\":");
  client.print(token);
  client.print("}\n\r");
  while (1) {
    if (INcamera.substring(24, 32) == "vf_start") {
      //delay(800); // show "CAPTURED" string on OSD screen for 0.8s, if camera confirmed capturing
      break;
    }
    INcamera = "";
    while (!client.available()) {}
    while (client.available()) {

      char k = client.read();
      INcamera += k;
      if (k == '}') {
#ifdef DEBUG_CAMERA_OUTPUT
        Serial.println(INcamera);
#endif
      }
    }
  }

#ifdef DEBUG
  Serial.println("shot finished");
#endif

  //delay(1000);
}


int record_on(String token) {

  String INcamera = "";
  client.flush();
  client.print("{\"msg_id\":513,\"token\":");
  client.print(token);
  client.print("}\n\r");
  int status_video = 0;
  while (1) {
    if (INcamera.substring(24, 42) == "start_video_record") {

#ifdef DEBUG
      Serial.println("video record ON");
#endif
      status_video = 1;
      break;
    }
    INcamera = "";
    while (!client.available()) {}

    while (client.available()) {

      char k = client.read();
      INcamera += k;
      if (k == '}') {
#ifdef DEBUG_CAMERA_OUTPUT
        Serial.println(INcamera);
#endif
      }
    }
  }
  //Serial.print("video status on : ");
  //Serial.println(status_video);
  return status_video;
}

int record_off(String token) {

  String INcamera = "";
  client.flush();
  client.print("{\"msg_id\":514,\"token\":");
  client.print(token);
  client.print("}\n\r");
  int status_video = 1;
  while (1) {

    if (INcamera.substring(24, 32) == "vf_start") {

#ifdef DEBUG
      Serial.println("video record OFF");
#endif
      status_video = 0;
      break;
    }
    INcamera = "";
    while (!client.available()) {}
    while (client.available()) {
      char k = client.read();
      INcamera += k;
      if (k == '}') {
#ifdef DEBUG_CAMERA_OUTPUT
        Serial.println(INcamera);
#endif
      }
    }
  }
  return status_video;
}



void loop() {

  ESP.wdtFeed();

  if (!connectToServerRC()) {
    //Serial.println ("return connectedToServer ");
    return;
  }


  unsigned long duration = pulseIn(pin_gipo2, HIGH);

#ifdef DEBUG
  Serial.print ("duration: ");
  Serial.print (duration);
  Serial.print ("   old_duration: ");
  Serial.println (old_duration);
#endif

  //filter um falsche Werte raus zu filtern, wenn man ein Signal falsch ist, Sender muss ein Signal senden, sonst darf er nicht in die Schleife gehen
  if ((duration >= 100) && (old_duration != 0) && ((old_duration + RC_FILTER) >= duration) && ((old_duration - RC_FILTER) <= duration) ) {
    String token = "";
    token = requestToken();

    //Serial.println("");

#ifdef DEBUG
    Serial.print("Token: ");
    Serial.println(token);
#endif

    //pruefen ob der token i.O ist (-1), wenn - im Token vorkommt ist er nicht i.O.
    int check_token = token.lastIndexOf('-');

    //ev noch einbauen, das entweder das eine oder das andere usw. gewaehlt werden kann, was passiert, wenn von Start Record auf Foto geschalten wird ohne Stop record

    //if (token.length() != 0 && (check_token == -1) && (old_token != token) && (switch_signal <= -150 )) { }
    if (token.length() != 0 && (check_token == -1) && (record_status == 0) && (old_token != token) && (duration <= 1300 )) {
#ifdef DEBUG_SMALL
      Serial.println("Foto");
#endif

      shoot(token);
      //alten Token speichern, damit nicht 2 mal mit den gleichen Token etwas aufgerufen wird
      old_token = token;
    }

    if (token.length() != 0 && (check_token == -1) && (old_token != token) && (record_status == 0)  && (duration >= 1700 )) {
      // if (token.length() != 0 && (check_token == -1) && (old_token != token) && (record_status == 0)  && (switch_signal >= 150 )) {}
      //shoot(token);
#ifdef DEBUG_SMALL
      Serial.println("Start record");
#endif

      record_status = record_on(token);
      //alten Token speichern, damit nicht 2 mal mit den gleichen Token etwas aufgerufen wird
      old_token = token;
    }

    //hier sicher stellen, dass wenn von Video aufnahme direkt auf Foto gestellt wird, das hier vorher noch die Aufnahme gestopt wird, sonst hängt sich die Kamera auf
    if (token.length() != 0 && (check_token == -1) && (old_token != token) && (record_status == 1)  && (duration < 1700 )) {
      //  if (token.length() != 0 && (check_token == -1) && (old_token != token) && (record_status == 1)  && (duration > 1300 )  && (duration < 1700 )) {}
      //if (token.length() != 0 && (check_token == -1) && (old_token != token) && (record_status == 1)  && (switch_signal > -120 )  && (switch_signal < 120 )) {}
      //shoot(token);
#ifdef DEBUG_SMALL
      Serial.println("Stop record");
#endif

      record_status = record_off(token);
      //alten Token speichern, damit nicht 2 mal mit den gleichen Token etwas aufgerufen wird
      old_token = token;
    }

  }

  old_duration = duration;
}


