//
// Main programme to create index structure for run-length encoded BWT string.
// e.g. ./index_rlebwt rlebwt.in
//

#include <string>
#include <memory>

#include "rlebwt.h"

using namespace std;

int main (int argc, char **argv) {
  if ( argc < 2 ) {
    cerr << "Require more arguments for index_rlebwt." << endl;
    return EXIT_FAILURE;
  }

  const string bwt_filename(argv[1]);
  const string idx_filename(bwt_filename+".bpi2");
  auto_ptr<RLEBWT> pbwt(new RLEBWT(string(argv[1])));
  pbwt->serialiseFMIndex(idx_filename);

  return EXIT_SUCCESS;
}

