#include <avr/wdt.h>
#include <math.h>

const int PIN_DIGITAL_RELAY = 12;
const int PIN_ANALOG_SENSOR = 1;
const int PIN_ANALOG_POTENTIOMETER = 0;
const int LOOP_DELAY_MS = 1000;

float meanRelayStatus = 0; //between 0 (OFF) and 1 (ON)
int delayRelayOffMs  = 0;
int delayRelayOnMs   = 0;

void setup() {
  wdt_disable();
  wdt_enable(WDTO_2S);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_DIGITAL_RELAY, OUTPUT);
  
  Serial.begin(115200);
  Serial.println("Start");
}

float kelvinToCelsius(float kelvinTemp) {
  return kelvinTemp - 273.15;
}

float sensorTemperature(int rawAdc) {
 float temp;
 temp = log(10000.0*((1024.0/rawAdc-1))); 
 temp = 1 / (0.001129148 + (0.000234125 + (0.0000000876741 * temp * temp ))* temp );
 return kelvinToCelsius(temp);
}

float selectedTemperature(int rawAdc) {
  const float highTemp = 250; // the real value is a 60% of this one
  const float lowTemp = 12;
  return lowTemp + (highTemp-lowTemp)*(1024-rawAdc)/1024.0f;
}

void resetDelays() {
    delayRelayOffMs = 0;
    delayRelayOnMs = 0;
}

bool shouldWaitOff() {
  return delayRelayOffMs > 0 && delayRelayOnMs == 0;
}

void updateRelayDelays(float sensorTemp, float selectedTemp) {
  const int minDelay = 3  * 1000;
  const int maxDelay = 30 * 1000;
  float delayOnAbsoluteTemperatureMs = 10000.0f / (1.0f + (300.0f - sensorTemp)); //25º: 357, 300º: 10000
  float delayOnTemperatureDifferenceMs = (selectedTemp - sensorTemp)*1000;
  float delayRelayOnMsFloat = delayOnAbsoluteTemperatureMs + delayOnTemperatureDifferenceMs;
  delayRelayOnMsFloat = min(float(maxDelay), delayRelayOnMsFloat);
  delayRelayOnMsFloat = max(float(minDelay), delayRelayOnMsFloat);
  delayRelayOnMsFloat *= 0.75f;
  bool wasBeforeOn = delayRelayOnMs == 0 && delayRelayOffMs > 0;
  if (!wasBeforeOn && meanRelayStatus < 0.5) {
    delayRelayOnMs = int(delayRelayOnMsFloat);
  }
  delayRelayOffMs = 30*1000 - int(delayRelayOnMsFloat);
}

bool shouldActivateRelay(float sensorTemp, float selectedTemp) {
  if (sensorTemp > selectedTemp) {
    resetDelays();
    return false;
  }
  
  const float MIN_TEMP_DIFFERENCE = 42;
  if ((selectedTemp - sensorTemp) > MIN_TEMP_DIFFERENCE) {
    resetDelays();
    return true;
  }

  if ((delayRelayOnMs == 0) && (delayRelayOffMs == 0)) {
    updateRelayDelays(sensorTemp, selectedTemp);
  }
  
  if (shouldWaitOff()) {
    delayRelayOffMs = max(0, delayRelayOffMs - LOOP_DELAY_MS);
  } else {
    delayRelayOnMs = max(0, delayRelayOnMs - LOOP_DELAY_MS);
  }
  return !shouldWaitOff();
}

bool shouldActivateRelayAndUpdateMeanStatus(float sensorTemp, float selectedTemp) {
  bool hadRemainingDelayOn  = delayRelayOnMs > 0;
  bool hadRemainingDelayOff = delayRelayOffMs > 0;
  
  bool shouldActivate = shouldActivateRelay(sensorTemp, selectedTemp);

  bool hasRemainingDelayOn  = delayRelayOnMs > 0;
  bool hasRemainingDelayOff = delayRelayOffMs > 0;
  if ((hadRemainingDelayOn  != hasRemainingDelayOn) ||
      (hadRemainingDelayOff != hasRemainingDelayOff)) {
    updateRelayDelays(sensorTemp, selectedTemp);
  }
  
  meanRelayStatus = meanRelayStatus*0.96f + (shouldActivate ? 1 : 0) * 0.04f;
  return shouldActivate;
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  wdt_reset();

  float sensorTemp = sensorTemperature(analogRead(PIN_ANALOG_SENSOR));
  float selectedTemp = selectedTemperature(analogRead(PIN_ANALOG_POTENTIOMETER));
  
  Serial.print("Sensor Temp: ");
  Serial.print(int(sensorTemp)); 
  Serial.print("Cº, Selected: ");
  Serial.print(int(selectedTemp)); 
  Serial.print("Cº, delayOff: "); 
  Serial.print(delayRelayOffMs); 
  Serial.print("ms., delayOn: "); 
  Serial.print(delayRelayOnMs); 
  Serial.print("ms. meanStatus: "); 
  Serial.print(meanRelayStatus); 
  
  if(shouldActivateRelayAndUpdateMeanStatus(sensorTemp, selectedTemp)){
    Serial.println(" ON");
    digitalWrite(PIN_DIGITAL_RELAY, HIGH);
  } else {
    Serial.println(" OFF");
    digitalWrite(PIN_DIGITAL_RELAY, LOW);
  }
  
  digitalWrite(LED_BUILTIN, LOW);
  delay(LOOP_DELAY_MS);
}
