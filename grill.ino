#include <avr/wdt.h>
#include <math.h>

const int PIN_DIGITAL_RELAY = 12;
const int PIN_ANALOG_SENSOR = 1;
const int PIN_ANALOG_POTENTIOMETER = 0;
const int LOOP_DELAY_MS = 1000;

int delaySensorOffMs  = 0;
int delaySensorOnMs   = 0;

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
  const float highTemp = 300;
  const float lowTemp = 12;
  return lowTemp + (highTemp-lowTemp)*(1024-rawAdc)/1024.0f;
}

bool activateRelay(float sensorTemp, float selectedTemp) {
  if (sensorTemp > selectedTemp) {
    delaySensorOffMs = 0;
    delaySensorOnMs = 0;
    return false;
  }
  
  if ((delaySensorOffMs > 0) && (delaySensorOnMs == 0)) {
    delaySensorOffMs = max(0, delaySensorOffMs - LOOP_DELAY_MS);
    return false;
  }

  const float minTemperatureDifference = 25;

  if ((selectedTemp - sensorTemp) > minTemperatureDifference) {
    delaySensorOffMs = 0;
    delaySensorOnMs = 0;
    return true;
  }
  
  const int minDelay = 3  * 1000;
  const int maxDelay = 30 * 1000;
  if (delaySensorOnMs > 0) {
    delaySensorOnMs = max(0, delaySensorOnMs - LOOP_DELAY_MS);
  } else {
    float delayOnAbsoluteTemperatureMs = 10000.0f / (1.0f + (300.0f - sensorTemp)); //25º: 357, 300º: 10000
    float delayOnTemperatureDifferenceMs = (selectedTemp - sensorTemp)*1000;
    float delaySensorOnMsFloat = delayOnAbsoluteTemperatureMs + delayOnTemperatureDifferenceMs;
    delaySensorOnMsFloat = min(float(maxDelay), delaySensorOnMsFloat);
    delaySensorOnMsFloat = max(float(minDelay), delaySensorOnMsFloat);
    delaySensorOnMs = int(delaySensorOnMsFloat);
    delaySensorOffMs = 30*1000 - delaySensorOnMs;
  }
  
  return true;
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
  Serial.print(delaySensorOffMs); 
  Serial.print("ms., delayOn: "); 
  Serial.print(delaySensorOnMs); 
  Serial.print("ms."); 
  
  if(activateRelay(sensorTemp, selectedTemp)){
    Serial.println(" ON");
    digitalWrite(PIN_DIGITAL_RELAY, HIGH);
  } else {
    Serial.println(" OFF");
    digitalWrite(PIN_DIGITAL_RELAY, LOW);
  }
  
  digitalWrite(LED_BUILTIN, LOW);
  delay(LOOP_DELAY_MS);
}
