#include <string>
#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <future>

#include "rocksdb/db.h"
#include "zmq.hpp"
#include "libconfig.h++"
#include "readserver.pb.h"
#include "threadpool.h"

using namespace std;
using namespace libconfig;

#define MAX_READ_LENGTH 100
#define MIN_READ_LENGTH 73

const size_t query_thread_size = 32;

std::mutex mtx;

class ReopenDbTask : public ThreadPool::Job {
protected:
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  vector<string> rocksdbs;
  string rocksdb_ext;

public:
  ReopenDbTask ( map<string, shared_ptr<rocksdb::DB> >* db, const vector<string>& vs, const string& ext ) :
    dbs(db),
    rocksdbs(vs),
    rocksdb_ext(ext)
  {}

  virtual void run ( void * ) {
    while ( 1 ) { // run forever
      std::this_thread::sleep_for(std::chrono::milliseconds(  1 * 60 * 60 * 1000)); // 1 hours

      // open dbs
      for ( vector<string>::iterator it=rocksdbs.begin(); it!=rocksdbs.end(); ++it ) {
        const string db_path(*it + rocksdb_ext);
        rocksdb::DB* dbptr;
        rocksdb::Options options;
        rocksdb::Status status = rocksdb::DB::OpenForReadOnly(options, db_path, &dbptr);
        if ( !(status.ok()) ) {
          std::cout << status.ToString() << std::endl;
          assert(status.ok());
        }

        const shared_ptr<rocksdb::DB> sdb(dbptr);
        const shared_ptr<rocksdb::DB> pdb((*dbs)[*it]);
        (*dbs)[*it] = sdb;
        std::this_thread::sleep_for(std::chrono::milliseconds(  1 * 1 * 1 * 1000)); // wait for 1 second for things to settle down.
      }
    }
  }
};

class DbTask : public ThreadPool::Job {
protected:
  string query;
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  map<string, string>* hash;
  zmq::socket_t* sender;
  
public:
  DbTask ( const string& q, map<string, shared_ptr<rocksdb::DB> >* m, map<string, string>* h, zmq::socket_t* s ) :
    query(q),
    dbs(m),
    hash(h),
    sender(s)
  {}
  
  virtual void run ( void * ) {
    map<string, shared_ptr<rocksdb::DB> >::iterator db = dbs->find(string(query.rbegin(), query.rbegin()+3));
    if ( db == dbs->end() ) {
      cerr << "Could not find db for " << string(query.rbegin(), query.rbegin()+3) << endl;
      return;
    }

    string value;
    rocksdb::Status status = db->second->Get(rocksdb::ReadOptions(), query, &value);
    if ( !(status.ok()) ) {
      std::cout << status.ToString() << std::endl;
      assert(status.ok());
    }

    Reply r = Reply();
    r.set_rt(Reply::ExactMatch);
    r.set_t(static_cast<Reply_ReplyType>(Reply_ReplyType_ResultSamples));
    r.set_q(query);
    
    ResultSamples* result = r.mutable_s();
    size_t pos = 0;
    while(pos < value.size()) {
      ReadInfo* info = result->add_s();
      info->set_g((*hash)[value.substr(pos,2)]);
      pos += 2;
      info->set_c((int)(value[pos])-33);
      ++pos;
      info->set_l((int)(value[pos])-33);
      ++pos;
    }

    //  Send results to sink
    zmq::message_t message(r.ByteSize());
    r.SerializeToArray(message.data(), r.ByteSize());

    mtx.lock();
    sender->send(message);
    mtx.unlock();
  }
};

