// Arduino Pitch Detection on A0 with autocorrelation and peak detection
// Original author(s): akellyirl, revised by robtillaart, MrMark, barthulsen
// http://forum.arduino.cc/index.php?topic=540969.15
// Continuous ADC ala http://www.instructables.com/id/Arduino-Audio-Input/ "Step 7"

#define sampleFrequency 9615.4
#define bufferSize 512

volatile byte  rawData[bufferSize] ;  // Buffer for ADC capture
volatile int sampleCnt ;                    // Pointer to ADC capture buffer
long currentSum, previousSum, twoPreviousSum ;
int threshold = 0;
float frequency = 0;
float correctFrequency;//the correct frequency for the string being played
float semitones; //(Frequency - correctFrequency)/half step*100
byte pdState = 0;

void setup() {

  Serial.begin(115200) ;
  pinMode(LED_BUILTIN, OUTPUT);
  cli();//disable interrupts

  //set up continuous sampling of analog pin 0
  //clear ADCSRA and ADCSRB registers
  ADCSRA = 0 ;
  ADCSRB = 0 ;

  ADMUX |= (1 << REFS0) ; //set reference voltage
  ADMUX |= (1 << ADLAR) ; //left align the ADC value- so we can read highest 8 bits from ADCH register only

  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // ADC clock 128 prescaler- 16mHz/128=125kHz->9615 sps
  ADCSRA |= (1 << ADATE); //enable auto trigger
  ADCSRA |= (1 << ADIE) ; //enable interrupts when measurement complete
  ADCSRA |= (1 << ADEN) ; //enable ADC
  ADCSRA |= (1 << ADSC) ; //start ADC measurements
}

ISR(ADC_vect) {     // When ADC sample ready, put in buffer if not full
  if (sampleCnt < bufferSize)
  {
    rawData[sampleCnt] = ADCH ;
    sampleCnt++ ;
  }
}

void readData() {
  sampleCnt = 0 ;
  sei() ;         // Enable interrupts, samples placed in buffer by ISR
  while (sampleCnt < bufferSize) ;  // Spin until buffer is full
  cli() ;         // Disable interrupts
}

void findFrequency() {
  // Calculate mean to remove DC offset
  long meanSum = 0 ;
  for (int k = 0; k < bufferSize; k++) {
    meanSum += rawData[k] ;
  }
  char mean = meanSum / bufferSize ;
  // Remove mean
  for (int k = 0; k < bufferSize; k++) {
    rawData[k] -= mean ;
  }

  // Autocorrelation
  currentSum = 0 ;
  pdState = 0 ;
  for (int i = 0; i < bufferSize && (pdState != 3); i++) {
    // Autocorrelation
    float period = 0 ;
    twoPreviousSum = previousSum ;
    previousSum = currentSum ;
    currentSum = 0 ;
    for (int k = 0; k < bufferSize - i; k++) {
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
      frequency = sampleFrequency / period;
   
  if(frequency>40.03&frequency<42.41){
    correctFrequency = 41.203;//E1 Low E on Bass
    semitones = (frequency-correctFrequency)/2.38*100;
    Serial.println(" ");
    Serial.print("E1-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }   
  if(frequency>53.43&frequency<56.61){
    correctFrequency = 55.0;//A1 Bass
    semitones = (frequency-correctFrequency)/3.18*100;
    Serial.println(" ");
    Serial.print("A1-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }   
  if(frequency>71.33&frequency<75.57){
    correctFrequency = 73.416;//D2 Bass
    semitones = (frequency-correctFrequency)/4.24*100;
    Serial.println(" ");
    Serial.print("D2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>80.06&frequency<84.82){
    correctFrequency = 82.407;//E2
    semitones = (frequency-correctFrequency)/4.76*100;
    Serial.println(" ");
    Serial.print("E2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>84.82&frequency<89.87){
    correctFrequency = 87.307;//F2
    semitones = (frequency-correctFrequency)/5.04*100;
    Serial.println(" ");
    Serial.print("F2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>95.21&frequency<100.87){
    correctFrequency = 97.999;//G2
    semitones = (frequency-correctFrequency)/5.66*100;
    Serial.println(" ");
    Serial.print("G2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>106.87&frequency<113.22){
    correctFrequency = 110.0;//A2
    semitones = (frequency-correctFrequency)/6.35*100;
    Serial.println(" ");
    Serial.print("A2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>119.96&frequency<127.09){
    correctFrequency = 123.471;//B2
    semitones = (frequency-correctFrequency)/7.13*100;
    Serial.println(" ");
    Serial.print("B2-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>127.09&frequency<134.65){
    correctFrequency = 130.813;//C3
    semitones = (frequency-correctFrequency)/7.56*100;
    Serial.println(" ");
    Serial.print("C3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>142.65&frequency<151.13){
    correctFrequency = 146.83;//D3
    semitones = (frequency-correctFrequency)/8.48*100;
    Serial.println(" ");
    Serial.print("D3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>160.12&frequency<169.64){
    correctFrequency = 164.814;//E3
    semitones = (frequency-correctFrequency)/9.52*100;
    Serial.println(" ");
    Serial.print("E3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>169.64&frequency<179.73){
    correctFrequency = 174.614;//F3
    semitones = (frequency-correctFrequency)/10.09*100;
    Serial.println(" ");
    Serial.print("F3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>190.42&frequency<201.74){
    correctFrequency = 195.998;//G3
    semitones = (frequency-correctFrequency)/11.32*100;
    Serial.println(" ");
    Serial.print("G3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>213.74&frequency<226.45){
    correctFrequency = 220;//A3
    semitones = (frequency-correctFrequency)/12.71*100;
    Serial.println(" ");
    Serial.print("A3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>239.91&frequency<254.18){
    correctFrequency = 246.942;//B3
    semitones = (frequency-correctFrequency)/14.27*100;
    Serial.println(" ");
    Serial.print("B3-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>254.18&frequency<269.29){
    correctFrequency = 261.626;//C4
    semitones = (frequency-correctFrequency)/15.11*100;
    Serial.println(" ");
    Serial.print("C4-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>285.30&frequency<302.27){
    correctFrequency = 293.665;//D4
    semitones = (frequency-correctFrequency)/16.97*100;
    Serial.println(" ");
    Serial.print("D4-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
  if(frequency>320.24&frequency<339.29){
    correctFrequency = 329.628;//E4
    semitones = (frequency-correctFrequency)/19.04*100;
    Serial.println(" ");
    Serial.print("E4-");
    Serial.print(correctFrequency,1);
    Serial.print("    ");
    Serial.print(frequency,1);
    Serial.print("Hz");
    Serial.print("    ");
    Serial.print(frequency-correctFrequency);
    Serial.print("dif    ");
    Serial.print(semitones,0);
    Serial.print(" cents ");
    if(semitones>0){Serial.println("sharp");}
    if(semitones<0){Serial.println("flat");}
  }
    }
  }
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  readData();
  digitalWrite(LED_BUILTIN, LOW);
  findFrequency();
}