// midi.h library info http://playground.arduino.cc/Main/MIDILibrary
// MIDI_Class Class Reference http://arduinomidilib.sourceforge.net/a00001.html

byte lastNote = 0;

void HandleClock(void) // what to do for 24ppq clock pulses
{
  lastClock = millis();
  seqRunning = false;
  pulse++; // 6 for a 16th
  autoCounter = (autoCounter < 95) ? autoCounter + 1 : 0; 
  static boolean long16th = true; // is this the first/longer 16th of the swing pair?
  char swingPulse = swing / 86; // to give us some swing when synced to midi clock (sadly only 2 levels: full swing and half swing)
  // takes a value between 0 and 255 and returns 0, 1 or 2
  swingPulse = (!long16th) ? (0 - swingPulse) : swingPulse;                              
  if (pulse >= (6 + swingPulse))
  {
    currentTime = millis();
    pulse = 0;
    seqMidiStep = true;
    //seqTrueStep = (seqTrueStep + 1)%32; 
    if (seqNextStep == -1 && seqCurrentStep == 0) // special case for reversed pattern
      seqCurrentStep = seqLength - 1;
    else
      seqCurrentStep = (seqCurrentStep + seqNextStep)%seqLength; // advance the step counter
    long16th = !long16th;

    sixteenthStartTime = millis();
  }
}
void HandleStart (void)
{
  seqTrueStep = 0;
  seqCurrentStep = 0;
  autoCounter = 0;
  clockPulse = 0;
  sixteenthStartTime = millis();
  seqMidiStep = true;
}

void HandleStop (void)
{
  //midiClock = false;
  seqTrueStep = 0;
  seqCurrentStep = 0;
  autoCounter = 0;
  sixteenthStartTime = 0;
  if (midiNoteOut)
  {
    for (byte i = 0; i < 128; i++) // turn off all midi notes
      midiA.sendNoteOff(i, 0, noteChannel);
  }
  seqRunning = false;
  noteOn = false; 
}

void HandleNoteOn(byte channel, byte pitch, byte velocity) 
{
  if (mode == 2 && channel == noteChannel)
  {
    if (velocity != 0) // some instruments send note off as note on with 0 velocity
    {
      seqNotes[seqCurrentStep] = pitch;
      seqCurrentStep = (seqCurrentStep + 1) % 16;
      noteOn = true;
      syncPhaseInc = pgm_read_word_near(midiTable + pitch);
      lastNote = pitch;

      //grainPhaseInc = mapPhaseInc(512 - (pot[0] >> 1))/ 2;  
      int newVelocity = (pot[2] + velocity < 1023) ? pot[2] + velocity : 1023; 
      grain2PhaseInc = mapPhaseInc(1023 - newVelocity) / 2;

      automate = false;
    }
    else // velocity = 0;
    {
      if (pitch == lastNote) 
        noteOn = false;
    }
  }
  else if (channel == noteChannel && velocity != 0) //transpose
  {
    char transposeAmount = (pitch - (48 + transposeFactor));
    transpose(transposeAmount);
    transposeFactor += transposeAmount;  
  }
  else if (channel == triggerChannel && pitch > 63 && pitch < 96) // receive patch changes on triggerChannel (default channel 10); there are 32 memory locations
  {
    byte patch = pitch - 64;
    // cue the patch
    if (checkLocation(patch)) // only cue a patch if the location is in use
    {
      flashLeds();
      cued = patch;
      head = patch;
      bank = patch / 4;
      getFollowMode();
      playingBank = patch / 4;
    }
    // load the bank follow mode  
  }
}

void sendCC()
{
  if (midiAutomationOut && automate)
  {
    // sendControlChange (byte ControlNumber, byte ControlValue, byte Channel)
    midiA.sendControlChange (midiAutomationCC, seqAutomate[autoCounter]>>1, noteChannel);
  }
}




