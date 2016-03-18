#ifndef RS_BWT_QUERY_H
#define RS_BWT_QUERY_H

#include <stdint.h>
#include <vector>
#include <string>

struct BWTInterval {
  uint64_t lower;
  uint64_t upper;
};

class BWT;

// Update the given interval using backwards search
// If the interval corrsponds to string S, it will be updated 
// for string bS
void updateInterval(BWTInterval& interval, const char& b, const BWT* pBWT);
  
// Initialize the interval of index idx to be the range containining all the b suffixes
void initInterval(BWTInterval& interval, const char& b, const BWT* pBWT);

// get the interval(s) in pBWT/pRevBWT that corresponds to the string w using a backward search algorithm
BWTInterval findInterval(const BWT* pBWT, const std::string& w);

std::string extractPrefix ( const BWT* pBWT, const size_t& index );

std::string extractPostfix ( const BWT* pBWT, const size_t& index );

std::vector<std::string> query ( const BWT* pBWT, const std::string& w );

bool query_exactmatch ( const BWT* pBWT, const std::string& w );

#endif
