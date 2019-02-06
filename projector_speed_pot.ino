// PINS
#define PEDAL A0
#define LED_MONITOR A1
#define LED 10
#define _MOTOR_PWM 5  // arduino pin number
#define _MOTOR_REVERSE A3  // arduino pin number

// Behaviour constants
#define PEDAL_THRESHOLD 600  // with the current resistor divider, there is basically nothing from 1023 (open) to 440 (minimum press)
                             // the pedal threshold can be increased from 440 to have more power delivered at the start of its range
#define MOTOR_MAX_POWER 180  // 8-bit pwm range
#define MOTOR_POWER_STEP_DT 4  // milliseconds

// private state tracking
uint8_t _motor_speed = 0;
bool _motor_reversing = false;


/**
 * gently handle motor speed changes
 * 
 * never write directly to _MOTOR_PWM or _MOTOR_REVERSE, call this function instead.
 * 
 * this version blocks, but it could be on a timer interrupt to be less intrusive if needed.
 * 
 * new_speed should be between 0-1024
 */
void drive(uint16_t new_speed, bool reverse = false) {
  uint8_t target = map(new_speed, 0, 1024, 0, MOTOR_MAX_POWER);
  while (_motor_reversing != reverse ||
         _motor_speed     != target) {
    delay(MOTOR_POWER_STEP_DT);

    if (_motor_speed == 0) {
      // only flip the direction signal when the motor is at rest
      _motor_reversing = reverse;
      digitalWrite(_MOTOR_REVERSE, reverse ? HIGH : LOW);  // swap low/high for motor wiring
    }

    if (_motor_reversing != reverse) {
      // need to change direction: ramp down to zero first first
      _motor_speed -= 1;
    } else {
      // ramp up or down toward the new speed
      _motor_speed += (_motor_speed < target) ? 1 : -1;
    }

    analogWrite(_MOTOR_PWM, map(_motor_speed, 0, 255, 0, MOTOR_MAX_POWER));
  }
}


uint16_t get_pedal() {
  uint16_t reading = analogRead(PEDAL);
  if (reading > PEDAL_THRESHOLD) {
    return 0;
  }
  return map(reading, PEDAL_THRESHOLD, 0, 0, 1024);
}


void setup() {
  // motor pin setup
  pinMode(_MOTOR_PWM, OUTPUT);
  analogWrite(_MOTOR_PWM, 0);
  pinMode(_MOTOR_REVERSE, OUTPUT);
  digitalWrite(_MOTOR_REVERSE, LOW);
  // end motor setup

  pinMode(PEDAL, INPUT);

  Serial.begin(9600);
}

void loop() {
  uint16_t reading = get_pedal();
  drive(reading);
//  Serial.println(reading);
//  delay(60);
}

