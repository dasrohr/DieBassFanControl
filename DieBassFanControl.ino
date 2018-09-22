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
const int tempHistoryLength = 5;

// define how many temerature sensors we are going to use with OneWire
const int tempSensorCount = 2;

// define temperature sensor history
int temperature[tempSensorCount][tempHistoryLength];

// initiate the array to store the counter in which indicates the consecutive occurences of measuring errors or measured values that can not be true
int tempMeasureErrorCount[tempSensorCount] = {};

// define the amount of bad measurements that are considered as bad
// reaching this limit means that cooling can not be ensured as the temperature is unknown
// this will result in a 'last resort' action and turn on the FANs to the max level
const int tempMeasureErrorCountLimit = 4;

// define the variable to store the highes measured value for every loop
int tempMeasuredMax;

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


define 3 Zones. every Zone represent a set of Fans. The Zones are used as Stages to group FANs together.
Zone 1 might run much earlier than Zone 2 for eg.*/
const int FanType           = 1;
const int FanZonePinPWM[3][2] = { { 6, 7 }, { 8, 44 }, { 45, 46 } };
// const int FanZoneCount      = sizeof(FanZonePinPWM) / sizeof(int);
const int FanZoneCount      = 3;

      int FanZonePWMValue[FanZoneCount];

// define the array to store the amount of fans for every zone in with the fixed length of the amount of zone
int FanZoneFanCount[FanZoneCount];




// const int FanPinPWM[] =         {  6,  7,  8, 44, 45 };
// const int FanSensorType[] =     {  1,  1,  1,  1,  1 };
// const int FanZoneMap[] =        {  0,  0,  1,  1,  2 };
//       int FanPWMValue[] =       { 20, 20, 20, 20, 20 };

// calculate the amount of Fans in this setup
// const int FanCount = sizeof(FanPinSensor) / sizeof(int);

// define the maximum PWM value that can be set (usually 255)
const int MaxPWM = 255;
// define the minimum PWM value that can be set
const int MinPWM = 20;

// define the steps to adjuste the PWM value
const int PWMStep = 25;

/*
=================
    FUNCTIONS
=================
*/
void getTemperature () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // preserve historical measurements
    for ( int a = 0; a < tempSensorCount; a++ ) {
        for ( int b = tempHistoryLength; b = 0; b-- ) {
            temperature[a][b - 1] = temperature[a][b];
        }
    }

    // define a Variable to store the highest measured Temperature
    // this serves as value to check if we hit any thresholds
    int tempMeasuredMax = 0;

    // collect temperatures
    for( int sensor = 0; sensor < tempSensorCount; sensor++ ) {

        // store measured temperature in a temporary place
        int tempMeasured = (int)sensors.getTempCByIndex(sensor);

        // check if we have a new measuredMax temperature
        if ( tempMeasuredMax < tempMeasured ) {
            tempMeasuredMax = tempMeasured;
        }

        // calculate the difference to the last measured value
        int tempDif = tempMeasured - temperature[sensor][0];

        // catch measuring errors
        if ( tempMeasured > -100 && tempDif < 25 && tempDif > -25 ) {
            temperature[sensor][0] = tempMeasured;
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
        if ( tempAvgDif >= 0 ) {
            // temperature is increasing or the same
            adjustPWM(true);
        } else {
            // temperature is decreasing
            adjustPWM(false);
        }

    }
}

// adjuste the current PWM Value to adjust the FAN speed
// preserve the defined Min/Max limits 
void adjustPWM(bool increase) {
    // find FANs which are mapped to the defined sensor
    for ( int b = 0; b < FanZoneCount; b++ ) {
        if ( increase ) {
            FanZonePWMValue[b] = FanZonePWMValue[b] + PWMStep;
            Serial.print("+ pwn in zone ");
            Serial.print(b);
            Serial.print(" to ");
            Serial.println(FanZonePWMValue[b]);
        } else {
            FanZonePWMValue[b] = FanZonePWMValue[b] - PWMStep;
            Serial.print("- pwn in zone ");
            Serial.print(b);
            Serial.print(" to ");
            Serial.println(FanZonePWMValue[b]);
        }

        // check if the new PWM Value meets the Min/Max
        if ( FanZonePWMValue[b] > MaxPWM ) {
            FanZonePWMValue[b] = MaxPWM;
        } else if ( FanZonePWMValue[b] < MinPWM ) {
            FanZonePWMValue[b] = MinPWM;
        }
    }
}

