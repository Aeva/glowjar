
#include <Adafruit_NeoPixel.h>
// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
// NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
// NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811)
// NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
// NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
#define SIZE 3
#define PIN 1
Adafruit_NeoPixel strip = Adafruit_NeoPixel(SIZE, PIN, NEO_GRB + NEO_KHZ800);

// Load appropriate i2c machinery + determine if we can debug over serial
#include <TinyWireM.h>
#define Wire TinyWireM
//#include <Wire.h>
//#define SERIAL_DEBUG
//define SERIAL_DEBUG_VERBOSE

// ----- accelerometer stuff 
// the SparkFun breakout board defaults to 1, set to 0 if SA0 jumper
// on the bottom of the board is set. 0x1D if SA0 is high, 0x1C if low
#define MMA8452_ADDRESS 0x1D
// define a few of the registers that we will
// be accessing on the MMA8452:
#define OUT_X_MSB 0x01
#define XYZ_DATA_CFG  0x0E
#define WHO_AM_I   0x0D
#define CTRL_REG1  0x2A
// Sets full-scale range to +/-2, 4, or 8g.
#define GSCALE 2


// other state tracking
bool i_am_error = true;
byte hue = 255;
unsigned long last_shake = 0;

// pertaining to color rotation
#define ERROR_SPEED 10 // delay of color rotation when accel is disconnected
#define FAST_SPEED 1   // time delay for fast color rotation
#define SLOW_SPEED 50  // time delay for slow color rotation
byte speed = SLOW_SPEED; // the current time delay for color rotation
byte transpired = 0; // number of times loop() has been called since last color

// pertaining to accelorometer / shake logic
#define ACCEL_THRESHOLD .5  // how much acceleration can trigger a shake
#define ATTENTION_STEP 5   // how much the color shift 'velocity' increments
#define ATTENTION_SPAN 255 // maximum 'velocity'
#define COMBO_MIN 1000  // minimum time between shakes to trigger
#define COMBO_MAX 2000 // maximum time between shakes to trigger
#define COMBO_TRIGGER 2
float last_accel[3] = {0, 0, 0};
float accel[3] = {0, 0, 0};
bool shake_dir[3] = {true, true, true};
byte active = 0; // is a count down
byte combo = 0;


void setup() {
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  Wire.begin();
  initMMA8452();
#ifdef SERIAL_DEBUG
  Serial.begin(57600);
  Serial.println("TEST");
#endif
  if (i_am_error) {
    speed = ERROR_SPEED;
  }
}


void loop() {
  transpired += 1;
  if (transpired >= speed) {
    transpired = 0;
    hue += 1;
    set_color(hue);
  }
  if (!i_am_error) {
    if (active > 0) {
      // if the bottle is not bored, become more bored
      active -= 1;
    }
    if (active == 0 && speed < SLOW_SPEED) {
      speed += (SLOW_SPEED - speed) / 2 + 1;
      if (combo > 0) {
        combo -=1;
      }
    }
    read_accel_data();
    motion_check();
  }
#ifdef SERIAL_DEBUG
  else {
    Serial.println("Accelorometer not connected.");
    delay(100);
  }
#endif
  delay(5);
}


void motion_check() {
  /*
    Read the current acceleration data from every axis.  For a given
    axis, if the direction of movement changed from the last time we
    checked (and if the overall difference between those two
    measurements is significant enough), and the overall time since
    the last significant acceleration activity is long enough (so that
    we catch shakes but not little taps), then call the 'shake event'
    function.
   */
  for (byte axis=0; axis<3; axis+=1) {
    if (abs(accel[axis]-last_accel[axis]) > ACCEL_THRESHOLD) {
      hue += 10;
#ifdef SERIAL_DEBUG
      Serial.println("SHAKE!");
#endif
      bool ref_dir = true ? accel[axis] > last_accel[axis] : false;
      if (ref_dir != shake_dir[axis]) {
        shake_dir[axis] = ref_dir;
        unsigned long now = millis();
        if (now < last_shake) {
          // in the odd event that the time counter overflowed, reset things
          last_shake = now;
        }
        unsigned int dt = now - last_shake;
        if (dt >= COMBO_MIN && dt <= COMBO_MAX) {
          shake_event();
        }
      }
    }
  }
#ifdef SERIAL_DEBUG
  Serial.print("Combo: ");
  Serial.print(combo);
  Serial.print("\n");
#endif 
}


void shake_event() {
  /*
    Increment the number of 'combos' recorded.  If the number is above
    COMBO_TRIGGER, then bump the hue a bit and raise the activity
    state slightly.
   */
  combo += 1;
  if (combo >= COMBO_TRIGGER) {
    hue += 3;
    active += ATTENTION_STEP;
  }
}


// 
int read_accel_data() {
  /*
    Reads data from the accelerometer, convert it into floating point
    values, then copies the previous acceleration data into
    'last_accel', and store the new data into 'accel'.
   */
  int accelCount[3];  // Stores the 12-bit signed value
  readAccelData(accelCount);  // Read the x/y/z adc values

#ifdef SERIAL_DEBUG_VERBOSE
  Serial.print("adjusted:");
#endif 

  // Now we'll calculate the accleration value into actual g's
  for (int i = 0 ; i < 3 ; i++) {
    // get actual g value, this depends on scale being set
    last_accel[i] = accel[i];
    accel[i] = (float) accelCount[i] / ((1<<12)/(2*GSCALE));
#ifdef SERIAL_DEBUG_VERBOSE
    if (i > 0) {
      Serial.print(", ");
    }
    Serial.print(accel[i]);
#endif
  }

#ifdef SERIAL_DEBUG_VERBOSE
  Serial.print("\n");
#endif 
}


