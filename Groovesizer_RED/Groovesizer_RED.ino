/************************************************************************
 ***   GROOVESIZER [RED] v.037 - Granular Synth and 16-Step Sequencer
 ***   http://groovesizer.com
 ***   Synth engine based on Auduino, the Lo-Fi granular synthesiser
 ***   by Peter Knight, Tinker.it http://tinker.it
 ***   Auduino help: http://code.google.com/p/tinkerit/wiki/Auduino
 ************************************************************************
 * Copyright (C) 2014 MoShang (Jean Marais) moshang@groovesizer.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/
 * 
 ************************************************************************/

// Version 38

#include <EEPROM.h> //for reading and writing patterns to EEPROM
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>       // included so we can use PROGMEM
#include <MIDI.h>               // MIDI library


// **** AUDUINO CODE *********
// incorporating DuaneB's volatile fix
uint16_t syncPhaseAcc;
volatile uint16_t syncPhaseInc;
uint16_t grainPhaseAcc;
volatile uint16_t grainPhaseInc;
uint16_t grainAmp;
volatile uint8_t grainDecay;
uint16_t grain2PhaseAcc;
volatile uint16_t grain2PhaseInc;
uint16_t grain2Amp;
volatile uint8_t grain2Decay;

#define PWM_PIN       3
#define PWM_VALUE     OCR2B
#define LED_PORT      PORTB
#define LED_BIT       5
#define PWM_INTERRUPT TIMER2_OVF_vect

// Smooth logarithmic mapping
const unsigned int antilogTable[] PROGMEM = {
  64830,64132,63441,62757,62081,61413,60751,60097,59449,58809,58176,57549,56929,56316,55709,55109,
  54515,53928,53347,52773,52204,51642,51085,50535,49991,49452,48920,48393,47871,47356,46846,46341,
  45842,45348,44859,44376,43898,43425,42958,42495,42037,41584,41136,40693,40255,39821,39392,38968,
  38548,38133,37722,37316,36914,36516,36123,35734,35349,34968,34591,34219,33850,33486,33125,32768
};
const unsigned int mapPhaseInc(uint16_t input) {
  return (pgm_read_word_near(antilogTable + (input & 0x3f))) >> (input >> 6);
}
// the arrays below have been altered to go into PROGMEM (flash/program memory) instead of SRAM
// Stepped chromatic mapping
const unsigned int midiTable[] PROGMEM = {
  0,18,19,20,22,23,24,26,27,29,31,32,34,36,38,41,43,46,48,51,54,58,61,65,69,73,
  77,82,86,92,97,103,109,115,122,129,137,145,154,163,173,183,194,206,218,231,
  244,259,274,291,308,326,346,366,388,411,435,461,489,518,549,581,616,652,691,
  732,776,822,871,923,978,1036,1097,1163,1232,1305,1383,1465,1552,1644,1742,
  1845,1955,2071,2195,2325,2463,2610,2765,2930,3104,3288,3484,3691,3910,4143,
  4389,4650,4927,5220,5530,5859,6207,6577,6968,7382,7821,8286,8779,9301,9854,
  10440,11060,11718,12415,13153,13935,14764,15642,16572,17557,18601,19708,20879,
  22121,23436,24830,0
}; // replacing the first and last with 0 since we're using them for mute and tie

void audioOn() {
  // Set up PWM to 31.25kHz, phase accurate
  TCCR2A = _BV(COM2B1) | _BV(WGM20);
  TCCR2B = _BV(CS20);
  TIMSK2 = _BV(TOIE2);
}

// ********** LEDS ***********
// setup code for controlling the LEDs via 74HC595 serial to parallel shift registers
// based on ShiftOut http://arduino.cc/en/Tutorial/ShiftOut
const byte LEDlatchPin = 2;
const byte LEDclockPin = 5;
const byte LEDdataPin = 4;

byte leds[2] = {
  0,0};

// ******** BUTTONS **********
// taken from http://www.adafruit.com/blog/2009/10/20/example-code-for-multi-button-checker-with-debouncing/
#define DEBOUNCE 5  // button debouncer, how many ms to debounce, 5+ ms is usually plenty

// here is where we define the buttons that we'll use. button "1" is the first, button "6" is the 6th, etc
byte buttons[] = {
  6, 7, 8, 9, 10}; // the analog 0-5 pins are also known as 14-19
// This handy macro lets us determine how big the array up above is, by checking the size
#define NUMBUTTONS 5 // we have 5 buttons
// we will track if a button is just pressed, just released, or 'currently pressed' 
byte pressed[NUMBUTTONS], justpressed[NUMBUTTONS], justreleased[NUMBUTTONS];

// ********** POTS ***********
int pot[5];
int bufferTone[4]; // the tone controls - corresponds to pot[0] to pot[3] / we need to buffer it so value is retained when we use the pots for other functions
int potLock[5]; // we need to lock the pots when jumping between different modes, so there are no jumps in the values

