// --------------------------------------------------------------------------------- //
// ----------------------------------- VARIABLES ----------------------------------- //
// --------------------------------------------------------------------------------- //
// Let's start by including the needed libraries
#include <Adafruit_NeoPixel.h>
#include <SoftwareSerial.h>
#include <MP3Player_KT403A.h>
#include "HUSKYLENS.h"
#include <Servo.h> 

// Then we define global constants
#define LED_PIN       4
#define NUMPIXELS    74
#define TOUCH_PIN     A0
#define SERVO_PIN_1   6
#define SERVO_PIN_2   7

// And the rest
SoftwareSerial mp3(2, 3);                     // The MP3 module is connected on pins 2 and 3
Adafruit_NeoPixel pixels(NUMPIXELS, LED_PIN);

int LED_BRIGHTNESS = 8;  // 0-255

HUSKYLENS huskylens;
HUSKYLENSResult face;
bool face_detected = false;
bool prev_touch_value = 0;

enum Emotion {NEUTRAL, OVERWHELMED, LOVE, LISTENING};
Emotion emotion = NEUTRAL;

Servo servo1, servo2;
float servo1_pos = 90, servo2_pos = 90;
float servo1_target = 90, servo2_target = 90;
float servo1_speed = 0, servo2_speed = 0;

long timer1, timer2, timer3;

bool pc_connected = false;
float servo1_target_pc = 90, servo2_target_pc = 90;

// --------------------------------------------------------------------------------- //
// ---------------------------------- EYE PATTERNS --------------------------------- //
// --------------------------------------------------------------------------------- //

byte neutral[] = {
  B0000,
  B01110,
  B011110,
  B0111110,
  B011110,
  B01110,
  B0000
};

byte blink1[] = {
  B0000,
  B00000,
  B011110,
  B0111110,
  B011110,
  B00000,
  B0000
};

byte blink2[] = {
  B0000,
  B00000,
  B000000,
  B1111111,
  B000000,
  B00000,
  B0000
};

byte love[] = {
  B0000,
  B11011,
  B111111,
  B0111110,
  B011110,
  B01110,
  B0110
};

byte overwhelmed[] = {
  B0000,
  B00000,
  B110000,
  B0111100,
  B011111,
  B00000,
  B0000
};


void setup() {
  // put your setup code here, to run once:
  pinMode(TOUCH_PIN, INPUT);

  // Initialize the leds
  pixels.begin();

  // Serial communication
  Serial.begin(115200);

  // MP3
  mp3.begin(9600);
  delay(100); // Wait 0.1 seconds for the MP3 player to boot up
  SelectPlayerDevice(0x02);       // Select SD card as the player device.
  SetVolume(0x1E);                // Set the volume, the range is 0x00 to 0x1E.

  // HuskyLens
  Wire.begin();
  while (!huskylens.begin(Wire)) {
      Serial.println(F("Begin failed!"));
      Serial.println(F("1.Please recheck the \"Protocol Type\" in HUSKYLENS (General Settings>>Protocol Type>>I2C)"));
      Serial.println(F("2.Please recheck the connection."));
      delay(100);
  }

  // Servos
  servo1.attach(SERVO_PIN_1);
  servo2.attach(SERVO_PIN_2);
  servo1.write(90);
  servo2.write(90);
}

void loop() {

  // Ever 20 milliseconds, update the servos
  if (millis() - timer1 >= 20){
    timer1 = millis();
    //move_servos();
    //husky_lens();
    //touch_sensor();
    run_emotions();
  }

  // Every 10 milliseconds, update the huskylens and touch sensor
  if (millis() - timer2 >= 10){
    timer2 = millis();
    communication();
  }

}

// --------------------------------------------------------------------------------- //
// -------------------------------------- EYES ------------------------------------- //
// --------------------------------------------------------------------------------- //
void display_eyes(byte arr[], int hue, bool flirt){
  if(!flirt){
    display_eye(arr, hue, true);
    display_eye(arr, hue, false);
  }
  else{
    display_eye(love, hue, false);
    display_eye(arr, hue, true);
  }
   
}

