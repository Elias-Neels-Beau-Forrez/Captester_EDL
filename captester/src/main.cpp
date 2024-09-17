#include <Arduino.h>
#include <LCD.h>

#define USER_LED_PIN  9 

#define SW2_PIN       4
#define SW1_PIN       3

#define OSC_IN_PIN    2
#define RANGE_SEL_PIN 7

#define MCP_RESET_PIN 8

LCD lcd(MCP_RESET_PIN);

unsigned long prevOscInt = 0;
unsigned long oscPeriod;

typedef enum ranges{
  range_nF,
  range_uF,
  outOfRange,
  noCap
} Range_t;

Range_t range = range_nF;
bool autoRange = true;
volatile bool oscOff = false;

unsigned long updatePeriod = 1000;
unsigned long prevUpdate;
int startupCounter = 10;

// set oscillator range
void setRange(Range_t r){
  range = r;
  switch (range){
    case noCap:{
      digitalWrite(RANGE_SEL_PIN, LOW);
    } break;

    case range_nF:{
      digitalWrite(RANGE_SEL_PIN, LOW);
    } break;

    case range_uF:{
      digitalWrite(RANGE_SEL_PIN, HIGH);
    } break;
    
    case outOfRange:{
      digitalWrite(RANGE_SEL_PIN, HIGH);
    } break;

    default:{
    } break;
  }
}

// function to compute the capacity from the oscillation period
float compute_c(){
  Serial.println(oscPeriod);
  float c = 1.443*oscPeriod;

  if(range == range_uF){
    c /= 1000;
  }
  else{
    c /= 100;
  }

  if(autoRange){
    switch (range){
      case noCap:{
        if(c > 1) setRange(range_nF);
      }

      case range_nF:{
        if(c < 1) setRange(noCap);
        if(c >= 1000){
          setRange(range_uF);
          c /= 1000;
        }
      } break;

      case range_uF:{
        if(c < 1) {
          setRange(range_nF);
          c *= 1000;
          if(c < 1) setRange(noCap);
        }
        else{
          if(c >= 200) setRange(outOfRange);
        }
      } break;
      
      case outOfRange:{
        if(c < 200) setRange(range_uF);
        if(c < 1) {
          setRange(range_nF);
          c *= 1000;
          if(c < 1) setRange(noCap);
        }
      } break;
    }
  }
  else{
    setRange(range_nF);
  }

#ifdef DEBUG
  Serial.print("range: ");
  switch (range){
    case noCap:{
      Serial.println(F("No cap"));
    } break;
    case range_nF:{
      Serial.println(F("nF"));
    } break;
    case range_uF:{
      Serial.println(F("µF"));
    } break;
    case outOfRange:{
      Serial.println(F("out of range"));
    } break;
    default:{
    } break;
  }
#endif
  return c;
}

void serialPrintC(float c){
  switch (range){
    case noCap:{
      Serial.println("No cap");
    } break;

    case range_nF:{
      Serial.print(c);
      Serial.println(" nF");
    } break;

    case range_uF:{
      Serial.print(c);
      Serial.println(" µF");
    } break;
    
    case outOfRange:{
      Serial.println("Out of range");
    } break;

    default:{
    } break;
  }
}

// oscillator interrupt isr
void osc_int(void){
  noInterrupts();
  EIFR = bit(INTF0);     //clear interrupt flag
  unsigned long now = micros();
  oscPeriod = (unsigned long) now - prevOscInt;
  prevOscInt = now;
  if(oscPeriod<20){
    oscOff = true;
    detachInterrupt(digitalPinToInterrupt(OSC_IN_PIN));
  }
  else{
    interrupts();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Setup..."));

  // IO setup
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(USER_LED_PIN, OUTPUT);
  digitalWrite(USER_LED_PIN, HIGH);

  // display setup
  lcd.begin();
  lcd.setBacklight(true);

  // oscillator setup
  prevOscInt = micros();
  pinMode(OSC_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), &osc_int, RISING);

  pinMode(RANGE_SEL_PIN, OUTPUT);
  setRange(range_uF);

  pinMode(SW2_PIN, INPUT);

  lcd.write("Starting ", 0, 0);
  prevUpdate = millis();      

  compute_c();
  while((startupCounter--) > 0){
    lcd.write(".");
    float c = compute_c();
    if(c > 1){
      startupCounter = 0;
      Serial.print("break: ");
      Serial.println(c);
    }
    EIFR = bit(INTF0);
    attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), &osc_int, RISING);
    interrupts();
    oscOff = false;
    delay(1000);
  }


  digitalWrite(USER_LED_PIN, LOW);
  Serial.println(F("Done"));
}

void loop() {
  // update display every "updatePeriod" ms
  unsigned long now = millis();
  if(now > prevUpdate + updatePeriod){
    prevUpdate = now;

    float c = compute_c();
    serialPrintC(c);

    char msg[16];
    uint16_t val1 = floor(c);
    uint16_t val2 = ((uint16_t)floor(c*10)) % 10;
    if(range == range_nF){
      sprintf(msg, "%d.%d nF", val1, val2);
    }
    if(range == range_uF){
      sprintf(msg, "%d.%d uF", val1, val2);
    }
    lcd.clear();
    lcd.write(msg, 0, 0);
  }

  // oscillator interrupts turned of because when no cap is present, osc freq. is to high
  if(oscOff){
      oscOff = false;
      Serial.println("osc off");
      char msg[] = "Insert capacitor";
      lcd.clear();
      lcd.write(msg, 0, 0);

      // let's try again
      delay(1000);
      EIFR = bit(INTF0);
      attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), &osc_int, RISING);
      interrupts();
      prevOscInt = micros()-100;
  }

  if(digitalRead(SW2_PIN) == LOW){
    if(lcd.getBacklight()) lcd.setBacklight(false);
    else lcd.setBacklight(true);
    delay(500);
  }
}