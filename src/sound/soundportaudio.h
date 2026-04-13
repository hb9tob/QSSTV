#ifndef SOUNDPORTAUDIO_H
#define SOUNDPORTAUDIO_H

#include "soundbase.h"
#include <portaudio.h>

void getPortAudioCardList(QStringList &inputList, QStringList &outputList);

class soundPortAudio : public soundBase
{
public:
  soundPortAudio();
  ~soundPortAudio();
  bool init(int samplerate);
  int read(int &countAvailable);
  int write(uint numFrames);

protected:
  void flushCapture();
  void flushPlayback();
  void closeDevices();
  void waitPlaybackEnd();

private:
  PaStream *captureStream;
  PaStream *playbackStream;
  bool paInitialized;
};

#endif // SOUNDPORTAUDIO_H
