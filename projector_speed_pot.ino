#define POT A0
#define _MOTOR_PWM 3  // arduino pin number
#define _MOTOR_REVERSE 4  // arduino pin number
#define _MOTOR_MAX_POWER 127  // 8-bit pwm range
#define _MOTOR_POWER_STEP_DT 4  // milliseconds

// never write to these variables directly
uint8_t _motor_speed = 0;
bool _motor_reversing = false;


/**
 * gently handle motor speed changes
 * 
 * never write directly to _MOTOR_PWM or _MOTOR_REVERSE, call this function instead.
 * 
 * this version blocks, but it could be on a timer interrupt to be less intrusive if needed.
 */
void drive(uint8_t new_speed, bool reverse = false) {
  while (_motor_reversing != reverse ||
         _motor_speed     != new_speed) {
    if (_motor_speed == 0) {
      // only flip the direction signal when the motor is at rest
      _motor_reversing = reverse;
      digitalWrite(_MOTOR_REVERSE, reverse ? LOW : HIGH);  // swap low/high for motor wiring
    }

    if (_motor_reversing != reverse) {
      // need to change direction: ramp down to zero first first
      _motor_speed -= 1;
    } else {
      // ramp up or down toward the new speed
      _motor_speed += (_motor_speed < new_speed) ? 1 : -1;
    }

    analogWrite(_MOTOR_PWM, map(_motor_speed, 0, 255, 0, _MOTOR_MAX_POWER));
    delay(_MOTOR_POWER_STEP_DT);
  }
}

volatile unsigned long last = 0;


void hi() {
  unsigned long now = millis();
  if (now == last) return;
  unsigned long dt = now - last;
  if (dt < 10) return;
  last = now;
  Serial.println(1.0 / ((float)dt / 1000));
}

void setup() {
  // motor pin setup
  pinMode(_MOTOR_PWM, OUTPUT);
  analogWrite(_MOTOR_PWM, 0);
  pinMode(_MOTOR_REVERSE, OUTPUT);
  digitalWrite(_MOTOR_REVERSE, LOW);
  // end motor setup

  pinMode(2, INPUT_PULLUP);
  last = millis();
  attachInterrupt(digitalPinToInterrupt(2), hi, FALLING);

  Serial.begin(9600);
}

void loop() {
  uint16_t reading = analogRead(POT);
  drive(reading >> 2);  // reduce 10-bit to 8-bit
//  Serial.println(reading);
  delay(2);
}
