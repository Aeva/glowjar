#include <math.h>
#include <Wire.h>
#include <Adafruit_LSM303.h>
#include <Adafruit_NeoPixel.h>

#define PIN 6
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, 6, NEO_GRB + NEO_KHZ800);

// Accelorometer thing
Adafruit_LSM303 lsm;

// some fudgable constants
#define ATTENTION_SPAN 10
#define CLEAR_TIME 300
#define COMBO_TRIGGER 2
#define ACCEL_THRESHOLD 10
#define SENSITIVITY 100.0


// data for state tracking
int shake_dir = 0;
int last_accel = 0;
unsigned long last_time = 0;
int combo = 0;

unsigned long bored = 0;  // fake timeout for when to do color rotation stuff
float base_hue = 0; // 0.0 to 255.0




// setup event
void setup() 
{
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  Serial.begin(9600); 
  // Try to initialise and warn if we couldn't detect the chip
  if (!lsm.begin())
  {
    Serial.println("Oops ... unable to initialize the LSM303. Check your wiring!");
    while (1);
  }
  else {
    Serial.println("All clear, boss!");
  }
}


// loop event
void loop() 
{
  // motion detection
  lsm.read();
  float x = lsm.accelData.x/SENSITIVITY;
  float y = lsm.accelData.y/SENSITIVITY;
  float z = lsm.accelData.z/SENSITIVITY;
  motion_check(y);
  
  // engage pixels
  unsigned long now = millis();
  if (now >= bored) {
    // bottle is bored and will start rotating color
    base_hue += .2;
  }
  
  
  base_hue = fmod(base_hue, 255);
  set_color(base_hue/255, 1, 1);
  // pause for effect
  delay(10);
}


// set all pixels to the given color.  params 0.0 to 1.0
void set_color(float hue, float sat, float value) {
  uint32_t c = HSV(hue, sat, value);
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
  }
}

// cool visual triggered by shakes
void shake_event() {
  // bottle is entertained
  bored = millis() + 1000*ATTENTION_SPAN;
  base_hue += 15;
}


// processes acceleration data and determines if anything interesting happened
void motion_check(float axis) {
  unsigned long stamp = millis();
  if (stamp - last_time > CLEAR_TIME) {
    last_time = stamp;
    combo = 0;
  }
  if (abs(axis-last_accel) > ACCEL_THRESHOLD) {
    int ref_dir = 1 ? axis > last_accel : -1;
    if (ref_dir != shake_dir) {
      combo += 1;
      last_accel = axis;
      last_time = stamp;
      shake_dir = ref_dir;
      if (combo % COMBO_TRIGGER == 0) {
        shake_event();
      }
    }
  }
}


uint32_t HSV(float hue, float sat, float value) {
  /* Params all between 0 and 1, wraps strip.Color.  Function adapted from:
  http://stackoverflow.com/questions/17242144/javascript-convert-hsb-hsv-color-to-rgb-accurately
  */
  float h = hue;
  float s = sat;
  float v = value;
  int i = floor(h*6);
  float f = h * 6 - i;
  float p = v * (1 - s);
  float q = v * (1 - f * s);
  float t = v * (1 - (1 - f) * s);
  float r;
  float g;
  float b;
  switch (i % 6) {
     case 0: r = v, g = t, b = p; break;
     case 1: r = q, g = v, b = p; break;
     case 2: r = p, g = v, b = t; break;
     case 3: r = p, g = q, b = v; break;
     case 4: r = t, g = p, b = v; break;
     case 5: r = v, g = p, b = q; break;
  }
  return strip.Color((int)(r*255), (int)(g*255), (int)(b*255));
}
