/*
============
    INIT
============
*/
// include libs
#include <OneWire.h>
#include <DallasTemperature.h>

/* ###################
   ### TEMPERATURE ###
   ################### */
// set pin of the data wire of ds18b20
#define bus_heat 4
// initiate bus on defined pin
OneWire wire_heat(bus_heat);
// tell dallas lib on wich bus the temperature sensors are located
DallasTemperature sensors(&wire_heat);

// set temperature thresholds
const int TempTarget = 35; // Temp that is considered as normal
const int TempCrit   = 45; // Temp that is considered as critical - Trigger to max out all Fans
const int TempLow    = 30; // Temp that is considered as cool     - Trigger to stop all Fans

// initiate the array to store temperatures in.
// The size of the array defines how many sensors are getting queried.
//  - eg. 2 elements means we have 2 sensors
int temperature[] = {0, 0};

// calculate the amount of temperature sensors in this setup
const int tempSensorCount = sizeof(temperature) / sizeof(int);

// define the array to store the temperature from the last measurement
int temperatureLast[5][tempSensorCount];

// calculate the amount of temperature sensors in this setup
const int tempSensorCount = sizeof(temperature) / sizeof(int);

/* ####################
   ### FAN CONRTROL ###
   #################### */
// Varibles used for RPM calculations
int NbTopsFan;
int currentRPM;

// Defines the structure for multiple fans and their dividers
typedef struct{
  char fantype;
  unsigned int fandiv;
}fanspec;

// Definitions of the fans
fanspec fanspace[3]={{0,1},{1,2},{2,8}};

/*
define and initialize values for Fan meassuring and conrolling
FanPinPwm     : set the Pin the PWM Pin is connected (yellow cable)
FanPinSensor  : set the Pin the Hallsensor is connected (blue cable)
FanSensorType : is used to select the devider for RPM calculations
                * 1 for unipole hall effect sensor
                * 2 for bipole hall effect sensor
FanPwmValue   : the PWM value set on the PWM pin (FanPinPwm). The values set here are just inital values
FanRPM        : used to store the RPM in. The values set here are just inital values

                              { fan1, fan2, ... } */
const int FanPinPwm[] =       { 2,    3 };
const int FanPinSensor[] =    { 9,    10 };
const int FanSensorType[] =   { 1,    1 };
      int FanPwmValue[] =     { 20,   20 };
      int FanRPM[] =          { 0,    0 };

// calculate the amount of Fans in this setup
const int FanCount = sizeof(FanPinSensor) / sizeof(int);

/*
=================
    FUNCTIONS
=================
*/
// This is the function that the interupt calls
void rpm () {
    NbTopsFan++;
}

// calculate the current RPM with the measured sensor signal on pinHallsensor
int getRpm (int pinHallsensor, int sensorType) {
    NbTopsFan = 0;	// Set NbTops to 0 ready for calculations
    sei();		    // Enables interrupts
    delay (1000);	// Wait 1 second
    cli();          // Disable interrupts
    currentRPM = ((NbTopsFan * 60)/fanspace[sensorType].fandiv); // Times NbTopsFan (which is apprioxiamately the fequency the fan is spinning at) by 60 seconds before dividing by the fan's divider

    Serial.print ("Sensor: ");
    Serial.print (pinHallsensor);   // print the sensor we read the sensor signal
    Serial.print (" - ");
    Serial.print (currentRPM, DEC); // print the calculated RPM
    Serial.print (" rpm\r\n");

    // return the calculated RPM
    return currentRPM;
}

void getTemperature () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // collect temperatures
    for( int sensor = 0; sensor < tempSensorCount; sensor++ ) {
        temperature[sensor] = sensors.getTempCByIndex(sensor);
    }
}


/*
=============
    SETUP
=============
*/
void setup() {
    // setup Serial
    Serial.begin(115200);

    // initiate temperature sensors
    sensors.begin();

    // define Interupt for RPM measurement
    attachInterrupt(0, rpm, RISING);

    // modify PWM regeister to change PWM freq
    // set PWM freq to 31kHz (31372.55 Hz) on ...
    TCCR3B = TCCR3B & B11111000 | B00000001; // ... Pins D2 D3 D5
    TCCR4B = TCCR4B & B11111000 | B00000001; // ... Pins D6 D7 D8

    // set Pin modes ...
    for( int cnt = 0; cnt < FanCount; cnt++ ) {
        // ... for the Fan Hallsensors (yellow wire)
        pinMode(FanPinSensor[cnt], INPUT);

        // ... for the Fan PWM signal to control the RPM (blue wire)
        pinMode(FanPinPwm[cnt], OUTPUT);
    }
}

/*
============
    LOOP
============
*/
void loop () {

    getTemperature();

    for( int fan = 0; fan < FanCount; fan++ ) {
        FanRPM[fan] = getRpm(FanPinSensor[fan], FanSensorType[fan]);
    }

    // calc needed rpm

    // write the PWM value to the Fan Pins
    for ( int fan = 0; fan < FanCount; fan++ ) {
        analogWrite( FanPinPwm[fan], FanPwmValue[fan] );
    }
}