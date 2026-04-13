/**************************************************************************
*   Copyright (C) 2000-2019 by Johan Maes                                 *
*   on4qz@telenet.be                                                      *
*   https://www.qsl.net/o/on4qz                                           *
*                                                                         *
*   PortAudio backend for cross-platform audio support                    *
*   (Windows, Linux, macOS)                                               *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "soundportaudio.h"
#include "configparams.h"

soundPortAudio::soundPortAudio()
{
  captureStream = NULL;
  playbackStream = NULL;
  paInitialized = false;

  PaError err = Pa_Initialize();
  if (err == paNoError)
    paInitialized = true;
}

soundPortAudio::~soundPortAudio()
{
  closeDevices();
  if (paInitialized)
    Pa_Terminate();
}

static int findDeviceByName(const QString &name, bool isInput)
{
  int numDevices = Pa_GetDeviceCount();
  for (int i = 0; i < numDevices; i++)
    {
      const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
      if (!info) continue;
      if (isInput && info->maxInputChannels < 1) continue;
      if (!isInput && info->maxOutputChannels < 1) continue;
      QString devName = QString::fromUtf8(info->name);
      if (name == "default" || name.isEmpty())
        {
          /* Return the PortAudio default device */
          if (isInput && i == Pa_GetDefaultInputDevice()) return i;
          if (!isInput && i == Pa_GetDefaultOutputDevice()) return i;
        }
      else if (devName.contains(name, Qt::CaseInsensitive))
        return i;
    }
  /* Fallback to PortAudio default */
  return isInput ? Pa_GetDefaultInputDevice() : Pa_GetDefaultOutputDevice();
}

bool soundPortAudio::init(int samplerate)
{
  soundDriverOK = false;
  sampleRate = samplerate;

  if (!paInitialized)
    {
      errorHandler("PortAudio", "Failed to initialize PortAudio");
      return false;
    }

  /* ---- Capture stream (mono) ---- */
  int capDev = findDeviceByName(inputAudioDevice, true);
  if (capDev < 0)
    {
      errorHandler("PortAudio", "No capture device found");
      return false;
    }

  PaStreamParameters capParams;
  capParams.device = capDev;
  capParams.channelCount = MONOCHANNEL;
  capParams.sampleFormat = paInt16;
  capParams.suggestedLatency = Pa_GetDeviceInfo(capDev)->defaultLowInputLatency;
  capParams.hostApiSpecificStreamInfo = NULL;

  PaError err = Pa_OpenStream(&captureStream,
                              &capParams,    /* input */
                              NULL,          /* no output */
                              sampleRate,
                              DOWNSAMPLESIZE,
                              paClipOff,
                              NULL, NULL);   /* blocking mode */
  if (err != paNoError)
    {
      /* Retry with stereo if mono not supported */
      capParams.channelCount = STEREOCHANNEL;
      err = Pa_OpenStream(&captureStream, &capParams, NULL,
                          sampleRate, DOWNSAMPLESIZE, paClipOff, NULL, NULL);
      if (err != paNoError)
        {
          errorHandler("PortAudio capture open", QString(Pa_GetErrorText(err)));
          return false;
        }
      isStereo = true;
    }
  else
    {
      isStereo = false;
    }

  err = Pa_StartStream(captureStream);
  if (err != paNoError)
    {
      errorHandler("PortAudio capture start", QString(Pa_GetErrorText(err)));
      return false;
    }

  /* ---- Playback stream (stereo) ---- */
  int playDev = findDeviceByName(outputAudioDevice, false);
  if (playDev < 0)
    {
      errorHandler("PortAudio", "No playback device found");
      return false;
    }

  PaStreamParameters playParams;
  playParams.device = playDev;
  playParams.channelCount = STEREOCHANNEL;
  playParams.sampleFormat = paInt16;
  playParams.suggestedLatency = Pa_GetDeviceInfo(playDev)->defaultLowOutputLatency;
  playParams.hostApiSpecificStreamInfo = NULL;

  err = Pa_OpenStream(&playbackStream,
                      NULL,          /* no input */
                      &playParams,   /* output */
                      sampleRate,
                      DOWNSAMPLESIZE,
                      paClipOff,
                      NULL, NULL);   /* blocking mode */
  if (err != paNoError)
    {
      errorHandler("PortAudio playback open", QString(Pa_GetErrorText(err)));
      return false;
    }

  err = Pa_StartStream(playbackStream);
  if (err != paNoError)
    {
      errorHandler("PortAudio playback start", QString(Pa_GetErrorText(err)));
      return false;
    }

  soundDriverOK = true;
  return true;
}

