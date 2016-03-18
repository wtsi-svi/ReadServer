#include <memory>
#include <fstream>
#include <future>
#include <string>

#include "rlebwt.h"
#include "rlebwt_reader.h"

using namespace std;

RLEBWT::RLEBWT ( const std::string& filename, const int smallSampleRate )
  : m_numStrings(0),
    m_numSymbols(0),
    m_smallSampleRate(smallSampleRate) {
  future<int> rbwt = async( launch::async, [ ] ( const string& filename, RLEBWT* pbwt ) {
      BWTReaderRLE* pReader = new BWTReaderRLE(filename);
      pReader->read(pbwt);
      delete pReader;
      return 1;
    }, filename, this );

  const string bpi = filename + ".bpi2";
  if ( FILE *file = fopen(bpi.c_str(), "r") ) {
    fclose(file);
    deserialiseFMIndex(bpi);
    rbwt.get();
  }
  else {
    rbwt.get(); // read the bwt string first.
    initialiseFMIndex();
  }
}

void RLEBWT::initialiseFMIndex () {
  size_t lsr = DEFAULT_SAMPLE_RATE_LARGE; // large sample rate
  size_t ssr = m_smallSampleRate; // small sample rate
  size_t sr = DEFAULT_SYMBOL_RATE; // symbol rate
  size_t lsv = 0; // large shift value
  size_t ssv = 0; // small shift value
  size_t sv = 0; // symbol shift value

  while ( lsr != 1 ) { lsr >>= 1; ++lsv; }
  while ( ssr != 1 ) { ssr >>= 1; ++ssv; }
  while ( sr != 1 ) { sr >>= 1; ++sv; }
  
  size_t numOfBuckets = m_rlString.size();
  size_t numOfCharsPerBucket = 1;
  size_t numOfBucketsPerBlock = 1;
  size_t maxCount = 1;

  numOfBuckets = numOfBuckets >> lsv;
  while ( numOfBuckets > 0 ) {
    numOfCharsPerBucket = numOfCharsPerBucket * DEFAULT_SAMPLE_RATE_LARGE;
    numOfBucketsPerBlock = DEFAULT_SAMPLE_RATE_LARGE;
    maxCount = numOfCharsPerBucket * numOfBucketsPerBlock * DEFAULT_MAX_COUNT;

    maxCount = maxCount >> 16;
    if ( maxCount < 1 ) {
      m_fmIndex.insertAtFront(shared_ptr<IBPNodes>(new BPNodes16(numOfBuckets+1, numOfCharsPerBucket, numOfBucketsPerBlock)));
    }
    else {
      maxCount = maxCount >> 16;
      if ( maxCount < 1 ) {
        m_fmIndex.insertAtFront(shared_ptr<IBPNodes>(new BPNodes32(numOfBuckets+1, numOfCharsPerBucket, numOfBucketsPerBlock)));
      }
      else {
        m_fmIndex.insertAtFront(shared_ptr<IBPNodes>(new BPNodes64(numOfBuckets+1, numOfCharsPerBucket, numOfBucketsPerBlock)));
      }
    }

    numOfBuckets = numOfBuckets >> lsv;
  }
   
  numOfBuckets = m_rlString.size();
  numOfBuckets = numOfBuckets >> ssv;
  numOfCharsPerBucket = m_smallSampleRate;
  numOfBucketsPerBlock = 1 << (lsv - ssv);
  m_fmIndex.push_back(shared_ptr<IBPNodes>(new BPNodes16(numOfBuckets+1, numOfCharsPerBucket, numOfBucketsPerBlock)));

  size_t numOfSumElems = m_numSymbols;
  numOfSumElems >>= sv;
  m_fmIndex.setSumVector(shared_ptr< vector<uint32_t> >(new vector<uint32_t>()));
  vector<uint32_t>* pvSum = (m_fmIndex.getSumVector()).get();
  pvSum->reserve(numOfSumElems+1);
  pvSum->push_back(0);
  size_t nextSum = DEFAULT_SYMBOL_RATE;
  size_t running_total = 0;

  const size_t& depth = m_fmIndex.depth();
  const size_t& bottom = depth - 1;
  const size_t& length = m_rlString.size();
  vector<size_t> nextBuckets(depth, 0);
  vector<IBPNodes*> nodes;
  for ( size_t i=0; i<depth; ++i ) {
    nodes.push_back((m_fmIndex.at(i)).get());
  }

  for ( size_t i=0; i<length; ++i ) {
    const RLUnit& unit = m_rlString[i];
    const char& b = unit.getChar();
    const uint8_t& run_len = unit.getCount();

    if ( running_total >= nextSum ) {
      pvSum->push_back(nodes[bottom]->getLength() - 1);
      nextSum += DEFAULT_SYMBOL_RATE;
    }

    if ( i == nextBuckets[bottom] ) {
      for ( size_t j=0; j<depth; ++j ) {
        if ( i == nextBuckets[j] ) {
          for ( size_t k=bottom; k>j; --k ) {
            const size_t l = k - 1;
            nodes[l]->addToLast(nodes[k]);
            nodes[k]->clearLast();
            nodes[k]->appendLast();
            nextBuckets[k] = nodes[k]->getBucketSize() * nodes[k]->getLength();
          }
          nodes[j]->appendLast();
          nextBuckets[j] = nodes[j]->getBucketSize() * nodes[j]->getLength();
          break; 
        }
      }
    }

    nodes[bottom]->addToLast(b, run_len);
    running_total += run_len;
  }

  if ( depth > 0 ) {
    size_t lastdollar = 0;
    size_t lasta = 0;
    size_t lastc = 0;
    size_t lastg = 0;
    for ( size_t i=0; i<depth; ++i ) {
      const shared_ptr<IBPNodes>& nodes = m_fmIndex.at(i);
      lastdollar += nodes->getLast('$');
      lasta += nodes->getLast('A');
      lastc += nodes->getLast('C');
      lastg += nodes->getLast('G');      
    }

    m_predCount.set('$', 0);
    m_predCount.set('A', lastdollar); 
    m_predCount.set('C', m_predCount.get('A') + lasta);
    m_predCount.set('G', m_predCount.get('C') + lastc);
    m_predCount.set('T', m_predCount.get('G') + lastg);
  }
}