// ******** SEQUENCER ********
// sequencer variables
boolean noteOn = true;
byte seqTrueStep; // we need to keep track of the true sequence position (independent of sequence length)
volatile byte seqCurrentStep; // the current step - can be changed in other various functions and value updates immediately
char seqNextStep = 1; // change this to -1 to reverse pattern playback
byte seqLength = 16; // the length of the sequence from 1 step to 32 
byte seqNotes[16]; // the notes of the sequence - stores the current sequence's lookup values (0 - 127); a value of 0 means the step is not active, 127 means the step is tied
byte seqAutomate[96]; // stores parameter automation
boolean automate = false;
boolean updateAuto = false;
byte autoCounter = 0; // keeps count of the parameter automation
byte seqTiedNote; // the current note lookup value of steps being tied
byte seqStepSlide[2]; // is the step marked as a slide
byte seqAccent[2]; // is the step accented
volatile boolean seqMidiStep; // advance to the next step with midi clock
byte bpm = 90;
unsigned long currentTime;
unsigned int clockPulse = 0; // keep count of the  pulses
byte clockPulseDur = (60000/bpm)/24; // the duration of one midi clock  pulse in milliseconds (6  pulses in a 16th, 12  pulses in an 8th, 24  pulses in a quarter)
unsigned long pulseStartTime = 0;
unsigned int sixteenthDur = (60000/bpm)/4; // the duration of a sixteenth note in milliseconds (6  pulses in a 16th, 12  pulses in an 8th, 24  pulses in a quarter)
unsigned int swing16thDur = 0; // a swung 16th's duration will differ from the straight one above
byte swing = 0; // the amount of swing from 0 - 255 
unsigned long sixteenthStartTime = 0;
boolean seqRunning = true; // is the sequencer running?
boolean stepGo = false; // should we fire the next step?
unsigned long offTime = 0; // when the note should be turned off in milliseconds
unsigned int onDur = 50; // how long the note should stay on
int decay = 100; // like filter decay/release
byte playMode = 0; // 0 = forward, 1 = reverse, 2 = pendulum, 3 = random 

byte followMode = 0; // the follow action mode set for the bank
byte followModeChange = 255; // set to 255 for no change
byte followAction = 0; // the behaviour of the pattern when it reaches the last step (0 = loop, 1 = next pattern, 2 = return to head)
// the following can probably go
boolean saved = false; // we need this variable for follow actions - only start doing the follow action once the pattern has been saved
// loading a pattern sets this variable to true - setting follow action to 1 or 2 sets this to false 
byte head = 0; // the first in a series of chained patterns - any pattern triggered by hand is marked as the head (whether a followAction is set or not)

