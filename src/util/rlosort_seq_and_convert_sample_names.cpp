//
// Main programme to rlo sort reads and convert their associated ids into encoding values
// e.g. ./rlosort_seq_and_convert_sample_names reads.ids.in sample.names.hash file.out
//

#include <string>
#include <fstream>
#include <map>
#include <vector>
#include <unordered_map>

#include "split.h"

using namespace std;

struct rlocomp {
  bool operator() ( const string& first, const string& second ) {
    return string(first.rbegin(), first.rend()) < string(second.rbegin(), second.rend());
  }
};

int main (int argc, char **argv) {
  if ( argc < 4 ) {
    cerr << "Require more arguments for rlosort_seq_and_convert_sample_names." << endl;
    return EXIT_FAILURE;
  }

  ifstream ifile(argv[2], ios::in); // A list of sample names hash
  string line;
  unordered_map<string, string> umss;
  while ( getline(ifile, line) ) {
    if ( line.size() > 0 ) {
      const vector<string>& vs = split(line, '\t');
      if ( vs.size() ==  2 ) {
        umss[vs[0]] = vs[1];
      }
    }
  }
  ifile.close();

  ifile.open(argv[1], ios::in); // input file contains seqs and ids
  map<string, string, rlocomp> data;
  string id;
  string seq;
  while ( getline(ifile, seq) && getline(ifile, id) ) {
    if ( id.size() > 0 && seq.size()>0 ) {
      const map<string, string>::iterator it = data.find(seq);
      if ( it == data.end() ) {
        data[seq] = umss[id];
      }
      else {
        data[seq].append(umss[id]);
      }
    }
  }
  ifile.close();
  
  const string linebreak("\n");
  ofstream ofile(argv[3], ios::out); // output rlosorted seqs & ids

  for ( map<string, string>::const_iterator it=data.begin(); it!=data.end(); ++it ) {
    ofile << it->first << linebreak << it->second << linebreak;
  }

  ofile.close();

  return EXIT_SUCCESS;
}
