#include <cstdlib>
#include <iostream>

#include "rlebwt_reader.h"
#include "rlebwt.h"

using namespace std;

BWTReaderRLE::BWTReaderRLE ( const string& filename ) 
  : m_stage(IOS_NONE),
    m_numRunsOnDisk(0),
    m_numRunsRead(0) {
  m_pReader = new ifstream(filename.c_str(), ios::binary);
  m_stage = IOS_HEADER;
}

BWTReaderRLE::~BWTReaderRLE () {
  delete m_pReader;
}

void BWTReaderRLE::read ( RLEBWT* pRLBWT ) {
  BWFlag flag;
  readHeader(pRLBWT->m_numStrings, pRLBWT->m_numSymbols, flag);
  readRuns(pRLBWT->m_rlString, m_numRunsOnDisk);
}

void BWTReaderRLE::readHeader ( size_t& num_strings, size_t& num_symbols, BWFlag& flag ) {
  uint16_t magic_number;
  m_pReader->read(reinterpret_cast<char*>(&magic_number), sizeof(magic_number));
    
  if ( magic_number != RLBWT_FILE_MAGIC ) {
    cerr << "BWT file is not properly formatted, aborting\n";
    exit(EXIT_FAILURE);
  }

  m_pReader->read(reinterpret_cast<char*>(&num_strings), sizeof(num_strings));
  m_pReader->read(reinterpret_cast<char*>(&num_symbols), sizeof(num_symbols));
  m_pReader->read(reinterpret_cast<char*>(&m_numRunsOnDisk), sizeof(m_numRunsOnDisk));
  m_pReader->read(reinterpret_cast<char*>(&flag), sizeof(flag));
    
  m_stage = IOS_BWSTR;    
}

void BWTReaderRLE::readRuns ( RLVector& out, size_t numRuns ) {
  out.resize(numRuns);
  m_pReader->read(reinterpret_cast<char*>(&out[0]), numRuns*sizeof(RLUnit));
  m_numRunsRead = numRuns;
}
