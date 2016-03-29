//
// Main programme to create map between samples and byte representation (that is
// going to be stored in database)
// e.g. ./create_samples_hash samples.names.in samples.hash.out
//

#include <fstream>
#include <set>
#include <string>

using namespace std;

int main ( int argc, char **argv ) {
  if ( argc < 3 ) {
    cerr << "Require more arguments for create_samples_hash." << endl;
    return EXIT_FAILURE;
  }

  set<string> samples;
  string sample;
  ifstream ifile(argv[1], ios::in);
  while ( getline(ifile, sample) ) {
    if ( sample.size() < 1 ) {
      continue;
    }

    samples.insert(sample);
  }

  const size_t offset = 33; // avoid first 33 chars
  const size_t numOfKeys = samples.size();
  size_t numOfBytesPerValue = 0;
  int remainder = numOfKeys;
  int divider = 256 - offset;
  while ( remainder > 0 ) {
    ++numOfBytesPerValue;
    remainder = remainder / divider;
  }

  ofstream ofile(argv[2], ios::out);
  size_t curr_value = 0;
  string value(numOfBytesPerValue, '\0');
  for ( const string& sample : samples ) {
    ++curr_value;
    remainder = curr_value;
    for ( size_t i=1; i<=numOfBytesPerValue; ++i ) {
      value[numOfBytesPerValue-i] = (uint8_t)(remainder % divider + offset);
      remainder = remainder / divider;
    }
    ofile << sample << "\t" << value << "\n";
  }

  return EXIT_SUCCESS;
}

