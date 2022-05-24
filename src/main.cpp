/**
 * LocalMoodLamp/main.cpp
 *
 * Main file for LocalMoodLamp Code.
 *  Basic lamp that accepts downloadable animations from a desktop app over serial
 *
 * Author: Shaqeeb Momen
 * Date: December 27, 2021
 */

#include <Arduino.h>

#if defined(MICRO) || defined(NANO)
#include <EEPROM.h>
#else
#include <extEEPROM.hpp>
#endif
#include <Adafruit_NeoPixel.h>
#include <AnimationDriver.h>
#include <DefaultAnimations.h>

// #define WRITE_EEPROM // Flag to write defaults to EEPROM (effectively reset EEPROM)

// DEBUG FLAGS
// #define DEBUG
// #define DEBUG_LED
// #define DEBUG_EEPROM
// #define DEBUG_EEPROM_SERIAL

// Routine enable flags
#define EN_ANIMATION

// Hardware defs
#ifdef MICRO
#define POT_PIN A0
#define PIXEL_PIN 10
#define BTN_UP_PIN 5
#define BTN_DWN_PIN 7
#endif

#ifdef NANO
#define POT_PIN A7
#define PIXEL_PIN 2
#define BTN_UP_PIN 3
#define BTN_DWN_PIN 4
#endif

#ifdef XIAO
#define POT_PIN A3    // D3
#define PIXEL_PIN 10  // D10
#define BTN_UP_PIN 9  // D9
#define BTN_DWN_PIN 8 // D8
#endif

// Numerical Constants
// #define T_LOOP 0     // Execution loop time
// Serial Constants
#define SERIAL_PACKET 142
#define FRAME_SIZE 7
#define META_SIZE 2
#define POT_THRES 20
#define BTN_TIME 200

#ifdef XIAO
extEEPROM EEPROM(0b1010000, 8192, 32, 256);
#endif