byte bpmTaps[10] = { // required for tap tempo
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

byte lastEdited = 255;   // the last sequencer note that was edited - used to add slides and accents
// we use 255 (arbitrary choice larger than 15) for "none"

// ******* LOAD & SAVE *******    
byte nowPlaying = 255; // the pattern that's currently playing. 255 is a value we'll use to check for if no pattern is currently playing
byte cued = 255; // the pattern that will play after this one. 255 is a value we'll use to check for if no pattern is currently cued
byte confirm = 255; // the memory location that will be overwritten - needs to be confirmed. 255 means nothing to confirm
byte bank = 0; // which of the 16 banks is selected
byte playingBank = 12; // which bank is currently playing

// ******* PREFERENCES *******
// indicated by RED LEDs in Preferences Mode
// we define the variables and initialize them with default values here, but values are actually read from EEPROM in setup()
// Button 1
boolean midiNoteOut = true; // send out MIDI notes to the next device in the chain (you may want to switch this off, eg. if you have more than one groovesizer in a chain and only want to use notes from one)
// Pot 1
byte noteChannel = 1; // the MIDI channel that note on/off messages are sent on
// Pot 2
byte triggerChannel = 10; // the MIDI channel that pattern change messages are sent and received on
// Pot 3
byte midiAutomationCC = 44; // Volca Keys' filter cutoff
byte automationCC[16] = {
  5, 11, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53}; // we define 16 possible values for midiAutomationCC
// Button 3
boolean midiAutomationOut = true;
// Pot 4
byte accentLevel = 200; // the amount by which to mark an accent (range 0 to 255)

// indicated by YELLOW LEDs in Preferences Mode
// Button 1
boolean midiSyncOut = true; // send out MIDI sync pulses
// Button 2
boolean sendStartStop = true; // send start and stop messages
// Button 3
boolean thruOn = true; // is MIDI thru on or off? If it's on, all MIDI messages received at input are echoed to the output
// Button 4
byte playEdited = true; // play the note being edited on every active step, or only hear the note being edited in its actual place in the pattern

// ********* SCALES **********
char scaleTranspose = 0;
byte scale = 0; // 0 = chromatic, 1 = major, 2 = minor (melodic), 3 = minor (harmonic), 4 = pentatonic (major)
// 5 = pentatonic (minor), 6 = blues, 7 = major triad, 8 = minor triad, 9 = major #7, 10 = minor 7, 11 = user 
byte *scalePointer; // a pointer variable to store the beginning of a scale array
byte scaleSize; // the size of the scale in question

byte major[7] = {
  1, 3, 5, 6, 8, 10, 12};
byte minorMel[7] = {
  1, 3, 4, 6, 8, 10, 12};
byte minorHarm[7] = {
  1, 3, 4, 6, 8, 9, 12};
byte pentaMaj[5] = {
  1, 3, 5, 8, 10};
byte pentaMin[5] = {
  1, 4, 6, 8, 11};  
byte bluesHex[6] = {
  1, 4, 6, 7, 8, 11};
byte bluesHept[7] = {
  1, 3, 4, 6, 7, 10, 11};
byte triadMaj[3] = {
  1, 5, 8};
byte triadMin[3] = {
  1, 4, 8};
byte triad7Maj[4] = {
  1, 5, 8, 12};
byte triad7Min[4] = {
  1, 4, 8, 11};

byte userScale[13] = { // a maximum of 12 notes and the 13th is used to determine the length
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// *********** UI ************
byte mode = 0;   // 0 - Edit Mode, 1 = Pattern Mode, 2 = Live Mode, 3 = Preferences Mode
unsigned long longPress = 0;
boolean savealert = false; // a special LED sequence when saving
boolean lastRed = false; // for Preferences Mode keep track of the last page we were on Red or Yellow;
boolean lastYellow = false; // for Preferences Mode keep track of the last page we were on Red or Yellow;
unsigned long prefsTimer = 0; // needed to keep track of saving longpress in Preferences Mode
boolean released[3] = { // needed so preferences aren't changed when buttons are released on entering Preferences Mode 
  0, 0, 0};
unsigned long valueDisplayTimer = 0; // needed so we can display values being edited in Preferences Mode
unsigned long doubleTap = 0; // used to stop playback

// ********** MIDI ***********
byte pulse = 0; // to count incoming MIDI clocks
char transposeFactor = 0; // needed for MIDI transpose - we'll reset it to zero when we load or save a patch, or when we clear a pattern
unsigned long lastClock = 1; // when was the last time we received a clock pulse (can't be zero at the start)
boolean midiClock = false; // true if we are receiving midi clock
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midiA);
// *************************************
//           THE SETUP
// *************************************

void setup() {

  // ****** AUDUINO CODE *******
  pinMode(PWM_PIN,OUTPUT);
  audioOn();

  // ********** LEDS ***********
  //define pin modes
  pinMode(LEDlatchPin, OUTPUT);
  pinMode(LEDclockPin, OUTPUT); 
  pinMode(LEDdataPin, OUTPUT);

  // ******** BUTTONS **********
  for (byte i=0; i< NUMBUTTONS; i++) 
  {
    pinMode(buttons[i], INPUT);
    digitalWrite(buttons[i], HIGH);
  }

  // ********** POTS ***********
  getPots();
  for (byte i=0; i < 5; i++)
  {
    potLock[i] = pot[i];
  }
  for (byte i=0; i < 4; i++)
  {
    bufferTone[i] = 800; 
  }
  // ********** MIDI ***********

  // Initiate MIDI communications, listen to all channels
  midiA.begin(MIDI_CHANNEL_OMNI);    

  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  midiA.setHandleNoteOn(HandleNoteOn);  // only put the name of the function here - functions defined in HandleMidi

  //midiA.setHandleNoteOff(HandleNoteOff);

  //midiA.setHandleControlChange (HandleCC);

  midiA.setHandleClock(HandleClock);

  midiA.setHandleStart(HandleStart);

  midiA.setHandleStop(HandleStop);

  // ******* SEQUENCER *********
  clearAll();

  // ********* EEPROM **********
  eepromInit(); // write all 0's to the EEPROM if the last byte is not 0 (ie. on first run)

  // ****** PREFERENCES ********
  loadPreferences();
}

// *************************************
//           THE AUDIO ENGINE
// *************************************

SIGNAL(PWM_INTERRUPT)
{

  uint8_t value;
  uint16_t output;

  syncPhaseAcc += syncPhaseInc;
  if (syncPhaseAcc < syncPhaseInc) {
    // Time to start the next grain
    grainPhaseAcc = 0;
    grainAmp = 0x7fff;
    grain2PhaseAcc = 0;
    grain2Amp = 0x7fff;
    LED_PORT ^= 1 << LED_BIT; // Faster than using digitalWrite
  }

  // Increment the phase of the grain oscillators
  grainPhaseAcc += grainPhaseInc;
  grain2PhaseAcc += grain2PhaseInc;

  // Convert phase into a triangle wave
  value = (grainPhaseAcc >> 7) & 0xff;
  if (grainPhaseAcc & 0x8000) value = ~value;
  // Multiply by current grain amplitude to get sample
  output = value * (grainAmp >> 8);

  // Repeat for second grain
  value = (grain2PhaseAcc >> 7) & 0xff;
  if (grain2PhaseAcc & 0x8000) value = ~value;
  output += value * (grain2Amp >> 8);

  // Make the grain amplitudes decay by a factor every sample (exponential decay)
  grainAmp -= (grainAmp >> 8) * grainDecay;
  grain2Amp -= (grain2Amp >> 8) * grain2Decay;

  // Scale output to the available range, clipping if necessary
  output >>= 9;
  if (output > 255) output = 255;

  // Output to PWM (this is faster than using analogWrite)  
  PWM_VALUE = output;
}

// *************************************
//           THE LOOP
// *************************************

void loop() {

  static boolean on = true; // a little helper because my logic sucks
  static boolean slide = false; // and another one
  static int pitchTarget = 0; // the pitch we're aiming for when we slide

  // *********** MIDI **********           
  static byte midiOn; // the note we're sending out via MIDI

  check_switches();
  getPots();
  midiA.read();

  midiClock = (millis() - lastClock < 150) ? true : false;

  ledsOff();

  if (!noteOn) // like filter decay release
  {
    grainPhaseInc = (grainPhaseInc > decay) ? grainPhaseInc - decay : 0;
    grain2PhaseInc = (grain2PhaseInc > decay) ? grain2PhaseInc - decay : 0;
  }
  else // the noteOn is true
  {
    if (automate) // automation is on
    {
      if (pressed[4] && potLock[0] == 9999) // if we don't add this "if" we get distortion as current pot[0] value and stored value are both played back
        grainPhaseInc  = mapPhaseInc(512 - (pot[0] >> 1)) / 2;
      else
        grainPhaseInc  = mapPhaseInc(512 - ((int)seqAutomate[autoCounter] << 1)) / 2;
    }
    else // automation is off
    {
      grainPhaseInc  = mapPhaseInc(512 - (bufferTone[0] >> 1)) / 2;
    }
    grainDecay     = (1023 - bufferTone[1]) / 8;

    if (mode != 2) // grain2PhaseInc is dictated by note velocity in Live Mode
    {
      if (checkAccent(seqCurrentStep)) //check for accents
      {
        grain2PhaseInc = map((int)accentLevel, 0, 255, (mapPhaseInc(1023 - bufferTone[2]) / 2), 32768);
      }
      else
        grain2PhaseInc = mapPhaseInc(1023 - bufferTone[2]) / 2;

    }
    grain2Decay    = (1023 - bufferTone[3]) / 4;
  }

  switch (mode)
  { 
  case 0:
    // *****************************
    // ***** MODE 0 - EDIT MODE ****
    // *****************************
    //     BUTTONS - NO SHIFT
    followAction = 0; // we don't want to do any follow actions in Edit Mode

    if (!pressed[4]) // shift is not held
    {
      if (justreleased[4]) // shift was just let go, so lock the pots
      {
        lockPots();
        clearJust();
      }

      if (nonePressed()) // none of the edit buttons are held down and the pot is unlocked, update the bufferTone array - ie. change the sound
      {
        for (byte i = 0; i < 4; i++)
        {
          if (justreleased[i])
          {
            lockPots();
            clearJust();
            lastEdited = 255;
          }
        }        
        changeTone();
      }

      // lock the pots if one of the note buttons was just pressed
      for (byte i = 0; i < 4; i++)
      { 
        if (justpressed[i])
        {
          lockPots();
          clearJust();
        }
      }

      // update the sequencer notes
      for (byte i = 0; i < 4; i++)
      {
        if (pressed[i]) // to check each of the 4 buttons
        {
          leds[(i * 4)/8] |= (B00001111 << (4 * (i % 2))); // light the four LEDs for this button
          for (byte j = 0; j < 4; j++) // to check each of the 4 pots pot[0] - to pot[3]
          {
            if (potLock[j] == 9999)
            {
              if (seqNotes[j + (i * 4)] != getScaleNotes(pot[j] >> 3)) // update only if it's changed
              {
                seqNotes[j + (i * 4)] = getScaleNotes(pot[j] >> 3);
                lastEdited = j + (i * 4);
              }
            }
            else if (difference(potLock[j], pot[j]) > 8)
              potLock[j] = 9999;
          }
        }
      }

      if (potLock[4] == 9999)
        if (lastEdited != 255) // add accent or slide 
        {
          if (pot[4] == 0) // clear the accent and slide for this step
          {
            clearAccentSlide(lastEdited);
          }
          else if (pot[4] < 511)// add an accent
          {           
            addAccent(lastEdited);
          }
          else // add a slide
          {
            addSlide(lastEdited);
          }
        }
        else // adjust the sequence length
        seqLength = (pot[4] / 64) + 1; // return a value between 1 an 16

      else if (difference(potLock[4], pot[4]) > 8)
        potLock[4] = 9999;
    }

    // *** MODE 0 - EDIT MODE ***
    //   BUTTONS - WITH SHIFT

    else // shift is held
    {
      if (justpressed[4]) // shift was just pressed, so lock the pots
      {
        lockPots();
        clearJust();
        if (nonePressed())
          checkSeqStartStop();

        // clear everything
        else if (pressed[0] && pressed[1] && pressed[2] && pressed[3])
        {
          clearAll();
        }
        else
          checkMode();
      }

      // adjust the tempo
      if(potLock[4] == 9999)
      {
        bpm = constrain(pot[4] >> 2, 45, 220);
        bpmChange();
      }
      else if (difference(potLock[4], pot[4]) > 8)
        potLock[4] = 9999;

      // adjust the duration of a step
      changeDuration();

      // adjust the decay
      changeDecay();

      // adjust the swing amount
      changeSwing();

      // record automation
      recordAutomation();

      // ********* BUTTONS *********

      // tap tempo or turn off automation
      if (justpressed[3])
      {
        if (automate)  
          automate = false; // turn off automation   
        else 
          tapTempo(); // tap tempo (if automate is not on)      
        clearJust();
      }

      // step through the play modes
      if (justpressed[2])
      {
        playMode = (playMode == 5) ? 0 : playMode + 1;
        // set stepNextStep according to the playMode
        switch (playMode)
        {
        case 0:
          seqNextStep = 1;
          break;
        case 1:
          seqNextStep = -1;
          break;
        case 2: 
          seqNextStep *= -1;
          break;
        case 3:
          seqCurrentStep = 0; // prevent it getting stuck
          seqTrueStep = 0;
          seqNextStep = random(0, seqLength);
          break;
        }
        clearJust();
      }

      // transpose up a semitone
      if (justpressed[1])
      {
        if (scale == 0)
          transpose(1);
        else
          scaleTranspose++;
        transpose(1);
        clearJust();
      }

      // transpose down a semitone
      if (justpressed[0])
      {
        if (scale == 0)
          transpose(-1);
        else
          scaleTranspose--;
        clearJust();
        transpose(-1);
      }
    }
    break;

  case 1:
    // *****************************
    // *** MODE 1 - PATTERN MODE ***
    // *****************************
    // turn on the LEDs for the bank we're in
    for (byte i = 0; i <= bank; i++)
      leds[i / 8] = bitSet(leds[i / 8],i % 8);

    if (bank < 8)
    {
      // turn on the LEDs for the follow mode of the bank
      for (byte i = 0; i <= followMode; i++)
        leds[1] = bitSet(leds[1],4 + i);   
    }

    if (!pressed[4]) // shift is not held
    {
      if (justreleased[4]) // shift was just let go, so lock the pots
      {
        lockPots();
        clearJust();
        // save the changed followMode
        if (followModeChange != 255)
        {
          saveFollowMode(followModeChange);
          followModeChange = 255;
        }                 
      }

      // *** MODE 1 - PATTERN MODE ***
      //       POTS - NO SHIFT

      //change the bank
      if (potLock[4] == 9999)
      {
        bank = map(pot[4], 0, 1023, 0, 11);

        // read the follow mode of the bank
        getFollowMode();        
      }
      else if (difference(potLock[4], pot[4]) > 8)
        potLock[4] = 9999;

      changeTone();

      // *** MODE 1 - PATTERN MODE ***
      //     BUTTONS - NO SHIFT

      for (byte i = 0; i < 4; i++)
      {
        if (justpressed[i])
        {
          if (confirm == 255)// nothing to confirm 
            longPress = millis();
          else if (confirm == (bank * 4) + i)// check if we're confirming a save or cancelling it
          {
            savePatch((bank * 4) + i); 
            confirm = 255;
          }
          else
          {
            confirm = 255;
          }
          clearJust();
        }
      }

      // save a patch
      for (byte i = 0; i < 4; i++)
      {
        if (pressed[i] && bank < 8 && longPress != 0 && (millis() - longPress) > 500)
        {
          if (!checkLocation((bank * 4) + i))
          {
            savePatch((bank * 4) + i);
          }
          else
            confirm = (bank * 4) + i;
          longPress = 0;
        }
        else if (pressed[i] && bank == 11 && longPress != 0 && (millis() - longPress) > 500) // save a user scale
        {
          saveScale(i);
        }
      }

      // cue a patch for loading or confirm a save
      for (byte i = 0; i < 4; i++)
      {
        if (justreleased[i] && longPress != 0)
        {
          if (bank < 8 && checkLocation((bank * 4) + i)) // only cue a patch if the location is in use
          {
            flashLeds();
            cued = (bank * 4) + i;
            head = (bank * 4) + i;
            playingBank = bank;
            if (triggerChannel != 0)
              midiA.sendNoteOn(64 + (bank * 4) + i, 127, triggerChannel); // send a note with velocity 127
          }
          else if (bank >= 8)          
          {
            playingBank = 12; // one that won't flash
            switch (bank)
            {
            case 8:
              switch (i)
              {
              case 0:
                generateChromatic();
                scale = 0;
                break;
              case 1:
                scalePointer = &major[0];
                scaleSize = sizeof(major);
                generateScale();
                scale = 1;
                break;
              case 2:
                scalePointer = &minorMel[0];
                scaleSize = sizeof(minorMel);
                generateScale();
                scale = 2;
                break;
              case 3:
                scalePointer = &minorHarm[0];
                scaleSize = sizeof(minorHarm);
                generateScale();
                scale = 3;
                break;
              }
              break;
            case 9:
              switch (i)
              {
              case 0:
                scalePointer = &pentaMaj[0];
                scaleSize = sizeof(pentaMaj);
                generateScale();
                scale = 4;
                break;
              case 1:
                scalePointer = &pentaMin[0];
                scaleSize = sizeof(pentaMin);
                generateScale();
                scale = 5;
                break;
              case 2:
                scalePointer = &bluesHex[0];
                scaleSize = sizeof(bluesHex);
                generateScale();
                scale = 6;
                break;
              case 3:
                scalePointer = &bluesHept[0];
                scaleSize = sizeof(bluesHept);
                generateScale();
                scale = 7;
                break;
              }
              break;
            case 10:
              switch (i)
              {
              case 0:
                scalePointer = &triadMaj[0];
                scaleSize = sizeof(triadMaj);
                generateScale();
                scale = 8;
                break;
              case 1:
                scalePointer = &triadMin[0];
                scaleSize = sizeof(triadMin);
                generateScale();
                scale = 9;
                break;
              case 2:
                scalePointer = &triad7Maj[0];
                scaleSize = sizeof(triad7Maj);
                generateScale();
                scale = 10;
                break;
              case 3:
                scalePointer = &triad7Min[0];
                scaleSize = sizeof(triad7Min);
                generateScale();
                scale = 11;
                break;
              }
              break;
            case 11: // user scales
              switch (i)
              {
              case 0:
                setUserScale(i);
                generateScale();
                break;
              case 1:
                setUserScale(i);
                generateScale();
                break;
              case 2:
                setUserScale(i);
                generateScale();
                break;
              case 3:
                setUserScale(i);
                generateScale();
                break;
              }
              break;
            }
          }

          longPress = 0;
          clearJust();
        }
      }
    }

    // *** MODE 1 - PATTERN MODE ***
    //       POTS - WITH SHIFT

    else // shift is held
    {
      if (justpressed[4]) // shift was just pressed, so lock the pots
      {
        lockPots();
        clearJust();
        if (nonePressed())
          checkSeqStartStop();
        else
          checkMode(); 
      }

      //change the follow mode for the bank, though we're only actually saving it when shift is released
      if (potLock[4] == 9999 && bank < 8)
      {
        followMode = pot[4] / 256;
        followModeChange = followMode;
      }
      else if (difference(potLock[4], pot[4]) > 8)
        potLock[4] = 9999; 

      // adjust the duration of a step
      changeDuration();

      // adjust the decay
      changeDecay();

      // adjust the swing amount
      changeSwing();

      // record automation
      recordAutomation();
    }

    // *** MODE 1 - PATTERN MODE ***
    //     BUTTONS - WITH SHIFT
    //        BANKS 9 - 16

    if (bank > 7) // change the scale, but don't generate a pattern
    {
      for (byte i = 0; i < 4; i++)
      {
        if (justpressed[i])
        {
          switch (bank)
          {
          case 8:
            switch (i)
            {
            case 0:
              scale = 0;
              break;
            case 1:
              scale = 1;
              break;
            case 2:
              scale = 2;
              break;
            case 3:
              scale = 3;
              break;
            }
            break;
          case 9:
            switch (i)
            {
            case 0:
              scale = 4;
              break;
            case 1:
              scale = 5;
              break;
            case 2: 
              scale = 6;
              break;
            case 3:
              scale = 7;
              break;
            }
            break;
          case 10:
            switch (i)
            {
            case 0:
              scale = 8;
              break;
            case 1:
              scale = 9;
              break;
            case 2:
              scale = 10;
              break;
            case 3:
              scale = 11;
              break;
            }
            break;
          case 11:
            setUserScale(i);
            break;
          }
          clearJust();
        }
      }
    }

    // *** MODE 1 - PATTERN MODE ***
    //     BUTTONS - WITH SHIFT
    //        BANKS 1 - 8

    else
    {
      if (justpressed[3])
      {
        if (seqRunning)// nudge tempo faster
        {
          bpm = (bpm < 220) ? bpm + 1 : 220;
          bpmChange();  
        }
        else // advance the pulse counter for manual sync adjust
        pulse = (pulse < 23) ? pulse + 1 : 23;
        clearJust();
      }

      if (justpressed[2])
      {
        if (seqRunning)// nudge tempo slower
        {
          bpm = (bpm > 45) ? bpm - 1 : 45;
          bpmChange();  
        }
        else // decrease the pulse counter for manual sync adjust
        pulse = (pulse > 0) ? pulse - 1 : 0;
        clearJust();
      }

      if (justpressed[1])
      {
        transpose(12);// transpose up an octave
        clearJust();
      }

      if (justpressed[0])
      {
        transpose(-12);// transpose down an octave
        clearJust();
      }
    }
    break;

  case 2:
    // *****************************
    // ***** MODE 2 - LIVE MODE ****
    // *****************************

    if (!pressed[4]) // shift is not held
    {
      // *** MODE 2 - LIVE MODE ***
      //     POTS - NO SHIFT

      changeTone();

      // select the step with pot 5
      if (potLock[4] == 9999) // ie. unlocked
        seqCurrentStep = pot[4] / 64;
      else if (difference(potLock[4], pot[4]) > 8)
        potLock[4] = 9999; 

      // *** MODE 2 - LIVE MODE ***
      //     BUTTONS - NO SHIFT

      if (potLock[4] == 9999) // ie. step select pot is unlocked
      {
        if (justreleased[0]) // clear the step
        {
          seqNotes[seqCurrentStep] = 0;
          flashLeds();
          clearJust;
        }
        else if (justreleased[1])
        {
          seqNotes[seqCurrentStep] = 127; // add a tie
          flashLeds();
          clearJust;
        }
        else if (justreleased[2]) // add an accent
        {
          bitSet(seqAccent[seqCurrentStep / 8], (seqCurrentStep % 8));
          bitClear(seqStepSlide[seqCurrentStep / 8], (seqCurrentStep % 8));
          flashLeds(); 
          clearJust;
        }
        else if (justreleased[3]) // add a slide
        {
          bitSet(seqStepSlide[seqCurrentStep / 8], (seqCurrentStep % 8)); 
          bitClear(seqAccent[seqCurrentStep / 8], (seqCurrentStep % 8));
          flashLeds();   
          clearJust;
        }
      }
    }

    // *** MODE 2 - LIVE MODE ***
    //    BUTTONS - WITH SHIFT

    else // shift is held
    {
      if (justpressed[4])
      {
        checkMode();
        clearJust();  
      }
    }
    break;

  case 3:
    // *****************************
    // ***** MODE 3 - PREFERENCES **
    // *****************************
    //     POTS - NO SHIFT
    if (!pressed[4]) // shift is not held
    {
      static unsigned long uiTimer;
      if (pot[4] < 512) // Red
      {
        if (lastRed) // last time it was also Red
        {
          lastRed = true;
          lastYellow = false;

          if (valueDisplayTimer == 0)
          {
            if (millis() - uiTimer < 500)
            {
              leds[0] = B00010001;
              leds[1] = B00010001;
            } 
            else 
              lightRedPrefs();
          }
        }
        else // last time it wasn't Red
        {
          lastRed = true;
          lastYellow = false;          
          uiTimer = millis();
        }
      }
      else // Yellow
      {
        if (lastYellow) // last time it was also Yellow
        {
          lastRed = false;
          lastYellow = true;
          if (valueDisplayTimer == 0)
          {
            if (millis() - uiTimer < 500)
            {
              leds[0] = B00100010;
              leds[1] = B00100010;
            } 
            else 
              lightYellowPrefs(); 
          }
        }
        else // last time it wasn't Yellow
        {
          lastRed = false;
          lastYellow = true;
          uiTimer = millis();
        }
      }

      if (lastRed) // we only have to check the pots for the red page
      { 
        for (byte i = 0; i < 4; i++)
        {
          if (potLock[i] == 9999)
          {
            byte tempValue;
            switch (i)
            {
            case 0:
              tempValue = map(pot[0], 0, 1023, 1, 16); 
              if (noteChannel != tempValue)
              {
                noteChannel = tempValue;
                valueDisplayTimer = millis();
              }
              else if (millis() - valueDisplayTimer > 500) // 500ms after the value stopped changing
              {
                valueDisplayTimer = 0;
                lockPots();
              }
              if (valueDisplayTimer != 0)
                showValue(noteChannel);
              break;
            case 1:
              tempValue = map(pot[1], 0, 1023, 1, 16); 
              if (triggerChannel != tempValue)
              {
                triggerChannel = tempValue;
                valueDisplayTimer = millis();
              }
              else if (millis() - valueDisplayTimer > 500) // 500ms after the value stopped changing
              {
                valueDisplayTimer = 0;
                lockPots();
              }
              if (valueDisplayTimer != 0)
                showValue(triggerChannel);
              break;
            case 2:
              tempValue = map(pot[2], 0, 1023, 1, 16); 
              if (midiAutomationCC != automationCC[tempValue - 1]) // -1 because the automationCC array is 0-indexed
              {
                midiAutomationCC = automationCC[tempValue - 1];
                valueDisplayTimer = millis();
              }
              else if (millis() - valueDisplayTimer > 500) // 500ms after the value stopped changing
              {
                valueDisplayTimer = 0;
                lockPots();
              }
              if (valueDisplayTimer != 0)
                showValue(tempValue);
              break;
            case 3:
              tempValue = map(pot[3], 0, 1023, 0, 255); 
              if (accentLevel != tempValue) 
              {
                accentLevel = tempValue;
                valueDisplayTimer = millis();
              }
              else if (millis() - valueDisplayTimer > 500) // 500ms after the value stopped changing
              {
                valueDisplayTimer = 0;
                lockPots();
              }
              if (valueDisplayTimer != 0)
                showValue(map(tempValue, 0, 255, 1, 16));
              break;
            }
          }
          else if (difference(potLock[i], pot[i]) > 8)
          {
            potLock[i] = 9999;
          }
        }
      }

      // *** MODE 3 - PREFERENCES ***
      //     BUTTONS - NO SHIFT

      if (justreleased[0])
      {
        if (released[0])
        {
          if (lastRed)
            midiNoteOut = !midiNoteOut;
          if (lastYellow)
            midiSyncOut = !midiSyncOut;
        }
        else
          released[0] = true;

        delay (10); // we seem to need longer time to debounce here - not sure why
        clearJust;
      }
      if (justreleased[1])
      { 
        if (released[1])
        {
          if (lastRed)
          {
            if (triggerChannel) // ie/ it's not set to 0
              triggerChannel = 0;
            else
              triggerChannel = 10; // if it is 0, set it ot the default of 10
          }
          if (lastYellow)
            sendStartStop = !sendStartStop;
        }
        else
          released[1] = true;

        delay (10);
        clearJust;
      }
      if (justreleased[2])
      {
        if (released[2])
        {
          if (lastRed)
            midiAutomationOut = !midiAutomationOut;
          if (lastYellow)
          {
            thruOn = !thruOn;
            if(!thruOn)
              midiA.turnThruOff();
            else
              midiA.turnThruOn(midi::Full);
          }
        }
        else
          released[2] = true;

        delay (10);
        clearJust;
      }
      if (justreleased[3])
      {
        if (lastYellow)
          playEdited = !playEdited;
        delay (10);
        clearJust;
      }
    }

    else // shift is held
    {
      if (justpressed[4])
      {
        checkMode();
        clearJust();  
        prefsTimer = millis();
      }
      if (prefsTimer != 0 && millis() - prefsTimer > 500)
      {
        savePreferences();
        savealert = true;
        prefsTimer = 0;
      }
      if (lastRed)
        lightRedPrefs();
      else if (lastYellow)
        lightYellowPrefs();
    }
    break;
  } 

  // *************************************
  //     SEQUENCER inside the LOOP
  // *************************************
  if (seqRunning)
  {
    if (currentTime == 0) // the fist time through the loop
    {
      stepGo = true;
      //midiA.sendSongPosition(0);
      sendCC(); // defined in HandleMidi
      currentTime = millis();
      sixteenthStartTime = currentTime;
      pulseStartTime = currentTime;
      swing16thDur = map(swing, 0, 255, sixteenthDur, ((2*sixteenthDur)/3)*2);
      autoCounter = 0;
    }
    else  
    {
      currentTime = millis();

      if (clockPulse < 11) // if the clockPulse is 11, wait for the next 8th (16th may swing) so they stay nicely in sync
      {
        if (currentTime - pulseStartTime >= clockPulseDur) // is it time to fire a clock pulse?
        {
          pulseStartTime = currentTime;
          if (midiSyncOut)
          {
            midiA.sendRealTime(midi::Clock); // send a midi clock pulse
          }
          sendCC(); // defined in HandleMidi
          clockPulse++; // advance the pulse counter
          autoCounter = (autoCounter < 95) ? autoCounter + 1 : 0;          
        }  
      }

      if (currentTime - sixteenthStartTime >= swing16thDur)
      { 
        //swing16thDur = (swing16thDur > sixteenthDur) ? ((2*sixteenthDur) - swing16thDur) : map(swing, 0, 255, sixteenthDur, ((2*sixteenthDur)/3)*2);
        //onDur = (onDur > swing16thDur) ? swing16thDur : onDur;
        stepGo = true;
        sixteenthStartTime = currentTime;
        seqTrueStep = (seqTrueStep+1)%32;
        if (seqTrueStep%2 != 0) // it's an uneven step, ie. the 2nd shorter 16th of the swing-pair
          swing16thDur = (2*sixteenthDur) - swing16thDur;
        else // it's an even step ie. the first longer 16th of the swing-pair
        {
          swing16thDur = map(swing, 0, 255, sixteenthDur, ((2*sixteenthDur)/3)*2);
          //midiA.sendSongPosition(seqCurrentStep); 

          while (clockPulse < 11)
          {
            if (midiSyncOut)
            {
              midiA.sendRealTime(midi::Clock); // send a midi clock pulse
            }
            sendCC(); // defined in HandleMidi
            clockPulse++;
            delay(1);
          }

          if (clockPulse = 11)
          {
            pulseStartTime = currentTime;
            clockPulse = 0;
            if (midiSyncOut)
            {
              midiA.sendRealTime(midi::Clock); // send a midi clock pulse
            }
            sendCC(); // defined in HandleMidi  
          }
        }
        onDur = (onDur > swing16thDur) ? swing16thDur : onDur;
        if (seqNextStep == -1 && seqCurrentStep == 0) // special case for reversed pattern
          seqCurrentStep = seqLength - 1;
        else
          seqCurrentStep = (seqCurrentStep + seqNextStep)%seqLength; // advance the step counter        

        if (seqCurrentStep == 0)
        {
          autoCounter = 0;
        }
        else  
          autoCounter = (autoCounter < 95) ? autoCounter + 1 : 0;
      }
    }
  }
  // is it time to move to the next step?
  if ((stepGo || seqMidiStep) && on)
  {
    if (seqCurrentStep == 0)
    {
      if (cued != 255)
      {
        loadPatch(cued); // defined in HelperFunctions - everything that happens with a patch/pattern change
      }

      else if (followAction != 0) // deal with the follow actions if we're on the first step
      {
        switch (followAction)
        {
        case 1: // play the next pattern      
          if (checkLocation(nowPlaying + 1)) // check if there is a pattern stored in the next location
            loadPatch(nowPlaying + 1); // load the next patch
          break;

        case 2:
          loadPatch(head); // load the patch marked as the head
          break;

        case 3: // play any of the 4 patches in the bank
          randomSeed(millis());
          loadPatch((playingBank * 4) + random(0, 4)); // returns a value from 0 to 3
          break;
        }
      }

      switch (playMode) // deal with the playMode for the first step
      {
      case 0: // forward
        seqNextStep = 1;
        break;
      case 1: // reverse
        seqNextStep = -1;
        break;
      case 2: // pendulum
        seqNextStep = 1;
        break;
      case 3: // random interval 
        seqNextStep = random(0, 16); 
        break;
      case 4: // drunk
        seqNextStep = (random(0, 2) == 0) ? 1 : -1; 
        break;
      }
    }
    else if (seqCurrentStep == seqLength - 1) // deal with the playMode for the last step
    {
      switch (playMode)
      {
      case 2:
        seqNextStep = - 1;
        break; 
      case 4:
        seqNextStep = (random(0, 2) == 0) ? 1 : -1;
        break;
      case 5:
        seqCurrentStep = random(0, 16); 
        break;
      }
    }
    else // deal with the playMode for other steps
    {
      switch (playMode) // deal with the playMode for the other steps
      {
      case 4: // drunk
        seqNextStep = (random(0, 2) == 0) ? 1 : -1; 
        break;
      case 5: // random
        seqCurrentStep = random(0, 16); 
        break;
      }
    }

    stepGo = false;
    offTime = sixteenthStartTime + onDur; // schedule when the note should be turned off
    if (seqNotes[seqCurrentStep] != 0)
      noteOn = true;
    seqMidiStep = false;
    on = false; // toggle, because we only want this section of code to run once    
    slide = false; // slide has to be turned off when the next note starts
    if (seqNotes[seqCurrentStep] != 127) // do this code only if this step is not tied (to the previous)
    {
      if (seqNotes[seqCurrentStep] == 0) //checkMute(seqCurrentStep))
      {
        noteOn = false;
      }

      else // if we got this far, we can safely change the pitch send out a midi note 
      {
        if (playEdited && lastEdited != 255) // should we play the note being edited
        {
          syncPhaseInc = pgm_read_word_near(midiTable +(seqNotes[lastEdited]));
          midiOn = (seqNotes[lastEdited] > 0) ? seqNotes[lastEdited] : 0;

          if (midiNoteOut && midiOn > 0 && midiOn < 127)
          { 
            if (checkAccent(lastEdited)) // does the step have an accent?
              midiA.sendNoteOn(midiOn, 127, noteChannel); // send a note with velocity 127
            else
              midiA.sendNoteOn(midiOn, map(accentLevel, 0, 255, 127, 0), noteChannel); // send a note with velocity 100
          }
        }

        else
        {
          syncPhaseInc = pgm_read_word_near(midiTable +(seqNotes[seqCurrentStep]));
          if (midiOn != 0 && midiNoteOut) // if we haven't turned off the midi note yet, do so now
          {
            midiA.sendNoteOff(midiOn, 0, noteChannel); // turn off the note we last turned on
          }

          midiOn = seqNotes[seqCurrentStep];
          if (midiNoteOut)
          { 
            if (checkAccent(seqCurrentStep)) // does the step have an accent?
              midiA.sendNoteOn(midiOn, 127, noteChannel); // send a note with velocity 127
            else
              midiA.sendNoteOn(midiOn, map(accentLevel, 0, 255, 127, 0), noteChannel); // send a note with velocity 100
          }
        }
      }
    }

    if (seqNotes[(seqCurrentStep + seqNextStep)%seqLength] == 127 && seqNotes[seqCurrentStep] != 127 && (seqNotes[seqCurrentStep] != 0)) // check if the next step is tied, but this one not, and this step is not muted
      seqTiedNote = seqNotes[seqCurrentStep]; // if so, assign this note's lookup-value to seqTiedNote

  }
  if (millis() >= offTime)
  {
    if (!on)
    {
      if (!checkSlide(seqCurrentStep)) // check that the step isn't marked to slide
      {
        on = true; // toggle, because we only want this section of code to run once 
        if (seqNotes[(seqCurrentStep + seqNextStep)%seqLength] != 127) // turn off the note unless the next step is tied (to this one)
        {
          noteOn = false;
          if (midiOn != 0 && midiNoteOut)
          {
            midiA.sendNoteOff(midiOn, 0, noteChannel); // turn off the note we last turned on
            midiOn = 0;
          }
        }
      }
      else if (!slide)// this step is marked as a slide, but we haven't toggled slide yet (we want to run the following code once only per slide)
      {
        slide = true;
        on = true; // toggle, because we only want this section of code to run once      
        pitchTarget = pgm_read_word_near(midiTable + (seqNotes[(seqCurrentStep + seqNextStep)%seqLength]));
      }
    }
    //do the following each time though the slide loop
    else if (slide)
    {
      if (pitchTarget > syncPhaseInc) 
        syncPhaseInc += (pitchTarget - syncPhaseInc)/((sixteenthStartTime + swing16thDur) - millis()); // work out by how much to change the pitch to get to the target pitch in time 

      else // the target must be smaller than the current increment
      {
        syncPhaseInc -= (syncPhaseInc - pitchTarget)/((sixteenthStartTime + swing16thDur) - millis());
      }
    }  
  }

  if (!savealert && confirm == 255 && mode != 3)
  {
    if (!(seqRunning || midiClock) && seqCurrentStep == 0)
      leds[0] = bitSet(leds[0], 0);
    else
      leds[(seqCurrentStep / 8)] = leds[(seqCurrentStep / 8)] ^ (1 << (seqCurrentStep % 8));
  }
  else if (savealert)
    saveAlert(); 
  else if (confirm != 255)// ie. confirm is not 255
      confirmAlert();
  if (mode == 1 && playingBank != bank && (seqRunning || midiClock))
    blinkPlayingBank();

  updateLeds(); // update the LEDs for this loop
}







































































