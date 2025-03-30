// Include needed libs
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Set the number of the module!!!
#define ADDR 1



#define PIN_BUZZER 2  // Pin for buzzer
#define PIN_LED 3  // Pin for LED that allows to signalize the loss connection to the teacher's module

// Settings for the radio and creating appropriate object
#define PIN_CE  10
#define PIN_CSN 9
RF24 radio(PIN_CE, PIN_CSN);

// Some time values for the time-management
long timeBorder, timeToReceive;


// Some standard initialization for the radio
void setupRadio(){
  radio.begin();
  radio.setDataRate (RF24_1MBPS);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setPayloadSize(sizeof(uint8_t));
}

// Allows to set the radio-channel according to the number of the module
void setRadioChannel(uint8_t ch){
  radio.setChannel(ch);
  radio.openWritingPipe(2);
  radio.openReadingPipe(1, 1);
  radio.startListening();
  
  Serial.print("Set channel to: ");
  Serial.println(ch);
}



void setup() {
  // Configure pins
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  // Set the radio
  setupRadio();
  setRadioChannel(ADDR);
  Serial.begin(9600);
}

void loop() {
  
  // time period that is needed to check if the connection to the teacher's module is lost
  timeToReceive = 10000;

  
  timeBorder = millis();
  while(timeToReceive > 0){

    // If there is some got info in the radio-mosule
    if (radio.available()){              
        uint8_t gotLoad;
        radio.read(&gotLoad, sizeof(uint8_t));             // fetch payload from FIFO

        // If the info is "0" --> So we got the message from the teacher
        // That's why the connection is established
        if (gotLoad == 0){
          Serial.print("Got number: ");
          Serial.println(gotLoad);

          // Send our number in order to show our presence
          radio.stopListening();
          delay(500);
          uint8_t payLoad = ADDR;
          radio.write(&payLoad, sizeof(uint8_t));
          digitalWrite(PIN_LED, LOW);
          digitalWrite(PIN_BUZZER, LOW);
          //delay(200);
          radio.startListening();
          break;
        }else{
          Serial.println("gotLoad is invalid");
        }
      }

    // This is some tick-function to check when we went beyond the time period
    if (millis() - timeBorder > 70){
      timeToReceive -= 70;
      timeBorder = millis();
    }
    
  }

  // If we went beyond the time period
  // So, switch the LED and buzzer on
  if (timeToReceive <= 0){
    digitalWrite(PIN_LED, HIGH);
    digitalWrite(PIN_BUZZER, HIGH);
  }
  
}
