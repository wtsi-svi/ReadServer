#ifndef RS_RLEBWT_H
#define RS_RLEBWT_H

#include <stdint.h>
#include <string>

#include "rlunit.h"
#include "BPTree.h"
#include "bwt.h"

#define DEFAULT_SAMPLE_RATE_SMALL 64 // strictly power of 2.
#define DEFAULT_SAMPLE_RATE_LARGE 1024 // strictly power of 2.
#define DEFAULT_MAX_COUNT 31 // maximum length per run-length encoded char. (2**5-1)

class RLEBWT : public BWT {
 public:
  RLEBWT ( const std::string& filename, const int smallSampleRate = DEFAULT_SAMPLE_RATE_SMALL );

  void initialiseFMIndex ();
  void serialiseFMIndex ( const std::string& filename );

  char getChar ( const uint64_t& index ) const;
  uint64_t getOcc ( const char& b, const uint64_t& index ) const;
  uint64_t getOccAt ( const char& b, const uint64_t& index ) const;    

  char getF ( const uint64_t& index ) const;
  uint64_t getPC ( const char& b ) const;
  uint64_t getBWLen () const;

  friend class BWTReaderRLE;

 private:
  RLEBWT () {}

  void deserialiseFMIndex ( const std::string& filename );

  inline size_t getNumStrings () const {
    return m_numStrings;
  }

  inline size_t getNumRuns () const {
    return m_rlString.size();
  }

  // The C(a) array
  AlphaCount64 m_predCount;
  
  // The number of strings in the collection
  uint64_t m_numStrings;

  // The total length of the bw string
  uint64_t m_numSymbols;

  // The sample rate used for the markers
  uint64_t m_smallSampleRate;

  // The run-length encoded string
  RLVector m_rlString;

  BPTree m_fmIndex;
};

#endif
