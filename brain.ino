/* 
this funciton is used to calculate the new pwm values for the fans
as it has to make some decissions and cosider certain variables, i called it brain.
*/

void brain() {

    int highestTemperature = getHighestTemperature();
    #ifdef DEBUG
        Serial.print("max temp: ");
        Serial.println(highestTemperature);
    #endif

    // enable and disblae stages based on the highest temperature measured
    for ( int st = 0; st < stageCount; st++ ) {
        if ( stages[st].trigger <= highestTemperature ) {
            stages[st].active = true;
        } else {
            stages[st].active = false;
        }
    }

    // counter to count the number of sernsors that hit the errorLimit
    int errorSensorCounter = 0;
    // determine the trend of the temperatures by getting an average o fthe history in comparison to the ceurren temperature
    for ( int s = 0; s < sensorCount; s++ ) {
        // check if any sensor hit the error limit and activate the critical stage as last resort
        if ( sensors[s].error >= sensorErrorLimit ) {
            // sensor hit limit, activate stage
            stages[stageCount - 1].active = true;
            analogWrite( sensorErrorLimitLed, HIGH );
            // count up the counter to determine later if the error state is clear and the led can be turned off
            errorSensorCounter++;
        }
        int grandTotal = 0;
        for ( int h = 0; h < sensorHistorySize; h++ ) {
            grandTotal = grandTotal + sensors[s].history[h];
        }
        sensors[s].historyAvg = grandTotal / sensorHistorySize;
        // decide wether the temperature is equal, in- or decreasing
        if ( sensors[s].historyAvg > sensors[s].current ) {
            // temperature is equal or increasing
            sensors[s].trend = 1;    // 1 means decresing
        } else if ( sensors[s].historyAvg < sensors[s].current ) {
            sensors[s].trend = 2;    // 2 means increasing
        } else {
            sensors[s].trend = 0;    // 0 means unchanged
        }
    }

    // if no sensor hit the error limit, turn off the led in case it got turned on before
    if ( errorSensorCounter = 0 ) { analogWrite( sensorErrorLimitLed, LOW ); }

    // get the global trend of temperature
    int trend = getTemperatureTrend();

    #ifdef DEBUG
        Serial.print("global trend: ");
        Serial.println(trend);
    #endif

    // check if stages are active and:
    //  - activate stopped fans
    //  - adjust speed for running fans
    for ( int st = 0; st < stageCount; st++ ) {
        if ( stages[st].active ) {
            // stage is active
            analogWrite( stages[st].ledPin, HIGH );
            for ( int f = 0; f < fanCount; f++ ) {
                if ( fans[f].stage == st + 1 ){
                    // Fan is mapped to this stage
                    if ( fans[f].active ) {
                        if ( trend > 1 ) {
                            // trend is grater 1, speed up
                            fans[f].pwmValuePri = fans[f].pwmValuePri + PWMStep;
                        } else if ( trend == 1 ) {
                            // trend is 1, slow down
                            fans[f].pwmValuePri = fans[f].pwmValuePri - PWMStep;
                        }
                        if ( fans[f].pwmValuePri > MaxPWM ) { fans[f].pwmValuePri = MaxPWM; }   // ensure to not run over the MaxPWM value
                        if ( fans[f].pwmValuePri < MinPWM ) { fans[f].pwmValuePri = MinPWM; }   // ensure to not run under the MinPWM value
                        if ( fans[f].pinSec != 0 ) { 
                            // dual Fan setup
                            if ( fans[f].pwmValuePri == MaxPWM ) {
                                // primary fan is on max, max out too
                                fans[f].pwmValueSec = MaxPWM;
                            } else if ( fans[f].pwmValuePri == MinPWM ) {
                                // primary fan is on min, limit to min
                                fans[f].pwmValueSec = MinPWM;
                            } else {
                                // set pwm for sec Fan and preserve the Offset
                                fans[f].pwmValueSec = fans[f].pwmValuePri - dualFanOffset;
                                if ( fans[f].pwmValueSec < MinPWM ) { fans[f].pwmValueSec = MinPWM; }
                            }
                        }
                    } else {
                        // Fan not active - activate
                        fans[f].active = true;
                        fans[f].pwmValuePri = MinPWM;
                        if ( fans[f].pinSec != 0 ) { fans[f].pwmValueSec = MinPWM; }
                    }
                }
            }
        } else {
            // stage is inative - power off fans, if powered on
            analogWrite( stages[st].ledPin, LOW );
            for ( int f = 0; f < fanCount; f++ ) {
                if ( fans[f].active && fans[f].stage == st + 1 ) {
                    fans[f].active = false;
                    fans[f].pwmValuePri = 0;
                    if ( fans[f].pinSec != 0 ) { fans[f].pwmValueSec = 0; }
                }
            }
        }
        // special case for the last stage which represents the critical termperature to turn on all fans to max
        if ( ( st == stageCount - 1 ) && stages[st].active ) {
            for ( int f = 0; f < fanCount; f++ ) {
                fans[f].active = true;
                fans[f].pwmValuePri = MaxPWM;
                if ( fans[f].pinSec != 0 ) { fans[f].pwmValueSec = MaxPWM; }
            }
        }
    }
}

int getHighestTemperature() {
    int a = 0;
    for ( int s = 0; s < sensorCount; s++ ) {
        if ( sensors[s].current > a ) { a = sensors[s].current; }
    }
    return a;
}

int getTemperatureTrend() {
    // check all sensors for trend = true. means temperature is increasing or equal to the historyAvg
    int a = 0;
    for ( int s = 0; s < sensorCount; s++ ) {
        if ( sensors[s].trend > a ) { a = sensors[s].trend; }
    }
    return a;
}