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

#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

#define BUTTON_PIN 14

volatile bool button_pressed = false;
volatile bool timer_has_run_out = false;
char * url = "ws://172.20.10.4:12345/";
volatile int64_t start_time = 0;

hd44780_I2Cexp lcd;  // Declare lcd object: auto-locates address

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

void setup_lcd() {
  
  int status = lcd.begin(16, 2);  // Adjust to 20,4 if using a 20x4 display
  delay(500);
  if (status) {                   // Check if initialization was successful
    Serial.println("LCD initialization failed");
    return;
  }
  lcd.backlight();
  
  lcd.setCursor(0, 0);  
  lcd.print("Hello, ESP32!");
}

SparkFun_APDS9960 apds = SparkFun_APDS9960();
#define APDS9960_INT 23


esp_timer_handle_t timer;
volatile bool timer_start = false;

 void onTimer(void* arg){  
    timer_has_run_out = true;
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
  //setting up lcd
  setup_lcd();
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

  if(timer_has_run_out) {
    Serial.println("Timer has run out");
    client.send("Timer has run out");
    button_pressed = false;
    timer_has_run_out = false;
  }

  if(timer_start){
    Serial.println("Starting the timer");
    esp_timer_start_once(timer, 10 * 1000000);
    start_time = esp_timer_get_time();
    timer_start = false;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Starting attempt");
    //client.send("Starting attempt: ");
  }

  if(button_pressed) {

      handleGesture();

      //create the countdown
      int64_t time_passed = (esp_timer_get_time()-start_time)/1000000; 
      lcd.setCursor(0,1);
      lcd.print(String(time_passed));
      
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