// *************************************
//           LEDS
// *************************************

void ledsOff()
{
  leds[0] = 0;
  leds[1] = 0;
}

void updateLeds()
{
  digitalWrite(LEDlatchPin, 0);
  //count up on GREEN LEDs
  shiftOut(LEDdataPin, LEDclockPin, leds[1]); 
  //count down on RED LEDs
  shiftOut(LEDdataPin, LEDclockPin, leds[0]);
  //return the latch pin high to signal chip that it 
  //no longer needs to listen for information
  digitalWrite(LEDlatchPin, 1);
}

void shiftOut(int myDataPin, int myClockPin, byte myDataOut) {
  // This shifts 8 bits out MSB first, 
  //on the rising edge of the clock,
  //clock idles low

  //internal function setup
  int i=0;
  int pinState;
  pinMode(myClockPin, OUTPUT);
  pinMode(myDataPin, OUTPUT);

  //clear everything out just in case to
  //prepare shift register for bit shifting
  digitalWrite(myDataPin, 0);
  digitalWrite(myClockPin, 0);

  //for each bit in the byte myDataOutï¿½
  //NOTICE THAT WE ARE COUNTING DOWN in our for loop
  //This means that %00000001 or "1" will go through such
  //that it will be pin Q0 that lights. 
  for (i=7; i>=0; i--)  {
    digitalWrite(myClockPin, 0);

    //if the value passed to myDataOut and a bitmask result 
    // true then... so if we are at i=6 and our value is
    // %11010100 it would the code compares it to %01000000 
    // and proceeds to set pinState to 1.
    if ( myDataOut & (1<<i) ) {
      pinState= 1;
    }
    else {	
      pinState= 0;
    }

    //Sets the pin to HIGH or LOW depending on pinState
    digitalWrite(myDataPin, pinState);
    //register shifts bits on upstroke of clock pin  
    digitalWrite(myClockPin, 1);
    //zero the data pin after shift to prevent bleed through
    digitalWrite(myDataPin, 0);
  }

  //stop shifting
  digitalWrite(myClockPin, 0);  
}

void saveAlert()
{
  static unsigned long timer = 0;
  static byte counter = 0;
  static byte tempLeds[2];

  if (counter == 0 || (timer != 0 && (millis() - timer > 50)))
  {
    tempLeds[0] = bitSet(tempLeds[0], 7 - counter);
    tempLeds[1] = bitSet(tempLeds[1], counter);

    if (counter > 1)
    {
      tempLeds[0] = bitClear(tempLeds[0], 7 - (counter - 2));
      tempLeds[1] = bitClear(tempLeds[1], (counter - 2));
    }

    counter++;
    timer = millis();
  }

  leds[0] = tempLeds[0];
  leds[1] = tempLeds[1];

  if (counter == 8)
  {
    counter = 0;
    savealert = false;
    timer = 0;
    tempLeds[0] = 0;
    tempLeds[1] = 0;
  }
}

void confirmAlert()
{
  static unsigned long timer;
  static boolean toggle;
  static byte tempLeds[2];

  if (timer == 0)
  {
    if (toggle)
    {
      tempLeds[0] = B01010101;
      tempLeds[1] = B01010101;
      timer = millis();
    }
    if (!toggle)
    {
      tempLeds[0] = B10101010;
      tempLeds[1] = B10101010;
      timer = millis();
    }
  }

  if (timer != 0 && (millis() - timer > 200))
  {
    timer = 0;
    toggle = !toggle;
  }

  leds[0] = tempLeds[0];
  leds[1] = tempLeds[1];
}

void blinkPlayingBank()
{
  static unsigned long timer;
  static boolean toggle;

  if (toggle)
  {
    if ((millis() - timer) <= 200)
      bitSet(leds[0], playingBank);
    else
    { 
      timer = millis();
      toggle = !toggle;
    }
  }
  else
  {
    if ((millis() - timer) <= 200)
      bitClear(leds[0], playingBank);
    else
    { 
      timer = millis();
      toggle = !toggle;
    }
  }
}

void flashLeds()
{
  leds[0] = B11111111;
  leds[1] = B11111111;
  updateLeds();
}

