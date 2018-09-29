/*
============
    INIT
============
*/
// include libs
#include <OneWire.h>
#include <DallasTemperature.h>

#define DEBUG

/* ###################
   ### TEMPERATURE ###
   ################### */
// set pin of the data wire of ds18b20
#define bus_heat 4
// initiate bus on defined pin
OneWire wire_heat(bus_heat);
// tell dallas lib on wich bus the temperature sensors are located
DallasTemperature sensorBus(&wire_heat);

// define how many Temperature measurements we will remember
const int sensorHistorySize = 5;

// define how many temerature sensors we are going to use with OneWire
const int sensorCount = 2;

// define the strcut that defines a sensor
struct Sensor {
    int current;
    int history[sensorHistorySize];
    int error;
    int historyAvg;
    bool trend;
};
// declare the list that stores al sensors
struct Sensor sensors[sensorCount];

// define the amount of bad measurements that are considered as bad
// reaching this limit means that cooling can not be ensured as the temperature is unknown
// this will result in a 'last resort' action and turn on the FANs to the max level
const int sensorErrorLimit = 4;

// set the Pin for the led to indicate that the sensorErrorLimit got hit
const int sensorErrorLimitLed = 24;

/* ##############
   ### STAGES ###
   ############## */
// set temperature stages an the pin for the indicator LED
const int stageSetup[][2] = { { 30, 20 }, { 35, 21 }, { 45, 22 }, { 47, 23 } };
// count the amount of stages. basicaly just for coding conviniece
const int stageCount = sizeof(stageSetup) / sizeof(stageSetup[0]);

// define the Pin for the LED which gets turned on on case the critial limit (last stage) gets activated
const int critialLedPin = 10;

// define struct that defines a stage
struct Stage {
    bool active;
    int trigger;
    int ledPin;
};
// declare the list that stores all stages
struct Stage stages[stageCount];

/* ####################
   ### FAN CONRTROL ###
   #################### */

/* this is the fan setup. every entry represents one Fan.
   syntax: { PIN primary Fan, PIN secodary Fan, stage mapping } */
const int fanSetup[][3] = {
    { 6, 0, 1 },
    { 7, 2, 2 },
    { 8, 0, 3 },
};

const int fanCount = sizeof(fanSetup) / sizeof(fanSetup[0]);

// define the struct that defines a fan
struct Fan {
    int pinPri;
    int pinSec ;
    int pwmValuePri;
    int pwmValueSec;
    int stage;
    bool active;
};
// define the list to store all fans in
struct Fan fans[fanCount];

// define the maximum PWM value that can be set (usually 255)
const int MaxPWM = 255;
// define the minimum PWM value that can be set
const int MinPWM = 20;
// define the steps to adjuste the PWM value
const int PWMStep = 25;
// define an offset for the primary and secondary fans if the fan has 2 rotors
const int dualFanOffset = 50;

/*
=================
    FUNCTIONS
=================
*/
bool temperatureInitialize () {
    // Send the command to get temperatures
    sensorBus.requestTemperatures();

    // define a bool which will be the return vaule of this function
    // it true by default and gets set to false when ANY inital sensor reading fails to inform the setup function of it.
    bool checkState = true;

    // collect temperatures
    for ( int s = 0; s < sensorCount; s++ ) {

        int x = 0;
        bool sensorState = true;
        for ( int x = 0; x <= 5; x++ ) {
            // store measured temperature in a temporary place
            int tempMeasured = (int)sensorBus.getTempCByIndex(s);

            // catch measuring errors
            if ( tempMeasured > -100 ) {
                sensors[s].current = tempMeasured;
                for ( int h = 0; h < sensorHistorySize; h++ ) {
                    sensors[s].history[h] = tempMeasured;
                }
                #ifdef DEBUG
                    Serial.print("init sensor ");
                    Serial.print(s);
                    Serial.print(" OK - ");
                    Serial.println(tempMeasured);
                #endif
                break;
            } else {
                sensorBus.requestTemperatures();
                #ifdef DEBUG
                    Serial.print("init sensor ");
                    Serial.print(s);
                    Serial.print(" FAIL (cnt ");
                    Serial.print(x);
                    Serial.print(") - ");
                    Serial.println(tempMeasured);
                #endif
            }
            // set the bool to false to notify the setup function that a sensor has a failure during init
            if ( x == 5 ) { checkState = false; }
        }

    }
    // returns false if
    return checkState;
}


