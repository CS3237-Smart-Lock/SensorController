#include "driver/gpio.h"
#include "esp_timer.h"
#include "soc/rtc_cntl_reg.h" //disable brownout problems
#include "soc/soc.h"          //disable brownout problems
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <SparkFun_APDS9960_mod.h>
#include <WiFi.h>
#include <Wire.h>
#include <base64.h>


#define BUTTON_PIN 14

#define WEBSOCKET_URL "ws://192.168.0.75:12345/"

volatile bool button_pressed = false;

using namespace websockets;
WebsocketsClient client;

///////////////////////////////////INITIALIZE
///FUNCTIONS///////////////////////////////////

int state;

esp_err_t init_wifi(int maxRetries = 20) {
  WiFi.begin("sam_zhang", "aaabbb1234");
  Serial.println("Starting Wifi");

  int retryCount = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retryCount++;

    if (retryCount >= maxRetries) {
      Serial.println("\nWiFi connection failed. Restarting ESP...");
      return ESP_FAIL;
    }
  }

  Serial.println("\nWiFi connected");
  return ESP_OK;
}

esp_err_t connect_to_websocket() {
  Serial.println("Connecting to websocket");

  bool connected = client.connect(WEBSOCKET_URL);

  while (!connected) {
    delay(500);
    Serial.print(".");
    connected = client.connect(WEBSOCKET_URL);
  }

  Serial.println("Websocket Connected!");
  return ESP_OK;
}

SparkFun_APDS9960 apds = SparkFun_APDS9960();
#define APDS9960_INT 23
int isr_flag = 0;
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

void onTimer(void *arg) {
  Serial.println("Timer has run out");
  button_pressed = false;
  input = 1;
  begin = 1;
}

void IRAM_ATTR isr_in() {
  button_pressed = true;
  timer_start = true;
}

void timer_setup() {
  // creating the timer
  esp_timer_create_args_t timerConfig = {.callback = &onTimer,
                                         .name = "boolToggleTimer"};

  // Initialize the timer
  esp_timer_create(&timerConfig, &timer);
}

void apds_setup() {
  // Initialize APDS-9960 (configure I2C and initial values)
  if (apds.init()) {
    Serial.println(F("APDS-9960 initialization complete"));
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
    // times to retry init?
  }

  if (apds.enableGestureSensor(false) // true to enable interrupt
      /* calibration values */
      // && apds.setGestureEnterThresh(40)
      // && apds.setGestureExitThresh(20)
      // && apds.setGestureGain(GGAIN_4X)
      // && apds.setGestureLEDDrive(LED_DRIVE_100MA) //max
      // && apds.setGestureWaitTime(GWTIME_8_4MS)
      // && apds.wireWriteDataByte(APDS9960_GPULSE, 0xD4)  // 32us, 20 pulses
      // 0b11,010100
  ) {
    Serial.println(F("Gesture sensor is now running"));
  } else {
    Serial.println(F("Something went wrong during gesture sensor init!"));
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  // pinMode(APDS9960_INT, INPUT);
  // Initialize Serial port
  Serial.begin(9600);

  Serial.setDebugOutput(true);

  if (init_wifi() == ESP_FAIL) {
    ESP.restart();
  };

  connect_to_websocket();

  // setting up the apds sensor
  apds_setup();

  // setting up the button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(BUTTON_PIN, isr_in, FALLING);

  // setting up the timer
  timer_setup();
}

void loop() {

  // Keep alive websocket
  if (!client.available()) {
    connect_to_websocket();
  } else {
    client.poll();
  }

  if (timer_start) {
    Serial.println("Starting the timer");
    esp_timer_start_once(timer, 10 * 1000000);
    timer_start = false;
  }

  if (button_pressed) {
    handleGesture();
  }
}

void sendGestureJson(const String &direction) {
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