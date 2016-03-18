#ifndef RS_BWTREADERRLE_H
#define RS_BWTREADERRLE_H

#include <fstream>

#include "rlunit.h"

enum BWIOStage {
    IOS_NONE,
    IOS_HEADER,
    IOS_BWSTR,
    IOS_PC,
    IOS_OCC,
    IOS_DONE
};

enum BWFlag {
    BWF_NOFMI = 0,
    BWF_HASFMI
};

const uint16_t RLBWT_FILE_MAGIC = 0xCACA;
const uint16_t BWT_FILE_MAGIC = 0xEFEF;

class RLEBWT;

class BWTReaderRLE
{
 public:
  BWTReaderRLE ( const std::string& filename );
  ~BWTReaderRLE ();

  void read ( RLEBWT* pBWT );

  void readHeader ( size_t& num_strings, size_t& num_symbols, BWFlag& flag );
  void readRuns ( RLVector& out, size_t numRuns );

 private:
  std::istream* m_pReader;
  BWIOStage m_stage;
  RLUnit m_currRun;
  size_t m_numRunsOnDisk;
  size_t m_numRunsRead;
};

#endif
