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
    int trend;
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
        sensors[s].trend      = 0;
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
        for( int a = 0; a < 50; a++ ) {
            digitalWrite(LED_BUILTIN, state);
            state = state ? LOW: HIGH;
            delay(80);
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
            if ( sensors[s].trend > 1 ) { Serial.println("+"); } else if ( sensors[s].trend == 1u ) { Serial.println("-"); } else { Serial.println("~"); }
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
    delay(20000);
}