void display_eye(byte arr[], int hue, bool left) {
  // We will draw a circle on the display
  // It is a hexagonal matrix, which means we have to do some math to know where each pixel is on the screen

  int rows[] = {4, 5, 6, 7, 6, 5, 4};      // The matrix has 4, 5, 6, 7, 6, 5, 4 rows.
  int NUM_COLUMNS = 7;                     // There are 7 columns
  int index = (left) ? 0 : 37;             // If we draw the left eye, we have to add an offset of 37 (4+5+6+7+6=5+4)
  for (int i = 0; i < NUM_COLUMNS; i++) {
    for (int j = 0; j < rows[i]; j++) {
      int brightness = LED_BRIGHTNESS * bitRead(arr[i], (left) ? rows[i] - 1 - j : j);
      pixels.setPixelColor(index, pixels.ColorHSV(hue * 256, 255, brightness));
      index ++;
    }
  }
}

void run_emotions(){
  pixels.clear();  

  switch (emotion) {
    case NEUTRAL:
      if (millis() % 5000 < 150) display_eyes(blink1, 43, false);
      else if (millis() % 5000 < 300) display_eyes(blink2, 43, false);
      else if (millis() % 5000 < 450) display_eyes(blink1, 43, false);
      else display_eyes(neutral, 43, false);

      if (face_detected) {
        servo1_target = 90.0 + float(face.xCenter - 160) / 320.00 * -50.00;
        servo2_target = 90.0 + float(face.yCenter - 120) / 240.00 * 50.00;
      }
      break;
    case OVERWHELMED:
      display_eyes(overwhelmed, 0, false);
      break;
      
    case LOVE:
      if (millis() % 5000 < 150) display_eyes(blink1, 150, true);
      else if (millis() % 5000 < 300) display_eyes(blink2, 150, true);
      else if (millis() % 5000 < 450) display_eyes(blink1, 150, true);
      else display_eyes(love, 150, false);
      
      break;
    
    case LISTENING:
      if (millis() % 5000 < 150) display_eyes(blink1, 85, false);
      else if (millis() % 5000 < 300) display_eyes(blink2, 85, false);
      else if (millis() % 5000 < 450) display_eyes(blink1, 85, false);
      else display_eyes(neutral, 85, false);
  }

  pixels.show();
}

// --------------------------------------------------------------------------------- //
// --------------------------------- COMMUNICATION --------------------------------- //
// --------------------------------------------------------------------------------- //
void communication() {
  char val = ' ';
  String data = "";
  if (Serial.available()) {
    do {
      val = Serial.read();
      if (val != -1) data = data + val;
    }
    while ( val != -1);
  }

  // data is a string of what we received, we will split it into the different values
  // We receive multiple values from our PC as in "123,abc,123,"
  // We can then split this string and extract the values out.
  if (data.length() > 1 && data.charAt(data.length() - 1) == ',') {
    Serial.print(data);
    pc_connected = true; // Once we get a message from the PC, we turn off the touch sensor and do everything with input from the PC

    String value;
    for (int i = 0; data.length() > 0; i++){
      value = data.substring(0, data.indexOf(','));
      data = data.substring(data.indexOf(',') + 1, data.length());

      if (i == 0) servo1_target_pc = value.toInt();
      if (i == 1) servo2_target_pc = value.toInt();
      if (i == 2) {
        if (value == "NEUTRAL") emotion = NEUTRAL;
        if (value == "LOVE") emotion = LOVE;
        if (value == "OVERWHELMED") emotion = OVERWHELMED;
        if (value == "LISTENING") emotion = LISTENING;
      }
      // If more values are needed, add other lines here, e.g. if (i == 3) ...
    }
  }
}
