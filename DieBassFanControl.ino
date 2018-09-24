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

// set the amount of reoccuring measurements with bad results
// a bad result can be either a error (like -127) or a jump in measured temperature higher than 25
const int tempMeasureErrorCountLimit = 4;

// define the variable to store the highest measured value for every loop
int tempMeasuredMax;

/* ####################
   ### FAN CONRTROL ###
   #################### */
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
const int FanZonePinPWM[3][2] = { { 6, 7 }, { 8, 44 }, { 45, 46 } };
// const int FanZoneCount      = sizeof(FanZonePinPWM) / sizeof(int);
const int FanZoneCount      = 3;
      int FanZonePWMValue[FanZoneCount];

// define the array to store the amount of fans for every zone in with the fixed length of the amount of zone
int FanZoneFanCount[FanZoneCount];

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
bool temperatureInitialize () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // define a bool which will be the return vaule of this function
    // it true by default and gets set to false when ANY inital sensor reading fails to inform the setup function of it. 
    bool checkState = true;

    // collect temperatures
    for ( int sensor = 0; sensor < tempSensorCount; sensor++ ) {

        int x = 0;
        bool SensorCheckOK = false;
        while ( !SensorCheckOK && x < 5 ) {
            // store measured temperature in a temporary place
            int tempMeasured = (int)sensors.getTempCByIndex(sensor);

            // catch measuring errors
            if ( tempMeasured > -100 ) {
                for ( int a = 0; a < tempHistoryLength; a++ ) {
                    temperature[sensor][a] = tempMeasured;
                }
                SensorCheckOK = true;
                #ifdef DEBUG
                    Serial.print("init sensor ");
                    Serial.print(sensor);
                    Serial.print(" OK - ");
                    Serial.println(tempMeasured);
                #endif
            } else {
                x++;
                #ifdef DEBUG
                    Serial.print("init sensor ");
                    Serial.print(sensor);
                    Serial.print(" FAIL (cnt ");
                    Serial.print(x);
                    Serial.print(" ) - ");
                    Serial.println(tempMeasured);
                #endif
            }
            // set the bool to false to notify the setup function that a sensor has a failure during init
            if ( x = 5 ) { checkState = false; }
        }
    }
    // returns false if
    return checkState;
}


