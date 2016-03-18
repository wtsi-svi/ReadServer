#include <algorithm>
#include <utility>
#include "query.h"
#include "bwt.h"

using namespace std;

// Update the given interval using backwards search
// If the interval corrsponds to string S, it will be updated 
// for string bS
void updateInterval ( BWTInterval& interval, const char& b, const BWT* pBWT ) {
  const size_t& pb = pBWT->getPC(b);
  interval.lower = pb + pBWT->getOcc(b, interval.lower - 1);
  interval.upper = pb + pBWT->getOcc(b, interval.upper) - 1;
}
  
// Initialize the interval of index idx to be the range containining all the b suffixes
void initInterval ( BWTInterval& interval, const char& b, const BWT* pBWT ) {
  interval.lower = pBWT->getPC(b);
  interval.upper = interval.lower + pBWT->getOcc(b, pBWT->getBWLen() - 1) - 1;
}

// get the interval(s) in pBWT/pRevBWT that corresponds to the string w using a backward search algorithm
BWTInterval findInterval ( const BWT* pBWT, const string& w ) {
  const size_t len = w.size();
  int j = len - 1;
  char curr = w[j];
  BWTInterval interval;
  initInterval(interval, curr, pBWT);
  --j;

  for ( ; j >= 0; --j ) {
    curr = w[j];
    updateInterval(interval, curr, pBWT);
    if ( interval.lower > interval.upper ) {
      return interval;
    }
  }

  return interval;
}

string extractPrefix ( const BWT* pBWT, const size_t& index ) {
  string prefix;
  size_t idx = index;
  char b;

  while ( 1 ) {
    b = pBWT->getChar(idx);
    idx = pBWT->getPC(b) + pBWT->getOcc(b, idx-1);

    if ( b == '$' ) {
      break;
    }
    else {
      prefix.push_back(b);
    }
  }

  reverse(prefix.begin(), prefix.end());

  return prefix; 
}

string extractPostfix ( const BWT* pBWT, const size_t& index ) {
  string postfix;
  size_t idx = index;
  char f;
  uint64_t fc;

  while ( 1 ) {
    f = pBWT->getF(idx);
    fc = idx - pBWT->getPC(f) + 1;
    idx = pBWT->getOccAt(f, fc);

    if ( f == '$') {
      break;
    }
    else {
      postfix.push_back(f);
    }
  }

  return postfix; 
}

vector<string> query ( const BWT* pBWT, const string& w ) {
  vector<string> seqs;

  if ( w.find_first_not_of("ACGT") != string::npos ) {
    return seqs;
  }

  BWTInterval interval = findInterval(pBWT, w);
  for ( size_t i=interval.lower; i<=interval.upper; ++i ) {
    seqs.push_back( std::move(extractPrefix(pBWT, i) + extractPostfix(pBWT, i)) );
  }

  return seqs;
}

bool query_exactmatch ( const BWT* pBWT, const string& w ) {
  if ( w.find_first_not_of("ACGT") != string::npos ) {
    return false;
  }

  BWTInterval interval = findInterval(pBWT, w);

  if ( interval.lower > interval.upper ) {
    return false;
  }

  for ( size_t i=interval.lower; i<=interval.upper; ++i ) {
    if ( w == extractPrefix(pBWT, i) + extractPostfix(pBWT, i) ) {
      return true;
    }
  }

  return false;
}
