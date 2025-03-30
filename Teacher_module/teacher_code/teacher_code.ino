// Include needed libs
#include <WiFi.h>                                     
#include <WebServer.h>                                
#include <EEPROM.h>
#include <TinyGPSPlus.h>
#include <PubSubClient.h>

// Include libs for nrf24L01
#include "SPI.h"
#include "nRF24L01.h"
#include "RF24.h"

// define pins
#define BTN_RESET_PIN 18  //button to reset esp32 and Tparams structure
#define BLUE_LED_PIN 19   // Led that signalize well-coonnected state (connected to teacher's AP)




//--------------------------------------------------------------Structure to collect params of teacher's AP-----------------------------------------------------
//This function allows to collect data from web-server and easilly save it
struct TParams
{
  char teacherSSID[21];
  char teacherPass[21];
  int numberOfModules;
  bool getFlag = false;
} params, newParams;
// newParams is a structure that is needed only for debugging
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------Clear EEPROM and RESET ESP32----------------------------------------------------------
// This function calls resetting the esp32 and resetting the data in TParams structure
void clearEEPROM()
{
  struct TParams cleanParams;
  for(int i = 0; i < 21; i++)
  {
    cleanParams.teacherSSID[i] = 0;
    cleanParams.teacherPass[i] = 0;
  }
  cleanParams.numberOfModules = 0;
  cleanParams.getFlag = false;

  EEPROM.put(0, cleanParams);   // Put structure "params" on Zero Address
  EEPROM.commit();              //Commit writing to EEPROM
}

void restartESP()
{
  Serial.println("Restarting ESP...");
  ESP.restart();
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------Settings for GPS-module--------------------------------------------------------------------------

// The TinyGPSPlus object
TinyGPSPlus gps;


struct Coords{
  double latit;
  double longit;
} coords;

using TCoords = Coords;

// Function that allows to get GPS data from hardware UART-port
TCoords getGPSCoords(){
  // Rid of the old data in UART2
  while (Serial2.available() > 0){
    Serial2.read();
  }

  TCoords coords = {0.0f ,0.0f};
  long timeToWait = 2000;
  long startTime = millis();
  do{
    if(Serial2.available() > 0){
      if (gps.encode(Serial2.read())){
        if (gps.location.isValid()){
        coords.latit = gps.location.lat();
        coords.longit = gps.location.lng();
        } 
      }
    }
  }while(millis() - startTime < timeToWait);

  
  Serial.print(F("Location: "));
  if (gps.location.isValid()){
    Serial.print("Lat: ");
    Serial.print(coords.latit, 6);
    Serial.print(F(","));
    Serial.print("Lng: ");
    Serial.print(coords.longit, 6);
    Serial.println("\n");
  }else{
    Serial.println("INVALID");
  }
  return coords;
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------MQTT-client settings-------------------------------------------------------------------------------
const char* mqtt_server = "dev.rightech.io";
String clientId = "teacher_test";
WiFiClient espClient;
PubSubClient client(espClient);

//set time period that is used to send data to the MQTT broker
int sendPeriod = 4000;
unsigned long lastSend = 0;

//This function allows us to wait until we're connected
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
    
  }
}

// This function send GPS-coords to the MQTT-broker
void sendCoords(){
  String latit = String(coords.latit, 6);
  String longit = String(coords.longit, 6);

  client.publish("pos.lat", latit.c_str());
  client.publish("pos.lon", longit.c_str());
}
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------


//-----------------------------------------------------------------------Webserver settings------------------------------------------------------------------------------------
// SSID and password that are used for the Access Point
const char* ssid = "Teacher";

// Configure IP addresses of the local access point
IPAddress local_IP(192,168,123,123);  // Literally this address we use in browser
IPAddress gateway(192,168,1,5);
IPAddress subnet(255,255,255,0);

// Create an object of the WebServer
WebServer server(80);


// start HTML code for server-mode
String startPageHTML = R"=====(
<!DOCTYPE html>
<html>
  <head>
    <meta charset = "utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="X-UA-Compatible" content="ie=edge">
    <title>MainModule</title>
    <style type="text/css">
      .button {
        background-color: #4CAF50;
        border: none;
        color: white;
        border-radius: 6px;
        padding: 12px 24px;
        text-align: center;
        text-decoration: none;
        display: inline-block;
        font-size: 18px;
      }

      .commonText {
        font-size: 18px;
      }
    </style>
  </head>
  <body style="background-color: #cccccc; Color: blue; ">
    <center>
      <h1>Настройка параметров подключения</h1>
      <form action="params">
      <p class="commonText">Имя сети: <input type="text" name="SSID" class="commonText" maxlength="20"></p>
      <p class="commonText">Пароль сети: <input type="text" name="pass" class="commonText" maxlength="20"></p>
      <p class="commonText">Число детских модулей: <input type="number" name="number" class="commonText"></p>
      <input type="submit" value="Отправить" class="button">
      </form>
    </center>
  </body>
