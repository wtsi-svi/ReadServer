//
// Main programme to merge two rlo sorted files
// e.g. ./merge_rlosorted_reads file.in1 file.in2 file.out
//

#include <string>
#include <fstream>

#include "merge_sorted_items.h"

using namespace std;

class RLOSequenceCompare {
 public:
  CompareResult operator() ( const ReadInfo& first, const ReadInfo& second ) const {
    if ( first.sequence == second.sequence ) {
      return CompareResult::Equal;
    }

    if ( string(first.sequence.rbegin(), first.sequence.rend()) < string(second.sequence.rbegin(), second.sequence.rend()) ) {
      return CompareResult::Smaller;
    }

    return CompareResult::Greater;
  }
};

class TwoLineReader {
 public:
  bool read ( ifstream& ifs, ReadInfo& ri ) const {
    if ( getline(ifs, ri.sequence) && getline(ifs, ri.ids) ) {
      return true;
    }

    return false;
  }
};

class TwoLineWriter {
 public:
  string write ( const ReadInfo& ri ) const {
    return ri.sequence + "\n" + ri.ids + "\n";
  }

  string write ( const ReadInfo& ri1, const ReadInfo& ri2 ) const {
    return ri1.sequence + "\n" + ri1.ids + ri2.ids + "\n";
  }
};

int main (int argc, char **argv) {
  if ( argc < 4 ) {
    cerr << "Require more arguments for merge_rlosorted_reads." << endl;
    return EXIT_FAILURE;
  }

  const string ifile1(argv[1]);
  const string ifile2(argv[2]);
  const string ofile(argv[3]);

  MergeSortedItems<ReadInfoReader<TwoLineReader>, ReadInfoWriter<TwoLineWriter>, RLOSequenceCompare> merger(ifile1, ifile2, ofile);
  merger.merge();

  return EXIT_SUCCESS;
}


