// PINS
#define PEDAL A0
#define POT_SPEED A8  // digital 8
#define MOTOR_PWM 5  // arduino pin number
#define MOTOR_REVERSE A3  // arduino pin number
#define DIRECTION A6 // digital 4

#define LED_PWM 10
#define LED_BRIGHTNESS A2
#define LED_MONITOR A1  // current-limiting resistor is 2.2R (measured; 2R rated)

#define _UNUSED_ 15  // it's wired in! ready to work as a switch (set INPUT_PULLUP) or a digital out

// Behaviour constants
#define PEDAL_THRESHOLD 600  // with the current resistor divider, there is basically nothing from 1023 (open) to 440 (minimum press)
                             // the pedal threshold can be increased from 440 to have more power delivered at the start of its range
#define DIRECTION_THRESHOLD 315
#define MOTOR_MAX_POWER 190  // 8-bit pwm range
#define MOTOR_POWER_STEP_DT 3  // milliseconds

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

uint16_t get_pot() {
  uint16_t reading = analogRead(POT_SPEED);
  float norm = (float)reading / 1023;
  norm = sqrt(norm);  // more power sooner
  return norm * 1023;
}

volatile bool takeover = false;
void open_shutter() {
  takeover = true;
  digitalWrite(MOTOR_REVERSE, HIGH);
  analogWrite(MOTOR_PWM, 100);
  delay(80);
  digitalWrite(MOTOR_PWM, 0);
  digitalWrite(MOTOR_REVERSE, LOW);
  takeover = false;
}

/**
 * gently handle motor speed changes.
 * 
 * this gets called every MOTOR_POWER_STEP_DT ms by an interrupt handler (TIMER0_COMPA_vect)
 */
void update_motor() {
  uint16_t pedal = get_pedal();
  uint16_t pot = get_pot();
  uint16_t combined = max(pedal, pot);
  if (takeover) {
    return;
  }
  uint8_t motor_target_speed = map(combined, 0, 1024, 0, MOTOR_MAX_POWER);
  motor_target_reverse = analogRead(DIRECTION) < DIRECTION_THRESHOLD;

  // maybe no update necessary
  if (_motor_current_reverse == motor_target_reverse &&
      _motor_current_speed == motor_target_speed) {
    return;
  }

  if (_motor_current_speed == 0) {
    // only flip the direction signal when the motor is at rest
    _motor_current_reverse = motor_target_reverse;
    digitalWrite(MOTOR_REVERSE, _motor_current_reverse ? LOW : HIGH);  // swap low/high for motor wiring
  }

  if (_motor_current_reverse != motor_target_reverse) {
    // need to change direction: ramp down to zero first first
    _motor_current_speed -= 1;
  } else if (_motor_current_speed != motor_target_speed) {
    // ramp up or down toward the new speed
    _motor_current_speed += (_motor_current_speed < motor_target_speed) ? 1 : -1;
  }

  analogWrite(MOTOR_PWM, map(_motor_current_speed, 0, 255, 0, MOTOR_MAX_POWER));
}

volatile uint8_t x = 0;
void update_lamp() {
  uint16_t brightness = analogRead(LED_BRIGHTNESS);
  uint8_t output = brightness >> 2;  // 10-bit to 8-bit
  if (_motor_current_speed == 0) {
    output = min(output, 204);  // max 80% when stopped
  }
  analogWrite(LED_PWM, output);
}

// every 1ms
SIGNAL(TIMER0_COMPA_vect) {
  if (++_motor_update_ms_counter > MOTOR_POWER_STEP_DT) {
    update_motor();
    _motor_update_ms_counter = 0;
  }
  update_lamp();
}

void setup() {
  // motor pin setup
  pinMode(MOTOR_PWM, OUTPUT);
  analogWrite(MOTOR_PWM, 0);
  pinMode(MOTOR_REVERSE, OUTPUT);
  digitalWrite(MOTOR_REVERSE, LOW);
  // end motor setup

  pinMode(PEDAL, INPUT);
  pinMode(POT_SPEED, INPUT);
  pinMode(DIRECTION, INPUT);

  pinMode(LED_PWM, OUTPUT);
  digitalWrite(LED_PWM, LOW);
  pinMode(LED_BRIGHTNESS, INPUT);
  pinMode(LED_MONITOR, INPUT);

  // set up timer0 to call our motor update speed function every 1ms
  OCR0A = 0xAF;  // arbitrary -- the count we hook into
  TIMSK0 |= _BV(OCIE0A);  // interrupt on that count please!

  // debug woo
  Serial.begin(9600);
}

void loop() {
}

