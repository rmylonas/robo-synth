#define SCALE_LEN 8
#define SEQ_NUM 2
#define SEQ_LEN 8

#include <MozziGuts.h>
#include <Oscil.h>
#include <EventDelay.h>
#include <ADSR.h>
#include <tables/sin8192_int8.h>
#include <mozzi_midi.h>
#include <Smooth.h>

const int playButtonPin = 2;

//#define CONTROL_RATE 256 // Hz, powers of 2 are most reliable

Oscil <8192, AUDIO_RATE> aOscil(SIN8192_DATA);
Oscil <8192, AUDIO_RATE> bOscil(SIN8192_DATA);
Oscil <8192, 64> kModIndex(SIN8192_DATA);

// for triggering the envelope
EventDelay noteDelay;

ADSR <AUDIO_RATE, AUDIO_RATE> envelope;

boolean note_is_on = true;

int cMajor[] = {72, 74, 76, 77, 79, 81, 83, 84};
int cMinor[] = {72, 74, 75, 77, 79, 80, 82, 84};

int seqMatrix[SEQ_NUM][SEQ_LEN];
int currNote = 0;

void setup(){
  Serial.begin(9600);
  
  kModIndex.setFreq(.768f); // sync with kNoteChangeDelay

  pinMode(playButtonPin, INPUT);

for (int i=0; i < SEQ_NUM; i++) {
  for (int j=0; j < SEQ_LEN; j++) {
    seqMatrix[i][j] = random(SEQ_LEN);
  }
}
  
  noteDelay.set(768); 
  //startMozzi();
}


unsigned int duration, attack, decay, sustain, release_ms;
int myNote;
char midiNote;

 Q16n16 carrier_freq;
 Q16n16 mod_freq;
 Q16n16 deviation;
 Q8n8 mod_index;
 Q7n8 smoothed_note;
 Q8n8 mod_to_carrier_ratio = float_to_Q8n8(0.5);

  Smooth <int> kSmoothNote(0.95f);

  boolean isPlaying = false;

void updateControl(){

  int currentPlay = digitalRead(playButtonPin);

  if(currentPlay == LOW){
    stopMozzi();
    isPlaying = false;
  }
  
  if(noteDelay.ready()){

      // choose envelope levels
      byte attack_level = 127;
      byte decay_level = 127;
      envelope.setADLevels(attack_level,decay_level);

    attack = 20;
    decay = 0;
    sustain = 0;
    release_ms = 200;
    
     envelope.setTimes(attack,decay,sustain,release_ms);
     envelope.noteOn();

     myNote = seqMatrix[1][currNote];
     midiNote = myNote ? (cMinor[myNote] - 1) : 10;

    setFreqs(midiNote, 1);
    //setFMFreqs(midiNote, 9);

     currNote = (currNote >= SCALE_LEN-1) ? 0 : currNote + 1;
     //noteDelay.start(attack+decay+sustain+release_ms);
     noteDelay.start(attack+decay+sustain+release_ms);
   }
}


void setFreqs(Q8n8 midi_note, char offset){
     aOscil.setFreq((int)mtof(midiNote));
     bOscil.setFreq((int)mtof(midiNote + offset));
}

void setFMFreqs(Q8n8 midi_note, char offset){
    // vary the modulation index
    mod_index = (Q8n8)350+kModIndex.next();
    smoothed_note = kSmoothNote.next(Q7n0_to_Q7n8(midi_note));
  
    carrier_freq = Q16n16_mtof(Q8n8_to_Q16n16(smoothed_note));
    
    mod_freq = ((carrier_freq>>8) * mod_to_carrier_ratio)  ; // (Q16n16>>8)   Q8n8 = Q16n16, beware of overflow
    //mod_freq = Q16n16_mtof(Q8n8_to_Q16n16(smoothed_note + 1));
    deviation = ((mod_freq>>16) * mod_index); // (Q16n16>>16)   Q8n8 = Q24n8, beware of overflow
  
     aOscil.setFreq_Q16n16(carrier_freq);
     bOscil.setFreq_Q16n16(mod_freq);
}


int updateAudio(){
   envelope.update();  
   unsigned char nEnv = envelope.next();

  Q15n16 modulation = deviation * bOscil.next() >> 8;
  //return ((long) nEnv * aOscil.phMod(modulation)) >> 8;
  //return ((long) nEnv * aOscil.phMod(modulation)) >> 8;
   
  return ((long)nEnv * aOscil.next() + (int)nEnv * bOscil.next())>>8; 
}


void loop(){

  if(! isPlaying){
    int currentPlay = digitalRead(playButtonPin);
    if(currentPlay == HIGH){
      startMozzi();
      isPlaying = true;
    }
  }else{
    audioHook();
  }
}