// set all pixels to the given color.  params 0 to 255
void set_color(byte h) {
  /*
    Set all of the colors in the "NeoPixel" strip to have the given
    hue.
   */
  byte r;
  byte g;
  byte b;
  byte f = 0xff & (h*6);
  switch (h*6/256) {
     case 0: r = 255,   g = f,     b = 0; break;
     case 1: r = 255-f, g = 255,   b = 0; break;
     case 2: r = 0,     g = 255,   b = f; break;
     case 3: r = 0,     g = 255-f, b = 255; break;
     case 4: r = f,     g = 0,     b = 255; break;
     case 5: r = 255,   g = 0,     b = 255-f; break;
  }
  uint32_t c = strip.Color(r, g, b);
  for(uint16_t i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, c);
      strip.show();
  }
}


// -- accelerometer stuff -----------------------------------

/*
  The following code is a modified version of this here:
  https://github.com/sparkfun/MMA8452_Accelerometer/ 
  
  Of which the original code is in the public domain, and was put
  together by Nathan Seidle of SparkFun Electronics.  Thanks Nathan!
 */


void readAccelData(int *destination)
{
  byte rawData[6];  // x/y/z accel register data stored here

  readRegisters(OUT_X_MSB, 6, rawData);  // Read the six raw data registers into data array

  // Loop to calculate 12-bit ADC and g value for each axis
  for(int i = 0; i < 3 ; i++)
  {
    int gCount = (rawData[i*2] << 8) | rawData[(i*2)+1];  //Combine the two 8 bit registers into one 12-bit number
    gCount >>= 4; //The registers are left align, here we right align the 12-bit integer

    // If the number is negative, we have to make it so manually (no 12-bit data type)
    if (rawData[i*2] > 0x7F)
    {  
      gCount = ~gCount + 1;
      gCount *= -1;  // Transform into negative 2's complement #
    }

    destination[i] = gCount; //Record this gCount into the 3 int array
  }
}

// Initialize the MMA8452 registers 
// See the many application notes for more info on setting all of these registers:
// http://www.freescale.com/webapp/sps/site/prod_summary.jsp?code=MMA8452Q
void initMMA8452()
{
  byte c = readRegister(WHO_AM_I);  // Read WHO_AM_I register
  if (c == 0x2A) // WHO_AM_I should always be 0x2A
  {
    i_am_error = false;
#ifdef SERIAL_DEBUG
    Serial.println("MMA8452Q is online...");
#endif
  }
  else
  {
    i_am_error = true;
#ifdef SERIAL_DEBUG
    while(1) {
      Serial.print("Could not connect to MMA8452Q: 0x");
      Serial.println(c, HEX);
    }
#endif
  }

  MMA8452Standby();  // Must be in standby to change registers

  // Set up the full scale range to 2, 4, or 8g.
  byte fsr = GSCALE;
  if(fsr > 8) fsr = 8; //Easy error check
  fsr >>= 2; // Neat trick, see page 22. 00 = 2G, 01 = 4A, 10 = 8G
  writeRegister(XYZ_DATA_CFG, fsr);

  //The default data rate is 800Hz and we don't modify it in this example code

  MMA8452Active();  // Set to active to start reading
}

// Sets the MMA8452 to standby mode. It must be in standby to change most register settings
void MMA8452Standby()
{
  byte c = readRegister(CTRL_REG1);
  writeRegister(CTRL_REG1, c & ~(0x01)); //Clear the active bit to go into standby
}

// Sets the MMA8452 to active mode. Needs to be in this mode to output data
void MMA8452Active()
{
  byte c = readRegister(CTRL_REG1);
  writeRegister(CTRL_REG1, c | 0x01); //Set the active bit to begin detection
}

// Read bytesToRead sequentially, starting at addressToRead into the dest byte array
void readRegisters(byte addressToRead, int bytesToRead, byte * dest)
{
  Wire.beginTransmission(MMA8452_ADDRESS);
  Wire.write(addressToRead);
  Wire.endTransmission(false); //endTransmission but keep the connection active

  Wire.requestFrom(MMA8452_ADDRESS, bytesToRead); //Ask for bytes, once done, bus is released by default

  while(Wire.available() < bytesToRead); //Hang out until we get the # of bytes we expect

  for(int x = 0 ; x < bytesToRead ; x++)
    dest[x] = Wire.read();    
}

// Read a single byte from addressToRead and return it as a byte
byte readRegister(byte addressToRead)
{
  Wire.beginTransmission(MMA8452_ADDRESS);
  Wire.write(addressToRead);
  Wire.endTransmission(false); //endTransmission but keep the connection active

  Wire.requestFrom(MMA8452_ADDRESS, 1); //Ask for 1 byte, once done, bus is released by default

  while(!Wire.available()) ; //Wait for the data to come back
  return Wire.read(); //Return this one byte
}

// Writes a single byte (dataToWrite) into addressToWrite
void writeRegister(byte addressToWrite, byte dataToWrite)
{
  Wire.beginTransmission(MMA8452_ADDRESS);
  Wire.write(addressToWrite);
  Wire.write(dataToWrite);
  Wire.endTransmission(); //Stop transmitting
}