void lightYellowPrefs()
{
  // light the LEDs for the appropriate preferences
  if (midiSyncOut)
    leds[0] = bitSet(leds[0], 1);
  if (sendStartStop)
    leds[0] = bitSet(leds[0], 5);
  if (thruOn)
    leds[1] = bitSet(leds[1], 1);
  if (playEdited)
    leds[1] = bitSet(leds[1], 5);  
}

void lightRedPrefs()
{
  // light the LEDs for the appropriate preferences
  if (midiNoteOut)
    leds[0] = bitSet(leds[0], 0);
  if (triggerChannel)
    leds[0] = bitSet(leds[0], 4);
  if (midiAutomationOut)
    leds[1] = bitSet(leds[1], 0);
  if (accentLevel)
    leds[1] = bitSet(leds[1], 4);
}

void showValue(byte value)
{
  value -= 1;
  leds[value / 8] = bitSet(leds[value / 8], (value % 8));
}

// *************************************
//           BUTTONS
// *************************************

void check_switches()
{
  static byte previousstate[NUMBUTTONS];
  static byte currentstate[NUMBUTTONS];
  static long lasttime;
  byte index;

  if (millis() < lasttime) {
    // we wrapped around, lets just try again
    lasttime = millis();
  }

  if ((lasttime + DEBOUNCE) > millis()) {
    // not enough time has passed to debounce
    return; 
  }
  // ok we have waited DEBOUNCE milliseconds, lets reset the timer
  lasttime = millis();

  for (index = 0; index < NUMBUTTONS; index++) {
    justpressed[index] = 0;       // when we start, we clear out the "just" indicators
    justreleased[index] = 0;

    currentstate[index] = digitalRead(buttons[index]);   // read the button

    if (currentstate[index] == previousstate[index]) {
      if ((pressed[index] == LOW) && (currentstate[index] == LOW)) {
        // just pressed
        justpressed[index] = 1;
      }
      else if ((pressed[index] == HIGH) && (currentstate[index] == HIGH)) {
        // just released
        justreleased[index] = 1;
      }
      pressed[index] = !currentstate[index];  // remember, digital HIGH means NOT pressed
    }

    previousstate[index] = currentstate[index];   // keep a running tally of the buttons
  }
}

boolean nonePressed() // are any of the note buttons pressed?
{
  for (byte j = 0; j < 4; j++)
  {
    if (pressed[j])
    {
      return false;
    }
  }
  return true;
}

void clearJust() // call this after button is checked and found to be true
{
  for (byte i = 0; i < 5; i++) {
    justpressed[i] = 0;       // when we start, we clear out the "just" indicators
    justreleased[i] = 0;
  }
}

void checkMode()
{
  if (pressed[0] && pressed[1] && pressed[2]) // Preferences Mode
  {
    mode = 3;
    prefsTimer = 0;
    lastRed = false;
    lastYellow = false;
    clearJust();
    lockPots();
    valueDisplayTimer = 0;
    for (byte i = 0; i < 3; i++)
      released[i] = false;
  }

  else if (pressed[0] && pressed[1]) // Live Mode
  {
    clearJust();
    mode = 2;
    seqRunning = false;
    noteOn = false;
    seqTrueStep = 0;
    seqCurrentStep = 0;
    lockPots();
    if (sendStartStop)
      midiA.sendRealTime(midi::Stop); // send a midi clock stop signal
  }

  else if (pressed[0]) // toggle Edit Mode / Pattern Mode
  {
    switch (mode)
    {
      clearJust();
    case 0:
      mode = 1;
      lockPots();
      clearJust();
      longPress = 0;
      getFollowMode(); // read the follow mode of the bank
      break;
    case 1:
      mode = 0;
      lockPots();
      break;
    case 2:
      mode = 0;
      lockPots();
      break;
    case 3:
      mode = 0;
      lockPots();
      break;
    }
  } 
}