void RLEBWT::serialiseFMIndex ( const string& filename ) {
  ofstream os;
  os.open(filename.c_str(), ios::out | ios::binary);

  const size_t depth = m_fmIndex.depth();
  os.write((char*)&depth, sizeof(depth));

  m_fmIndex.serialise(os);

  m_predCount.serialise(os);
  os.close();
}

void RLEBWT::deserialiseFMIndex ( const string& filename ) {
  ifstream is;
  is.open(filename.c_str(), ios::binary);

  size_t depth = 0;
  is.read((char*)&depth, sizeof(depth));

  for ( size_t i=0; i<depth; ++i ) {
    size_t bytes = 0;
    is.read((char*)&bytes, sizeof(bytes));

    switch ( bytes ) {
    case 2:
      m_fmIndex.push_back(shared_ptr<IBPNodes>(new BPNodes16(is)));
      break;
    case 4:
      m_fmIndex.push_back(shared_ptr<IBPNodes>(new BPNodes32(is)));
      break;
    case 8:
      m_fmIndex.push_back(shared_ptr<IBPNodes>(new BPNodes64(is)));
      break;
    default:
      break;
    }
  }

  size_t sz = 0;
  is.read((char*)&sz, sizeof(sz));

  m_fmIndex.setSumVector(shared_ptr< vector<uint32_t> >(new vector<uint32_t>()));
  vector<uint32_t>* pvSum = (m_fmIndex.getSumVector()).get();
  pvSum->resize(sz);

  is.read((char*)(&(*(pvSum->data()))), sz * sizeof(uint32_t));
  m_predCount = AlphaCount64(is);

  is.close();
}

char RLEBWT::getChar ( const uint64_t& index ) const {
  const size_t idx = index + 1;
  const Marker& mk = m_fmIndex.access(idx);
  const size_t begin = mk.index * m_smallSampleRate;
  size_t end = begin + m_smallSampleRate - 1;
  size_t offset = idx - mk.sum;
  const size_t length = m_rlString.size();

  if ( end >= length ) {
    end = length - 1;
  }

  for ( size_t i=begin; i<=end; ++i ) {
    const RLUnit& unit = m_rlString[i];
    size_t count = unit.getCount();

    if ( offset <= count ) {
      return unit.getChar();
    }
    else {
      offset -= count;
    }
  }

  return m_rlString[end].getChar();
}

uint64_t RLEBWT::getPC ( const char& b ) const {
  return m_predCount.get(b);
}

uint64_t RLEBWT::getOccAt ( const char& b, const uint64_t& bc ) const {
  const Marker& mk = m_fmIndex.select(b, bc);
  const size_t begin = mk.index * m_smallSampleRate;
  size_t end = begin + m_smallSampleRate - 1;
  size_t offset = bc - mk.count;
  const size_t length = m_rlString.size();

  if ( end >= length ) {
    end = length - 1;
  }

  size_t index = mk.sum;

  for ( size_t i=begin; i<=end; ++i ) {
    const RLUnit& unit = m_rlString[i];
    const size_t count = unit.getCount();

    if ( b != unit.getChar() ) {
      index += count;
      continue;
    }

    if ( offset <= count ) {
      index += offset - 1;
      break;
    }
    else {
      offset -= count;
      index += count;
    }
  }

  return index;
}

uint64_t RLEBWT::getOcc ( const char& b, const uint64_t& index ) const {
  const size_t idx = index + 1;
  const Marker& mk = m_fmIndex.rank(b, idx);
  const size_t begin = mk.index * m_smallSampleRate;
  size_t occ = mk.count;
  size_t offset = idx - mk.sum;
  const size_t length = m_rlString.size();
  size_t end = begin + m_smallSampleRate - 1;

  if ( end >= length ) {
    end = length - 1;
  }

  for ( size_t i=begin; i<=end; ++i ) {
    const RLUnit& unit = m_rlString[i];
    const size_t count = unit.getCount();

    if ( offset <= count ) {
      if ( b == unit.getChar() ) {
        occ += offset;
      }
      break;
    }
    else {
      offset -= count;

      if ( b == unit.getChar() ) {
        occ += count;
      }
    }
  }

  return occ;
}

uint64_t RLEBWT::getBWLen () const {
  return m_numSymbols;
}

char RLEBWT::getF ( const uint64_t& idx ) const {
  size_t ci = 0;
  while ( ci < ALPHABET_SIZE && m_predCount.getByIdx(ci) <= idx ) {
    ci++;
  }

  return RANK_ALPHABET[ci - 1];    
}
