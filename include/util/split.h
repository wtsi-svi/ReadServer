#ifndef __SPLIT_H
#define __SPLIT_H

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iterator>

bool empty ( const std::string& s ) {
  return s.empty();
}

std::vector<std::string> split ( const std::string& s, const char& delimiter ) {
  std::vector<std::string> tokens;

  size_t start = 0;
  size_t pos = 0;
  while ( start < s.size() ) {
    pos = s.find_first_of(delimiter, start);
    if ( pos != std::string::npos ) {
      tokens.push_back(s.substr(start, pos-start));
      start = pos + 1;
    }
    else {
      tokens.push_back(s.substr(start));
      break;
    }
  }

  std::vector<std::string>::iterator iter = std::remove_if(tokens.begin(), tokens.end(), empty);
  tokens.erase(iter, tokens.end());

  return tokens;
}

std::string join ( const std::vector<std::string>& vs, const char& delimiter ) {
  switch ( vs.size() ) {
  case 0:
    return "";
  case 1:
    return vs[0];
  default:
    const std::string s(1, delimiter);
    std::ostringstream os;
    std::copy(vs.begin(), vs.end()-1, std::ostream_iterator<std::string>(os, s.c_str()));
    os << *vs.rbegin();
    return os.str();    
  }
}

#endif
