//
// Main programme to load data into rocksdb.
// e.g. ./load_data_into_rocksdb reads.ids.in rocksdb.name
//

#include <string>
#include <fstream>
#include <iostream>

#include "rocksdb/db.h"
#include "rocksdb/write_batch.h"


using namespace std;
using namespace rocksdb;

int main (int argc, char **argv) {
  if ( argc < 3 ) {
    cerr << "Require more arguments for load_data_into_rocksdb." << endl;
    return EXIT_FAILURE;
  }

  // Open database for write
  DB* db;
  Options options;
  options.create_if_missing = true;
  Status status = DB::Open(options, argv[2], &db);

  if ( !(status.ok()) ) {
    cerr << status.ToString() << endl;
    delete db;
    return EXIT_FAILURE;
  }

  const size_t BUFFER_SIZE = 1000;
  size_t count = 0;
  WriteBatch batch;

  ifstream ireads(argv[1], ios::in);
  string ids;
  string read;


  while ( getline(ireads, read) && getline(ireads, ids)) {
    if ( count < BUFFER_SIZE ) {
      batch.Put(read, ids);
      ++count;
    }

    if ( count == BUFFER_SIZE ) {
      Status status = db->Write(WriteOptions(), &batch);
      
      if ( !(status.ok()) ) {
        cerr << "rocksdb batch write error!\n" << endl;
        delete db;
        return EXIT_FAILURE;
      }

      batch.Clear();
      count = 0;
    }
  }

  if ( count > 0 ) {
      Status status = db->Write(WriteOptions(), &batch);
      
      if ( !(status.ok()) ) {
        cerr << "rocksdb batch write error!\n" << endl;
        delete db;
        return EXIT_FAILURE;
      }

      batch.Clear();
      count = 0;
  }

  CompactRangeOptions coptions;
  coptions.change_level = false;
  coptions.target_level = -1;
  coptions.target_path_id = 0;
  db->CompactRange(coptions, NULL, NULL);
  
  delete db;
  return EXIT_SUCCESS;
}
