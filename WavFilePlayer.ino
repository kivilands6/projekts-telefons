// Simple WAV file player example
//
// Three types of output may be used, by configuring the code below.
//
//   1: Digital I2S - Normally used with the audio shield:
//         http://www.pjrc.com/store/teensy3_audio.html
//
//   2: Digital S/PDIF - Connect pin 22 to a S/PDIF transmitter
//         https://www.oshpark.com/shared_projects/KcDBKHta
//
//   3: Analog DAC - Connect the DAC pin to an amplified speaker
//         http://www.pjrc.com/teensy/gui/?info=AudioOutputAnalog
//
// To configure the output type, first uncomment one of the three
// output objects.  If not using the audio shield, comment out
// the sgtl5000_1 lines in setup(), so it does not wait forever
// trying to configure the SGTL5000 codec chip.
//
// The SD card may connect to different pins, depending on the
// hardware you are using.  Uncomment or configure the SD card
// pins to match your hardware.
//
// Data files to put on your SD card can be downloaded here:
//   http://www.pjrc.com/teensy/td_libs_AudioDataFiles.html
//
// This example code is in the public domain.

#include <Bounce.h>
#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

AudioSynthWaveform       waveform1;
AudioPlaySdWav           playWav1;
AudioPlaySdRaw           playRaw1;
// Use one of these 3 output types: Digital I2S, Digital S/PDIF, or Analog DAC
AudioOutputI2S           audioOutput;
AudioInputI2S            audioInput;
AudioRecordQueue         queue1;
AudioMixer4              mixer;

// Connections
AudioConnection patchCord1(waveform1, 0, mixer, 0); // wave to mixer 
AudioConnection patchCord2(playRaw1, 0, mixer, 1); // raw audio to mixer
AudioConnection patchCord3(playWav1, 0, mixer, 2); // wav file playback mixer
AudioConnection patchCord4(mixer, 0, audioOutput, 0); // mixer output to speaker (L)
AudioConnection patchCord5(audioInput, 0, queue1, 0); // mic input to queue (L)
AudioControlSGTL5000     sgtl5000_1;

// Use these with the Teensy Audio Shield
#define SDCARD_CS_PIN    10
#define SDCARD_MOSI_PIN  7
#define SDCARD_SCK_PIN   14

#define HOOK_PIN 0
#define PLAYBACK_BUTTON_PIN 1

// Filename to save audio recording on SD card
char filename[15];
// The file object itself
File frec;

Bounce buttonRecord = Bounce(HOOK_PIN, 40);
Bounce buttonPlay = Bounce(PLAYBACK_BUTTON_PIN, 8);


enum Mode {Initialising, Ready, Prompting, Recording, Playing};
Mode mode = Mode::Initialising;

void setup() {
  Serial.begin(9600);
 pinMode(HOOK_PIN, INPUT_PULLUP);
 pinMode(PLAYBACK_BUTTON_PIN, INPUT_PULLUP);

  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(60);

  // Comment these out if not using the audio adaptor board.
  // This may wait forever if the SDA & SCL pins lack
  // pullup resistors
  sgtl5000_1.enable();
  sgtl5000_1.inputSelect(AUDIO_INPUT_MIC);
  sgtl5000_1.volume(0.5);

  // Play a beep to indicate system is online
  waveform1.begin(WAVEFORM_SINE);
  waveform1.frequency(440);
  waveform1.amplitude(0.9);
  wait(250);
  waveform1.amplitude(0);
  delay(1000);

  SPI.setMOSI(SDCARD_MOSI_PIN);
  SPI.setSCK(SDCARD_SCK_PIN);
  if (!(SD.begin(SDCARD_CS_PIN))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }
  
  //sgtl5000_1.micGain(5);
  mode = Mode::Ready;
}

void playFile(const char *filename)
{
  Serial.print("Playing file: ");
  Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  playWav1.play(filename);

  // A brief delay for the library read WAV info
  delay(25);

  // Simply wait for the file to finish playing.
  while (playWav1.isPlaying()) {
    buttonPlay.update();
    buttonRecord.update();

    if(buttonPlay.fallingEdge()) {
          playWav1.stop();
          return;
    }
    else if (buttonRecord.risingEdge()){
        playWav1.stop();
        mode = Mode::Ready;
        break;
    }
  }
}


