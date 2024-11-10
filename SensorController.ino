#include <Wire.h>
#include <SparkFun_APDS9960_mod.h>
#include "esp_timer.h"
#include <ArduinoJson.h>
#include <base64.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "soc/soc.h" //disable brownout problems
#include "soc/rtc_cntl_reg.h"  //disable brownout problems
#include "driver/gpio.h"
#define BUTTON_PIN 14

volatile bool button_pressed = false;

char * url = "ws://172.20.10.4:12345/";

using namespace websockets;
WebsocketsClient client;

///////////////////////////////////CALLBACK FUNCTIONS///////////////////////////////////
void onMessageCallback(WebsocketsMessage message) {
  Serial.print("Got Message: ");
  Serial.println(message.data());
}

///////////////////////////////////INITIALIZE FUNCTIONS///////////////////////////////////

int state;

esp_err_t init_wifi() {
  WiFi.begin("iPhone von Luis", "12345678");
  Serial.println("Starting Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("Connecting to websocket");
  client.onMessage(onMessageCallback);
  bool connected = client.connect(url);
  if (!connected) {
    Serial.println("Cannot connect to websocket server!");
    state = 3;
    return ESP_FAIL;
  }
  if (state == 3) {
    return ESP_FAIL;
  }

  Serial.println("Websocket Connected!");
  // client.send("deviceId"); // for verification
  return ESP_OK;
};



SparkFun_APDS9960 apds = SparkFun_APDS9960();
#define APDS9960_INT 23
int isr_flag = 0;
int pw_input[4] = { 0, 0, 0, 0 };
int pw_pos = 0;
bool input = 0;
bool begin = 1;

/* Direction definitions */
//   0  DIR_NONE,
//   1  DIR_LEFT,
//   2  DIR_RIGHT,
//   3  DIR_UP,
//   4  DIR_DOWN,
//   5  DIR_NEAR,
//   6  DIR_FAR,
//   7  DIR_ALL
esp_timer_handle_t timer;
volatile bool timer_start = false;

 void onTimer(void* arg){  
    Serial.println("Timer has run out");
    client.send("Timer has run out");
    button_pressed = false;
    pw_pos = 0;
    input = 1;
    begin = 1;
 }

 void IRAM_ATTR isr_in() {
  button_pressed = true;
  timer_start = true;
  
  
}


void timer_setup() {
  //creating the timer
  esp_timer_create_args_t timerConfig = {
        .callback = &onTimer,
        .name = "boolToggleTimer"
    };

    // Initialize the timer
    esp_timer_create(&timerConfig, &timer);
}

void apds_setup() {
  // Initialize APDS-9960 (configure I2C and initial values)
  if (apds.init()) {
    Serial.println(F("APDS-9960 initialization complete"));
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
    //times to retry init?
  }

    if (apds.enableGestureSensor(false) //true to enable interrupt
      /* calibration values */
      // && apds.setGestureEnterThresh(40)
      // && apds.setGestureExitThresh(20)
      // && apds.setGestureGain(GGAIN_4X)
      // && apds.setGestureLEDDrive(LED_DRIVE_100MA) //max
      // && apds.setGestureWaitTime(GWTIME_8_4MS)
      // && apds.wireWriteDataByte(APDS9960_GPULSE, 0xD4)  // 32us, 20 pulses 0b11,010100
  ) {
    Serial.println(F("Gesture sensor is now running"));
  } else {
    Serial.println(F("Something went wrong during gesture sensor init!"));
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  // pinMode(APDS9960_INT, INPUT);
  // Initialize Serial port
  Serial.begin(9600);

  Serial.setDebugOutput(true);
  init_wifi();

  // attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);

  //setting up the apds sensor
  apds_setup();

  //setting up the button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, isr_in, FALLING);


  //setting up the timer
  timer_setup();

}

void loop() {

  client.poll();

  if(timer_start){
    Serial.println("Starting the timer");
    esp_timer_start_once(timer, 10 * 1000000);
    timer_start = false;
    client.send("Starting attempt: ");
  }

  if(button_pressed) {
     
      /*if (begin) {
        Serial.println("Input password:");
        begin = 0;
      }*/

      handleGesture();
      /*
      if (!input and pw_pos == 4) {
        sendPW();
      }
      */
  } else {
    //Serial.println("Button not activated yet");
  }
  
}

void sendGestureJson(const String& direction) {
  // Encode the direction in base64
  String encodedData = base64::encode(direction);

  // Create JSON document
  StaticJsonDocument<256> jsonDoc;
  jsonDoc["type"] = "gesture";
  jsonDoc["data"] = encodedData;

  // Convert JSON to string
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Send JSON over websocket
  client.send(jsonString);
}

// Modified handleGesture to send JSON message
void handleGesture() {
  if (apds.isGestureAvailable()) {
    int gest = apds.readGesture();
    switch (gest) {
      case DIR_LEFT:
        Serial.println("1 LEFT");
        sendGestureJson("left");
        break;
      case DIR_RIGHT:
        Serial.println("2 RIGHT");
        sendGestureJson("right");
        break;
      case DIR_UP:
        Serial.println("3 UP");
        sendGestureJson("up");
        break;
      case DIR_DOWN:
        Serial.println("4 DOWN");
        sendGestureJson("down");
        break;
      default:
        return;
    }
  }
}

/*void sendPW() {
  //fill in with packet sending stuff
  esp_timer_stop(timer);
  Serial.print("Your input: ");
  for (int i = 0; i < pw_pos; i++) {
    Serial.print(pw_input[i]);
  }
  Serial.println();
  pw_pos = 0;
  input = 1;
  begin = 1;
  button_pressed = false;
  client.send("Attempted password: " + String(pw_input[0]) + String(pw_input[1]) + String(pw_input[2]) + String(pw_input[3]));
}

void handleReply(bool pwcorrect) {
  //polling/interrupt to handle reply from backend
  //fill in with packet parsing stuff
  if (pwcorrect) Serial.println("Unlocking");  //print statements could be replaced with led feedback
  else Serial.println("Try again!");
}
*/