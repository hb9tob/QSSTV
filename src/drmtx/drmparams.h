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
  int imageCodec;   // 0=JP2, 1=AVIF
  int resolution;   // 0=auto, 1=original, 2=1920x1080, 3=1280x720, 4=640x480, 5=320x240
};



drmTxParams modeToParams(unsigned int mode);
unsigned int paramsToMode(drmTxParams prm);
extern int numTxFrames;
extern   drmTxParams drmParams;
extern QList<short unsigned int> fixBlockList;


#endif // DRMPARAMS_H
