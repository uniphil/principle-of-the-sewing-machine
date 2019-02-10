// PINS
#define PEDAL A0  // sewing machine pedal!
#define POT_SPEED A8  // pot for setting speed to a constant value
#define MOTOR_PWM 5  // motor speed drive (PWM. caution: don't drive 100% -- see MOTOR_MAX_POWER)

#define MOTOR_REVERSE A3  // arduino pin number
#define DIRECTION A6 // digital 4

#define LED_PWM 10  // LED output drive
#define LED_BRIGHTNESS A2  // control pot
#define LED_MONITOR A1  // current-limiting resistor is 2.2R (measured; 2R rated)

#define _UNUSED_ 15  // it's wired in! ready to work as a switch (set INPUT_PULLUP) or a digital out

// Behaviour constants
#define PEDAL_THRESHOLD 600  // with the current resistor divider, there is basically nothing from 1023 (open) to 440 (minimum press)
                             // the pedal threshold can be increased from 440 to have more power delivered at the start of its range
#define DIRECTION_THRESHOLD 315  // the "about half-way" point for the yellow direction knob (found by experimentation)
#define MOTOR_MAX_POWER 190  // 8-bit pwm range (0–255). Full 255 power is super fast (mechanical stress concern for projector)
#define MOTOR_POWER_STEP_DT 3  // milliseconds. higher: smoother response; lower: more immediate (and possibly rough)
#define BRIGHTNESS_NOISE_HISTORY 7  // brightness samples to keep, 3–255. averaged with 2 extremes removed for anti-flicker. subtle effect.

// Machine constants
#define RESISTOR_OHMS_MEASURED 2.2

// Motor state tracking
// code anywhere can read these, but only update_motor() should modify.
volatile bool motor_target_reverse = false;
volatile uint8_t _motor_current_speed = 0;
volatile bool _motor_current_reverse = false;
volatile uint8_t _motor_update_ms_counter = 0;

// LED state tracking
volatile uint16_t _brightness_history[BRIGHTNESS_NOISE_HISTORY];  // probably don't need to read this outside the lamp interrupt, but if so,
                                                                  // reaad some docs about how volatile works with arrays. it's got 16-bit
                                                                  // numbers anyway, so dissabling interrupts to guard access will be
                                                                  // necessary regardless (volatile keyword only protects for 8-bit)
volatile uint8_t _brightness_history_index = 0;

uint16_t get_pedal() {
  uint16_t reading = analogRead(PEDAL);
  if (reading > PEDAL_THRESHOLD) {
    return 0;
  }
  return map(reading, PEDAL_THRESHOLD, 0, 0, 1024);
}

// fixed motor power adjustment potentiometer
uint16_t get_pot() {
  uint16_t reading = analogRead(POT_SPEED);
  float norm = (float)reading / 1023;
  norm = sqrt(norm);  // curve a bit for more power sooner
  return norm * 1023;
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
  } else if (_motor_current_speed != motor_target_speed) {  // DO NOT REMOVE THIS CHECK (underflow risk if current == target == 0)
    // ramp up or down toward the new speed
    _motor_current_speed += (_motor_current_speed < motor_target_speed) ? 1 : -1;
  }

  // note: looking back at this code since Aitzi left, I think this map shouldn't be here, since we already map for motor_target_speed.
  // use caution if removing -- MOTOR_MAX_POWER will probably need to be changed.
  // => I *think* we're clamping the range to MOTOR_MAX_POWER twice, so the current 190 is effectively 141 (55% not 74%).
  // ...this makes me feel a little better because having the max at 75% did feel pretty high. phew.
  analogWrite(MOTOR_PWM, map(_motor_current_speed, 0, 255, 0, MOTOR_MAX_POWER));
}

volatile uint8_t x = 0;
void update_lamp() {
  uint16_t brightness = analogRead(LED_BRIGHTNESS);

  // include the latest sample in our history
  _brightness_history_index += 1;
  _brightness_history_index %= BRIGHTNESS_NOISE_HISTORY;  // wrap the index around so we cycle the range
  _brightness_history[_brightness_history_index] = brightness;  // just overwrite the oldest

  // the the total brightness and the two extremes
  unsigned long total_history_brightness = 0;
  uint16_t dimmest = 1023;
  uint16_t brightest = 0;
  for (uint8_t i = 0; i < BRIGHTNESS_NOISE_HISTORY; i++) {
    uint16_t b = _brightness_history[i];
    total_history_brightness += b;
    dimmest = min(dimmest, b);
    brightest = max(brightest, b);
  }
  // remove the extremes (most likely to be noise)
  total_history_brightness -= dimmest;
  total_history_brightness -= brightest;

  uint16_t denoised = total_history_brightness / (BRIGHTNESS_NOISE_HISTORY - 2);  // we took out two samples (extremes)
  uint8_t output = denoised >> 2;  // 10-bit to 8-bit

  // future development if flicker continues to be an issue:
  // 0. probably look at the pot output with a scope because there might be some specific noise characte we can try to
  //    deal with.
  // 1. keep more samples and/or remove more than 2 extremes. already-tricky code might get more complex...
  // 2. limit brightness adjustment step rate, like for motor control.

  if (_motor_current_speed == 0) {
    output = min(output, 204);  // cut to max 80% when stopped to stay cool.
                                // with shutter stopped open, we're already passing 2x more light
                                // and it kind of keeps the spirit of of the little arm that used to drop to limit light
  }

  analogWrite(LED_PWM, output);
}

// every 1ms
// https://learn.adafruit.com/multi-tasking-the-arduino-part-1/using-millis-for-timing
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

  for (uint8_t i = 0; i < BRIGHTNESS_NOISE_HISTORY; i++) {
    _brightness_history[i] = 0;
  }

  // set up timer0 to call our motor update speed function every 1ms
  // https://learn.adafruit.com/multi-tasking-the-arduino-part-1/using-millis-for-timing
  OCR0A = 0xAF;  // the timer0 counter threshold we activate at
  TIMSK0 |= _BV(OCIE0A);  // mask: interrupt and run our routine on that count please!

  // debug woo
  Serial.begin(9600);
}

void loop() {
  uint16_t reading = analogRead(LED_MONITOR);
  float current = (float)reading / 1023 * 5 / RESISTOR_OHMS_MEASURED;
  if (current > 0.5) {
    if (current > 1.04) {
      Serial.print("WARNING ");
    }
    Serial.print("current reading: ");
    Serial.print(reading);
    Serial.print("\t");
    Serial.print(current);
    Serial.print("A");
    Serial.println("\tnominal: 423 / 0.94A nominal. max: 473 / 1.05A.");
  }
  delay(100);
}