void loop() {
  buttonRecord.update();
  buttonPlay.update();

  switch(mode){
    case Mode::Ready:
      // Button record falling = up / rising = phone put down
      if (buttonRecord.fallingEdge()) {
        mode = Mode::Prompting;
      }
      else if(buttonPlay.fallingEdge()) {
        playAllRecordings();
      }
      break;

    case Mode::Prompting:
      // Wait a second for users to put the handset to their ear
      wait(1000);
      // Play the greeting inviting them to record their message
        // run while the file plays.
      playWav1.play("greeting.WAV");
    
      // A brief delay for the library read WAV info
      delay(25);
    
      // Simply wait for the file to finish playing.
      while (playWav1.isPlaying()) {
        buttonPlay.update();
        buttonRecord.update();
    
        if(buttonPlay.fallingEdge()) {
              playWav1.stop();
              mode = Mode::Ready;
              return;
        }
        else if (buttonRecord.risingEdge()){
            playWav1.stop();
            mode = Mode::Ready;
            return;
        }
      } 
      
      
      // Debug message
      Serial.println("Starting Recording");
      // Play the tone sound effect
      waveform1.frequency(440);
      waveform1.amplitude(0.9);
      wait(250);
      waveform1.amplitude(0);
      // Start the recording function
      startRecording();
      break;

   case Mode::Recording:
      // Handset is replaced
      if(buttonRecord.risingEdge()){
        // Debug log
        Serial.println("Stopping Recording");
        // Stop recording
        stopRecording();
        // Play audio tone to confirm recording has ended
        waveform1.frequency(523.25);
        waveform1.amplitude(0.9);
        wait(50);
        waveform1.amplitude(0);
        wait(50);
        waveform1.amplitude(0.9);
        wait(50);
        waveform1.amplitude(0);
      }
      else {
        continueRecording();
      }
      break;

   case Mode::Playing:
      break;
  }

}

void startRecording() {
  // Find the first available file number
  for (uint8_t i=0; i<9999; i++) {
    // Format the counter as a five-digit number with leading zeroes, followed by file extension
    snprintf(filename, 11, " %05d.RAW", i);
    // Create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }
  frec = SD.open(filename, FILE_WRITE);
  if(frec) {
    Serial.print("Recording to ");
    Serial.println(filename);
    queue1.begin();
    mode = Mode::Recording;
  }
  else {
    Serial.println("Couldn't open file to record!");
  }
}

void continueRecording() {
  // Check if there is data in the queue
  if (queue1.available() >= 2) {
    byte buffer[512];
    // Fetch 2 blocks from the audio library and copy
    // into a 512 byte buffer.  The Arduino SD library
    // is most efficient when full 512 byte sector size
    // writes are used.
    memcpy(buffer, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    memcpy(buffer+256, queue1.readBuffer(), 256);
    queue1.freeBuffer();
    // Write all 512 bytes to the SD card
    frec.write(buffer, 512);
  }
}

void stopRecording() {
  // Stop adding any new data to the queue
  queue1.end();
  // Flush all existing remaining data from the queue
  while (queue1.available() > 0) {
    // Save to open file
    frec.write((byte*)queue1.readBuffer(), 256);
    queue1.freeBuffer();
  }
  // Close the file
  frec.close();
  mode = Mode::Ready;
}

void playAllRecordings() {
  // Recording files are saved in the root directory
  File dir = SD.open("/");

  while (true) {
    File entry =  dir.openNextFile();
    if (!entry) {
      // no more files
      entry.close();
      break;
    }

    int8_t len = strlen(entry.name());
    if (strstr(strlwr(entry.name() + (len - 4)), ".raw")) {
      Serial.print("Now playing ");
      Serial.println(entry.name());
      // Play a short beep before each message
      waveform1.amplitude(0.5);
      wait(500);
      waveform1.amplitude(0);
      // Play the file
      playRaw1.play(entry.name());
      mode = Mode::Playing;
    }
    entry.close();

    while (playRaw1.isPlaying()) {
      buttonPlay.update();
      buttonRecord.update();
      // Button is pressed again
      if(buttonPlay.risingEdge()) {
        playRaw1.stop();
      }
      if(buttonRecord.risingEdge()) {
        playRaw1.stop();
        mode = Mode::Ready;
        return;
      }   
    }
  }
  // All files have been played
  mode = Mode::Ready;
}

// Non-blocking delay, which pauses execution of main program logic,
// but while still listening for input 
void wait(unsigned int milliseconds) {
  elapsedMillis msec=0;

  while (msec <= milliseconds) {
    buttonRecord.update();
    buttonPlay.update();
    if (buttonRecord.fallingEdge()) Serial.println("Button (pin 0) Press");
    if (buttonPlay.fallingEdge()) Serial.println("Button (pin 1) Press");
    if (buttonRecord.risingEdge()) Serial.println("Button (pin 0) Release");
    if (buttonPlay.risingEdge()) Serial.println("Button (pin 1) Release");
  }
}
