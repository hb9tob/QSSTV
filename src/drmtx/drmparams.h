#ifndef DRMPARAMS_H
#define DRMPARAMS_H

#include <QString>
#include <QList>

#define MINDRMSIZE 4000
#define MAXDRMSIZE 100000

struct drmTxParams
{
  int robMode;
  int qam;
  int bandwith;
  int interleaver;
  int protection;
  QString callsign;
  int reedSolomon;
  int fecMode;      // 0=Viterbi, 1=LDPC
  int imageCodec;   // 0=JP2, 1=AVIF
  int ldpcRate;     // 0=1/2, 1=2/3, 2=3/4, 3=5/6
  int resolution;   // 0=auto, 1=original, 2=1920x1080, 3=1280x720, 4=640x480, 5=320x240
};



drmTxParams modeToParams(unsigned int mode);
unsigned int paramsToMode(drmTxParams prm);
extern int numTxFrames;
extern   drmTxParams drmParams;
extern QList<short unsigned int> fixBlockList;


#endif // DRMPARAMS_H
