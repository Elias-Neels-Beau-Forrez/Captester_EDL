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

typedef enum ranges {
  range_nF,
  range_uF,
  outOfRange,
  noCap
} Range_t;

Range_t range = range_nF;  // Start with nF range by default
bool autoRange = true;
volatile bool oscOff = false;

unsigned long updatePeriod = 1000;
unsigned long prevUpdate;
int startupCounter = 10;

// Calibration constants
const float OSC_CONSTANT = 1.443;  // RC oscillator constant
const float NF_SCALE = 1.0;      // Scaling factor for nF range
const float UF_SCALE = 1000.0;   // Scaling factor for µF range

void setRange(Range_t r) {
  range = r;
  switch (range) {
    case noCap:
    case range_nF:
      digitalWrite(RANGE_SEL_PIN, LOW);
      break;
    case range_uF:
    case outOfRange:
      digitalWrite(RANGE_SEL_PIN, HIGH);
      break;
  }
}

float compute_c() {
  if (oscPeriod < 20 || oscPeriod > 1000000) {
    return 0.0;
  }

  // Calculate base capacitance value
  float c = OSC_CONSTANT * oscPeriod;
  
  // Apply appropriate scaling factor
  if (range == range_uF) {
    c /= UF_SCALE;  // Convert to µF
  } else {
    c /= NF_SCALE;  // Convert to nF
  }

  // Auto-ranging logic
  if (autoRange) {
    if (c < 0.1) {
      setRange(noCap);
    } else if (c > 200 && range == range_uF) {
      setRange(outOfRange);
    }
  }

  return c;
}

void displayCapacitance(float c) {
  static Range_t prevRange = range_nF;
  char msg[16];
  lcd.clear();

  // Check if there's no capacitor inserted or if the reading is out of range
  if (range == noCap || c < 0.1) {
    lcd.write("Insert capacitor", 0, 0);
  } else if (range == outOfRange) {
    lcd.write("Out of range", 0, 0);
  } else {
    // Apply hysteresis to maintain stable display of the lowest unit
    if (c >= 1000.0 && (prevRange == range_nF || range == range_nF)) {
      // Switch to µF if capacitance is 1000 nF or more
      c /= 1000.0;  // Convert to µF
      range = range_uF;  // Change range to µF
      prevRange = range_uF;
    } else if (c < 1.0 && (prevRange == range_uF || range == range_uF)) {
      // Switch back to nF if capacitance falls below 1 µF
      c *= 1000.0;  // Convert back to nF
      range = range_nF;  // Change range to nF
      prevRange = range_nF;
    } else {
      prevRange = range;
    }

    // Format the capacitance with appropriate precision
    if (c >= 100) {
      dtostrf(c, 6, 1, msg);  // 1 decimal place for values >= 100
    } else if (c >= 10) {
      dtostrf(c, 6, 2, msg);  // 2 decimal places for values >= 10
    } else {
      dtostrf(c, 6, 3, msg);  // 3 decimal places for values < 10
    }

    // Trim leading spaces
    char *ptr = msg;
    while (*ptr == ' ') ptr++;

    // Add appropriate units based on the current range
    strcat(ptr, (range == range_nF) ? " nF" : " uF");
    lcd.write(ptr, 0, 0);
  }
}

void osc_int() {
  noInterrupts();
  EIFR = bit(INTF0);
  unsigned long now = micros();
  oscPeriod = now - prevOscInt;
  prevOscInt = now;
  
  if (oscPeriod < 20 || oscPeriod > 1000000) {
    oscOff = true;
    detachInterrupt(digitalPinToInterrupt(OSC_IN_PIN));
  }
  interrupts();
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Capacitor Tester Starting..."));
  
  pinMode(SW1_PIN, INPUT);
  pinMode(SW2_PIN, INPUT);
  pinMode(USER_LED_PIN, OUTPUT);
  pinMode(OSC_IN_PIN, INPUT);
  pinMode(RANGE_SEL_PIN, OUTPUT);
  
  digitalWrite(USER_LED_PIN, HIGH);

  lcd.begin();
  lcd.setBacklight(true);
  lcd.clear();
  lcd.write("Starting ", 0, 0);

  // Initialize oscillator
  prevOscInt = micros();
  attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), osc_int, RISING);
  setRange(range_nF);

  // Startup calibration
  prevUpdate = millis();
  while (startupCounter > 0) {
    lcd.write(".");
    float c = compute_c();
    if (c > 0.1) {
      break;
    }
    EIFR = bit(INTF0);
    attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), osc_int, RISING);
    interrupts();
    oscOff = false;
    delay(500);
    startupCounter--;
  }

  digitalWrite(USER_LED_PIN, LOW);
  Serial.println(F("Ready"));
}

void loop() {
  unsigned long now = millis();
  
  if (now - prevUpdate >= updatePeriod) {
    prevUpdate = now;
    
    if (oscOff || oscPeriod < 20 || oscPeriod > 1000000) {
      lcd.clear();
      lcd.write("Insert capacitor", 0, 0);
      delay(500);
      
      // Reset oscillator
      oscOff = false;
      EIFR = bit(INTF0);
      attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), osc_int, RISING);
      interrupts();
      prevOscInt = micros();
    } else {
      float c = compute_c();
      displayCapacitance(c);
      
      // Debug output
      Serial.print("Period: ");
      Serial.print(oscPeriod);
      Serial.print("us, Cap: ");
      Serial.print(c);
      Serial.println((range == range_nF) ? " nF" : " uF");
    }
  }

  // Handle oscillator recovery
  if (oscOff) {
    Serial.println("Oscillator reset");
    lcd.clear();
    lcd.write("Insert capacitor", 0, 0);
    delay(1000);
    
    oscOff = false;
    EIFR = bit(INTF0);
    attachInterrupt(digitalPinToInterrupt(OSC_IN_PIN), osc_int, RISING);
    interrupts();
    prevOscInt = micros();
  }

  // Backlight control
  if (digitalRead(SW2_PIN) == LOW) {
    lcd.setBacklight(!lcd.getBacklight());
    delay(500);  // Debounce
  }
}
