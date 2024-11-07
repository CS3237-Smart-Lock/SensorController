#include <Wire.h>
#include <SparkFun_APDS9960_mod.h>

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

void setup() {

  // pinMode(APDS9960_INT, INPUT);
  // Initialize Serial port
  Serial.begin(9600);

  // attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);

  // Initialize APDS-9960 (configure I2C and initial values)
  if (apds.init()) {
    Serial.println(F("APDS-9960 initialization complete"));
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
    //times to retry init?
  }


  // Start running the APDS-9960 gesture sensor engine
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

void loop() {
  if (begin) {
    Serial.println("Input password:");
    begin = 0;
  }
  // if (isr_flag == 1) {
  // detachInterrupt(APDS9960_INT);
  handleGesture();
  // isr_flag = 0;
  // attachInterrupt(APDS9960_INT, interruptRoutine, FALLING);
  // }
  if (!input and pw_pos == 4) {
    sendPW();
  }
}
void interruptRoutine() {
  isr_flag = 1;
}

void handleGesture() {
  if (apds.isGestureAvailable()) {
    int gest = apds.readGesture();
    switch (gest) {
      case DIR_LEFT:
        Serial.println("1 LEFT");
        break;
      case DIR_RIGHT:
        Serial.println("2 RIGHT");
        break;
      case DIR_UP:
        Serial.println("3 UP");
        break;
      case DIR_DOWN:
        Serial.println("4 DOWN");
        break;
      case DIR_NEAR:
        Serial.println("5 NEAR");
        break;
      case DIR_FAR:
        Serial.println("6 FAR");
        break;
      default:
        //Serial.println("NOT DETECTED");
        return;
    }
    pw_input[pw_pos++] = gest;
    if (pw_pos == 4) {
      input = 0;
    }
  }
}

void sendPW() {
  //fill in with packet sending stuff
  Serial.print("Your input: ");
  for (int i = 0; i < pw_pos; i++) {
    Serial.print(pw_input[i]);
  }
  Serial.println();
  pw_pos = 0;
  input = 1;
  begin = 1;
}

void handleReply(bool pwcorrect) {
  //polling/interrupt to handle reply from backend
  //fill in with packet parsing stuff
  if (pwcorrect) Serial.println("Unlocking");  //print statements could be replaced with led feedback
  else Serial.println("Try again!");
}