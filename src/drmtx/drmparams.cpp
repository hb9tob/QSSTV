#include "drmparams.h"
#include "configparams.h"

int numTxFrames;
drmTxParams drmParams;
QList<short unsigned int> fixBlockList;

drmTxParams modeToParams(unsigned int mode)
{
  drmTxParams prm;
  prm.imageCodec=(mode/1000000);
  mode-=(mode/1000000)*1000000;
  prm.fecMode=(mode/100000);
  mode-=(mode/100000)*100000;
  prm.ldpcRate=(mode/10000) % 10;
  /* Preserve backward compat: if fecMode encoded, robMode is always 0 */
  prm.robMode=(mode/10000) % 10;
  if (prm.fecMode > 0) prm.robMode = 0;
  mode-=(mode/10000)*10000;
  prm.bandwith=mode/1000;
  mode-=(mode/1000)*1000;
  prm.protection=mode/100;
  mode-=(mode/100)*100;
  prm.qam=mode/10;
  prm.interleaver=0;
  prm.callsign=myCallsign;
  return prm;
}

unsigned int paramsToMode(drmTxParams prm)
{
  uint mode=1;
  mode+=prm.imageCodec*1000000;
  mode+=prm.fecMode*100000;
  mode+=prm.robMode*10000;
  mode+=prm.bandwith*1000;
  mode+=prm.protection*100;
  mode+=prm.qam*10;
  mode+=prm.interleaver;
  return mode;
}