void checkSeqStartStop()
{
  if (!midiClock) // we're not receiving MIDI clock
  {
    if (!seqRunning)  // the sequencer is not running
    {
      seqRunning = true;
      seqTrueStep = 0;
      seqCurrentStep = 0;
      clockPulse = 0;
      lockPots();
      currentTime = 0; // so we trigger the "first time" code section in the sequencer
      if (sendStartStop)
        midiA.sendRealTime(midi::Start); // send a midi clock start signal
    }
    else // the sequencer is running
    {
      if (millis() - doubleTap < 250) // 250ms between taps to register a double tap
      {
        seqRunning = false;
        noteOn = false;
        seqTrueStep = 0;
        seqCurrentStep = 0;
        lockPots();
        currentTime = 0; // so we trigger the "first time" code section in the sequencer
        if (sendStartStop)
          midiA.sendRealTime(midi::Stop); // send a midi clock start signal
        if (midiNoteOut)
        {
          for (byte i = 0; i < 128; i++) // turn off all midi notes
            midiA.sendNoteOff(i, 0, noteChannel);
        }
      }
      else
        doubleTap = millis();
    }
  }
}

// *************************************
//              POTS
// *************************************

void getPots()
{
  pot[0] = analogRead(A0);
  pot[1] = analogRead(A1);
  pot[2] = analogRead(A2);
  pot[3] = analogRead(A3);
  pot[4] = analogRead(A4);
}

void lockPots()
{
  for (byte i = 0; i < 5; i++)
    potLock[i] = pot[i];
}

// *************************************
//          LOADING & SAVING
// *************************************

void loadPatch(byte patchNumber) // load the specified location number
{
  if (patchNumber < 32 && checkLocation(patchNumber)) // only load a patch if the location is used and a patch number under 32 is called
  {
    // load the sequencer notes
    for (byte i = 0; i < 16; i++)
      seqNotes[i] = EEPROM.read((patchNumber * 30) + i); // a patch is 30 bytes long

    // load the slide bytes
    seqStepSlide[0] = EEPROM.read((patchNumber * 30) + 16);
    seqStepSlide[1] = EEPROM.read((patchNumber * 30) + 17);

    // load the accent bytes
    seqAccent[0] = EEPROM.read((patchNumber * 30) + 18);
    seqAccent[1] = EEPROM.read((patchNumber * 30) + 19);

    // load the tone controls
    for (byte i = 0; i < 4; i++)
      bufferTone[i] = ((int)EEPROM.read((patchNumber * 30) + 20 + i)) << 2;

    lockPots();

    // load the BPM
    bpm = EEPROM.read((patchNumber * 30) + 24); // we'll also use this to check if a patch location is in use or not (0 for not in use)
    bpmChange();

    // load the followAction
    followAction = EEPROM.read((patchNumber * 30) + 25);

    // load the note duration
    onDur = EEPROM.read((patchNumber * 30) + 26) << 2; // change the byte to an int

    // load the decay amount
    decay = EEPROM.read((patchNumber * 30) + 27) << 2; // change the byte to an int

    // load the swing amount
    swing = EEPROM.read((patchNumber * 30) + 28);

    // load the pattern length
    seqLength = EEPROM.read((patchNumber * 30) + 29);

    cued = 255; // nothing is cued
    nowPlaying = patchNumber;
    transposeFactor = 0;
  }
}

void savePatch(byte patchNumber)
{
  savealert = true; // do the LED display
  transposeFactor = 0;

  // save the sequencer notes
  for (byte i = 0; i < 16; i++)
    EEPROM.write((patchNumber * 30) + i, seqNotes[i]); // a patch is 30 bytes long

  // save the slide bytes
  EEPROM.write((patchNumber * 30) + 16, seqStepSlide[0]);
  EEPROM.write((patchNumber * 30) + 17, seqStepSlide[1]);

  // save the accent bytes
  EEPROM.write((patchNumber * 30) + 18, seqAccent[0]);
  EEPROM.write((patchNumber * 30) + 19, seqAccent[1]);

  // save the tone controls
  for (byte i = 0; i < 4; i++)
  {
    if (bufferTone[i] == 9999)
      EEPROM.write((patchNumber * 30) + 20 + i, (pot[i] >> 2)); // take the pot[i] value from 10 bits long to 8 bits  
    else  
      EEPROM.write((patchNumber * 30) + 20 + i, (bufferTone[i] >> 2)); // a patch is 26 bytes long
  }

  // save the BPM
  EEPROM.write((patchNumber * 30) + 24, bpm); // we'll also use this to check if a patch location is in use or not (0 for not in use)

  // followAction is saved on a per bank basis elsewhere
  //EEPROM.write((patchNumber * 30) + 25, 1);

  // save the note duration
  EEPROM.write((patchNumber * 30) + 26, (onDur >> 2)); // change the int to a byte

  // save the decay amount
  EEPROM.write((patchNumber * 30) + 27, (decay >> 2)); // change the int to a byte

  // save the swing amount
  EEPROM.write((patchNumber * 30) + 28, swing);

  // save the pattern length
  EEPROM.write((patchNumber * 30) + 29, seqLength);

  playingBank = bank;
  loadPatch(patchNumber);
}  