</html>
)=====";

// end HTML code for webserver
String lastPage = R"=====(
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8">
        <meta name="author" content="Serjuice">
        <meta name="description" content="Samsung project page">
        <meta name="keywords" content="html, Samsung, test">
        <title>Завершение</title>
        <style>
          .commonText {
            font-size: 25px;
          }
        </style>
      </head>
      <body style="background-color: #cccccc; Color: blue; ">
        <center>
          <br><br><br>
          <h1 class="commonText">Параметры сети были успешно переданы</h1><br>
          <h2 class="commonText">Модуль будет подключён через 15 секунд</h2>
        </center>   
      </body>
    </html>
    )=====";


//Send main html page to the server
void handleRoot()
{
  server.send( 200, "text/html", startPageHTML);
  

  
  // Here we read the struct newParams just for debugging
  EEPROM.get(0, newParams);   // прочитать из адреса 0

  Serial.println("-----------------------");
  Serial.print("newParams.teacherSSID: ");
  Serial.println(newParams.teacherSSID);
  Serial.print("newParams.teacherPass: ");
  Serial.println(newParams.teacherPass);
  Serial.print("newParams.numberOfModules: ");
  Serial.println(newParams.numberOfModules);
  Serial.print("newParams.getFlag: ");
  Serial.println(newParams.getFlag);
  Serial.println("-----------------------");
  
}

//Allows to collect data sent from the server
void getPhoneData()
{
  byte counter = 0;
  int testNum = 0;
  // Get all params of the teacher's AP and number of tracked modules
  for(int i = 0; i < server.args(); i++)
  {
    if(server.argName(i) == "SSID" && server.arg(i) != "")    // Get SSID
    {
      String phoneSSID = "";
      phoneSSID = server.arg(i);
      phoneSSID.toCharArray(params.teacherSSID, 20);
      counter++;
      Serial.print("params.teacherSSID: "); Serial.println(params.teacherSSID);
    }
    else if(server.argName(i) == "pass" && server.arg(i) != "")   // Get pass
    {
      String phonePass = "";
      phonePass = server.arg(i);
      phonePass.toCharArray(params.teacherPass, 20);
      counter++;
      Serial.print("params.teacherPass: "); Serial.println(params.teacherPass);
      
    }
    else if (server.argName(i) == "number" && server.arg(i) != "")  // get number of modules
    {
      String numberModules = "";
      numberModules = server.arg(i);
      params.numberOfModules = numberModules.toInt();
      counter++;
      Serial.print("params.numberOfModules: "); Serial.println(params.numberOfModules);
      
    }
  }

  // If we get all params --> set the flag
  if(counter > 2 && params.numberOfModules < 33){
    params.getFlag = true;
  }

  // If we got all params --> send lest HTML-page
  if(params.getFlag)
  {
    server.send(200, "text/html", lastPage);


    //Here we need to save struct "params" to EEPROM
    EEPROM.put(0, params);   // Put structure "params" on Zero Address
    EEPROM.commit();         //Commit writing to EEPROM
  
   //Delay for 15 seconds
   delay(15000);


   //Reset the esp8266
   restartESP();
    
  }
  else    // If we haven't got right params --> send start page again
  {
    server.send( 200, "text/html", startPageHTML);
  }
}

//This function allows to show absence of the page for the request
void handle_NotFound()
{
  server.send(404, "text/plain", "Not found");
}

//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------Survey of the cheldrens' modules------------------------------------------------------------------------

#define PRESENCE_LED 21   // This pin is responsible for the led that allows to show that some childrens' modules are lost

// My defines for SPI pins
#define MY_MISO 25
#define MY_MOSI 27
#define MY_SCLK 26
#define MY_SS   32  // pass MY_SS as the csn_pin parameter to the RF24 constructor

 
// create object of the radio-module
RF24 radio(12, 14); 
 
SPIClass* hspi = nullptr; 


//This function setup radio at all
void setupRadio(){
  hspi = new SPIClass(HSPI);
  // Use the custom defined pins
  hspi->begin(MY_SCLK, MY_MISO, MY_MOSI, MY_SS);
  
  radio.begin(hspi);  
  radio.setDataRate (RF24_1MBPS); 
  radio.setPALevel(RF24_PA_HIGH); 
  radio.setPayloadSize(sizeof(uint8_t));

}

// Every module occupies its own channel
// So we need to reset channel before survey of one new module
void setRadioChannel(uint8_t ch){
  radio.setChannel(ch); 
  radio.openWritingPipe(1);
  radio.openReadingPipe(1, 2);
  radio.stopListening();
  
  Serial.print("Set channel to: ");
  Serial.println(ch);
}

