typedef struct{
  char fantype;
  unsigned int fandiv;
}fanspec;

// Definitions of the fans
fanspec fanspace[3]={{0,1},{1,2},{2,8}};


const int FanPinPWM[] =         { 6,  7 };
const int FanPinSensor[] =      { 2,  3 };
const int FanSensorType[] =     { 1,  1 };
      int FanPWMValue[] =       { 20, 20 };
      
// calculate the amount of Fans in this setup
const int FanCount = sizeof(FanPinSensor) / sizeof(int);

volatile int FanInterruptValue[FanCount];
int FanRPM[FanCount];




void FanInterrupt0 () { FanInterruptValue[0]++; }
void FanInterrupt1 () { FanInterruptValue[1]++; }
void FanInterrupt2 () { FanInterruptValue[2]++; }
void FanInterrupt3 () { FanInterruptValue[3]++; }
void FanInterrupt4 () { FanInterruptValue[4]++; }
void FanInterrupt5 () { FanInterruptValue[5]++; }


// calculate the current RPM with the measured sensor signal by using interrupts
void getRPM () {
    // clear all counted Values generated through Interrupts from Fans
    for ( int a = 0; a < FanCount; a++ ) {
        FanInterruptValue[a] = 0;
    }

    sei();		    // Enables interrupts
    delay (1000);	// Wait 1 second
    cli();          // Disable interrupts

    for ( int a = 0; a < FanCount; a++ ) {
        /* Calculate the RPM from the measured FanInterruptValues
           take the counted value from one sec times 60 for one minute
           devide this through a defined value depending on the selected FanSensorType

           This should result in a RPM value in FanRPM for each fan.
        */
        FanRPM[a] = ((FanInterruptValue[a] * 60) / fanspace[FanSensorType[a]].fandiv);
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("start...");


    TCCR4B = TCCR4B & B11111000 | B00000001; // ... Pins D6  D7  D8
    TCCR5B = TCCR5B & B11111000 | B00000001; // ... Pins D44 D45 D46

    // if ( FanCount >= 1 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[0]), FanInterrupt0, RISING); }
    // if ( FanCount >= 1 ) { attachInterrupt(digitalPinToInterrupt(FanPinSensor[0]), FanInterrupt0, RISING); }
    // attachInterrupt(0, FanInterrupt0, RISING);
    attachInterrupt( digitalPinToInterrupt(3), FanInterrupt1, CHANGE);

    for( int cnt = 0; cnt < FanCount; cnt++ ) {
        // ... for the Fan Hallsensors (yellow wire)
        pinMode(FanPinSensor[cnt], INPUT);

        // ... for the Fan PWM signal to control the RPM (blue wire)
        pinMode(FanPinPWM[cnt], OUTPUT);
    }
}

void loop() {
    getRPM();

    for ( int a = 0; a < FanCount; a++ ) {
        Serial.print("fan ");
        Serial.print(a);
        Serial.print(" - ");
        Serial.println(FanRPM[a]);
    }

    Serial.print("FanCount: ");
    Serial.println(FanCount);
    Serial.print("Interrupts fan 0: ");
    Serial.println(FanInterruptValue[0]);
    Serial.print("Interrupts fan 1: ");
    Serial.println(FanInterruptValue[1]);

    for ( int fan = 0; fan < FanCount; fan++ ) {
        analogWrite( FanPinPWM[fan], FanPWMValue[fan] );
    }
}