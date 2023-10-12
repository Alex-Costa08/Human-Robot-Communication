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

int LED_BRIGHTNESS = 10;  // 0-255

HUSKYLENS huskylens;
HUSKYLENSResult face;
bool face_detected = false;
bool prev_touch_value = 0;

enum Emotion {NEUTRAL, OVERWHELMED, LOVE, LISTENING, MOOD, EVANA};
Emotion emotion = NEUTRAL;

Servo servo1, servo2;
float servo1_pos = 90, servo2_pos = 80;
float servo1_target = 90, servo2_target = 90;
float servo1_speed = 0, servo2_speed = 0;

long timer1, timer2, timer3, start_timer1, start_timer2;

bool pc_connected = false;
float servo1_target_pc = 90, servo2_target_pc = 90;

int face_count=0, face_temp = 0, t_counter = 0;

unsigned long previousMusicTime = 0;
unsigned long currentMillis;
bool playing = false;
// --------------------------------------------------------------------------------- //
// ---------------------------------- EYE PATTERNS --------------------------------- //
// --------------------------------------------------------------------------------- //

byte neutral[] = {
  B0000,
  B00000,
  B000000,
  B0000000,
  B000000,
  B11111,
  B1111
};

byte listening[] = {
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

byte mood[] = {
  B0000,
  B00000,
  B000000,
  B0000110,
  B001111,
  B01111,
  B1111
};

byte mood_blink1[] = {
  B0000,
  B00000,
  B000000,
  B0000010,
  B000011,
  B01111,
  B1111
};

byte mood_blink2[] = {
  B0000,
  B00000,
  B000000,
  B0000000,
  B000001,
  B00001,
  B0111
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

  SpecifyMusicPlay(1);            //Start the Song for Neutral State

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
  servo2.write(80);
}

void loop() {

  // Ever 20 milliseconds, update the servos
  if (millis() - timer1 >= 20){
    timer1 = millis();
    move_servos();
    face_temp = husky_lens();
    face_counter();
    state_switching();
    //touch_sensor();
    run_emotions();
  }

  // Every 10 milliseconds, update the huskylens
  if (millis() - timer2 >= 10){
    timer2 = millis();
    //only uses the comms fucntion if it is not in IDLE or PROMOTING INTERACTION (LOVE), in other words it doesn't listen to the users in those states
    if(emotion == LISTENING || emotion == OVERWHELMED || emotion == MOOD || emotion == EVANA){communication();} 
  }

}

// --------------------------------------------------------------------------------- //
// -------------------------------------- FACES------------------------------------- //
// --------------------------------------------------------------------------------- //

//avoid errors when detetcting the number of faces in a room (it has to detect less than 2 faces for more than 3 seconds).
void face_counter(){

  if(face_count == face_temp){t_counter = 0;}

  else if(face_count < face_temp){face_count = face_temp; t_counter = 0;}

  else{
    t_counter++;
    //considering this function is called every 20ms, 50 iteractions is equal to 1 sec.
    if(t_counter >= 150){face_count = face_temp; t_counter = 0;}
  }

}

//After we have all the sounds, we need to change the indexes of the function to play the correct ones!!!
void state_switching(){

  //Assure that the sounds being played are the right ones!!!

  switch (emotion) {
    case  NEUTRAL:
        if(face_count > 1){emotion = LOVE; SpecifyMusicPlay(2); servo2_target = 120; start_timer1 = millis();LED_BRIGHTNESS = 80;}

        break;
        
    case LOVE:
        if(face_count < 2){emotion = NEUTRAL; SpecifyMusicPlay(1); servo2_target = 100;LED_BRIGHTNESS = 10;}
        if((millis() - start_timer1) > 5000) {emotion = LISTENING; clearSerialBuffer();}  //we call the clean buffer function so it cleans everything that was said in the NEUTRAL/LOVE states

        break;

    case LISTENING:
        if(face_count < 2){emotion = NEUTRAL;SpecifyMusicPlay(1);servo2_target = 100;LED_BRIGHTNESS = 10;}
        //the switch to MOOD and OVERWHELMED is done in comunication()

        break;

    case OVERWHELMED:
      if((millis() - start_timer2) > 8000) emotion = LISTENING;
      break;

    case MOOD:
      if((millis() - start_timer2) > 8000)  emotion = LISTENING;
      break;

  }
  
}

// --------------------------------------------------------------------------------- //
// -------------------------------------- EYES ------------------------------------- //
// --------------------------------------------------------------------------------- //
void display_eyes(byte arr[], int hue, int mode){

  switch(mode){
    case 1:
      //normal case (both eyes syncronized)
      display_eye(arr, hue, true);
      display_eye(arr, hue, false);
      break;
  
    case 2:
      //flirt case (only the left eye blinks)
      display_eye(love, hue, false);
      display_eye(arr, hue, true);
      break;
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
      display_eyes(neutral, 110, 1);
      /*if (millis() % 5000 < 150) display_eyes(blink1, 43, 1);
      else if (millis() % 5000 < 300) display_eyes(blink2, 43, 1);
      else if (millis() % 5000 < 450) display_eyes(blink1, 43, 1);
      else display_eyes(neutral, 43, 1);*/

      /*if (face_detected) {
        servo1_target = 90.0 + float(face.xCenter - 160) / 320.00 * -50.00;
        servo2_target = 90.0 + float(face.yCenter - 120) / 240.00 * 50.00;
      }*/

      if (millis() % 10000 < 2000){servo1_target = 90;servo2_target = 100;}
      else if (millis() % 10000 < 3000) servo1_target = 120;
      else if (millis() % 10000 < 4000) servo1_target = 50;
      else servo1_target = 90;

      break;

    case OVERWHELMED:
      //change the movement so the servo1 is faster than the servo2!
      display_eyes(overwhelmed, 0, 1);
      if (millis() % 1500 < 500){servo1_target = 120;servo2_target = 120;}
      else if (millis() % 1500 < 1000) {servo1_target = 90;servo2_target = 140;}
      else {servo1_target = 40;servo2_target = 120;}

      currentMillis = millis();
      if (currentMillis - previousMusicTime >= 3000) {
        SpecifyMusicPlay(8);    
        previousMusicTime = currentMillis;  // Save the current time as the new previous time
      }

      break;
      
    
    case LOVE:
      if (millis() % 2500 < 150) display_eyes(blink1, 150, 2);
      else if (millis() % 2500 < 300) display_eyes(blink2, 150, 2);
      else if (millis() % 2500 < 450) display_eyes(blink1, 150, 2);
      else display_eyes(love, 150, 1);
      
      if (millis() % 3500 < 800){servo1_target = 90;servo2_target = 120;}
      else if (millis() % 3500 < 1300) servo1_target = 120;
      else if (millis() % 3500 < 1800) servo1_target = 50;
      else servo1_target = 90;

      break;
    
    case LISTENING:
      if (millis() % 5000 < 150) display_eyes(blink1, 85, 1);
      else if (millis() % 5000 < 300) display_eyes(blink2, 85, 1);
      else if (millis() % 5000 < 450) display_eyes(blink1, 85, 1);
      else display_eyes(listening, 85, 1);

      if (millis() % 6000 < 500){servo1_target = 90;servo2_target = 150;}
      else if (millis() % 6000 < 2500) {servo1_target = 120;servo2_target = 160;}
      else if (millis() % 6000 < 4500) {servo2_target = 150;}
      else if (millis() % 6000 < 5000) {servo1_target = 50;servo2_target = 160;}
      else servo1_target = 90;

      break;
    case MOOD:
      if (millis() % 2000 < 150) display_eyes(mood_blink1, 100, 1);
      else if (millis() % 2000 < 300) display_eyes(mood_blink2, 100, 1);
      else if (millis() % 2000 < 450) display_eyes(mood_blink1, 100, 1);
      else display_eyes(mood, 100, 1);
      

      /*if (millis() % 3000 < 750){servo1_target = 90;servo2_target = 140;}
      else if (millis() % 3000 < 1200) servo1_target = 120;
      else if (millis() % 3000 < 1700) servo1_target = 50;
      else servo1_target = 90;*/

      if (millis() % 6000 < 500){servo1_target = 90;servo2_target = 150;}
      else if (millis() % 6000 < 2500) {servo1_target = 120;servo2_target = 160;}
      else if (millis() % 6000 < 4500) {servo2_target = 150;}
      else if (millis() % 6000 < 5000) {servo1_target = 50;servo2_target = 160;}
      else servo1_target = 90;

      currentMillis = millis();
      if (currentMillis - previousMusicTime >= 3000) {
        SpecifyMusicPlay(2);    
        previousMusicTime = currentMillis;  // Save the current time as the new previous time
      }


      break;
    case EVANA:
      display_eyes(overwhelmed, 175, 1);
      if(!playing){
        SpecifyMusicPlay(12); 
        playing = true;
      }
      
      break;

  }

  pixels.show();
}

// --------------------------------------------------------------------------------- //
// ------------------------------------ CAMERA  ------------------------------------ //
// --------------------------------------------------------------------------------- //
int husky_lens() {
  if (!huskylens.request()) {}
  else if (!huskylens.available()) {
  //   Serial.println(F("No face appears on the screen!"));
    face_detected = false;
  } else {
  //    Serial.println(F("###########"));

  // We loop through all faces received by the HuskyLens. If it's a face that we've learned (ID=1), we will track that face.
  // If no learned face is on the screen, we take the first face returned (which is the face closest to the center)
  face_detected = false;
  int face_index = 0;
  while (huskylens.available()) {
    HUSKYLENSResult result = huskylens.read();
    if (result.command == COMMAND_RETURN_BLOCK) {
      //        Serial.println(String() + F("Block:xCenter=") + result.xCenter + F(",yCenter=") + result.yCenter + F(",width=") + result.width + F(",height=") + result.height + F(",ID=") + result.ID);
      if (face_index == 0 || result.ID == 1) face = result;
      face_index ++;
      face_detected = true;
    }
  }
  return face_index;
  //    Serial.println(String() + F("Block:xCenter=") + face.xCenter + F(",yCenter=") + face.yCenter + F(",width=") + face.width + F(",height=") + face.height + F(",ID=") + face.ID);
  }

  return 0;
}

// --------------------------------------------------------------------------------- //
// ---------------------------------- SERVO MOTORS --------------------------------- //
// --------------------------------------------------------------------------------- //
void move_servos(){
  // We apply some smoothing to the servos and limit the speed
  // We do this because abrubt movements cause a big spike in current draw
  // If we are connected to the PC, we use the PC angles. Otherwise we use the angles from the Arduino
  float servo1_target_ = (pc_connected) ? servo1_target_pc : servo1_target;
  float servo2_target_ = (pc_connected) ? servo2_target_pc : servo2_target;

  if (abs(servo1_target_ - servo1_pos) < 1) {
    servo1.write(servo1_target_);
    servo1_pos = servo1_target_;
  } else {
    servo1_speed = constrain(constrain(servo1_target_ - servo1_pos, servo1_speed - 0.1, servo1_speed + 0.1), -1.0, 1.0);
    servo1_pos += servo1_speed;
    servo1.write(servo1_pos);
  }

  if (abs(servo2_target_ - servo2_pos) < 1) {
    servo2.write(servo2_target_);
    servo2_pos = servo2_target_;
  } else {
    servo2_speed = constrain(constrain(servo2_target_ - servo2_pos, servo2_speed - 0.1, servo2_speed + 0.1), -1.0, 1.0);
    servo2_pos += servo2_speed;
    servo2.write(servo2_pos);
  }
}

// --------------------------------------------------------------------------------- //
// --------------------------------- COMMUNICATION --------------------------------- //
// --------------------------------------------------------------------------------- //

void communication() {
  char val = ' ';
  String data = "";

  //Serial.println("SENDING"); 
  
  //clearSerialBuffer();

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
    //Serial.print(data);
    //pc_connected = true; // Once we get a message from the PC, we turn off the touch sensor and do everything with input from the PC

    String value;
    for (int i = 0; data.length() > 0; i++){
      value = data.substring(0, data.indexOf(','));
      data = data.substring(data.indexOf(',') + 1, data.length());
  
      if (i == 0) servo1_target_pc = value.toInt();
      if (i == 1) servo2_target_pc = value.toInt();
      if(i == 2) {
        if (value == "OVERWHELMED") {emotion = OVERWHELMED; start_timer2 = millis();}
        if (value == "MOOD") {emotion = MOOD;servo2_target = 140; start_timer2 = millis();}
        if (value == "TECHNO") {emotion = EVANA;playing=false;}
        }
    }
  }
}

void clearSerialBuffer() {
  while (Serial.available() > 0) {
    char incomingData = Serial.read(); // Read and discard data from the buffer 
  }
}