boolean checkLocation(byte patchNumber)
{
  byte checker = EEPROM.read((patchNumber * 30) + 24);
  if (checker != 0)
    return true;
  else
    return false;
}

void eepromInit()
{
  leds[0] = EEPROM.read(1023);
  leds[1] = EEPROM.read(1023);
  updateLeds();
  if (EEPROM.read(1023) != 0) // we're using the final EEPROM byte (1023) to check whether the EEPROM has been initialized yet - if it's 0, the EEPROM has been initialized
  {
    for (int i = 0; i < 1024; i++)
      EEPROM.write(i, 0);
  }
}

void saveFollowMode(byte modeNum)
{
  switch(modeNum)
  {
  case 0:
    for (byte i = 0; i < 4; i++)
      EEPROM.write((bank * 120) + 25 + (i * 30), 0); // 120 bytes per bank, followAction is byte 25, each patch is 30 bytes long
    break;
  case 1:
    EEPROM.write((bank * 120) + 25, 1);
    EEPROM.write((bank * 120) + 55, 2);
    EEPROM.write((bank * 120) + 85, 1);
    EEPROM.write((bank * 120) + 115, 2);
    break;
  case 2:
    EEPROM.write((bank * 120) + 25, 1);
    EEPROM.write((bank * 120) + 55, 1);
    EEPROM.write((bank * 120) + 85, 1);
    EEPROM.write((bank * 120) + 115, 2);
    break;
  case 3:
    EEPROM.write((bank * 120) + 25, 3);
    EEPROM.write((bank * 120) + 55, 3);
    EEPROM.write((bank * 120) + 85, 3);
    EEPROM.write((bank * 120) + 115, 3);
    break;   
  }
}

void getFollowMode()
{
    byte modeTotal = 0;
    for (byte i = 0; i < 4; i++)
      modeTotal += EEPROM.read((bank * 120) + 25 + (i * 30)); // 120 bytes per bank, followAction is byte 25, each patch is 30 bytes long

    switch (modeTotal) // we're just adding followAction variables (4 per bank) and checking for the sum
    {
    case 0:
      followMode = 0; // 0+0+0+0 ie. 0 - loop each bar
      break;
    case 6:
      followMode = 1; // 1+2+1+2 ie. 6 - loop 2 sets of 2 bars
      break;
    case 5:
      followMode = 2; // 1+1+1+2 ie. 5  - loop 4 bars
      break;
    case 12:
      followMode = 3; // 3+3+3+3 ie. 12 - random
      break;
    }
}

void loadPreferences()
{
  byte checker = 0;
  for (byte i = 0; i < 10; i++)
    checker += EEPROM.read(1013 + i);
  if (checker == 0) // ie. preferences have never been saved before
  {
    savePreferences(); // save preferences with the default values
    savealert = true;
  }
  else // load the preferences from EEPROM
  {
    midiNoteOut = EEPROM.read(1013); // send out MIDI notes to the next device in the chain (you may want to switch this off, eg. if you have more than one groovesizer in a chain and only want to use notes from one)
    noteChannel = EEPROM.read(1014); // the MIDI channel that note on/off messages are sent on
    triggerChannel = EEPROM.read(1015); // the MIDI channel that pattern change messages are sent and received on
    midiAutomationCC = EEPROM.read(1016); // the MIDI CC
    midiAutomationOut = EEPROM.read(1017);
    accentLevel = EEPROM.read(1018); // the amount by which to mark an accent (range 0 to 255)
    midiSyncOut = EEPROM.read(1019); // send out MIDI sync pulses
    sendStartStop = EEPROM.read(1020); // send start and stop messages
    thruOn = EEPROM.read(1021); // is MIDI thru on or off? If it's on, all MIDI messages received at input are echoed to the output
    if(!thruOn)
      midiA.turnThruOff();
    else
      midiA.turnThruOn(midi::Full);
    playEdited = EEPROM.read(1022); // play the note being edited on every active step, or only hear the note being edited in its actual place in the pattern
  }
}

