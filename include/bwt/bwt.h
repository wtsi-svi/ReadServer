#ifndef RS_BWT_H
#define RS_BWT_H

#include <stdint.h>

class BWT {
 public:
  virtual char getChar ( const uint64_t& index ) const = 0;
  virtual uint64_t getOccAt ( const char& b, const uint64_t& index ) const = 0;    
  virtual uint64_t getOcc ( const char& b, const uint64_t& index ) const = 0;

  virtual char getF ( const uint64_t& index ) const = 0;
  virtual uint64_t getPC ( const char& b ) const = 0;
  virtual uint64_t getBWLen () const = 0;
};

#endif