// unsigned long loopTimer;
Adafruit_NeoPixel strip(NUM_LEDS, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

AnimationDriver::AnimationDriver animator(millis);
// Default animations

// Note animation size is 168 bytes & eeprom is 1kB
const AnimationDriver::animation Solid_White PROGMEM = SOLID_COLOR(255, 255, 255);
const AnimationDriver::animation Solid_Red PROGMEM = SOLID_COLOR(255, 0, 0);
const AnimationDriver::animation Breathe_White PROGMEM = BREATHE_COLOR(255, 255, 255, 3000UL);
const AnimationDriver::animation Solid_Green PROGMEM = SOLID_COLOR(0, 255, 0);
const AnimationDriver::animation Rainbow PROGMEM = RAINBOW(4000UL);
const AnimationDriver::animation Solid_Blue PROGMEM = SOLID_COLOR(0, 0, 255);

const AnimationDriver::animation defaults[] PROGMEM = {
    Solid_White,
    Solid_Red,
    Breathe_White,
    Solid_Green,
    Rainbow,
    Solid_Blue};

AnimationDriver::animation currentAnim;

// Current and previous values for LED brightness (used to only change brightness when needed)
uint16_t LEDscale;
uint16_t prevLEDScale;

uint32_t btnTimer = 0;

// Function used for resetting programmatically
void (*resetFunc)(void) = 0;

// Load specific animation from eeprom into currentAnim
void EEPROM_Load(uint8_t index)
{
#ifdef DEBUG_EERPROM
  Serial.print("Getting Index: ");
  Serial.print(index);
  Serial.print(" Addr: ");
  Serial.println((int)(index * sizeof(currentAnim)));
#endif
#ifdef XIAO
  EEPROM.get((uint32_t)(index * sizeof(AnimationDriver::animation)), &currentAnim, sizeof(AnimationDriver::animation));
#else
  EEPROM.get((int)(index * sizeof(AnimationDriver::animation)), &currentAnim);
#endif
#ifdef DEBUG_EEPROM
  Serial.println(currentAnim.frameCount);
  Serial.println("Animation Loaded");
  Serial.flush();
#endif
}

// Write defaults to eeprom
void EEPROM_WriteDefaults()
{
#ifdef DEBUG_EEPROM
  Serial.println("RESETTING ANIMATIONS");
  Serial.flush();
#endif
  // Write to defaults to eeprom
  for (size_t i = 0; i < sizeof(defaults) / sizeof(Rainbow); i++)
  {
    AnimationDriver::animation animBuff;
    memcpy_P(&animBuff, &defaults[i], sizeof(animBuff));
#ifdef DEBUG_EEPROM
    Serial.print("Writing To: ");
    Serial.println((int)(i * sizeof(animBuff)));
#endif
#ifdef XIAO
    EEPROM.put((uint32_t)(i * sizeof(AnimationDriver::animation)), &animBuff, sizeof(AnimationDriver::animation));
#else
    EEPROM.put((int)(i * sizeof(AnimationDriver::animation)), &animBuff);
#endif
  }
  Serial.println("DEFAULTS WRITTEN TO EEPROM");
  Serial.flush();
}

// Serial Methods

// Parse out an animation object from a serial buffer and store in EEPROM
void saveAnimationFromSerial(byte *buff)
{
  AnimationDriver::animation _a;
  _a.frameCount = buff[1];
  // For each frame
  for (byte i = 0; i < buff[1]; i++)
  {
    byte baseIndex = i * FRAME_SIZE + 2;
    _a.frames[i].color[0] = buff[baseIndex];                                                                                                                            // Red
    _a.frames[i].color[1] = buff[baseIndex + 1];                                                                                                                        // Green
    _a.frames[i].color[2] = buff[baseIndex + 2];                                                                                                                        // Blue
    _a.frames[i].time = (uint32_t)buff[baseIndex + 3] << 24 | (uint32_t)buff[baseIndex + 4] << 16 | (uint32_t)buff[baseIndex + 5] << 8 | (uint32_t)buff[baseIndex + 6]; // time
    if (i == buff[1] - 1)
    {
      _a.time = _a.frames[i].time;
    }
  }
#ifdef XIAO
  EEPROM.put((buff[0] * sizeof(AnimationDriver::animation)), &_a, sizeof(AnimationDriver::animation));
#else
  EEPROM.put(buff[0] * sizeof(AnimationDriver::animation), _a);
#endif
}

// Waits for acknowledge byte (0xff) from pc
bool waitForAck(uint32_t timeout)
{
  // Start timer
  uint32_t timer = millis();
  //  Serial.println(F("Waiting for ACK"));
  while (true)
  {
    // Check for Serial data or
    if (Serial.available() > 0)
    {
      uint8_t ack = (byte)Serial.read();
      if (ack == 0xff)
      {
        // Success
        return true;
      }
      else
      {
        // fail
        Serial.println(F("ACK Fail"));
        Serial.flush();
        return false;
      }
    }
    // Check if timer has ran out
    else if (millis() - timer > timeout)
    {
      Serial.println(F("ACK Fail"));
      Serial.flush();
      return false;
    }
  }
}

// Handle an an upload request
void handleUploadRequest()
{
  byte localBuff[SERIAL_PACKET];
  byte buffCount = META_SIZE;
  // Wait for first 2 bytes to come in
  while (Serial.available() < META_SIZE)
    ;
#ifdef XIAO
  Serial.readBytes((char *)localBuff, META_SIZE);
#else
  Serial.readBytes(localBuff, META_SIZE);
#endif

  // While the pc is sending data, store it in the buffer
  // Loop untill all bytes expected are read
  while (buffCount < (localBuff[1] * FRAME_SIZE + META_SIZE))
  {
    // Let a byte come in
    while (Serial.available() < 1)
      ;
    if (buffCount <= SERIAL_PACKET)
    {
      // Append to buffer
      byte data = (byte)Serial.read();
      localBuff[buffCount] = data;
      buffCount++;
    }
    // Writing outside buffer space, send an error back
    else
    {
      Serial.println();
      return;
    }
  }
  // Once all the data has been received, write it back to the pc
  Serial.write(localBuff, buffCount);
  // Read a check character (0x00 -> fail, 0xff -> success)
  if (waitForAck(1000))
  {
    // Success
    // Store data in memory if check character came back okay
    saveAnimationFromSerial(localBuff);
    // Send one more string back to indicate write finished
    Serial.println(F("Done"));
    Serial.flush();
  }
  else
  {
    resetFunc();
  }
}

// Handle request for download
void handleDownloadRequest()
{
  for (uint8_t i = 0; i < 6; i++)
  {
    AnimationDriver::animation _a;
#ifdef XIAO
    EEPROM.get((i * sizeof(AnimationDriver::animation)), &_a, sizeof(AnimationDriver::animation));
#else
    EEPROM.get(i * sizeof(AnimationDriver::animation), _a);
#endif
    // Write the frame count
    if (!waitForAck(1000))
    {
      resetFunc();
    }
    Serial.write(i);
    Serial.write(_a.frameCount);
    Serial.flush();
    // Wait for an acknowledge or timeout
    if (!waitForAck(1000))
    {
      resetFunc();
    }
    // Send rest of animation frames
    // Parse animation object into uint8_t array
    uint8_t frameBuff[_a.frameCount * FRAME_SIZE];
    for (uint8_t frame = 0; frame < _a.frameCount; frame++)
    {
      uint8_t baseIndex = frame * FRAME_SIZE;
      // Red
      frameBuff[baseIndex] = _a.frames[frame].color[0];
      // Green
      frameBuff[baseIndex + 1] = _a.frames[frame].color[1];
      // Blue
      frameBuff[baseIndex + 2] = _a.frames[frame].color[2];
      // Timestamp (four bytes)
      frameBuff[baseIndex + 3] = (uint8_t)(_a.frames[frame].time >> 24);
      frameBuff[baseIndex + 4] = (uint8_t)(_a.frames[frame].time >> 16);
      frameBuff[baseIndex + 5] = (uint8_t)(_a.frames[frame].time >> 8);
      frameBuff[baseIndex + 6] = (uint8_t)(_a.frames[frame].time);
    }
    // Send buffer
    Serial.write(frameBuff, _a.frameCount * FRAME_SIZE);
    Serial.flush();
    // Wait for acknowledge or timeout
    if (!waitForAck(1000))
    {
      resetFunc();
    }
  }
}

// Handle overall Serial Communication
void handleSerial()
{
  // Read until code ends
  String code = Serial.readStringUntil('-');
  // Echo Back a ready string and acknowledge the code received
  Serial.print(F("ready_"));
  Serial.println(code);
  Serial.flush();
  // Do something useful with the intent code
  switch (code[0])
  {
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
    handleUploadRequest();
    break;
  case 'd':
    handleDownloadRequest();
    break;
  default:
    Serial.println();
    break;
  }
}

// DEBUG Functions
#ifdef DEBUG_EEPROM_SERIAL
void EEPROM_Dump_Anim(uint8_t index)
{
  AnimationDriver::animation _anim;
#ifdef XIAO
  EEPROM.get((uint32_t)(index * sizeof(AnimationDriver::animation)), &_anim, sizeof(AnimationDriver::animation));
#else
  EEPROM.get(index * sizeof(AnimationDriver::animation), _anim);
#endif
  Serial.print(F("Animation at Index "));
  Serial.println(index);
  Serial.print(F("Frame Count: "));
  Serial.println(_anim.frameCount);
  Serial.print(F("Total Time: "));
  Serial.println(_anim.time);
  Serial.println(F("Frames: "));
  for (uint8_t i = 0; i < _anim.frameCount; i++)
  {
    Serial.print(F("Frame: "));
    Serial.println(i);
    Serial.print(F("R: "));
    Serial.println(_anim.frames[i].color[0]);
    Serial.print(F("G: "));
    Serial.println(_anim.frames[i].color[1]);
    Serial.print(F("B: "));
    Serial.println(_anim.frames[i].color[2]);
    Serial.print(F("Time: "));
    Serial.println(_anim.frames[i].time);
  }
}
#endif
uint16_t buttonFSM()
{
  enum state
  {
    IDLE,
    TRIGGERED,
    RELEASE
  };
  static state currentState = IDLE;
  static bool changeUP = false;
  static uint32_t timer = 0;
  static uint16_t outputMode = 0;

  switch (currentState)
  {
  case IDLE:
    if (!digitalRead(BTN_UP_PIN))
    {
      currentState = TRIGGERED;
      changeUP = true;
      timer = millis();
      break;
    }
    if (!digitalRead(BTN_DWN_PIN))
    {
      currentState = TRIGGERED;
      changeUP = false;
      timer = millis();
      break;
    }
    break;

  case TRIGGERED:
    if (millis() - timer > BTN_TIME) // timer passes
    {
      // If output is still appropriate, make changes
      if (changeUP && !digitalRead(BTN_UP_PIN))
      {
        outputMode = (outputMode + 1) % (sizeof(defaults) / sizeof(AnimationDriver::animation));
      }
      else if (!changeUP && !digitalRead(BTN_DWN_PIN))
      {
        if (outputMode < 1)
        {
          outputMode = sizeof(defaults) / sizeof(AnimationDriver::animation) - 1;
        }
        else
        {
          outputMode--;
        }
      }
      currentState = RELEASE;
    }
    break;

  case RELEASE:
    if (digitalRead(BTN_UP_PIN) && digitalRead(BTN_DWN_PIN))
      currentState = IDLE;
    break;
  }
  return outputMode;
}

void setup()
{
  // Start Serial Communication
  Serial.begin(115200);
  Serial.println(F("ready"));
  // LED Setup
  strip.begin();
  strip.show();
  // Initial Brightness
  LEDscale = analogRead(POT_PIN);
  strip.setBrightness(LEDscale / 4);

#ifdef XIAO
  EEPROM.init();
#endif

#ifdef WRITE_EEPROM
  EEPROM_WriteDefaults();
#endif
  // Animation Controller
  EEPROM_Load(0);
  animator.updateAnimation(currentAnim);
  // Initialize timers
  btnTimer = millis();
  // Button Setup
  pinMode(BTN_DWN_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);

#ifdef DEBUG_EEPROM_SERIAL
  for (uint8_t i = 0; i < 6; i++)
  {
    Serial.println(F("------------------------"));
    EEPROM_Dump_Anim(i);
    delay(1000);
  }
  Serial.println(F("------------------------"));
#endif
}

void loop()
{
  static uint16_t lastMode = 0;
  uint16_t currentMode = 0;
  if (Serial.available() > 0)
  {
    // Handle Serial Request
    handleSerial();
    EEPROM_Load(currentMode);
    animator.updateAnimation(currentAnim);
    // resetFunc();
  }
  else
  {
    /************ BRIGHTNESS KNOB ***********/
    LEDscale = analogRead(POT_PIN) / 4;
    if (abs(LEDscale - prevLEDScale) > POT_THRES)
    {
      strip.setBrightness(LEDscale);
      prevLEDScale = LEDscale;
    }
    currentMode = buttonFSM();
    // Only trigger updates on changes
    if (currentMode != lastMode)
    {
      EEPROM_Load(currentMode);
      animator.updateAnimation(currentAnim);
      lastMode = currentMode;
    }

    /************ DRIVING LEDS ***********/
    // Pass current animation, time stamp, brightness, into animation driving function
#ifdef EN_ANIMATION
    animator.run([](uint8_t r, uint8_t g, uint8_t b)
                 {
                   strip.fill(strip.Color(r, g, b));
                   strip.show(); });
#endif
  }

  /************ DUBUGGING HELP ***********/

#ifdef DEBUG
  Serial.println();
  Serial.flush();
#endif
}