void getTemperature () {
    // Send the command to get temperatures
    sensorBus.requestTemperatures();

    for ( int s = 0; s < sensorCount; s++ ) {
        // preserve historical measurements
        for ( int h = sensorHistorySize - 1; h >= 1; h-- ) {
            sensors[s].history[h] = sensors[s].history[h - 1];
        }
        // make last measurement the most recent history entry
        sensors[s].history[0] = sensors[s].current;

        // store measured temperature in a temporary place
        int measurement = (int)sensorBus.getTempCByIndex(s);
        
        // calculate the difference to the previous measured value
        int difference = measurement - sensors[s].history[0];

        // catch measuring errors
        if ( measurement > -100 && difference < 25 && difference > -25 ) {
            sensors[s].current = measurement;
            sensors[s].error = 0;
        } else {
            sensors[s].error++;
        }
    }
}

// void calcPWM () {
//     // detect if temp is in or de-creasing
//     for ( int sensor = 0; sensor < tempSensorCount; sensor++ ) {

//         // define a variable to store the sum of all temperatures in
//         int tempAll = 0;

//         // summarise all historical temeratures
//         for ( int a = 1; a < tempHistoryLength; a++ ) {
//             tempAll = tempAll + temperature[sensor][a];
//         }

//         // calculate average from those
//         int tempAvg = tempAll / ( tempHistoryLength - 1 );
//         int tempAvgDif = temperature[sensor][0] - tempAvg;

//         // decide if PWM need adjustment to in-/decrease cooling
//         if ( tempAvgDif >= 0 ) {
//             // temperature is increasing or the same
//             adjustPWM(true);
//         } else {
//             // temperature is decreasing
//             adjustPWM(false);
//         }

//     }
// }

// // adjuste the current PWM Value to adjust the FAN speed
// // preserve the defined Min/Max limits
// void adjustPWM(bool increase) {
//     // find FANs which are mapped to the defined sensor
//     for ( int b = 0; b < FanZoneCount; b++ ) {
//         if ( increase ) {
//             FanZonePWMValue[b] = FanZonePWMValue[b] + PWMStep;
//             #ifdef DEBUG
//                 Serial.print("+ pwn in zone ");
//                 Serial.print(b);
//                 Serial.print(" to ");
//                 Serial.println(FanZonePWMValue[b]);
//             #endif
//         } else {
//             FanZonePWMValue[b] = FanZonePWMValue[b] - PWMStep;
//             #ifdef DEBUG
//                 Serial.print("- pwn in zone ");
//                 Serial.print(b);
//                 Serial.print(" to ");
//                 Serial.println(FanZonePWMValue[b]);
//             #endif
//         }

//         // check if the new PWM Value meets the Min/Max
//         if ( FanZonePWMValue[b] > MaxPWM ) {
//             FanZonePWMValue[b] = MaxPWM;
//         } else if ( FanZonePWMValue[b] < MinPWM ) {
//             FanZonePWMValue[b] = MinPWM;
//         }
//     }
// }

// void checkThresholds () {
//     // check for Zone 1
//     // 120mm fans
//     // i fthreshold is true, zone 1 will start if not running
//     // tempertature under threshold, power off zone 1 if running
//     if ( tempMeasuredMax >= tempZone1 && FanZonePWMValue[0] < MinPWM ) {
//         FanZonePWMValue[0] = MinPWM;
//         #ifdef DEBUG
//             Serial.print("power on: zone 1 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//     } else if ( tempMeasuredMax < tempZone1 && FanZonePWMValue[0] > 0 ) {
//         FanZonePWMValue[0] = 0;
//         #ifdef DEBUG
//             Serial.print("power off: zone 1 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//     }