int main (int argc, char **argv) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if ( argc < 2 ) {
    cerr << "Require more arguments to run the programme." << endl;
    return(EXIT_FAILURE);
  }

  // Get all the configs.
  Config cfg;
  // Read the file. If there is an error, report it and exit.
  try
  {
    cfg.readFile(argv[1]);
  }
  catch(const FileIOException &fioex)
  {
    cerr << "I/O error while reading file." << endl;
    return EXIT_FAILURE;
  }
  catch(const ParseException &pex)
  {
    cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << endl;
    return EXIT_FAILURE;
  }

  // Configs from config file.
  string hashfile;
  string pull_socket;
  string push_socket;
  string rocksdb_ext;
  vector<string> rocksdb_paths;
  vector<string> rocksdbs;
  try
  {
    hashfile = cfg.lookup("hashfile").c_str();
    pull_socket = cfg.lookup("pull").c_str();
    push_socket = cfg.lookup("push").c_str();
    rocksdb_ext = cfg.lookup("rocksdb_ext").c_str();

    const Setting& s = cfg.lookup("rocksdb");
    const size_t length = s.getLength();
    for ( size_t i=0; i<length; ++i ) {
      rocksdbs.push_back(s[i].c_str());
    }

    const Setting& ps = cfg.lookup("rocksdb_path");
    const size_t len = ps.getLength();
    for ( size_t i=0; i<len; ++i ) {
      rocksdb_paths.push_back(ps[i].c_str());
    }
  }
  catch(const SettingNotFoundException &nfex)
  {
    cerr << "No 'name' setting in configuration file." << endl;
    return EXIT_FAILURE;
  }

  // Open dbs
  map<string, shared_ptr<rocksdb::DB> > dbs;
  vector<future<rocksdb::DB*> > fut_dbs;
  for ( vector<string>::iterator it=rocksdb_paths.begin(); it!=rocksdb_paths.end(); ++it ) {
    const string db_path(*it + rocksdb_ext);
    fut_dbs.push_back( async( launch::async, [ ] ( const string& db_path ) {
          rocksdb::DB* dbptr;
          rocksdb::Options options;
          rocksdb::Status status = rocksdb::DB::OpenForReadOnly(options, db_path, &dbptr);
          if ( !(status.ok()) ) {
            std::cout << status.ToString() << std::endl;
            assert(status.ok());
          }
          return dbptr;
        }, db_path ) );
  }

  // Get meta data mapping
  map<string, string> hash;
  string group;
  ifstream ifile(hashfile.c_str(), ios::in);
  while ( getline(ifile, group) ) {
    if ( group.size() < 1 ) {
      continue;
    }
    const size_t start = 0;
    const size_t pos = group.find("\t", start);
    if ( pos == string::npos ) {
      continue;
    }
    hash.insert(make_pair<string, string>(group.substr(pos+1), group.substr(start, pos)));
  }

  cout << "Got mappings" << endl;

  // Set up messaging channel
  zmq::context_t context(1);
  //  Socket to receive messages on
  zmq::socket_t receiver(context, ZMQ_SUB);
  for ( vector<string>::iterator it=rocksdbs.begin(); it!=rocksdbs.end(); ++it ) {
    receiver.setsockopt(ZMQ_SUBSCRIBE, it->c_str(), 3);
  }
  int hwm = 0;
  receiver.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
  receiver.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
  receiver.connect(pull_socket.c_str());

  //  Socket to send messages to
  zmq::socket_t sender(context, ZMQ_PUSH);
  sender.connect(push_socket.c_str());

  // Set up thread pool
  const unique_ptr<ThreadPool::ThreadPool> pool(new ThreadPool::ThreadPool(query_thread_size));

  for ( size_t i = 0; i < rocksdbs.size(); ++i ) {
    dbs.insert( pair<string, shared_ptr<rocksdb::DB> >(rocksdbs[i], shared_ptr<rocksdb::DB>(fut_dbs[i].get())) );
  }
  cout << "Opened dbs" << endl;

  shared_ptr<ReopenDbTask> rdbtask(new ReopenDbTask(&dbs, rocksdb_paths, rocksdb_ext));
  pool->run(rdbtask); // delete rdbtask after finish job.

  cout << "ready to serve." << endl;

  //  Process tasks forever
  while (1) {
    // Receive a message
    zmq::message_t message;
    if ( !(receiver.recv(&message)) ) {
      cerr << "failed to receive" << endl;
    }

    // Create Request object from message
    Request w;
    w.ParseFromArray((static_cast<char*>(message.data()))+4, message.size()-4);

    const string& query = w.q();

    if ( query.size() != MIN_READ_LENGTH && query.size() != MAX_READ_LENGTH ) {
      cout << "query length is not right: " << query  << "\t" << string((const char*)(message.data()), message.size()) << endl;
      continue;
    }
    if ( query.find_first_not_of("ACGT") != string::npos ) {
      cout << "query contain non-acgt character: " << query << endl;
      continue;
    }

    switch ( w.t() ) {
    case Request_RequestType_ExactMatch: {
      switch ( w.rt() ) {
      default: // Assign to thread pool.
        shared_ptr<DbTask> qtf(new DbTask(query, &dbs, &hash, &sender));
        pool->run(qtf); // delete qtf after finish job.
        break;
      }

      break;
    }

    default:
      cout << "unrecognised request type." << endl;
      break;
    }    

  }
  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  return EXIT_SUCCESS;
}