int soundPortAudio::read(int &countAvailable)
{
  if (!soundDriverOK || !captureStream) return 0;

  long avail = Pa_GetStreamReadAvailable(captureStream);
  if (avail < 0)
    {
      errorHandler("PortAudio read avail", QString(Pa_GetErrorText((PaError)avail)));
      return -1;
    }
  countAvailable = (int)avail;

  if (countAvailable >= DOWNSAMPLESIZE)
    {
      PaError err = Pa_ReadStream(captureStream, tempRXBuffer, DOWNSAMPLESIZE);
      if (err != paNoError && err != paInputOverflowed)
        {
          errorHandler("PortAudio read", QString(Pa_GetErrorText(err)));
          return -1;
        }
      if (isStereo)
        {
          /* Downmix stereo to mono: keep left channel */
          for (int i = 1; i < DOWNSAMPLESIZE; i++)
            {
              tempRXBuffer[i] = tempRXBuffer[2 * i];
            }
        }
      return DOWNSAMPLESIZE;
    }
  return 0;
}

int soundPortAudio::write(uint numFrames)
{
  if (!soundDriverOK || !playbackStream) return 0;
  if (numFrames == 0) return 0;

  PaError err = Pa_WriteStream(playbackStream, tempTXBuffer, numFrames);
  if (err != paNoError && err != paOutputUnderflowed)
    {
      errorHandler("PortAudio write", QString(Pa_GetErrorText(err)));
      return -1;
    }
  return numFrames;
}

void soundPortAudio::flushCapture()
{
  if (!soundDriverOK || !captureStream) return;

  long avail = Pa_GetStreamReadAvailable(captureStream);
  while (avail >= DOWNSAMPLESIZE)
    {
      Pa_ReadStream(captureStream, tempRXBuffer, DOWNSAMPLESIZE);
      avail -= DOWNSAMPLESIZE;
    }
  if (avail > 0)
    {
      /* Read remaining frames to discard them.
         Use a temp buffer sized for the remaining frames. */
      qint16 tmpBuf[DOWNSAMPLESIZE * 2 * 2];
      Pa_ReadStream(captureStream, tmpBuf, avail);
    }
}

void soundPortAudio::flushPlayback()
{
  if (!soundDriverOK || !playbackStream) return;
  Pa_AbortStream(playbackStream);
  Pa_StartStream(playbackStream);
}

void soundPortAudio::waitPlaybackEnd()
{
  /* Nothing to do - Pa_WriteStream is blocking */
}

void soundPortAudio::closeDevices()
{
  if (captureStream)
    {
      Pa_StopStream(captureStream);
      Pa_CloseStream(captureStream);
      captureStream = NULL;
    }
  if (playbackStream)
    {
      Pa_StopStream(playbackStream);
      Pa_CloseStream(playbackStream);
      playbackStream = NULL;
    }
}

void getPortAudioCardList(QStringList &inputList, QStringList &outputList)
{
  inputList.clear();
  outputList.clear();

  /* Ensure PortAudio is initialized */
  PaError initErr = Pa_Initialize();
  if (initErr != paNoError) return;

  int numDevices = Pa_GetDeviceCount();
  for (int i = 0; i < numDevices; i++)
    {
      const PaDeviceInfo *info = Pa_GetDeviceInfo(i);
      if (!info) continue;
      const PaHostApiInfo *apiInfo = Pa_GetHostApiInfo(info->hostApi);
      QString apiName = apiInfo ? QString::fromUtf8(apiInfo->name) : "";
      QString devName = QString::fromUtf8(info->name);
      QString fullName = devName + " (" + apiName + ")";

      if (info->maxInputChannels > 0)
        inputList.append(fullName);
      if (info->maxOutputChannels > 0)
        outputList.append(fullName);
    }

  Pa_Terminate();
}