//     // check for zone 2
//     // 2x 80mm fans
//     // if threshold is true, zone2 will start if not running already
//     // temperatrue under threshold, power off zone 2 if running
//     if ( tempMeasuredMax >= tempZone2 && FanZonePWMValue[1] < MinPWM ) {
//         FanZonePWMValue[1] = MinPWM;
//         #ifdef DEBUG
//             Serial.print("power on: zone 2 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//     } else if ( tempMeasuredMax < tempZone2 && FanZonePWMValue[1] > 0) {
//         #ifdef DEBUG
//             Serial.print("power off: zone 2 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//         FanZonePWMValue[1] = 0;
//     }

//     // check for zone 3
//     // all fans
//     // if threshod is true, all fans max RPM
//     // temperature is under thresold, power off zone 3, if runnning
//     if ( tempMeasuredMax >= tempZone3 ) {
//         for ( int a = 0; a > FanZoneCount; a++ ) {
//             FanZonePWMValue[a] = MaxPWM;
//         }
//         #ifdef DEBUG
//             Serial.print("power on: zone 3 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//     } else if ( tempMeasuredMax < tempZone3 && FanZonePWMValue > 0 ) {
//         #ifdef DEBUG
//             Serial.print("power off: zone 3 - temp: ");
//             Serial.println(tempMeasuredMax);
//         #endif
//         FanZonePWMValue[2] = 0;
//     }
// }

// void checkError() {
//     // bool to detect if any sensor has an failed state
//     bool hasError = false;

//     for ( int a = 0; a < tempSensorCount; a++ ) {
//         if ( tempMeasureErrorCount[a] >= tempMeasureErrorCountLimit ) {
//             hasError = true;
//             for ( int a = 0; a < FanZoneCount; a++ ) {
//                 FanZonePWMValue[a] = MaxPWM;
//             }
//         }
//     }
//     if ( hasError ) {
//         // turn on onboard led to show this state externaly
//         digitalWrite(LED_BUILTIN, HIGH);
//         #ifdef DEBUG
//             Serial.print("error count ");
//             for ( int a = 0; a < tempSensorCount; a++ ) {
//                 Serial.print(tempMeasureErrorCount[a]);
//                 Serial.print(" ");
//             }
//             Serial.println();
//         #endif
//     } else {
//         // turn off onboard led to show state is ok
//         digitalWrite(LED_BUILTIN, LOW);
//         #ifdef DEBUG
//             Serial.println("cleared error state");
//         #endif
//     }
// }

void writePWM() {
    // write the PWM value to the Fan Pins
    for ( int f = 0; f < fanCount; f++ ) {
        analogWrite( fans[f].pinPri, fans[f].pwmValuePri );
        if ( fans[f].pinSec != 0 ) {
            analogWrite( fans[f].pinSec, fans[f].pwmValueSec );
        }
    }
}

