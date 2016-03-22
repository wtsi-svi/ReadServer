#ifndef RS_BPNODES_H
#define RS_BPNODES_H

#include <vector>
#include <limits>
#include <iostream>
#include <memory>

#include "alphabet.h"

class IBPNodes {
 public:
  virtual size_t getLength () const = 0 ;
  virtual size_t getBucketSize () const = 0;
  virtual size_t getBlockSize () const = 0;
  virtual size_t getLast ( const char& b ) const = 0;
  virtual size_t getLastByIdx ( const size_t& i ) const = 0;

  virtual void appendLast () = 0;
  virtual void addToLast ( const char& b, const uint8_t& len ) = 0;
  virtual void addToLast ( const IBPNodes* rhs ) = 0;
  virtual void clearLast () = 0;


  virtual size_t sum ( const size_t& index ) const = 0;
  virtual size_t count ( const char& b, const size_t& index ) const = 0;
  virtual size_t access ( const size_t& cnt, const size_t& offset ) const = 0;
  virtual size_t access ( const size_t& cnt, const size_t& lower, const size_t& upper ) const = 0;
  virtual size_t select ( const char& b, const size_t& cnt, const size_t& offset ) const = 0;

  virtual void serialise ( std::ostream& os ) const = 0;
};

template< typename IntType >
class BPNodes : public IBPNodes {
 public:

  BPNodes ( const size_t& l, const size_t& bucket, const size_t& block )
    : m_length(0),
      m_block_size(block),
      m_bucket_size(bucket) {
    m_array.reset(new std::vector< AlphaCount<IntType> >);
    m_array->clear();
    m_array->reserve(l);

    m_sum.reset(new std::vector< IntType >);
    m_sum->clear();
    m_sum->reserve(l);
  }

  BPNodes ( std::istream& is )
    : m_length(0),
      m_block_size(0),
      m_bucket_size(0) {
    is.read((char*)&m_length, sizeof(m_length));
    is.read((char*)&m_block_size, sizeof(m_block_size));
    is.read((char*)&m_bucket_size, sizeof(m_bucket_size));

    m_array.reset(new std::vector< AlphaCount<IntType> >);
    m_array->clear();
    m_array->resize(m_length);
    is.read((char*)(&(*(m_array->data()))), m_length * sizeof(AlphaCount<IntType>) );

    m_sum.reset(new std::vector< IntType >);
    m_sum->clear();
    m_sum->resize(m_length);
    is.read((char*)(m_sum->data()), m_length * sizeof(IntType) );
  }

  inline size_t getLength() const {
    return m_length;
  }  

  inline size_t getBucketSize () const {
    return m_bucket_size;
  }

  inline size_t getBlockSize () const {
    return m_block_size;
  }

  inline size_t getLast ( const char& b ) const {
    return m_last.get(b);
  }

  inline size_t getLastByIdx ( const size_t& i ) const {
    return m_last.getByIdx(i);
  }

  inline void appendLast () {
    m_array->push_back(m_last);
    m_sum->push_back(m_last.getSum());
    ++m_length;
  }

  inline void addToLast ( const char& b, const uint8_t& len ) {
    m_last.add(b, len);
  }

  inline void addToLast ( const IBPNodes* rhs ) {
    for ( size_t i=0; i<ALPHABET_SIZE; ++i ) {
      m_last.addByIdx(i, rhs->getLastByIdx(i));
    }
  }

  inline void clearLast() {
    m_last.clear();
  }

  inline size_t sum ( const size_t& index ) const {
    return (*m_sum)[index];
  }

  inline size_t count ( const char& b, const size_t& index ) const {
    return get_count(b, index);
  }

  inline size_t access ( const size_t& cnt, const size_t& lower, const size_t& upper ) const {
    if ( lower == upper ) {
      return lower;
    }

    return get_access(cnt, lower, upper);
  }

  inline size_t access ( const size_t& cnt, const size_t& offset ) const {
    const size_t begin = m_block_size * offset;
    const size_t end = m_block_size + begin - 1;

    return get_access(cnt, begin, end);
  }
  
  inline size_t get_access ( const size_t& cnt, const size_t& lower, const size_t& upper ) const {
    size_t begin = lower;
    size_t end = upper;

    if ( end > m_length - 1 ) {
      end = m_length - 1;
    }

    size_t p = 0;
    while ( end > begin ) {
      p =  ( begin + end + 1) >> 1;

      if ( get_sum(p) >= cnt ) {
        if ( end == p ) {
          break;
        }

        end = p;
      }
      else {
        if ( begin == p ) {
          break;
        }

        begin = p;
      }
    }

    return begin;
  }

  inline size_t select ( const char& b, const size_t& cnt, const size_t& offset ) const {
    size_t begin = m_block_size * offset;
    size_t end = m_block_size + begin - 1;

    if ( cnt <= 0 ) {
      return begin;
    }

    if ( end > m_length - 1 ) {
      end = m_length - 1;
    }

    while ( end > begin ) {
      const size_t p = ( begin + end + 1 ) >> 1;

      if ( get_count(b, p) >= cnt ) {
        if ( end == p ) {
          break;
        }

        end = p;
      }
      else {
        if ( begin == p ) {
          break;
        }

        begin = p;
      }
    }

    return begin;
  }

  void serialise ( std::ostream& os ) const {
    const size_t sz = sizeof(IntType);
    os.write((char*)&sz, sizeof(sz));
    os.write((char*)&m_length, sizeof(m_length));
    os.write((char*)&m_block_size, sizeof(m_block_size));
    os.write((char*)&m_bucket_size, sizeof(m_bucket_size));

    for ( typename std::vector< AlphaCount<IntType> >::iterator it=m_array->begin(); it!=m_array->end(); ++it ) {
      it->serialise(os);
    }

    IntType tmp = 0;
    for ( typename std::vector< IntType >::iterator it=m_sum->begin(); it!=m_sum->end(); ++it ) {
      tmp = *it;
      os.write((char*)&tmp, sz);
    }
  }
  
 private:
  
  BPNodes () {}
  
  BPNodes ( const BPNodes& rhs ) {}

  inline size_t get_sum ( const size_t& index ) const {
    return (*m_sum)[index];
  }

  inline size_t get_count ( const char& b, const size_t& index ) const {
    return (*m_array)[index].get(b);
  }

  size_t m_length; // number of buckets in the whole array.

  size_t m_block_size; // number of buckets in a block (each interval at upper level nodes)

  size_t m_bucket_size; // number of BWT chars per bucket

  AlphaCount<IntType> m_last; // copy of last element in m_array  

  std::shared_ptr< std::vector< AlphaCount<IntType> > > m_array;  

  std::shared_ptr< std::vector< IntType > > m_sum;  
}; 

typedef BPNodes<uint16_t> BPNodes16;
typedef BPNodes<uint32_t> BPNodes32;
typedef BPNodes<uint64_t> BPNodes64;

#endif

