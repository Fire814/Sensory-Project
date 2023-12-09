//Import statement for TinyPICO Helper Library
#include <TinyPICO.h>

//Initialize TinyPICO Library
TinyPICO tp = TinyPICO();

/******************************************************************************************/

//Import Statements for the sensors, heartrate, and SPO2
#include <Wire.h>
#include <MAX30105.h>

//Heartrate import
#include "heartRate.h"

// SPO2 import
#include "spo2_algorithm.h"

/******************************************************************************************/
//NeoPixel Global Variabless
#include <Adafruit_NeoPixel.h>

//Declaring how many NeoPixels are attached to the Arduino
#define LED_COUNT 1

//Define the pin-out for the LED
#define LED_PIN 5

//Declare NeoPixel strip object(s) (Number of pixels in count, pinNUmber, neo_GRB, NEO_KHZ800)
Adafruit_NeoPixel pixels(1, 5, NEO_GRB + NEO_KHZ800);

//Defines the intensity/saturation of color (Red, Green, Blue), NOT the brightness LED Range
int brightness = 0; //Brightness of LED

/******************************************************************************************/
//Heartrate Sensor Global Variables

//Calls particle sensor
MAX30105 particleSensor;

//Variable to store the maximum brightness of the heartrate sensor's red LED for reading blood-oxygen
#define MAX_BRIGHTNESS 255

//Heartrate global variables

//Byte rate value for averaging (the higher, the more averaging done). 4 is good
//Indicates the amount of readings that will be averaged for each printed heart rate
const byte RATE_SIZE = 4;

//Array of heart rates to help calculate the average
byte rates[RATE_SIZE];

//Initial value for the rate spot
byte rateSpot = 0;

//Time in which the last beat occurred
long lastBeat = 0;

//Determine if the sensor reading is an actual heartbeat (0 if false, 1 if true)
bool sensedBeat = 0;

//Most recently calculated beat per minute (bpm)
float beatsPerMinute;

//Average of the last array's bpm's
int beatAvg;

//Sensor reading to calculate bpm
long irValue;

//Determine if the sensor is contacting the skin or not (0 if no, 1 if yes)
bool heartFingerOn = 0;

//SPO2 global variables

//Sets the array size of the SPO2 to 30
#define SPO2ArraySize 30

//Create double values for the blood-ox result, IR reading, and red LED on the MAX30105
double SPO2_result;
double SPO2_IR;
double SPO2_RED;


//Variable for counting in the code
int count = 0;

/******************************************************************************************/

void setup() {
  //SPO2 and Heartrate use 115200 serial baud rate
  Serial.begin(115200);

  //Initialization of the MAX30105 sensor for both SPO2 AND Heartrate
  maxInitialization();
}

/* -------------------------------------------------------------------------------------- */

//Code to check if the MAX30105 sensor was proper initialized
void maxInitialization(){

  //Uses the default I2C port, sets the sensor read/write speed to 400 kHz
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)){
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1);
  }
}

/* -------------------------------------------------------------------------------------- */

void loop() {

  //Cycles the LED on the TinyPICO every 25 milliseconds
  tp.DotStar_CycleColor(25);
  
  
  //Calls function to set the MAX30105 sensor to read SPO2 (blood ox)
  checkSPO2Initialization();

  //Gives a second for the sensor to set parameters to read for SPO2
  delay(1000);

  //Calls function to calculate SPO2 value
  checkSPO2(SPO2_result, SPO2_IR, SPO2_RED);

  //Prints out SPO2_result
//  Serial.print("SPO2_result = ");
//  Serial.println(SPO2_result);

  //Calls function to set the MAX30105 sensor to read heart rate
  checkHeartInitialization();

  //Give a second for sensor to set params to read heart rate
  delay(1000);

  //Printing multiple results (1000 times)
  for(int i = 0; i < 10000; i ++){

    //Function to check hear rate
    checkHeart(irValue, beatsPerMinute, beatAvg);

    //Prints out all of the results
    if (sensedBeat == 1){
    Serial.print("bpm_notavg = ");
    Serial.println(beatsPerMinute);
    Serial.print("bpm_avg = ");
    Serial.println(beatAvg);
    Serial.print("SPO2_result = ");
    Serial.println(SPO2_result);
    }

    //Wait 10ms between iterations so the device isnt overloaded
    delay(10);
  }

}