/*
=============
    SETUP
=============
*/
void setup() {
    #ifdef DEBUG
        // setup Serial
        Serial.begin(9600);
        Serial.println("SETUP\t>>>>>");
    #endif

    // build the list of all configured fans
    for ( int f = 0; f < fanCount; f++ ) {
        fans[f].pinPri      = fanSetup[f][0];
        fans[f].pinSec      = fanSetup[f][1];
        fans[f].stage       = fanSetup[f][2];
        fans[f].pwmValuePri = 0;
        fans[f].pwmValueSec = 0;
        fans[f].active      = false;

        // set pin modes for fan
        pinMode( fans[f].pinPri, OUTPUT );
        if ( fans[f].pinSec != 0 ) {
            pinMode( fans[f].pinSec, OUTPUT );
        }
    }

    // build the list of all configured sensors
    for ( int s = 0; s < sensorCount; s++ ) {
        sensors[s].current    = 0;
        sensors[s].error      = 0;
        sensors[s].historyAvg = 0;
        sensors[s].trend      = false;
        for ( int h = 0; h < sensorHistorySize; h++ ) {
            sensors[s].history[h] = 0;
        }
    }

    // build the list of all configured stages
    for ( int st = 0; st < stageCount; st++ ) {
        stages[st].active  = false;
        stages[st].trigger = stageSetup[st][0];
        stages[st].ledPin  = stageSetup[st][1];
    }

    // initiate temperature sensors
    sensorBus.begin();

    // modify PWM regeister to change PWM freq
    // set PWM freq to 31kHz (31372.55 Hz) on ...
    TCCR3B = TCCR3B & B11111000 | B00000001; // ... Pins D2  D3  D5
    TCCR4B = TCCR4B & B11111000 | B00000001; // ... Pins D6  D7  D8
    TCCR5B = TCCR5B & B11111000 | B00000001; // ... Pins D44 D45 D46

    // use the internal LED to show active 'last resort' state
    pinMode(LED_BUILTIN, OUTPUT);

    // turn of LED
    digitalWrite(LED_BUILTIN, LOW);

    if ( temperatureInitialize() ) {
        // temp init successfull
        // blink at start to show successfull end of setup
        bool state = HIGH;
        for( int a = 0; a < 10; a++ ) {
            digitalWrite(LED_BUILTIN, state);
            state = state ? LOW: HIGH;
            delay(1000);
        }
    } else {
        digitalWrite(LED_BUILTIN, HIGH);
    }

    #ifdef DEBUG
        Serial.println(">>>>>\tEND");
    #endif

}

/*
============
    LOOP
============
*/
void loop () {

    // get temperatures from sensors
    getTemperature();

    // // calc the new PWM values based on tmerature changes
    // calcPWM();

    // // check if one of the tempMeasuredMax hit any defined threshold
    // checkThresholds();

    // // check for measurement errors. if the threshold of the number of errors in a row is hit, enter a 'last resort' state and turn all fans to maximum
    // // this will overwrite all previously calculated values
    // checkError();

    brain();

    // write the pwm values to the pins
    writePWM();

    #ifdef DEBUG
        for ( int f = 0; f < fanCount; f++ ){
            Serial.print("fan ");
            Serial.print(f);
            Serial.print("\tpin1: ");
            Serial.print(fans[f].pinPri);
            Serial.print("\tpin2: ");
            Serial.print(fans[f].pinSec);
            Serial.print("\tpwm1: ");
            Serial.print(fans[f].pwmValuePri);
            Serial.print("\tpwm2: ");
            Serial.print(fans[f].pwmValueSec);
            Serial.print("\tstage: ");
            Serial.println(fans[f].stage);
        }
        for ( int s = 0; s < sensorCount; s++ ) {
            Serial.print("sensor ");
            Serial.print(s);
            Serial.print("\tcurrent: ");
            Serial.print(sensors[s].current);
            Serial.print("\terror: ");
            Serial.print(sensors[s].error);
            Serial.print("\thistory: ");
            for ( int h = 0; h < sensorHistorySize; h++ ) {
                Serial.print("\t");
                Serial.print(sensors[s].history[h]);
            }
            Serial.print("\tavg: ");
            Serial.print(sensors[s].historyAvg);
            Serial.print("\ttrend: ");
            if ( sensors[s].trend ) { Serial.println("+"); } else { Serial.println("-"); }
        }
        for ( int st = 0; st < stageCount; st++ ) {
            Serial.print("stage ");
            Serial.print(st);
            Serial.print("\tactive: ");
            Serial.print(stages[st].active);
            Serial.print("\ttrigger: ");
            Serial.println(stages[st].trigger);
        }
    #endif

    // delay loop to slow things down
    delay(2000);
}