void savePreferences()
{
  EEPROM.write(1013, midiNoteOut); // send out MIDI notes to the next device in the chain (you may want to switch this off, eg. if you have more than one groovesizer in a chain and only want to use notes from one)
  EEPROM.write(1014, noteChannel); // the MIDI channel that note on/off messages are sent on
  EEPROM.write(1015, triggerChannel); // the MIDI channel that pattern change messages are sent and received on
  EEPROM.write(1016, midiAutomationCC); // the MIDI CC
  EEPROM.write(1017, midiAutomationOut);
  EEPROM.write(1018, accentLevel); // the amount by which to mark an accent (range 0 to 255)
  EEPROM.write(1019, midiSyncOut); // send out MIDI sync pulses
  EEPROM.write(1020, sendStartStop); // send start and stop messages
  EEPROM.write(1021, thruOn); // is MIDI thru on or off? If it's on, all MIDI messages received at input are echoed to the output
  EEPROM.write(1022, playEdited); // play the note being edited on every active step, or only hear the note being edited in its actual place in the pattern
}

void saveScale(byte number) // number 0 - 3
{
  byte scaleLength = (seqLength < 13) ? seqLength : 12;
  // save the number of notes in the scale
  EEPROM.write((960 + (13 * number) + 12), scaleLength); // 960 is the first location for first scale, user scales are 13 bytes long, the length is saved in the 13th byte
  // save the notes of the scale
  for (byte i = 0; i < scaleLength; i++)
  {
    byte degree = (seqNotes[i] >= seqNotes[0]) ? seqNotes[i] % seqNotes[0] : 0; // if the next note in the scale is lower than the tonic, make it equal to the tonic  
    EEPROM.write(960 + (13 * number) + i, degree + 1); // the first note is the tonic and the others are noteNumber % (noteNumber of the first note)
  }
  savealert = true;
}

void loadScale(byte number) // 0 - 4
{
  for (byte i = 0; i < 13 ; i++)
    userScale[i] = EEPROM.read(960 + (13 * number) + i);
}

// *************************************
//              SYNTH
// *************************************
void changeTone()
{
  for (byte i = 0; i < 4; i++)
  {
    if (potLock[i] == 9999)
      bufferTone[i] = pot[i];
    else if (difference(potLock[i], pot[i]) > 8)
      potLock[i] = 9999;         
  }
}

// *************************************
//              SEQUENCER
// *************************************

void tapTempo()
{
  static unsigned long lastTap = 0;
  unsigned long now = millis(); // the current time in milliseconds
  static byte tapsNumber = 0; // how many bpms have been added to the bpmTaps array
  int bpmSum = 0; // the sum of the values in the bpmTaps[] array 

  if ((now - lastTap) < 1334) // a quarter is 1333ms long at 45bpm (our slowest allowed bpm)
  {
    bpmTaps[tapsNumber % 10] = 60000/(now - lastTap);
    tapsNumber++;
    lastTap = now;
  }
  else
  {
    for (byte i = 0; i < 10; i++) // clear the bpmTaps array
    {
      bpmTaps[i] = 0;
      tapsNumber = 0;
    }
    lastTap = now;
  }

  for (byte i = 0; i < 10; i++)
  {
    if (bpmTaps[i] > 0)
    {
      bpmSum += bpmTaps[i];
    }
  }

  if (bpmSum > 0)
  {
    byte i = (tapsNumber < 11) ? tapsNumber : 10;
    bpm = bpmSum / i;
    bpmChange();
  }

  seqTrueStep = seqLength - 1;
  seqCurrentStep = seqLength - 1;
  autoCounter = 0;
  sixteenthStartTime = 0;
}