void getTemperature () {
    // Send the command to get temperatures
    sensors.requestTemperatures();

    // preserve historical measurements
    for ( int sensor = 0; sensor < tempSensorCount; sensor++ ) {
        for ( int b = tempHistoryLength - 1; b >= 1; b-- ) {
            temperature[sensor][b] = temperature[sensor][b - 1];
        }
    }

    // clear Variable to store the highest measured Temperature
    // this serves as value to check if we hit any thresholds
    tempMeasuredMax = 0;

    // collect temperatures
    for( int sensor = 0; sensor < tempSensorCount; sensor++ ) {

        // store measured temperature in a temporary place
        int tempMeasured = (int)sensors.getTempCByIndex(sensor);

        // check if we have a new measuredMax temperature
        if ( tempMeasuredMax < tempMeasured ) {
            tempMeasuredMax = tempMeasured;
            #ifdef DEBUG
                Serial.print("new measured max: ");
                Serial.println(tempMeasuredMax);
            #endif
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

        #ifdef DEBUG
            Serial.print("temp measured on ");
            Serial.print(sensor);
            Serial.print(" ");
            Serial.println(tempMeasured);
        #endif
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
            #ifdef DEBUG
                Serial.print("+ pwn in zone ");
                Serial.print(b);
                Serial.print(" to ");
                Serial.println(FanZonePWMValue[b]);
            #endif
        } else {
            FanZonePWMValue[b] = FanZonePWMValue[b] - PWMStep;
            #ifdef DEBUG
                Serial.print("- pwn in zone ");
                Serial.print(b);
                Serial.print(" to ");
                Serial.println(FanZonePWMValue[b]);
            #endif
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
        #ifdef DEBUG
            Serial.print("power on: zone 1 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
    } else if ( tempMeasuredMax < tempZone1 && FanZonePWMValue[0] > 0 ) {
        FanZonePWMValue[0] = 0;
        #ifdef DEBUG
            Serial.print("power off: zone 1 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
    }

    // check for zone 2
    // 2x 80mm fans
    // if threshold is true, zone2 will start if not running already
    // temperatrue under threshold, power off zone 2 if running
    if ( tempMeasuredMax >= tempZone2 && FanZonePWMValue[1] < MinPWM ) {
        FanZonePWMValue[1] = MinPWM;
        #ifdef DEBUG
            Serial.print("power on: zone 2 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
    } else if ( tempMeasuredMax < tempZone2 && FanZonePWMValue[1] > 0) {
        #ifdef DEBUG
            Serial.print("power off: zone 2 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
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
        #ifdef DEBUG    
            Serial.print("power on: zone 3 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
    } else if ( tempMeasuredMax < tempZone3 && FanZonePWMValue > 0 ) {
        #ifdef DEBUG
            Serial.print("power off: zone 3 - temp: ");
            Serial.println(tempMeasuredMax);
        #endif
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
        #ifdef DEBUG
            Serial.print("error count ");
            for ( int a = 0; a < tempSensorCount; a++ ) {
                Serial.print(tempMeasureErrorCount[a]);
                Serial.print(" ");
            }
            Serial.println();
        #endif
    } else {
        // turn off onboard led to show state is ok
        digitalWrite(LED_BUILTIN, LOW);
        #ifdef DEBUG
            Serial.println("cleared error state");
        #endif
    }
}

void writePWM() {
    // write the PWM value to the Fan Pins
    for ( int zone = 0; zone < FanZoneCount; zone++ ) {
        for ( int fan = 0; fan < FanZoneFanCount[zone]; fan++ ) {
            analogWrite( FanZonePinPWM[zone][fan], FanZonePWMValue[zone] );                        
            #ifdef DEBUG
                Serial.print("pwm value pin ");
                Serial.println(FanZonePinPWM[zone][fan]);
                Serial.print("pwm value ");
                Serial.println(FanZonePWMValue[zone]);
            #endif
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
        Serial.println("SETUP >>>>>");
    #endif

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

    // turn of LED
    digitalWrite(LED_BUILTIN, LOW);

    if ( temperatureInitialize() ) {
        // temp init successfull
        // blink at start to show successfull end of setup
        int state = HIGH; 
        for( int a = 0; a = 10; a++ ) {
            digitalWrite(LED_BUILTIN, state);
            state = state ? LOW: HIGH;
            delay(1000);
        }
    } else {
        digitalWrite(LED_BUILTIN, HIGH);
    }

    #ifdef DEBUG
        Serial.println(">>>>> END");
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

    // calc the new PWM values based on tmerature changes
    calcPWM();

    // check if one of the tempMeasuredMax hit any defined threshold
    checkThresholds();

    // check for measurement errors. if the threshold of the number of errors in a row is hit, enter a 'last resort' state and turn all fans to maximum
    // this will overwrite all previously calculated values
    checkError();

    // write the pwm values to the pins
    writePWM();

    #ifdef DEBUG
        for ( int sensor = 0; sensor < tempSensorCount; sensor++ ) {
            Serial.print("temps of sensor ");
            Serial.print(sensor);
            Serial.print(" :");
            for ( int a = 0; a < tempHistoryLength; a++ ) {
                Serial.print(" ");
                Serial.print(temperature[sensor][a]);
            }
            Serial.println();
        }
        Serial.println("==============================================");
    #endif

    // delay loop to slow things down
    delay(2000);


    /*
    thoughs
    atm, if the first sensor is raising the pmw because temp is = or +, but sensor 2 temp is - then it reverts the pwm change from sensor 1

    so positive changes to the pwm are not allowed to be overruled.

    remember the pwm values of every zone for ever sensor seperately.
    after all sensors are processed and the new pwm values are calculated, compare them and find the highest value and set this value to the pins
    because, if any sensor decides to raise the pwm it means that temperatrue is increasing or stayed equal, but we want it to decrease.

    so any decreased pwm value is bad. unless all sensors decide to reducde pwm value. then it is ok.

    and i think the same goes for the thresholds.


    furthermore,
    there is a logical error in handling the zones ( besides that zones should be renamed to stages )

    rather than overwrite the pwm values when checking the thresholds, it should be checked if an adjustment of the pwm values is neccessary
     at all. because, if the threshold for stage 1 is not hit, there is no reason to adjust the value.
     same for all stages besides 3 as this threshold is the critical limit which turns on all fans on max value.

    this still makes it necessary to avoid that the sensors overwrite the decission to raise a pwm value made by the previous one.
    */

}