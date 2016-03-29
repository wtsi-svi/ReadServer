#ifndef RS_ALPHABET_H
#define RS_ALPHABET_H

#include <fstream>
#include <stdint.h>
#include <string.h>

const uint8_t ALPHABET_SIZE = 5;
const char RANK_ALPHABET[ALPHABET_SIZE] = {'$', 'A', 'C', 'G', 'T'};

static const size_t s_bwtLexoRankLUT[256] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,1,0,2,0,0,0,3,0,0,0,0,0,0,0,0,
  0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

inline size_t getRank ( const char& b ) {
  return s_bwtLexoRankLUT[static_cast<uint8_t>(b)];
}
    
inline char getSymbol ( const uint8_t& idx ) {
  return RANK_ALPHABET[idx];
}

//
// A simple class holding the count for each base of a DNA string (plus the terminator)
// Note that this uses RANK_ALPHABET
template<typename Storage>
class AlphaCount {
 public:
  inline AlphaCount () {
    clear();
  }

  inline AlphaCount ( std::istream& is ) {
    is.read((char*)&(m_counts[0]), ALPHABET_SIZE * sizeof(Storage));
  }

  inline void clear () {
    memset(m_counts, 0, ALPHABET_SIZE * sizeof(Storage));
  }

  inline void set ( const char& b, const Storage& v ) {
    m_counts[getRank(b)] = v;
  }

  inline void setByIdx ( const size_t i, const Storage& v ) {
    m_counts[i] = v;
  }

  inline void add ( const char& b, const Storage& v ) {
    m_counts[getRank(b)] += v;
  }

  inline void addByIdx ( const size_t& i, const Storage& v ) {
    m_counts[i] += v;
  }

  inline Storage get ( const char& b ) const {
    return m_counts[getRank(b)];
  }

  inline Storage getByIdx ( const size_t& i ) const {
    return m_counts[i];
  }

  inline uint64_t getSum () const {
    uint64_t sum = 0;
    for ( uint64_t i=0; i<ALPHABET_SIZE; ++i ) {
      sum += m_counts[i];
    }

    return sum;
  }

  void serialise ( std::ostream& os ) const {
    os.write((char*)&(m_counts[0]), ALPHABET_SIZE * sizeof(Storage));
  }

 private:
  Storage m_counts[ALPHABET_SIZE];
};

// Typedef commonly used AlphaCounts
typedef AlphaCount<uint64_t> AlphaCount64;
typedef AlphaCount<uint32_t> AlphaCount32;
typedef AlphaCount<uint16_t> AlphaCount16;

#endif