// things that need to happen as a consequence of the bpm changing
void bpmChange()
{
  clockPulseDur = (60000/bpm)/24;
  sixteenthDur = (60000/bpm)/4;
  if (onDur >= sixteenthDur) // && durationPU != 9999)
  {
    onDur = sixteenthDur - 10;
    offTime = sixteenthStartTime + onDur;
  }      
  if (seqTrueStep%2 != 0) // it's an uneven step, ie. the 2nd shorter 16th of the swing-pair
    swing16thDur = (2*sixteenthDur) - swing16thDur;
  else // it's an even step ie. the first longer 16th of the swing-pair
  {
    swing16thDur = map(swing, 0, 255, sixteenthDur, ((2*sixteenthDur)/3)*2);
  }
}

void transpose(int8_t interval) // int8_t here gives us an signed 8-bit variable (we need to be able to transpose by negative values)
{
  for (byte i = 0; i < 16; i++)
  { 
    if (seqNotes[i] != B00000000)
    {
      if (interval < 0) // we are transposing down
        seqNotes[i] = (seqNotes[i] > (0 - interval)) ? (seqNotes[i] + interval) : ((12*10) - ((0 - interval) - seqNotes[i]));
      else // we are transposing up
      seqNotes[i] = ((seqNotes[i] + interval) <= 127) ? (seqNotes[i] + interval) : (interval - (127 - seqNotes[i])) + 7; // the final 7 is 127 (top midi note) - 10 octaves(12 * 10) ie. 127 - 120
    }
  }
}

void clearAll() // clear everything
{
  for (byte i = 0; i < 16; i++)
    seqNotes[i] = 0;
  clearAccents();
  clearSlides();
  transposeFactor = 0;
  automate = false;
  if (midiNoteOut)
  {
    for (byte i = 0; i < 128; i++) // turn off all midi notes
      midiA.sendNoteOff(i, 0, noteChannel);
  }
}


void clearAccents() // clear all accents
{
  seqAccent[0] = 0;
  seqAccent[1] = 0;  
}

void clearSlides() // clear all slides
{
  seqStepSlide[0] = 0;
  seqStepSlide[1] = 0; 
}

void clearAccentSlide(byte editedStep) // clear 
{ 
  for (int steps = editedStep; steps >= 0; steps -= 4)
  {
    if (pressed[steps / 4])
    {
      bitClear(seqAccent[steps / 8], (steps % 8));
      bitClear(seqStepSlide[steps / 8], (steps % 8));
    }
  }
}

void addSlide(byte editedStep)
{
  for (int steps = editedStep; steps >= 0; steps -= 4)
  {
    if (pressed[steps / 4])
    {
      bitSet(seqStepSlide[steps / 8], (steps % 8));
      bitClear(seqAccent[steps / 8], (steps % 8));     
    }
  }
}

void addAccent(byte editedStep)
{
  for (int steps = editedStep; steps >= 0; steps -= 4)
  {
    if (pressed[steps / 4])
    {
      bitSet(seqAccent[steps / 8], (steps % 8));
      bitClear(seqStepSlide[steps / 8], (steps % 8));     
    }
  }
}

boolean checkAccent(byte thisStep) // check whether this step has an accent or not - returns true if a step is tied, false if not
{
  if (seqAccent[thisStep/8] & (1<<thisStep%8)) //seqStepSlide is an array of unsigned bytes, so dividing thisStep by 8 will return 0 or 1 (ie. the byte we're concerned with)
    return true;
  else
    return false; 
}

boolean checkSlide(byte thisStep) // check whether a step is a slide or not - returns true if it's a slide, false if not
{
  if (seqStepSlide[thisStep/8] & (1<<thisStep%8)) //seqStepSlide is an array of unsigned bytes, so dividing thisStep by 8 will return 0 or 1 (ie. the byte we're concerned with)
    return true;
  else
    return false; 
}