/* -------------------------------------------------------------------------------------- */
//Function that can be called to manually cycle the RGB between different colors in the different functions below
void cycleColor(int time){
  for(int i = 0; i < 255; i++){
    for(int j = 0; j < 255; j++){
      //Sets color for LED (in like a queue of size of amount of led which is 1)
      pixels.setPixelColor(j, pixels.Color(brightness, i, j));
    }
  }
  pixels.show();
  delay(time); //Delays the loop based on the inputted time variables
  
}

/* -------------------------------------------------------------------------------------- */
//Function to set the MAX30105 to read for heart rate
void checkHeartInitialization(){
  
  //Configure the MAX30105 sensor with its default settings
  particleSensor.setup();

  //Turn Red LED to low to indicate the sensor is running
  particleSensor.setPulseAmplitudeRed(0x0A);

  //Turn off the green LED
  particleSensor.setPulseAmplitudeGreen(0);
  
}

/* -------------------------------------------------------------------------------------- */
//Function to detect, calculate, and store/calculate the (average) heart rate from the last 4 readings
void checkHeart(long irValue, float beatsPerMinute, int beatAvg){

  //Obtain the sensor reading
  irValue = particleSensor.getIR();

  //Check if the sensor has picked up a heartbeat using the hearbeat algorithm from the GitHub library
  if (checkForBeat(irValue) == true){

    //Sets boolean to true
    sensedBeat = 1;

    //Prints out code indicating a beat was sensed by the sensor
    Serial.println("##########################################################################################################################################################################");
    Serial.println("##########################################################################################################################################################################");
    Serial.println("We sensed a beat!");
    Serial.println("##########################################################################################################################################################################");
    Serial.println("##########################################################################################################################################################################");
    

    long delta = millis() - lastBeat;
    lastBeat = millis();

    //Calculate heart bpm (which is 60 seconds over the delta milliseconds converted to seconds
    beatsPerMinute = 60 / (delta / 1000.0);

    //The average bpm displayed will only be changed if the calculated bpm is somewhat realistic (i.e., less than 255 and greater than 20 bpm)
    if (beatsPerMinute < 255 && beatsPerMinute > 20){
      
      //Converst the new bpm into a byte, and stroes it in the originally initialized array in the next spot
      rates[rateSpot++] = (byte)beatsPerMinute;

      //Wrap array index to always be in range [0, RATE_SIZE)
      rateSpot %= RATE_SIZE; 


      //Takes the average of the last RATE_SIZE array bpm readings
      beatAvg = 0;
      for(byte x = 0; x < RATE_SIZE; x++){
        beatAvg += rates[x];
      }
      
      //Divides the avg beat by 4 to get an actual reading
      beatAvg /= RATE_SIZE; 
    }
  } else{
    //Heart beat not sensed, boolean set to false or 0
    sensedBeat = 0;

    //Print statement to print out heartbeat
    //Serial.println("NOT BEAT");
    
    //Lowers the brightness
    brightness = 100;
  }
  
    //Checking if a finger is placed on the MAX30105 sensor 
    //If irValue detection value is less than 50000, no finger is detected
    if (irValue < 50000) {
      Serial.println("ERROR HEARTBEAT: No finger???"); 
      
      //Sets the LED on the TinyPICO using (R, G, B) values
      //tp.DotStar_SetPixelColor(255, 0, 0);
      
      heartFingerOn = 0;
    } 
    
    else {
      heartFingerOn = 1;
    }

    //Command to turn off all pixel colors on the NeoPixel RGB
    pixels.clear(); 


    //Blue LED ON This code will be rebased for Bluetooth implementation
    if (heartFingerOn == 0){  
  //    pixels.setPixelColor(0, pixels.Color(0,0 , brightness));
  //    pixels.show();
        //pixels.clear(); // Set all pixel colors to 'off'
        pixels.show();
    }

    //When there is a finger on the sensor and it's detecting a heartrate
    else {
      //Turn on the Green LED when SPO2 is at least 90, indicating good blood ox
      if (SPO2_result >= 90) {
        
        //Sets color for LED (in like a queue of size of amount of led which is 1)
        pixels.setPixelColor(0, pixels.Color(0, brightness, 0));
        
        //Executes the que of set colors
        pixels.show();

        //Cycles the NeoPixel's colors when there is a good reading, based on the inputted time variable
        cycleColor(100);

        //Cycles the LED on the TinyPICO every 25 milliseconds
        //tp.DotStar_CycleColor(25);
      
      //Turn on the Yellow LED when the SPO2 is between 85 and 90, indicating ok blood ox
      if (85 <= SPO2_result < 90) {
      
        //Sets color for LED (in like a queue of size of amount of led which is 1)
        //pixels.setPixelColor(0, pixels.Color(brightness, brightness, 0));
        
        //Executes the que of set colors on the NeoPixel
        //pixels.show();

        //Sets the LED on the TinyPICO using (R, G, B) values to yellow
        //tp.DotStar_SetPixelColor(0, 255, 255);
        
      }
      
      //Turn the Red LED on when the SPO2 is below 85, indicating bad blood ox
      if (SPO2_result < 85) {
      
        //Sets color for LED (in like a queue of size of amount of led which is 1)
        pixels.setPixelColor(0, pixels.Color(brightness, 0, 0));
        
        //Executes the que of set colors on the NeoPixel
        pixels.show();

        //Sets the LED on the TinyPICO using (R, G, B) values to orange
        //tp.DotStar_SetPixelColor(255, 165, 0);
      }
    } 
  }
}