// Function that allows to carry out the whole survey of all childrens' modules
// and return the result of the survey
bool childSurvey(int numOfModules){
  bool presenceFlag = true;
  long timeToAnswer, timeBorder;
  
  for(int i = 0; i < numOfModules; ++i){
    setRadioChannel(i+1);

    uint8_t payload = 0;
    radio.write(&payload, sizeof(uint8_t));
    radio.startListening();

    timeToAnswer = 2000;
    timeBorder = millis();
    while(timeToAnswer > 0){

      if (radio.available()){              // is there a payload?
        uint8_t gotLoad;
        radio.read(&gotLoad, sizeof(uint8_t));             // fetch payload from FIFO
  
        if (gotLoad == i+1){
          Serial.print("Got number: ");
          Serial.println(gotLoad);
          break;
        }else{
          presenceFlag = false;
          Serial.println("gotLoad is invalid");
          break;
        }
      }

    
      if (millis() - timeBorder > 70){
        timeToAnswer -= 70;
        timeBorder = millis();
      }
      
    }

    if (timeToAnswer <= 0){
      Serial.println("TimeToAnswer is timed out");
      presenceFlag = false;
    }

    radio.stopListening();
  }

  return presenceFlag;
}


//------------------------------------------------------------------------------------------------------------------------------------------------------------------------------



void setup() {
  //Configure the pins
  pinMode(BTN_RESET_PIN, INPUT_PULLUP);
  
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, LOW);

  pinMode(PRESENCE_LED, OUTPUT);
  digitalWrite(PRESENCE_LED, LOW);

  
  // init serial port for debugging (Serial1) and for GPS-module (Serial2)
  Serial.begin(9600);
  Serial2.begin(9600);
  
  //Start EEPROM, 100 --> used mem in bytes
  EEPROM.begin(100);
  // Get data from EEPROM and save it to params structure
  EEPROM.get(0, params);


  // If we already have the params to connect
  if(params.getFlag)
  {
    //-----------------------------------------Connecting to local Wi-fi--------------------------------------
    Serial.println("Connecting to ");
    Serial.println(params.teacherSSID);

    // Connect to the teacher's AP
    WiFi.begin(params.teacherSSID, params.teacherPass);

    // checking the connection
    while (WiFi.status() != WL_CONNECTED) 
    {
      digitalWrite(BLUE_LED_PIN, HIGH);
      delay(500);
      digitalWrite(BLUE_LED_PIN, LOW);
      delay(500);
      Serial.print(".");

      // If we want to reset settings of the AP --> hold the button for some time
      if(digitalRead(BTN_RESET_PIN)== LOW)
      {
        clearEEPROM();
        restartESP();
      }
    }
    digitalWrite(BLUE_LED_PIN, HIGH);
    Serial.println("");
    Serial.println("WiFi connected..!");
    Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

    //-----------------------------------------Connecting to the MQTT-server----------------------------------
    client.setServer(mqtt_server, 1883);
    //--------------------------------------------------------------------------------------------------------

    //-------------------------------------------Setup radio (nrf24L01)---------------------------------------
    setupRadio();
    //--------------------------------------------------------------------------------------------------------


  // If we don't have the params to connect to the teacher's AP
  // Start webserver
  }else{
    // init the esp-32-AP
    Serial.print("Setting up Access Point ... ");
    Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet) ? "Ready" : "Failed!");
  
    Serial.print("Starting Access Point ... ");
    Serial.println(WiFi.softAP(ssid) ? "Ready" : "Failed!");
  
    Serial.print("current IP of the esp32-AP = ");
    Serial.println(WiFi.softAPIP());
    delay(100);
  
  
    //setting responses of the server
    server.on("/params", getPhoneData);
    server.on("/", handleRoot);
    server.onNotFound(handle_NotFound);
  
  
  
    // start webserver
    server.begin(); // init the server
    Serial.println("HTTP server started");
  }
}
  
void loop() {

  if (params.getFlag){

    // mqtt-client handler
    if (!client.connected())
    {
      reconnect();
    }
    client.loop();


    // GPS and childrens' modules survey
    if(millis() - lastSend > sendPeriod)
    {
      //Get coordinates from GPS-module
      coords = getGPSCoords();
      //Send coordinates to the MQTT-server
      sendCoords();



      // Childrens' modules survey
      if(childSurvey(params.numberOfModules)){
        Serial.println("true");
        client.publish("modules", "true");
        digitalWrite(PRESENCE_LED, LOW);
      }else{
        Serial.println("false");
        client.publish("modules", "false");
        digitalWrite(PRESENCE_LED, HIGH);
      }
      
      lastSend = millis();
    }


    
    // If we want to reset settings of the AP --> hold the button for some time
    if(digitalRead(BTN_RESET_PIN)== LOW)
    {
      clearEEPROM();
      restartESP();
    }

  }else{
    // If we don't have params to attach to the teacher's AP
    // Handle the webserver
    server.handleClient();  
  }
  
}