void changeDuration ()// adjust the duration of a step
{
  if(potLock[3] == 9999)
    onDur = map(pot[3], 0, 1023, 15, (sixteenthDur - 10));
  else if (difference(potLock[3], pot[3]) > 8)
    potLock[3] = 9999;
}

void changeDecay() // adjust the decay
{
  if(potLock[2] == 9999)
    decay = 1023 - pot[2];
  else if (difference(potLock[2], pot[2]) > 8)
    potLock[2] = 9999;
}

void changeSwing() // adjust the swing amount
{
  if(potLock[1] == 9999)
    swing = map(pot[1], 0, 1023, 0, 255);
  else if (difference(potLock[1], pot[1]) > 8)
    potLock[1] = 9999;
}

void recordAutomation() // record automation
{
  if(potLock[0] == 9999)
    seqAutomate[autoCounter] = pot[0] >> 2;
  else if (difference(potLock[0], pot[0]) > 8)
  {
    potLock[0] = 9999;     
    automate = true;  
  }
}

// *************************************
//              SCALES
// *************************************

byte getScaleNotes(byte input)
{
  if (input == 0)
    return 0;
  else if (input == 127)
    return 127;
  else
  {
    byte length; // how many notes in the scale?
    byte octave = input / 12;
    byte inputNote = input - (12 * octave);

    switch (scale)
    {
    case 0: // chromatic
      return input;

    case 1: // major
      length = sizeof(major) - 1; // subtract 1 because arrays are zero-indexed      
      return major[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 2: // minor (melodic)
      length = sizeof(minorMel) - 1; // subtract 1 because arrays are zero-indexed      
      return minorMel[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 3: // minor (harmonic)
      length = sizeof(minorHarm) - 1; // subtract 1 because arrays are zero-indexed      
      return minorHarm[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 4: // pentatonic (major)
      length = sizeof(pentaMaj) - 1; // subtract 1 because arrays are zero-indexed      
      return pentaMaj[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 5: // pentatonic (minor)
      length = sizeof(pentaMin) - 1; // subtract 1 because arrays are zero-indexed      
      return pentaMin[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 6: // blues (hexatonic)
      length = sizeof(bluesHex) - 1; // subtract 1 because arrays are zero-indexed      
      return bluesHex[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 7: // blues (heptatonic)
      length = sizeof(bluesHept) - 1; // subtract 1 because arrays are zero-indexed      
      return bluesHept[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 8: // major triad
      length = sizeof(triadMaj) - 1; // subtract 1 because arrays are zero-indexed      
      return triadMaj[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 9: // minor triad
      length = sizeof(triadMin) - 1; // subtract 1 because arrays are zero-indexed      
      return triadMin[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 10: // major #7
      length = sizeof(triad7Maj) - 1; // subtract 1 because arrays are zero-indexed      
      return triad7Maj[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 11: // minor 7
      length = sizeof(triad7Min) - 1; // subtract 1 because arrays are zero-indexed      
      return triad7Min[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;

    case 12: // userScale
      length = userScale[12] - 1; // subtract 1 because arrays are zero-indexed   
      return userScale[map(inputNote, 0, 11, 0, length)] + (12 * octave) + scaleTranspose - 1;
    }
  }
}

void generateScale()
{
  for (byte i = 0; i < 16; i++)
    seqNotes[i] = 47 + *(scalePointer + (i % scaleSize)) + (12 * (i / scaleSize)) + scaleTranspose; // 48 = c3, but the tonic has a value of 1, so 47 for the tonic to be 48   
}

void generateChromatic()
{
  for (byte i = 0; i < 16; i++)
    seqNotes[i] = 48 + i; // 48 = c3 
}

void setUserScale(byte number)
{
  if (EEPROM.read(960 + (13 * number) + 12) != 0) // check that the byte we use for length is not zero
  {
    loadScale(number);
    scalePointer = &userScale[0];
    scaleSize = userScale[12];
    scale = 12;
  }
}

// *************************************
//              UTILITY
// *************************************

// a handy DIFFERENCE FUNCTION
int difference(int i, int j)
{
  int k = i - j;
  if (k < 0)
    k = j - i;
  return k;
}





























