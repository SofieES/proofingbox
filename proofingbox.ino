// *************************************************************************
// Arduino Leonardo based Proofing cabinet
// version 1.0 Elmer Stickel
// date: 11/04/2019
// Description: first attempt to build a liquid-based bread proofing cabinet
// using an Arduino Leonardo, potmeter, 2x16 LCD, thermometer, two relays,
// heater, buzzer  and aquariumpump. On the LCD the target-temperature (Tt),
// current temperature (Tc), status of the pump and heater and the run-time
// are displayed. A number of safety checks are performed and in case of a
// possible problem the system is shut down, an error message is shown and a
// buzzer produces an interval beep.
// *************************************************************************

#include <Wire.h> 
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// Global variables
#define ONE_WIRE_BUS 5 // Pin that is used for I2C communication with thermometer
#define ON   0
#define OFF  1
#define DHTPIN 8
#define DHTTYPE DHT11


// *************************************************************************
// ** The following variables need to be adjusted to your own setup! **
// *************************************************************************
int MinTemp = 5; // Lowest possible temp
int MaxTemp = 35; // Highest possible temp. (check max operating temp of your pump!)
int WaterVolume = 10; // in Liters
int PowerHeater = 300; // in Watts
int MaxRunTime = 1440; // Timelimit system is allowed to run (in mins)
// **************************************************************************

const int HEATER_PIN = 4; // Pin connected to heater relais
const int PUMP_PIN = 6; // Pin connected to pump-relais
const int POTMETER_PIN = A0; // Pin connected to POTMETER_PIN
const int BEEP_PIN = 7;
float TargetTemperature;
int HeaterStatus = OFF;
int PumpStatus = OFF;
int PreviousHeaterStatus = OFF;

float MaxHeaterOnTime;

unsigned long RunTime;
unsigned long TimeHeaterOn;
unsigned long TimeLastSafetyCheck;

// For storing and averaging the last measurements
const int NumberOfMeasurements = 10;
float Measurements[NumberOfMeasurements];
float SumMeasurements;
float AverageTemp;
float TempLastSafetyCheck; // a 30 seconds old value

int IntTemp;

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
LiquidCrystal_I2C lcd(0x27,16,2);
DHT dht(DHTPIN, DHTTYPE);

void setup(void)
{
 Serial.begin(9600); 
 dht.begin();
 pinMode(4, OUTPUT); // set pin that controls the relais for heater as OUTPUT pin
 pinMode(6, OUTPUT); // set pin that controls the relais for pump as OUTPUT pin
 digitalWrite(HEATER_PIN, OFF);
 sensors.begin();
 MaxHeaterOnTime = MaxTemp*WaterVolume*4.1868*1000000/PowerHeater; // in millis
 initScreen();
 initHeater();
 TempLastSafetyCheck=AverageTemp;
 initDashboard();
 PumpStatus = ON;
 getInternalTemp();
}

// Get reliable temperature reading before continuing.
void initHeater(void){
  lcd.print("Initializing...");
  unsigned long time_now;
  for (int i=0;i<NumberOfMeasurements;i++){
    time_now = millis();
    getTemperature();
    while(millis() < time_now + 1000){;}
  }
}

void getInternalTemp(void)
{
  IntTemp = dht.readTemperature();
}

void initScreen(void)
{
 // ** Start the LCD screen and print static information
 
 // ** Symbols for pump and heater made using https://maxpromer.github.io/LCD-Character-Creator/
 byte pump[] = {B00100,B01110,B10101,B00100,B00100,B00100,B00100,B00100};
 byte heater[] = {B00000,B00010, B10001, B01001, B01010, B10010, B10001,B01000};

 lcd.init();
 lcd.backlight();
 lcd.createChar(0, pump);
 lcd.createChar(1, heater);
 lcd.clear();
}

// ** For getting ever present text and icons on the LCD
//
void initDashboard(void)
{
 lcd.clear();
 lcd.setCursor(0,0);
 lcd.print("Tt");
 lcd.setCursor(0,1);
 lcd.print("Tc");
}


// ** For reading the current temperature and averaging over the last 10 readings
void getTemperature(void)
{
 int i=NumberOfMeasurements-1;
 float CurrentTemperature;
 SumMeasurements=SumMeasurements-Measurements[i]; // Remove oldest measurement from total
 for (i; i>0; i--){  // Shift the array with measurements one value to right
  Measurements[i]=Measurements[i-1];
 }
 sensors.requestTemperatures(); // Send the command to get temperature readings 
 CurrentTemperature = sensors.getTempCByIndex(0);
 Measurements[0]=CurrentTemperature;
 SumMeasurements=SumMeasurements+Measurements[0];
 AverageTemp=SumMeasurements/float(NumberOfMeasurements);
}