void checkThresholds () {
    // check for Zone 1
    // 120mm fans
    // i fthreshold is true, zone 1 will start if not running
    // tempertature under threshold, power off zone 1 if running
    if ( tempMeasuredMax >= tempZone1 && FanZonePWMValue[0] < MinPWM ) {
        FanZonePWMValue[0] = MinPWM;
        Serial.print("power on: zone 1 - temp: ");
        Serial.println(tempMeasuredMax);
    } else if ( tempMeasuredMax < tempZone1 && FanZonePWMValue[0] > 0 ) {
        Serial.print("power off: zone 1 - temp: ");
        Serial.println(tempMeasuredMax);
        FanZonePWMValue[0] = 0;
    }

    // check for zone 2
    // 2x 80mm fans
    // if threshold is true, zone2 will start if not running already
    // temperatrue under threshold, power off zone 2 if running
    if ( tempMeasuredMax >= tempZone2 && FanZonePWMValue[1] < MinPWM ) {
        FanZonePWMValue[1] = MinPWM;
        Serial.print("power on: zone 2 - temp: ");
        Serial.println(tempMeasuredMax);
    } else if ( tempMeasuredMax < tempZone2 && FanZonePWMValue[1] > 0) {
        Serial.print("power off: zone 2 - temp: ");
        Serial.println(tempMeasuredMax);
        FanZonePWMValue[1] = 0;
    }

    // check for zone 3
    // all fans
    // if threshod is true, all fans max RPM
    // temperature is under thresold, power off zone 3, if runnning
    if ( tempMeasuredMax >= tempZone3 ) {
        for ( int a = 0; a > FanZoneCount; a++ ) {
            FanZonePWMValue[a] = MaxPWM;
        }
        Serial.print("power on: zone 3 - temp: ");
        Serial.println(tempMeasuredMax);
    } else if ( tempMeasuredMax < tempZone3 && FanZonePWMValue > 0 ) {
        Serial.print("power off: zone 3 - temp: ");
        Serial.println(tempMeasuredMax);
        FanZonePWMValue[2] = 0;
    }
}

void checkError() {
    // bool to detect if any sensor has an failed state
    bool hasError = false;

    for ( int a = 0; a < tempSensorCount; a++ ) {
        if ( tempMeasureErrorCount[a] >= tempMeasureErrorCountLimit ) {
            hasError = true;
            for ( int a = 0; a < FanZoneCount; a++ ) {
                FanZonePWMValue[a] = MaxPWM;
            }
        }
    }
    if ( hasError ) {
        // turn on onboard led to show this state externaly
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.print("error count ");
        for ( int a = 0; a < tempSensorCount; a++ ) {
            Serial.print(tempMeasureErrorCount[a]);
            Serial.print(" ");
        }
        Serial.println();
    } else {
        // turn off onboard led to show state is ok
        digitalWrite(LED_BUILTIN, LOW);
        Serial.println("cleared error state");
    }
}

void writePWM() {
    // write the PWM value to the Fan Pins
    for ( int zone = 0; zone < FanZoneCount; zone++ ) {
        for ( int fan = 0; fan < FanZoneFanCount[zone]; fan++ ) {
            analogWrite( FanZonePinPWM[zone][fan], FanZonePWMValue[zone] );
                        
            Serial.print("pwm value pin ");
            Serial.println(FanZonePinPWM[zone][fan]);
            Serial.print("pwm value ");
            Serial.println(FanZonePWMValue[zone]);
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

    // count amount of fans in the zones
    for ( int zone = 0; zone < FanZoneCount; zone++ ) {
        FanZoneFanCount[zone] = sizeof(FanZonePinPWM[zone]) / sizeof(int);
    }

    // initiate temperature sensors
    sensors.begin();

    // modify PWM regeister to change PWM freq
    // set PWM freq to 31kHz (31372.55 Hz) on ...
//  TCCR3B = TCCR3B & B11111000 | B00000001; // ... Pins D2  D3  D5
    TCCR4B = TCCR4B & B11111000 | B00000001; // ... Pins D6  D7  D8
    TCCR5B = TCCR5B & B11111000 | B00000001; // ... Pins D44 D45 D46

    // set Pin modes ...
    for ( int zone = 0; zone < FanZoneCount; zone++ ) {
        for ( int fan = 0; fan < FanZoneFanCount[zone]; fan++ ) {
            pinMode( FanZonePinPWM[zone][fan], OUTPUT);
        }
    }
    
    // use the internal LED to show active 'last resort' state
    pinMode(LED_BUILTIN, OUTPUT);
    // brink once during start to show end of setup
    digitalWrite(LED_BUILTIN, HIGH);
    delay(1000);
    digitalWrite(LED_BUILTIN, LOW);
}

/*
============
    LOOP
============
*/
void loop () {

    // get temperatures from sensors
    getTemperature();

    // calc the new PWM values based on tmerature changes
    calcPWM();

    // check if one of the tempMeasuredMax hit any defined threshold
    checkThresholds();

    // check for measurement errors. if the threshold of the number of errors in a row is hit, enter a 'last resort' state and turn all fans to maximum
    // this will overwrite all previously calculated values
    checkError();

    // write the pwm values to the pins
    writePWM();

    Serial.print("temperatures: ");
    Serial.print(temperature[0][0]);
    Serial.print(" ");
    Serial.println(temperature[0][1]);

    // delay loop to slow things down
    delay(30000);
}