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
const int tempZone1 = 30; // Temperature that is considered as cool     - Trigger to stop all Fans
const int tempZone2 = 35; // Temperature that is considered as normal
const int tempZone3 = 45; // Temperature that is considered as critical - Trigger to max out all Fans

// define how many Temperature measurements we will remember
const int tempHistoryLength = 10;

// define how many temerature sensors we are going to use with OneWire
const int tempSensorCount = 2;

// define temperature sensor history
float temperature[tempHistoryLength];

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
FanSensorMap  : associate the FAN with a sensor. Means that the FAN speed will change depending on the temperature of the sensor
FanZoneMap    : 
FanPWMValue   : the PWM value set on the PWM pin (FanPinPWM). The values set here are just inital values

                                { fan1, fan2, ... } */
const int FanPinPWM[] =         {  6,  7,  8, 44, 45 };
const int FanPinSensor[] =      {  2,  3, 21, 20, 19 };
const int FanSensorType[] =     {  1,  1,  1,  1,  1 };
const int FanZoneMap[] =        {  0,  0,  1,  1,  2 };
      int FanPWMValue[] =       { 20, 20, 20, 20, 20 };

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
void getTemperature () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // define a Variable to store the highest measured Temperature
    // this serves as value to check if we hit any thresholds
    int tempMeasuredMax = 0;

    // collect temperatures
    for( int sensor = 0; sensor < tempSensorCount; sensor++ ) {
        // preserve historical measurements
        for ( int a = tempHistoryLength; a = 0; a-- ) {
            temperature[a - 1] = temperature[a];
        }

        // store measured temperature in a temporary place
        int tempMeasured = sensors.getTempCByIndex(sensor);

        // check if we have a new measuredMax temperature
        if ( tempMeasuredMax < tempMeasured ) {
            tempMeasuredMax = tempMeasured;
        }
        // calculate the difference to the last measured value
        int tempDif = tempMeasured - temperature[0];

        // catch measuring errors
        if ( tempMeasured > -100 || tempDif > 25 || tempDif < -25 ) {
            temperature[0] = sensors.getTempCByIndex(sensor);
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
        int tempAvgDif = temperature[sensor][0] - tempAvg;

        // decide if PWM need adjustment to in-/decrease cooling
        if ( tempAvgDif <= 0 ) {
            // temperature is increasing or the same
            adjustPWM(TRUE, sensor);

        } else if ( tempAvgDif > 0 ) {
            // temperature is decreasing
            adjustPWM(FALSE, sensor);

        } else {
            // temperature is unchanged

        }

    }
}

// adjuste the current PWM Value to adjust the FAN speed
// preserve the defined Min/Max limits 
adjustPWM(bool increase, sensor) {
    // find FANs which are mapped to the defined sensor
    for ( int b = 0; b < FanCount; b++ ) {
        if ( FanMap[b] = sensor ) {
            if ( increase ) {
                FanPWMValue[b] = FanPWMValue[b] + PWMStep;
            } else {
                FanPWMValue[b] = FanPWMValue[b] - PWMStep;
            }

            // check if the new PWM Value mets the Min/Max
            if ( FanPWMValue[b] > MaxPWM ) {
                FanPWMValue[b] = MaxPWM;
            } else if ( FanPWMValue[b] < MinPWM ) {
                FanPWMValue[b] = MinPWM;
            }

            // check if we hit some temerature thresholds

        }
    }
}

void writePWM() {
    // write the PWM value to the Fan Pins
    for ( int fan = 0; fan < FanCount; fan++ ) {
        analogWrite( FanPinPWM[fan], FanPWMValue[fan] );
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

    // 

    writePWM();


    // delay loop to slow things down?
    // delay(10000)
}