// PINS
#define PEDAL A0
#define LED_MONITOR A1  // current-limiting resistor is 2.2R (measured; 2R rated)
#define LED 10
#define _MOTOR_PWM 5  // arduino pin number
#define _MOTOR_REVERSE A3  // arduino pin number

#define LED_BRIGHTNESS A2
#define LED_RESPONSIVE A6 // digital 4
#define POT_SPEED A8  // digital 8
#define REVERSE 15


// Behaviour constants
#define PEDAL_THRESHOLD 600  // with the current resistor divider, there is basically nothing from 1023 (open) to 440 (minimum press)
                             // the pedal threshold can be increased from 440 to have more power delivered at the start of its range
#define MOTOR_MAX_POWER 190  // 8-bit pwm range
#define MOTOR_POWER_STEP_DT 4  // milliseconds

// public target updates
//volatile uint8_t motor_target_speed = 0;
volatile bool motor_target_reverse = false;


// private state tracking
volatile uint8_t _motor_current_speed = 0;
volatile bool _motor_current_reverse = false;
volatile uint8_t _motor_update_ms_counter = 0;

uint16_t get_pedal() {
  uint16_t reading = analogRead(PEDAL);
  if (reading > PEDAL_THRESHOLD) {
    return 0;
  }
  return map(reading, PEDAL_THRESHOLD, 0, 0, 1024);
}

/**
 * gently handle motor speed changes.
 * 
 * this gets called every MOTOR_POWER_STEP_DT ms by an interrupt handler (TIMER0_COMPA_vect)
 */
void update_motor() {
  uint16_t pedal = get_pedal();
  uint8_t motor_target_speed = map(pedal, 0, 1024, 0, MOTOR_MAX_POWER);

  // maybe no update necessary
  if (_motor_current_reverse == motor_target_reverse &&
      _motor_current_speed == motor_target_speed) {
    return;
  }

  if (_motor_current_speed == 0) {
    // only flip the direction signal when the motor is at rest
    _motor_current_reverse = motor_target_reverse;
    digitalWrite(_MOTOR_REVERSE, _motor_current_reverse ? HIGH : LOW);  // swap low/high for motor wiring
  }

  if (_motor_current_reverse != motor_target_reverse) {
    // need to change direction: ramp down to zero first first
    _motor_current_speed -= 1;
  } else {
    // ramp up or down toward the new speed
    _motor_current_speed += (_motor_current_speed < motor_target_speed) ? 1 : -1;
  }

  analogWrite(_MOTOR_PWM, map(_motor_current_speed, 0, 255, 0, MOTOR_MAX_POWER));
}

// every 1ms
SIGNAL(TIMER0_COMPA_vect) {
  _motor_update_ms_counter += 1;
  _motor_update_ms_counter %= MOTOR_POWER_STEP_DT;
  if (_motor_update_ms_counter == 0) {
    update_motor();
  }
//  update_lamp();
}

void setup() {
  // motor pin setup
  pinMode(_MOTOR_PWM, OUTPUT);
  analogWrite(_MOTOR_PWM, 0);
  pinMode(_MOTOR_REVERSE, OUTPUT);
  digitalWrite(_MOTOR_REVERSE, LOW);
  // end motor setup

  pinMode(PEDAL, INPUT);

  pinMode(LED, OUTPUT);
  analogWrite(LED, 127);

  // set up timer0 to call our motor update speed function every 1ms
  OCR0A = 0xAF;  // arbitrary -- the count we hook into
  TIMSK0 |= _BV(OCIE0A);  // interrupt on that count please!

  Serial.begin(9600);
}

void loop() {
}

