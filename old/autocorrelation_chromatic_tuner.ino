#include "arduinoFFT.h"
 
#define SAMPLES 4096
#define SAMPLING_FREQUENCY 4096

#define LED_BUILTIN 35

unsigned int samplingPeriod;
unsigned long startTime;
 
volatile uint16_t rawData[SAMPLES];
// double vImag[SAMPLES];
 
int serialCommand = 0;
double serialValue = 0;

long currentSum, previousSum, twoPreviousSum ;
int threshold = 0;
float frequency = 0;
byte pdState = 0;

// This is the GPIO pin that it will read from.
const int adcPin = 5;

void setup() 
{
    pinMode(adcPin, INPUT);
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);
    // samplingPeriod = round(1000000*(1.0/SAMPLING_FREQUENCY)); // 1 second
    samplingPeriod = round(250000*(1.0/SAMPLING_FREQUENCY)); // 1/4 of a second

    pinMode(LED_BUILTIN, OUTPUT);
}


void loop() {  
  int samplesObtained = 0;
  digitalWrite(LED_BUILTIN, HIGH);
    Serial.println("Starting sample");
    for(int i = 0; i < SAMPLES; i++) {
        startTime = micros();
     
        rawData[i] = analogRead(adcPin);
        samplesObtained++;

        while(micros() < (startTime + samplingPeriod)) {
          // This appears to be here just to let the amount of time
          // pass by to make the sampling period be as accurate as
          // possible.
        }
    }
    Serial.printf("Finished sampling: %d samples obtained\n", samplesObtained);
  digitalWrite(LED_BUILTIN, LOW);

  findFrequencyWithAutocorrelation();
}

//
// Autocorrelation
//
void findFrequencyWithAutocorrelation() {
  // Calculate mean to remove DC offset
  long meanSum = 0 ;
  for (int k = 0; k < SAMPLES; k++) {
    meanSum += rawData[k] ;
  }
  char mean = meanSum / SAMPLES ;
  // Remove mean
  for (int k = 0; k < SAMPLES; k++) {
    rawData[k] -= mean ;
  }

  // Autocorrelation
  currentSum = 0 ;
  pdState = 0 ;
  for (int i = 0; i < SAMPLES && (pdState != 3); i++) {
    // Autocorrelation
    float period = 0 ;
    twoPreviousSum = previousSum ;
    previousSum = currentSum ;
    currentSum = 0 ;
    for (int k = 0; k < SAMPLES - i; k++) {
      currentSum += char(rawData[k]) * char(rawData[k + i]) ;
    }
    // Peak detection
    switch (pdState) {
      case 0:   // Set threshold based on zero lag autocorrelation
        threshold = currentSum / 2 ;
        pdState = 1 ;
        break ;
      case 1:   // Look for over threshold and increasing
        if ((currentSum > threshold) && (currentSum - previousSum) > 0) pdState = 2 ;
        break ;
      case 2:   // Look for decreasing (past peak over threshold)
        if ((currentSum - previousSum) <= 0) {
          // quadratic interpolation
          float interpolationValue = 0.5 * (currentSum - twoPreviousSum) / (2 * previousSum - twoPreviousSum - currentSum) ;
          period = i - 1 + interpolationValue ;
          pdState = 3 ;
        }
        break ;
      default:
        pdState = 3 ;
        break ;
    }

    // Frequency identified in Hz
     if (threshold > 100) {
      frequency = SAMPLING_FREQUENCY / period;
      if (frequency < 2000) {
        Serial.printf("Frequency detected: %f\n", frequency);
       }
     }
  }
}