// ** For controlling the heater
void controlHeater(void){
 if ((AverageTemp+0.25) < TargetTemperature && HeaterStatus == OFF) { // Precision of the thermometer is 0.5deg C.
   changeHeaterStatus(ON);
 }
 else if (HeaterStatus == ON && AverageTemp >= TargetTemperature){
  changeHeaterStatus(OFF);
 }
}

// ** For reading the target temperature from pot meter **
void getTargetTemperature(void){
 int sensorValue[10];
 int sum_sensorValue=0;
 int avg_sensorValue;
 for (int i = 0; i<10; i++){ // Take 10 reading and average
  sensorValue[i]=analogRead(POTMETER_PIN);
  sum_sensorValue=sum_sensorValue+sensorValue[i];
 }
 avg_sensorValue=sum_sensorValue/10;
 //Check whether the target temperature has changed. If so, reset the timer and set T_TARGET
 TargetTemperature = (((map(avg_sensorValue, 0, 1023, (MinTemp*10), (MaxTemp*10))+4)/5)*5)/10.0; // Convert to half-degree values
}


void changeHeaterStatus(int status){
  digitalWrite(HEATER_PIN, status);
  HeaterStatus = status;
  TimeHeaterOn=millis(); //reset timer
}


void changePumpStatus(int status){
  digitalWrite(PUMP_PIN, status);
  PumpStatus = status;
}


void updateLCD(void){
   // ** For printing information in the LCD
 lcd.setCursor(3,0);
 lcd.print(TargetTemperature,1);
 lcd.setCursor(3,1);
 lcd.print(AverageTemp,1);
 lcd.setCursor(12,0);
 if (HeaterStatus == ON){
  lcd.write(1);
 }
 else {
  lcd.print(' ');
 }
 if (PumpStatus == ON){
  lcd.write(0);
 }
 else {
  lcd.print(' ');
 }
 lcd.setCursor(9,1); // update time
 lcd.print(RunTime);
 lcd.print("min");
}

void loop(void) 
{
 unsigned long TimeNow=millis();
 RunTime=TimeNow/1000/60; // update RunTime, in mins
 safetyCheck();
 getTargetTemperature();
 getTemperature();
 controlHeater();
 updateLCD();
} 

void safetyCheck()
{
  // Checks if thermometer works by checking whether the temperature rises when the heater is on.
  // Checks whether the heater hasn't reached a safety time MaxHeaterOnTime (in milliseconds).
  // Checks if watertemp is not above TempMax.
  // Checks if MaxRunTime is not exceeded
  // In case there's something wrong, heater and pump are turned off and, a warning is displayed and
  // beep produces. Check is only performed every 30 seconds.
  unsigned long TimeNow = millis();
  if (TimeNow>(TimeLastSafetyCheck+120000)){ //
    TimeLastSafetyCheck=millis(); // reset timer
    if (PreviousHeaterStatus==HeaterStatus && HeaterStatus==ON){
      if (AverageTemp<=TempLastSafetyCheck){ // 120 seconds of heating didn't cause temp increase, likely something wrong!
        missionAbort("E1-no T increase");
      }
      else{
        TempLastSafetyCheck=AverageTemp;
      }
      if ((TimeNow-TimeHeaterOn)>MaxHeaterOnTime){
        missionAbort("E2-heat time lim");
      }
    }
    else if (HeaterStatus==ON){
      PreviousHeaterStatus=ON;
    }
    else if (HeaterStatus==OFF){
      PreviousHeaterStatus=OFF;
    }
  if (((TimeNow)/1000/60)>MaxRunTime){
    missionAbort("E4-timelim excee");
  }
  if (AverageTemp>MaxTemp){
    missionAbort("E3-temp too high");
  }
  getInternalTemp();
  if (IntTemp > 40){
    missionAbort("E5-int temp >40C");
  }
  }
}

void missionAbort(char message[]){
  changeHeaterStatus(OFF);
  changePumpStatus(OFF);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("System halted");
  lcd.setCursor(0,1);
  lcd.print(message);
  while(1 == 1){ // Infitite loop to stop everything
      tone(7,2000,1000);
      delay(15000);
  } 
}
