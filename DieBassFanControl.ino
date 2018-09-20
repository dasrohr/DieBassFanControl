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
const int tempTarget = 35; // Temperature that is considered as normal
const int tempCrit   = 45; // Temperature that is considered as critical - Trigger to max out all Fans
const int tempLow    = 30; // Temperature that is considered as cool     - Trigger to stop all Fans

// define how many Temperature measurements we will remember
const int tempHistoryLength = 5;

// define how many temerature sensors we are going to use with OneWire
const int tempSensorCount = 2;

/* initiate a 3D array to store temperatures and the past TempHistoryLength measurements in.
   The size of the second array defines how many sensors are getting queried.
    - eg. 2 elements means we have 2 sensors

   how history should work:
    - temperature[0][] will contain the most recent values for each sensor
    - before a temperature gets measured, we alter the array. [tempHistoryLength - 1][] -> [tempHistoryLength - 2][] -> and so on ...
      ending up with [0][] and [1][] beeing the same so that [0][] can be replaced with the new current temperatures 
   with that we can calculate better if the temperature is falling or rising to avoid flapping.
   I intend to use the past values to calculate an average and compare this one with the current value. this should make things more smooth
     when the fans speeds are set. */
float temperature[tempSensorCount][tempHistoryLength];

// initiate the array to store the counter in which indicates the consecutive occurences of measuring errors or measured values that can not be true
int tempMeasureErrorCount[tempSensorCount];

// define the amount of bad measurements that are considered as bad
// reaching this limit means that cooling can not be ensured as the temperature is unknown
// this will result in a 'last resort' action and turn on the FANs to the max level
const int tempMeasureErrorCountLimit = 10;

/* ####################
   ### FAN CONRTROL ###
   #################### */
// Defines the structure for multiple fans and their dividers
typedef struct{
  char fantype;
  unsigned int fandiv;
}fanspec;

// Definitions of the fans
fanspec fanspace[3]={{0,1},{1,2},{2,8}};

/*
define and initialize values for Fan meassuring and conrolling
FanPinPWM     : set the Pin the PWM Pin is connected (yellow cable)
FanPinSensor  : set the Pin the Hallsensor is connected (blue cable)
FanSensorType : is used to select the devider for RPM calculations
                * 1 for unipole hall effect sensor
                * 2 for bipole hall effect sensor
FanPWMValue   : the PWM value set on the PWM pin (FanPinPWM). The values set here are just inital values

                                { fan1, fan2, ... } */
const int FanPinPWM[] =         { 6,  7,  8, 44, 45, 46 };
const int FanPinSensor[] =      { 2,  3, 21, 20, 19, 18 };
const int FanSensorType[] =     { 1,  1,  1,  1,  1,  1 };
      int FanPWMValue[] =       { 20, 20, 20, 20, 20, 20 };

// calculate the amount of Fans in this setup
const int FanCount = sizeof(FanPinSensor) / sizeof(int);

// define the maximum PWM value that can be set (usually 255)
const int MaxPWM = 255;
// define the minimum PWM value that can be set
const int MinPWM = 15;

// define the steps to adjuste the PWM value
const int PWMStep = 5;

/*
=================
    FUNCTIONS
=================
*/
// those functions are executed when the specific FAN creates an interrupt
// the functions are basically just counting the ammount of interrupts
// they are NOT allowed to return anything 
void FanInterrupt0 () { FanInterruptValue[0]++; }
void FanInterrupt1 () { FanInterruptValue[1]++; }
void FanInterrupt2 () { FanInterruptValue[2]++; }
void FanInterrupt3 () { FanInterruptValue[3]++; }
void FanInterrupt4 () { FanInterruptValue[4]++; }
void FanInterrupt5 () { FanInterruptValue[5]++; }


void getTemperature () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // collect temperatures
    for( int sensor = 0; sensor < tempSensorCount; sensor++ ) {
        // preserve historical measurements
        for ( int a = tempHistoryLength; a = 0; a-- ) {
            temperature[sensor][a - 1] = temperature[sensor][a];
        }

        // store measured temperature in a temporary place
        int temp = sensors.getTempCByIndex(sensor);
        // calculate the difference to the last measured value
        int tempDif = temp - temperature[0][sensor];

        // catch measuring errors
        if ( temp > -100 || tempDif > 25 || tempDif < -25 ) {
            temperature[sensor][0] = sensors.getTempCByIndex(sensor);
            tempMeasureErrorCount[sensor] = 0;
        } else {
            tempMeasureErrorCount[sensor]++;
        }
    }
}

void calcPWM () {
    // detect if temp is in or de-creasing
    for ( int sensor = 0; sensor < tempSensorCount; sensor++ ) {

        // define a variable to store the sum of all temperatures in
        int tempAll = 0;

        // summarise all historical temeratures 
        for ( int a = 1; a < tempHistoryLength; a++ ) {
            tempAll = tempAll + temperature[sensor][a];
        }

        // calculate average from those
        int tempAvg = tempAll / ( tempHistoryLength - 1 );

        // decide if RPM need adjustment to in- decrease cooling
        if ( tempAvg < temperature[sensor][0] ) {
            // temperature is increasing


/* thoughts:
we can have a different amount of sensors and fans. so there is no 1:1 assignment of fans and sensors.
so we have to define which fan reacts on temperature changes of which sensor

i think this is important
*/

            // increaseRPM(sensor);


        } else {
            // temperature is decreasing

        }

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

    // define Interrupts for RPM measurement, if needed - depending on the amount of defined Fans
    if ( FanCount >= 1 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[0]), FanInterrupt0, RISING); }
    if ( FanCount >= 2 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[1]), FanInterrupt1, RISING); }
    if ( FanCount >= 3 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[2]), FanInterrupt2, RISING); }
    if ( FanCount >= 4 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[3]), FanInterrupt3, RISING); }
    if ( FanCount >= 5 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[4]), FanInterrupt4, RISING); }
    if ( FanCount == 6 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[5]), FanInterrupt5, RISING); }

    // modify PWM regeister to change PWM freq
    // set PWM freq to 31kHz (31372.55 Hz) on ...
//  TCCR3B = TCCR3B & B11111000 | B00000001; // ... Pins D2  D3  D5
    TCCR4B = TCCR4B & B11111000 | B00000001; // ... Pins D6  D7  D8
    TCCR5B = TCCR5B & B11111000 | B00000001; // ... Pins D44 D45 D46

    // set Pin modes ...
    for( int cnt = 0; cnt < FanCount; cnt++ ) {
        // ... for the Fan Hallsensors (yellow wire)
        pinMode(FanPinSensor[cnt], INPUT);

        // ... for the Fan PWM signal to control the RPM (blue wire)
        pinMode(FanPinPWM[cnt], OUTPUT);
    }
}

/*
============
    LOOP
============
*/
void loop () {

    getTemperature();

    // calc the new PWM values based on tmerature changes
    calcPWM();

    // write the PWM value to the Fan Pins
    for ( int fan = 0; fan < FanCount; fan++ ) {
        analogWrite( FanPinPWM[fan], FanPWMValue[fan] );
    }

    // delay loop to slow things down?
    // delay(10000)
}