/* -------------------------------------------------------------------------------------- */
//Function to set the MAX30105 parameters to read for SPO2
void checkSPO2Initialization() {

  //Sets the LED brightness on the sensor
  byte ledBrightness = 60; // Options: 0=Off to 255=50mA

  //Determines the sampling average, or how many samples to average for an actual reading
  byte sampleAverage = 4; // Options: 1, 2, 4, 8, 16, 32

  //Sets the LED color of the sensor
  byte ledMode = 2; // Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green

  //Sets the sampling rate, or how many samples are read between averages
  byte sampleRate = 100; // Options: 50, 100, 200, 400, 800, 1000, 1600, 3200

  int pulseWidth = 411; // Options: 69, 118, 215, 411
  
  int adcRange = 4096; // Options: 2048, 4096, 8192, 16384

  //Setup the MAX3015 sensor with the above specified values
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

/* -------------------------------------------------------------------------------------- */
//Function to calculate and store the SPO2 results
void checkSPO2(double& SPO2_result, double& SPO2_IR, double& SPO2_RED) {
  
  //Infrared LED sensor data
  uint16_t irArray[SPO2ArraySize]; // Infrared LED sensor data
  
  //Red LED sensor data
  uint16_t redArray[SPO2ArraySize];
  long SPO2nowTime[SPO2ArraySize];

  //Stores the valley-peak-valley ir-data and time
  uint16_t irVPV[3][2]; // Stores valley-peak-valley and time

  //Stores the valley-peak-valley data and time
  uint16_t redVPV[3][2];

  for (int i = 0; i < SPO2ArraySize; i++) {
    
    //Check if there is new data from the sensor
    while (particleSensor.available() == false){
      //Check sensor for data
      particleSensor.check(); 
    }
    
    //Use google sheets split to get data in columns
    redArray[i] = particleSensor.getRed();
    irArray[i] = particleSensor.getIR();
    SPO2nowTime[i] = millis();

    //Tell MAX30105 to move to next sample for calculations and reading
    particleSensor.nextSample();
  }

  //Setting initial values for the peak/valleys of sensor readings
  byte redFirstValley = 0;
  byte irFirstValley = 0;
  byte redPeaked = 0;
  byte irPeaked = 0;

  //For loop to help create the proper arrays and averaging/smoothing of heart and SPO2 data
  //For loop is a low/high pass filter to ensure no extraneous values are being read
  for (int i = 0; i < SPO2ArraySize; i++) {
    if (i > 0) {
      if ((redArray[i] < redArray[i-1]) && (redArray[i] < redArray[i+1])) {
        if (redFirstValley == 0) {
          redVPV[0][0] = redArray[i];
          redVPV[0][1] = SPO2nowTime[i];
          redFirstValley = 1;
        } else if ((redFirstValley == 1) && (redPeaked == 1)) {
          redVPV[2][0] = redArray[i];
          redVPV[2][1] = SPO2nowTime[i];
          redFirstValley = 2;
        }
      }
      if ((redPeaked == 0) && (redFirstValley == 1)) {
        if ((redArray[i] > redArray[i-1]) && (redArray[i] > redArray[i+1])) {
          redVPV[1][0] = redArray[i];
          redVPV[1][1] = SPO2nowTime[i];
          redPeaked = 1;
        }
      }
      if ((irArray[i] < irArray[i-1]) && (irArray[i] < irArray[i+1])) {
        if (irFirstValley == 0) {
          irVPV[0][0] = irArray[i];
          irVPV[0][1] = SPO2nowTime[i];
          irFirstValley = 1;
        } else if ((irFirstValley == 1)  && (irPeaked == 1)) {
          irVPV[2][0] = irArray[i];
          irVPV[2][1] = SPO2nowTime[i];
          irFirstValley = 2;
        }
      }
      if ((irPeaked == 0) && (irFirstValley == 1)) {
        if ((irArray[i] > irArray[i-1]) && (irArray[i] > irArray[i+1])) {
          irVPV[1][0] = irArray[i];
          irVPV[1][1] = SPO2nowTime[i];
          irPeaked = 1;
        }
      }
    }
  }
  
//Commented-out print values for the different sensor readings
//  Serial.print("R V1: ");
//  Serial.print(redVPV[0][0]);
//  Serial.print(" ");
//  Serial.print(redVPV[0][1]);
//  Serial.println();
//  Serial.print("R P: ");
//  Serial.print(redVPV[1][0]);
//  Serial.print(" ");
//  Serial.print(redVPV[1][1]);
//  Serial.println();
//  Serial.print("R V2: ");
//  Serial.print(redVPV[2][0]);
//  Serial.print(" ");
//  Serial.print(redVPV[2][1]);
//  Serial.println();
//  Serial.print("IR V1: ");
//  Serial.print(irVPV[0][0]);
//  Serial.print(" ");
//  Serial.print(irVPV[0][1]);
//  Serial.println();
//  Serial.print("IR P: ");
//  Serial.print(irVPV[1][0]);
//  Serial.print(" ");
//  Serial.print(irVPV[1][1]);
//  Serial.println();
//  Serial.print("IR V2: ");
//  Serial.print(irVPV[2][0]);
//  Serial.print(" ");
//  Serial.print(irVPV[2][1]);
//  Serial.println();

  //Calculates the AC/DC voltages based on the above array values
  double rDC = redVPV[0][0] + ((redVPV[2][0]-redVPV[0][0])/(redVPV[2][1]-redVPV[0][1]))*(redVPV[1][1] - redVPV[0][1]);
  double rAC = redVPV[1][0] - rDC;
  double irDC = irVPV[0][0] + ((irVPV[2][0]-irVPV[0][0])/(irVPV[2][1]-irVPV[0][1]))*(irVPV[1][1] - irVPV[0][1]);
  double irAC = irVPV[1][0] - irDC;
  double R = (rAC/rDC)/(irAC/irDC);
  SPO2_RED = rDC;
  SPO2_IR = irDC;
  
//  if ((R >= 0.4)&&(R <= 3.4)) {
//    double SPO2_result = 104.0 - 17.0*R;
//    Serial.println(SPO2_result);
//  }

  //Calculates the SPO2_result
  SPO2_result = 104.0 - 17.0*R;

}
