#include <string>
#include <vector>
#include <unordered_set>
#include <map>
#include <memory>
#include <mutex>
#include <algorithm>
#include <utility>
#include <fstream>
#include <future>
#include <iterator>
#include "stdio.h"

#include <chrono>
#include <thread>

#include <signal.h>
#include <stdlib.h>


#include "rocksdb/db.h"
#include "zmq.hpp"
#include "libconfig.h++"

#include "threadpool.h"
#include "ksw.h"

#include "bwt.h"
#include "rlebwt.h"
#include "query.h"
#include "readserver.pb.h"

#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

void handler(int sig) {
  void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}


using namespace std;
using namespace libconfig;

#define MAX_READ_LENGTH 100
#define MIN_READ_LENGTH 73

size_t sizeofSample = 2;
bool hasOtherMetaData = true;

unsigned char seq_nt4_table[256] = {
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  3, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 0, 4, 1,  4, 4, 4, 2,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  3, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4, 
	4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4,  4, 4, 4, 4
};

const int residue_type = 5;
const int gapo = 6; // gap opening penalty
const int gape = 1; // gap extension penalty

const size_t async_chunk_size = 96;
const size_t large_match_size = 2048;
const size_t large_align_size = 256;
const size_t query_thread_size = 8;
const size_t db_thread_size = 64;

const size_t max_interval_size = 10000; // 10,000

std::mutex mtx;
std::mutex count_mtx;

const unique_ptr<ThreadPool::ThreadPool> pool(new ThreadPool::ThreadPool(query_thread_size));
const unique_ptr<ThreadPool::ThreadPool> dbpool(new ThreadPool::ThreadPool(db_thread_size));

enum class Strand { Forward, RevComp };

class MatchTask;
vector<string> find_reads ( BWT* pBWT, const string& w, const string& s = "" );
unordered_set<string> find_gt_reads ( const BWT* pBWT, const string& w, const size_t& index, const size_t& kmers, const size_t& skip, const size_t& pos );

bool align_gt_read ( const string& read, const string& target, const size_t pos, const string& alt, const bool isalt ) {
  // initialize scoring matrix
  int8_t mat[25];
  int sa = 1, sb = 4, i = 0, j = 0, k = 0;
  for (i = k = 0; i < 4; ++i) {
    for (j = 0; j < 4; ++j)
      mat[k++] = i == j? sa : -sb;
    mat[k++] = -1; // ambiguous base
  }
  for (j = 0; j < 5; ++j) mat[k++] = -1;

  const size_t length = target.size();
  unsigned char ref[length];
  for ( size_t i=0; i<length; ++i ) {
    ref[i] = seq_nt4_table[(size_t)(target[i])];
  }

  unsigned char altseq[length];
  for ( size_t i=0; i<length; ++i ) {
    altseq[i] = ref[i];
  }
  altseq[pos-1] = alt[0]; // should check boundary

  const size_t read_len = read.size();
  unsigned char read_seq[read_len];
  for ( size_t i=0; i<read_len; ++i ) {
    read_seq[i] = seq_nt4_table[(size_t)(read[i])];
  }

  kswr_t r = ksw_align(read_len, (uint8_t*)(&read_seq), length, (uint8_t*)(&ref), residue_type, mat, gapo, gape, KSW_XSTART, 0);
  kswr_t ra = ksw_align(read_len, (uint8_t*)(&read_seq), length, (uint8_t*)(&altseq), residue_type, mat, gapo, gape, KSW_XSTART, 0);

  if ( isalt ) { // looking for alt
    if ( ra.score >= r.score ) { // align better to ref
      return false;
    }
  }
  else { // looking for ref
    if ( r.score < ra.score ) { // align better to alt
      return false;
    }
  }

  // check alignment position at target
  const size_t pos_index = pos > 0 ? ( pos - 1 ) : 0;
  if ( r.tb > (int)pos_index || r.te < (int)pos_index ) { // no match at pos
    return false;
  }

  const int tgtbases = r.te + 1 - r.tb;
  const int qrybases = r.qe + 1 - r.qb;
  const int indels = qrybases - tgtbases; // >0 insertions, <0 deletions

  if ( read_len - qrybases > 4 ) { // have more than 4 mismatches on either end or both ends.
    return false;
  }

  // check alignment score
  if ( indels == 0 ) { // assuming no indels ( possiblly insertion + deletion )
    const int matches = read_len - 3;
    const int score = matches - 3 * 4;
    if ( r.score < score ) { // absolute minimum, allowing 3 mismatches
      return false;
    }
  }
  else if ( indels > 0 ) { // insertions
    const int matches = read_len - indels - 2;
    const int score = matches - 2 * 4 - (6 + indels - 1);
    if ( r.score < score ) { // allowing 1 indels + 2 mismatches
      return false;
    }
  }
  else { // deletions
    const int matches = read_len - 2;
    const int score = matches - 2 * 4 - (6 + indels - 1);
    if ( r.score < score ) { // allowing 1 indels + 2 mismatches
      return false;
    }
  }

  // check gt position.

  if ( tgtbases == qrybases ) {
    // no indels as insertion(s) + deletion(s) will be heavily penalised.
    // should contain only mismatches.
    if ( target[pos_index] != read[r.qb+pos_index-r.tb] ) {
      // mismatch on gt position
      return false;
    }
  }
  else { // contain indels
    if ( r.te - pos_index > pos_index - r.tb ) { // more bases align on the right
      int sub_read_len = r.te - pos_index + 1;
      unsigned char sub_target[sub_read_len];
      for ( int i=0; i<sub_read_len; ++i ) {
        sub_target[i] = ref[pos_index+i];
      }
      // now read becomes target
      kswr_t rr = ksw_align(sub_read_len, (uint8_t*)(&sub_target), read_len, (uint8_t*)(&read_seq), residue_type, mat, gapo, gape, KSW_XSTART, 0);                       

      if ( rr.qb != 0 ) { // not aligned at pos_index
        return false;
      }
    }
    else {
      int sub_read_len = pos_index - r.tb + 1;
      unsigned char sub_target[sub_read_len];
      for ( int i=0; i<sub_read_len; ++i ) {
        sub_target[i] = ref[r.tb+i];
      }
      // now read becomes target
      kswr_t rr = ksw_align(sub_read_len, (uint8_t*)(&sub_target), read_len, (uint8_t*)(&read_seq), residue_type, mat, gapo, gape, KSW_XSTART, 0);

      if ( rr.qe+1 != sub_read_len ) { // not aligned at pos_index
        return false;
      }
    }
  }

  return true;
}

// if w ends with s.
inline bool is_suffix_of ( const string& s, const string& w ) {
  return w.size() >= s.size() && equal(s.rbegin(), s.rend(), w.rbegin());
}

unordered_set<string> get_tiles ( const string& w, const size_t& kmer, const size_t& skip ) {
  unordered_set<string> vs;

  const size_t length = w.size();
  if ( length < kmer ) {
    return vs;
  }

  for ( size_t i=0; i<=length-kmer; ) {
    vs.insert(std::move(w.substr(i, kmer)));
    i += skip + 1;
  }

  return vs;
}

unordered_set<string> get_tiles ( const string& w, const size_t& sz ) {
  return get_tiles(w, sz, 0);
}

// reverse complement
string rev_comp ( const string& w ) {
  string s(w.rbegin(), w.rend());

  for ( string::iterator it=s.begin(); it!=s.end(); ++it ) {
    switch (*it) {
    case 'A':
      *it = 'T';
      break;
    case 'T':
      *it = 'A';
      break;
    case 'C':
      *it = 'G';
      break;
    case 'G':
      *it = 'C';
      break;
    default:
      break;
    }
  }

  return s;
}

// Find the interval for matched reads and send a message over to consumer.
void count_reads ( BWT* pbwt, zmq::socket_t* sender, const string& q, const Strand& dir, const Reply_RequestType& rt ) {
  Reply count = Reply();
  count.set_rt(rt);
  count.set_t(Reply_ReplyType_ReplyCount);
  count.set_q(q); // Use original query string here.

  ReplyCount * rc = count.mutable_c();
  ResultCount * resultc = NULL;

  string query = q;
  if ( dir == Strand::RevComp ) {
    query = rev_comp(query);
    resultc = rc->mutable_revcomp_matches();
  }
  else {
    resultc = rc->mutable_forward_matches();
  }

  // Do the work
  {
    if ( query.find_first_not_of("ACGT") != string::npos ) {
      resultc->set_c(0);
    }
    else {
      BWTInterval itv = findInterval(pbwt, query);
      resultc->set_c( itv.upper >= itv.lower ? (itv.upper - itv.lower + 1) : 0 );
    }

    //  Send results to sink
    zmq::message_t message(count.ByteSize());
    count.SerializeToArray(message.data(), count.ByteSize());

    mtx.lock();
    sender->send(message);
    mtx.unlock();
  }
}

class ReopenDbTask : public ThreadPool::Job {
protected:
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  vector<string> rocksdbs;
  string rocksdb_path;
  string rocksdb_ext;

public:
  ReopenDbTask ( map<string, shared_ptr<rocksdb::DB> >* db, const vector<string>& vs, const string& path, const string& ext ) :
    dbs(db),
    rocksdbs(vs),
    rocksdb_path(path),
    rocksdb_ext(ext)
  {}

  virtual void run ( void * ) {
    while ( 1 ) { // run forever
      std::this_thread::sleep_for(std::chrono::milliseconds(  2 * 60 * 60 * 1000)); // 2 hours

      // open dbs
      for ( vector<string>::iterator it=rocksdbs.begin(); it!=rocksdbs.end(); ++it ) {
        const string db_path(rocksdb_path + *it + rocksdb_ext);
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

class ExtractTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  BWTInterval itv;
  shared_ptr<vector<string> > seqs;
  shared_ptr<promise<int> > p;

public:
  ExtractTask ( BWT* bwt, const BWTInterval& i, const shared_ptr<vector<string> >& vs, const shared_ptr<promise<int> >& pp ) :
    pbwt(bwt),
    itv(i),
    seqs(vs),
    p(pp)
  {}
  
  virtual void run ( void * ) {
    for ( size_t i=itv.lower; i<=itv.upper; ++i ) {
      seqs->push_back( std::move(extractPrefix(pbwt, i) + extractPostfix(pbwt, i)) );
    }

    p->set_value(1);
  }
};

class MatchTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  string w;
  shared_ptr<vector<string> > seqs;
  shared_ptr<promise<int> > p;

public:
  MatchTask ( BWT* bwt, const string& q, const shared_ptr<vector<string> >& vs, const shared_ptr<promise<int> >& pp ) :
    pbwt(bwt),
    w(q),
    seqs(vs),
    p(pp)
  {}
  
  virtual void run ( void * ) {
    *seqs = find_reads(pbwt, w, "");
    p->set_value(1);
  }
};

class FindGtReadsTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  string query;
  size_t index;
  size_t kmers;
  size_t skip;
  size_t pos;
  shared_ptr<unordered_set<string> > seqs;
  shared_ptr<promise<int> > p;

public:
  FindGtReadsTask ( BWT* bwt, const string& w, const size_t i, const size_t k, const size_t s, const size_t po, const shared_ptr<unordered_set<string> >& vs, const shared_ptr<promise<int> >& pp ) :
    pbwt(bwt),
    query(w),
    index(i),
    kmers(k),
    skip(s),
    pos(po),
    seqs(vs),
    p(pp)
  {}
  
  virtual void run ( void * ) {
    *seqs = find_gt_reads(pbwt, query, index, kmers, skip, pos);
    p->set_value(1);
  }
};

class AlignGtReadsTask : public ThreadPool::Job {
protected:
  shared_ptr<vector<string> > reads;
  string query;
  size_t pos;
  string alt;
  bool isalt;
  shared_ptr<vector<string> > seqs;
  shared_ptr<promise<int> > p;

public:
  AlignGtReadsTask ( const shared_ptr<vector<string> >& svs, const string& w, const size_t po, const string& a, const bool isa, const shared_ptr<vector<string> >& vs, const shared_ptr<promise<int> >& pp ) :
    reads(svs),
    query(w),
    pos(po),
    alt(a),
    isalt(isa),
    seqs(vs),
    p(pp)
  {}
  
  virtual void run ( void * ) {
    for ( vector<string>::const_iterator it=reads->begin(); it!=reads->end(); ++it ) {
      if ( align_gt_read(*it, query, pos, alt, isalt) ) {
        seqs->push_back(*it);
      }
    }

    p->set_value(1);
  }
};

// Find reads that matches one of the kmers from the query string.
unordered_set<string> find_kmer_reads ( BWT* pBWT, const string& w, const size_t& kmers, const size_t& skip, ThreadPool::ThreadPool* dbpool ) {
  unordered_set<string> seqs;
  vector<shared_ptr<promise<int> > > vpromises;
  vector<shared_ptr<vector<string> > > vvseqs;

  const unordered_set<string> tiles = get_tiles(w, kmers, skip);
  const size_t vsize = tiles.size();
  size_t count = 0;

  for ( unordered_set<string>::const_iterator it = tiles.begin(); it != tiles.end(); ++it ) {
    ++count;
    if ( it->find_first_not_of("ACGT") != string::npos ) {
      continue;
    }

    if ( count < vsize ) {
      shared_ptr<promise<int> > p = make_shared<promise<int> >();
      vpromises.push_back(p);
      shared_ptr<vector<string> > vs = make_shared<vector<string> >();
      vvseqs.push_back(vs);
      shared_ptr<MatchTask> mtask(new MatchTask(pBWT, *it, vs, p));
      dbpool->run(mtask); // delete mtask after finish job.
      continue;
    }

    const vector<string> vseqs = find_reads(pBWT, *it);
    seqs.insert(vseqs.begin(), vseqs.end());
  }

  for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );

  for ( auto &ptr : vvseqs ) {
    seqs.insert(ptr->begin(), ptr->end());
  }

  return seqs;
}

// Find reads that:
// 1. matches the kmer string (w).
// 2. spans over the position pos (allowing indels).
unordered_set<string> find_gt_reads ( const BWT* pBWT, const string& w, const size_t& index, const size_t& kmers, const size_t& skip, const size_t& pos ) {
  unordered_set<string> seqs;
  const size_t indel_allowance = 4;
  const size_t length = w.size();

  string kmer = w.substr((skip+1)*index, kmers);
  if ( kmer.find_first_not_of("ACGT") != string::npos) { // don't bother with N
    return seqs;
  }

  BWTInterval interval = findInterval(pBWT, kmer);

  size_t start = ( skip + 1 ) * index + 1; // 1-based
  size_t end = start + kmers - 1; // 1-based

  if ( pos > end ) { // kmer on the left
    if ( interval.upper - interval.lower + 1 > max_interval_size ) { // interval too large
      {
        // extend it to the right first
        size_t count = 0;
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
          ++end;
        }

        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          // check postfix to see if it extends to pos.
          const string& postfix = extractPostfix(pBWT, j); // postfix include the kmer.
          const size_t postfix_size = postfix.size() - kmers;
          if ( pos - end > postfix_size + indel_allowance ) { // matched read does not extend to position
            continue;
          }
          const string& prefix = extractPrefix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }

      if ( index != 0 ) { // not starting at left most
        // reset start and end
        start = ( skip + 1 ) * index + 1; // 1-based
        end = start + kmers - 1; // 1-based
        // extend it to the left
        size_t count = 1;
        --start;
        kmer = w.substr(start-1, kmers+count);
        interval = findInterval(pBWT, kmer); // reset interal as it has been changed above.
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          if ( start > 1 ) { // keep extending to the left.
            --start;
          }
          else { // reach left most, continue to extend to the right.
            ++end;
          }
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
        }

        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          // check postfix to see if it extends to pos.
          const string& postfix = extractPostfix(pBWT, j); // postfix include the kmer.
          const size_t postfix_size = postfix.size() - kmers;
          if ( pos - end > postfix_size + indel_allowance ) { // matched read does not extend to position
            continue;
          }
          const string& prefix = extractPrefix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }
    }
    else {
      for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
        // check postfix to see if it extends to pos.
        const string& postfix = extractPostfix(pBWT, j); // postfix include the kmer.
        const size_t postfix_size = postfix.size() - kmers;
        if ( pos - end > postfix_size + indel_allowance ) { // matched read does not extend to position
          continue;
        }

        const string& prefix = extractPrefix(pBWT, j);
        seqs.insert(prefix+postfix);
      }
    }
  }
  else if ( pos < start ) { // kmer on the right.
    if ( interval.upper - interval.lower + 1 > max_interval_size ) { // interval too large
      {
        // extend it to the left first
        size_t count = 0;
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          --start;
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
        }
        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          // check prefix to see if it extends to pos.
          const string& prefix = extractPrefix(pBWT, j);
          const size_t prefix_size = prefix.size();
          if ( start - pos > prefix_size + indel_allowance ) { // matched read does not extend to position
            continue;
          }
          const string& postfix = extractPostfix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }

      if ( end < length ) { // not ending at the right most
        // reset start and end
        start = ( skip + 1 ) * index + 1; // 1-based
        end = start + kmers - 1; // 1-based
        // extend it to the right
        size_t count = 1;
        ++end;
        kmer = w.substr(start-1, kmers+count);
        interval = findInterval(pBWT, kmer); // reset interal as it has been changed above.
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          if ( end < length ) { // keep extending to the right.
            ++end;
          }
          else { // reach right most, continue to extend to the left.
            --start;
          }
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
        }
        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          // check prefix to see if it extends to pos.
          const string& prefix = extractPrefix(pBWT, j);
          const size_t prefix_size = prefix.size();
          if ( start - pos > prefix_size + indel_allowance ) { // matched read does not extend to position
            continue;
          }
          const string& postfix = extractPostfix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }
    }
    else {
      for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
        // check prefix to see if it extends to pos.
        const string& prefix = extractPrefix(pBWT, j);
        const size_t prefix_size = prefix.size();
        if ( start - pos > prefix_size + indel_allowance ) { // matched read does not extend to position
          continue;
        }
        const string& postfix = extractPostfix(pBWT, j);
        seqs.insert(prefix+postfix);
      }
    }
  }
  else { // kmer at the position.
    if ( interval.upper - interval.lower + 1 > max_interval_size ) { // interval too large
      {
        // extend it to the right first
        size_t count = 0;
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
          ++end;
        }
        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          const string& prefix = extractPrefix(pBWT, j);
          const string& postfix = extractPostfix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }

      {
        // reset start and end
        start = ( skip + 1 ) * index + 1; // 1-based
        end = start + kmers - 1; // 1-based
        // extend it to the left
        size_t count = 1;
        --start;
        kmer = w.substr(start-1, kmers+count);
        interval = findInterval(pBWT, kmer); // reset interal as it has been changed above.
        while ( interval.upper - interval.lower + 1 > max_interval_size ) {
          ++count;
          --start;
          kmer = w.substr(start-1, kmers+count);
          interval = findInterval(pBWT, kmer);
        }
        for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
          const string& prefix = extractPrefix(pBWT, j);
          const string& postfix = extractPostfix(pBWT, j);
          seqs.insert(prefix+postfix);
        }
      }      
    }
    else {
      for ( size_t j=interval.lower; j<=interval.upper; ++j ) {
        const string& prefix = extractPrefix(pBWT, j);
        const string& postfix = extractPostfix(pBWT, j);
        seqs.insert(prefix+postfix);
      }
    }
  }

  return seqs;
}

// Find reads that match the query string.
vector<string> find_reads ( BWT* pBWT, const string& w, const string& s ) {
  vector<string> seqs;

  const size_t sz = w.size();
  if ( sz < MIN_READ_LENGTH ) {
    BWTInterval interval = findInterval(pBWT, w);
    if ( interval.upper < interval.lower ) {
      return seqs;
    }

    const size_t local_match_size = large_match_size * 2;
    size_t start = interval.lower;
    vector<shared_ptr<promise<int> > > vpromises;
    vector<shared_ptr<vector<string> > > vvseqs;

    while ( interval.upper - start > local_match_size ) {
      shared_ptr<promise<int> > p = make_shared<promise<int> >();
      vpromises.push_back(p);
      shared_ptr<vector<string> > vs = make_shared<vector<string> >();
      vvseqs.push_back(vs);
      BWTInterval itv;
      itv.lower = start;
      itv.upper = start+large_match_size-1;
      shared_ptr<ExtractTask> etask(new ExtractTask(pBWT, itv, vs, p));
      dbpool->run(etask); // delete etask after finish job.
      start += large_match_size;
    }

    for ( ; start<=interval.upper; ++start) {
      seqs.push_back( std::move(extractPrefix(pBWT, start) + extractPostfix(pBWT, start)) );
    }

    for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );

    for ( auto &ptr : vvseqs ) {
      copy(ptr->begin(), ptr->end(), back_inserter(seqs));
    }

    return seqs;
  }

  if ( sz < MAX_READ_LENGTH ) { // query < 100
    if ( sz != MIN_READ_LENGTH ) { // query != 73
      // find matches for each tiles with length 73
      const unordered_set<string>& vs = get_tiles(w, MIN_READ_LENGTH);
      for ( unordered_set<string>::const_iterator it=vs.begin(); it!=vs.end(); ++it ) {
        if ( is_suffix_of(s, *it) && query_exactmatch(pBWT, *it) ) {
          seqs.push_back(std::move(*it));
        }
      }
    }

    // find all reads that contain w.
    const vector<string>& source = query(pBWT, w);
    std::copy(source.begin(), source.end(), std::back_inserter(seqs));

    return seqs;
  }

  if ( sz >= MAX_READ_LENGTH ) { // query >= 100
    {
      // find matches for each tiles with length 100
      const unordered_set<string>& vs = get_tiles(w, MAX_READ_LENGTH);
      for ( unordered_set<string>::const_iterator it=vs.begin(); it!=vs.end(); ++it ) {
        if ( is_suffix_of(s, *it) && query_exactmatch(pBWT, *it) ) {
          seqs.push_back(std::move(*it));
        }
      }
    }

    {
      // find matches for each tiles with length 73
      const unordered_set<string>& vs = get_tiles(w, MIN_READ_LENGTH);
      for ( unordered_set<string>::const_iterator it=vs.begin(); it!=vs.end(); ++it ) {
        if ( is_suffix_of(s, *it) && query_exactmatch(pBWT, *it) ) {
          seqs.push_back(std::move(*it));
        }
      }
    }
    return seqs;
  }

  return seqs;
}

class DbTask : public ThreadPool::Job {
protected:
  shared_ptr<vector<string> > seqs;
  shared_ptr<vector<ResultAll*> > results;
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  map<string, string>* hash;
  shared_ptr<promise<int> > p;

public:
  DbTask ( const shared_ptr<vector<string> >& svs, const shared_ptr<vector<ResultAll*> >& svr, map<string, shared_ptr<rocksdb::DB> >* m, map<string, string>* h, const shared_ptr<promise<int> >& pp ) :
    seqs(svs),
    results(svr),
    dbs(m),
    hash(h),
    p(pp)
  {}
  
  virtual void run ( void * ) {
    vector<ResultAll*>::const_iterator result = results->begin();
    for ( vector<string>::const_iterator it = seqs->begin(); it != seqs->end(); ++it, ++result ) {
      string value;
      shared_ptr<rocksdb::DB> sdb = (*dbs)[string(it->rbegin(), it->rbegin()+3)];
      rocksdb::Status status = sdb->Get(rocksdb::ReadOptions(), *it, &value);
      if ( !(status.ok()) ) {
        std::cout << status.ToString() << std::endl;
        assert(status.ok());
      }
      
      if ( result != results->end() ) {
        (*result)->set_r(*it);
        size_t pos = 0;
        while(pos < value.size()) {
          ReadInfo* info = (*result)->add_s();
          info->set_g((*hash)[value.substr(pos,sizeofSample)]);
          pos += sizeofSample;
          if ( hasOtherMetaData ) {
            info->set_c((int)(value[pos])-33);
            ++pos;
            info->set_l((int)(value[pos])-33);
            ++pos;
          }
          else {
            info->set_c(0);
            info->set_l(0);
          }
        }
      }
    }

    p->set_value(1);
  }
};

class KmerTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  zmq::socket_t* sender;
  map<string, string>* hash;
  shared_ptr<Request> request;
  bool isRevComp;
  ThreadPool::ThreadPool* dbpool;

public:
  KmerTask( BWT* bwt, map<string, shared_ptr<rocksdb::DB> >* m, zmq::socket_t* s, map<string, string>* h, const shared_ptr<Request>& req, const Strand& dir, ThreadPool::ThreadPool* pool ) : 
    pbwt(bwt),
    dbs(m),
    sender(s),
    hash(h),
    request(req),
    isRevComp(dir == Strand::RevComp),
    dbpool(pool)
  {}

  virtual void run ( void * ) {
    string query(request->q());
    const size_t kmer = request->k();
    const size_t skip = request->s();

    Reply r = Reply();
    r.set_rt(Reply::KmerMatch);
    r.set_t(static_cast<Reply_ReplyType>(request->rt()));
    r.set_q(query); // Make sure we set the original query string in the response.

    // fix up query string
    if ( isRevComp ) {
      query = rev_comp(query);
    }

    // Do the work
    {
      const unordered_set<string> seqs = find_kmer_reads(pbwt, query, kmer, skip, dbpool);
      const size_t vsize = seqs.size();

      switch ( request->rt() ) {
      case Request_ReturnType_Count: {
        ReplyCount * rc = r.mutable_c();
        if ( isRevComp ) {
          (rc->mutable_revcomp_matches())->set_c(vsize);
        }
        else {
          (rc->mutable_forward_matches())->set_c(vsize);
        }
        break;
      }
      case Request_ReturnType_Reads: { // Return matched read sequences only.
        ReplyReads * rr = r.mutable_r();
        if ( isRevComp ) {
          for ( unordered_set<string>::const_iterator it = seqs.begin(); it != seqs.end(); ++it ) {
            (rr->add_revcomp_matches())->set_r(*it);
          }
        }
        else {
          for ( unordered_set<string>::const_iterator it = seqs.begin(); it != seqs.end(); ++it ) {
            (rr->add_forward_matches())->set_r(*it);
          }
        }
        break;
      }
      default: { // Return all information
        ReplyAll * ra = r.mutable_a();
        vector<ResultAll*> vresults(vsize, 0); // Reply object will clean up
        if ( isRevComp ) {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_revcomp_matches();
          }
        }
        else {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_forward_matches();
          }
        }

        unordered_set<string>::const_iterator it = seqs.begin();
        unordered_set<string>::const_iterator end = seqs.end();
        vector<ResultAll*>::const_iterator result = vresults.begin();
        vector<shared_ptr<promise<int> > > vpromises;

        const size_t local_chunk_size = async_chunk_size * 2;

        if ( vsize > local_chunk_size ) { // Async fetch
          unordered_set<string>::const_iterator front = seqs.begin();
          advance(front, async_chunk_size);
          for ( ; distance(it, end) > (int)local_chunk_size; advance(it, async_chunk_size), advance(result, async_chunk_size), advance(front, async_chunk_size) ) {
            shared_ptr<promise<int> > p = make_shared<promise<int> >();
            vpromises.push_back(p);
            shared_ptr<DbTask> dbtask(new DbTask(make_shared<vector<string> >(it, front), make_shared<vector<ResultAll*> >(result, result+async_chunk_size), dbs, hash, p ));
            dbpool->run(dbtask); // delete dbtask after finish job.
          }
        }

        // Proceed to sync fetch
        for ( ; it!=seqs.end(); ++it, ++result ) {
          string value;
          shared_ptr<rocksdb::DB> sdb = (*dbs)[string(it->rbegin(), it->rbegin()+3)];
          rocksdb::Status status = sdb->Get(rocksdb::ReadOptions(), *it, &value);
          if ( !(status.ok()) ) {
            std::cout << status.ToString() << std::endl;
            assert(status.ok());
          }

          (*result)->set_r(*it);
          size_t pos = 0;
          while(pos < value.size()) {
            ReadInfo* info = (*result)->add_s();
            info->set_g((*hash)[value.substr(pos,sizeofSample)]);
            pos += sizeofSample;
            if ( hasOtherMetaData ) {
              info->set_c((int)(value[pos])-33);
              ++pos;
              info->set_l((int)(value[pos])-33);
              ++pos;
            }
            else {
              info->set_c(0);
              info->set_l(0);
            }
          }
        }

        for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );
        break;
      }        
      }
    }

    //  Send results to sink
    zmq::message_t message(r.ByteSize());
    r.SerializeToArray(message.data(), r.ByteSize());

    mtx.lock();
    sender->send(message);
    mtx.unlock();
  }
};

class GtTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  zmq::socket_t* sender;
  map<string, string>* hash;
  shared_ptr<Request> request;
  bool isRevComp;
  ThreadPool::ThreadPool* dbpool;

public:
  GtTask( BWT* bwt, map<string, shared_ptr<rocksdb::DB> >* m, zmq::socket_t* s, map<string, string>* h, const shared_ptr<Request>& req, const Strand& dir, ThreadPool::ThreadPool* pool ) : 
    pbwt(bwt),
    dbs(m),
    sender(s),
    hash(h),
    request(req),
    isRevComp(dir == Strand::RevComp),
    dbpool(pool)
  {}

  virtual void run ( void * ) {
    string query(request->q());
    size_t pos = request->p();
    string alt(request->a());
    const bool isalt = ( 1 == request->isalt() ? true : false );
    const size_t kmer = request->k();
    const size_t skip = request->s();

    Reply r = Reply();
    r.set_rt(Reply::SiteMatch);
    r.set_t(static_cast<Reply_ReplyType>(request->rt()));
    r.set_q(query); // Make sure we set the original query string in the response.

    // fix up query string and position
    if ( isRevComp ) {
      query = rev_comp(query);
      pos = query.size() - pos + 1;
      alt = rev_comp(alt);
    }

    if ( pos <= MAX_READ_LENGTH ) {
      query = query.substr(0, MAX_READ_LENGTH + pos - 1);
    }
    else {
      query = query.substr(pos-MAX_READ_LENGTH, MAX_READ_LENGTH * 2 - 1);
      pos = MAX_READ_LENGTH;
    }

    // sanity check
    const size_t wsize = query.size();
    if ( pos > wsize ) {
      cout << "ERROR: pos=" << pos << "; wsize=" << wsize << "; w=" << query << endl;
    }
    else { // Do the work
      const size_t tiles_size = (( wsize - kmer ) / (skip + 1)) + 1;

      unordered_set<string> seqs;
      vector<shared_ptr<promise<int> > > vpromises;
      vector<shared_ptr<unordered_set<string> > > vvseqs;

      for ( size_t i=0; i<tiles_size; ++i ) {
        if ( i+1 < tiles_size ) {
          shared_ptr<promise<int> > p = make_shared<promise<int> >();
          vpromises.push_back(p);
          shared_ptr<unordered_set<string> > vs = make_shared<unordered_set<string> >();
          vvseqs.push_back(vs);
          shared_ptr<FindGtReadsTask> mtask(new FindGtReadsTask(pbwt, query, i, kmer, skip, pos, vs, p));
          dbpool->run(mtask); // delete mtask after finish job.
          continue;
        }

        const unordered_set<string> vseqs = find_gt_reads(pbwt, query, i, kmer, skip, pos);
        seqs.insert(vseqs.begin(), vseqs.end());
      }

      for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );
      for ( auto &ptr : vvseqs ) {
        seqs.insert(ptr->begin(), ptr->end());
      }

      vpromises.clear();
      vvseqs.clear();

      vector<shared_ptr<vector<string> > > vseqs;

      const size_t seqs_size = seqs.size();
      const size_t local_align_size = large_align_size * 2;
      unordered_set<string>::const_iterator it = seqs.begin();
      unordered_set<string>::const_iterator end = seqs.end();

      if ( seqs_size > local_align_size ) {
        unordered_set<string>::const_iterator front = seqs.begin();
        advance(front, large_align_size);
        for ( ; distance(it, end) > (int)local_align_size; advance(it, large_align_size), advance(front, large_align_size) ) {
          shared_ptr<promise<int> > p = make_shared<promise<int> >();
          vpromises.push_back(p);
          shared_ptr<vector<string> > vs = make_shared<vector<string> >();
          vseqs.push_back(vs);
          shared_ptr<AlignGtReadsTask> mtask(new AlignGtReadsTask(make_shared<vector<string> >(it, front), query, pos, alt, isalt, vs, p));
          dbpool->run(mtask); // delete mtask after finish job.
          continue;
        }
      }

      vector<string> aligned_reads;
      for ( ; it!=seqs.end(); ++it ) {
        if ( align_gt_read(*it, query, pos, alt, isalt) ) {
          aligned_reads.push_back(*it);
        }
      }
      
      for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );
      for ( auto &ptr : vseqs ) {
        aligned_reads.insert(aligned_reads.end(), ptr->begin(), ptr->end());
      }

      const size_t vsize = aligned_reads.size();

      switch ( request->rt() ) {
      case Request_ReturnType_Count: {
        ReplyCount * rc = r.mutable_c();
        if ( isRevComp ) {
          (rc->mutable_revcomp_matches())->set_c(vsize);
        }
        else {
          (rc->mutable_forward_matches())->set_c(vsize);
        }
        break;
      }
      case Request_ReturnType_Reads: { // Return matched read sequences only.
        ReplyReads * rr = r.mutable_r();
        if ( isRevComp ) {
          for ( vector<string>::const_iterator it = aligned_reads.begin(); it != aligned_reads.end(); ++it ) {
            (rr->add_revcomp_matches())->set_r(*it);
          }
        }
        else {
          for ( vector<string>::const_iterator it = aligned_reads.begin(); it != aligned_reads.end(); ++it ) {
            (rr->add_forward_matches())->set_r(*it);
          }
        }
        break;
      }
      default: { // Return all information
        ReplyAll * ra = r.mutable_a();
        vector<ResultAll*> vresults(vsize, 0); // Reply object will clean up
        if ( isRevComp ) {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_revcomp_matches();
          }
        }
        else {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_forward_matches();
          }
        }

        vector<string>::const_iterator it = aligned_reads.begin();
        vector<string>::const_iterator end = aligned_reads.end();
        vector<ResultAll*>::const_iterator result = vresults.begin();
        vector<shared_ptr<promise<int> > > vpromises;

        const size_t local_chunk_size = async_chunk_size * 2;

        if ( vsize > local_chunk_size ) { // Async fetch
          vector<string>::const_iterator front = aligned_reads.begin();
          advance(front, async_chunk_size);
          for ( ; distance(it, end) > (int)local_chunk_size; advance(it, async_chunk_size), advance(result, async_chunk_size), advance(front, async_chunk_size) ) {
            shared_ptr<promise<int> > p = make_shared<promise<int> >();
            vpromises.push_back(p);
            shared_ptr<DbTask> dbtask(new DbTask(make_shared<vector<string> >(it, front), make_shared<vector<ResultAll*> >(result, result+async_chunk_size), dbs, hash, p ));
            dbpool->run(dbtask); // delete dbtask after finish job.
          }
        }

        // Proceed to sync fetch
        for ( ; it!=aligned_reads.end(); ++it, ++result ) {
          string value;
          shared_ptr<rocksdb::DB> sdb = (*dbs)[string(it->rbegin(), it->rbegin()+3)];
          rocksdb::Status status = sdb->Get(rocksdb::ReadOptions(), *it, &value);
          if ( !(status.ok()) ) {
            std::cout << status.ToString() << std::endl;
            assert(status.ok());
          }

          (*result)->set_r(*it);
          size_t pos = 0;
          while(pos < value.size()) {
            ReadInfo* info = (*result)->add_s();
            info->set_g((*hash)[value.substr(pos,sizeofSample)]);
            pos += sizeofSample;
            if ( hasOtherMetaData ) {
              info->set_c((int)(value[pos])-33);
              ++pos;
              info->set_l((int)(value[pos])-33);
              ++pos;
            }
            else {
              info->set_c(0);
              info->set_l(0);
            }
          }
        }

        for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );
        break;
      }
      }
    }

    //  Send results to sink
    zmq::message_t message(r.ByteSize());
    r.SerializeToArray(message.data(), r.ByteSize());

    mtx.lock();
    sender->send(message);
    mtx.unlock();    
  }
  
};

class CountTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  zmq::socket_t* sender;
  string query;
  Strand st;
  Reply_RequestType rt;

public:
  CountTask( BWT* bwt, zmq::socket_t* s, const string& w, const Strand& dir, const Reply_RequestType& t ) : 
    pbwt(bwt),
    sender(s),
    query(w),
    st(dir),
    rt(t)
  {}

  virtual void run ( void * ) {
    count_reads(pbwt, sender, query, st, rt);
  }
};

class QueryTask : public ThreadPool::Job {
protected:
  BWT* pbwt;
  map<string, shared_ptr<rocksdb::DB> >* dbs;
  zmq::socket_t* sender;
  map<string, string>* hash;
  shared_ptr<Request> request;
  string suffix;
  bool isRevComp;
  ThreadPool::ThreadPool* dbpool;

public:
  QueryTask( BWT* bwt, map<string, shared_ptr<rocksdb::DB> >* m, zmq::socket_t* s, map<string, string>* h, const shared_ptr<Request>& req, const string& su, const Strand& dir, ThreadPool::ThreadPool* pool ) : 
    pbwt(bwt),
    dbs(m),
    sender(s),
    hash(h),
    request(req),
    suffix(su),
    isRevComp(dir == Strand::RevComp),
    dbpool(pool)
  {}

  virtual void run ( void * ) {
    // Construct Reply object.
    Reply r = Reply();
    r.set_rt(Reply::ExactMatch);
    r.set_t(static_cast<Reply_ReplyType>(request->rt())); // return type
    r.set_q(request->q());

    string query(request->q());

    if ( isRevComp ) {
      query = rev_comp(query);
    }

    // Do the work
    {
      const vector<string>& seqs = find_reads(pbwt, query, suffix); // match forward string
      const size_t vsize = seqs.size();

      switch ( request->rt() ) {
      case Request_ReturnType_Reads: { // Return matched read sequences only.
        ReplyReads * rr = r.mutable_r();
        if ( isRevComp ) {
          for ( vector<string>::const_iterator it = seqs.begin(); it != seqs.end(); ++it ) {
            (rr->add_revcomp_matches())->set_r(*it);
          }
        }
        else {
          for ( vector<string>::const_iterator it = seqs.begin(); it != seqs.end(); ++it ) {
            (rr->add_forward_matches())->set_r(*it);
          }
        }
        break;
      }
      default: { // Return all information
        ReplyAll * ra = r.mutable_a();
        vector<ResultAll*> vresults(vsize, 0); // Reply object will clean up
        if ( isRevComp ) {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_revcomp_matches();
          }
        }
        else {
          for ( auto &ptr : vresults ) {
            ptr = ra->add_forward_matches();
          }
        }

        vector<string>::const_iterator it = seqs.begin();
        vector<string>::const_iterator end = seqs.end();
        vector<ResultAll*>::const_iterator result = vresults.begin();
        vector<shared_ptr<promise<int> > > vpromises;

        const size_t local_chunk_size = async_chunk_size * 2;

        if ( vsize > local_chunk_size ) { // Async fetch
          for ( ; distance(it, end) > (int)local_chunk_size; advance(it, async_chunk_size), advance(result, async_chunk_size) ) {
            shared_ptr<promise<int> > p = make_shared<promise<int> >();
            vpromises.push_back(p);
            shared_ptr<DbTask> dbtask(new DbTask(make_shared<vector<string> >(it, it+async_chunk_size), make_shared<vector<ResultAll*> >(result, result+async_chunk_size), dbs, hash, p ));
            dbpool->run(dbtask); // delete dbtask after finish job.
          }
        }

        // Proceed to sync fetch
        for ( ; it!=seqs.end(); ++it, ++result ) {
          string value;
          shared_ptr<rocksdb::DB> sdb = (*dbs)[string(it->rbegin(), it->rbegin()+3)];
          rocksdb::Status status = sdb->Get(rocksdb::ReadOptions(), *it, &value);
          if ( !(status.ok()) ) {
            std::cout << status.ToString() << std::endl;
            assert(status.ok());
          }
          
          (*result)->set_r(*it);
          size_t pos = 0;
          while(pos < value.size()) {
            ReadInfo* info = (*result)->add_s();
            info->set_g((*hash)[value.substr(pos,sizeofSample)]);
            pos += sizeofSample;
            if ( hasOtherMetaData ) {
              info->set_c((int)(value[pos])-33);
              ++pos;
              info->set_l((int)(value[pos])-33);
              ++pos;
            }
            else {
              info->set_c(0);
              info->set_l(0);
            }
          }
        }

        for_each(vpromises.begin(), vpromises.end(), [ ] ( const shared_ptr<promise<int> >& p ) { (p->get_future()).get(); } );
        break;
      }
      }
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

  signal(SIGSEGV, handler); 

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
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << endl;
    return(EXIT_FAILURE);
  }

  // Configs from config file.
  string prefix;
  string suffix;
  string hashfile;
  string pull_socket;
  string push_socket;
  string push_socket_count;
  string rocksdb_path;
  string rocksdb_ext;
  vector<string> rocksdbs;
  try
  {
    if ( cfg.exists("sizeofSample") ) {
      sizeofSample = (int)cfg.lookup("sizeofSample");
    }
    if ( cfg.exists("hasOtherMetaData") ) {
      hasOtherMetaData = (bool)cfg.lookup("hasOtherMetaData");
    }

    prefix = cfg.lookup("prefix").c_str();
    suffix = cfg.lookup("suffix").c_str();
    hashfile = cfg.lookup("hashfile").c_str();
    pull_socket = cfg.lookup("pull").c_str();
    push_socket = cfg.lookup("push").c_str();
    push_socket_count = cfg.lookup("push_count").c_str();
    rocksdb_path = cfg.lookup("rocksdb_path").c_str();
    rocksdb_ext = cfg.lookup("rocksdb_ext").c_str();

    const Setting& s = cfg.lookup("rocksdb");
    const size_t length = s.getLength();
    for ( size_t i=0; i<length; ++i ) {
      rocksdbs.push_back(s[i].c_str());
    }
  }
  catch(const SettingNotFoundException &nfex)
  {
    cerr << "No 'name' setting in configuration file." << endl;
    return(EXIT_FAILURE);
  }

  cout << "starting server for " << suffix << endl;

  // Load bwt string and its index structure.
  const string bwt(prefix+".bwt");
  future<BWT*> fut_pbwt = async( launch::async, [ ] ( const string& bwt ) {
      BWT* pbwt = new RLEBWT(bwt);
      cout << "loaded bwt and bpi." << endl;
      return pbwt;
    }, bwt );

  // Open dbs
  map<string, shared_ptr<rocksdb::DB> > dbs;
  vector<future<rocksdb::DB*> > fut_dbs;
  for ( vector<string>::iterator it=rocksdbs.begin(); it!=rocksdbs.end(); ++it ) {
    const string db_path(rocksdb_path + *it + rocksdb_ext);
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
  ifstream ifile(hashfile.c_str());
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
  receiver.connect(pull_socket.c_str());
  receiver.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  //  Socket to send messages to
  zmq::socket_t sender(context, ZMQ_PUSH);
  sender.connect(push_socket.c_str());
  zmq::socket_t sender_count(context, ZMQ_PUSH);
  sender_count.connect(push_socket_count.c_str());

  // Set up thread pool
  const unique_ptr<ThreadPool::ThreadPool> pool(new ThreadPool::ThreadPool(query_thread_size));
  const unique_ptr<ThreadPool::ThreadPool> dbpool(new ThreadPool::ThreadPool(db_thread_size));

  for ( size_t i = 0; i < rocksdbs.size(); ++i ) {
    dbs.insert( pair<string, shared_ptr<rocksdb::DB> >(rocksdbs[i], shared_ptr<rocksdb::DB>(fut_dbs[i].get())) );
  }
  cout << "Opened dbs" << endl;

  const unique_ptr<BWT> pBWT(fut_pbwt.get());

  shared_ptr<ReopenDbTask> rdbtask(new ReopenDbTask(&dbs, rocksdbs, rocksdb_path, rocksdb_ext));
  dbpool->run(rdbtask); // delete rdbtask after finish job.

  cout << "ready to serve from " << suffix << endl;

  //  Process tasks forever
  while (1) {
    // Receive a message
    zmq::message_t message;
    receiver.recv(&message);

    // Create Request object from message
    shared_ptr<Request> w = make_shared<Request>();
    w->ParseFromArray(message.data(), message.size());

    switch ( w->t() ) {
    case Request_RequestType_SiteMatch: { // Assign to thread pool.
      shared_ptr<GtTask> gtf(new GtTask(pBWT.get(), &dbs, &sender, &hash, w, Strand::Forward, dbpool.get()));
      pool->run(gtf); // delete qtf after finish job.
      shared_ptr<GtTask> gtr(new GtTask(pBWT.get(), &dbs, &sender, &hash, w, Strand::RevComp, dbpool.get()));
      pool->run(gtr); // delete qtr after finish job.
      break;
    }

    case Request_RequestType_KmerMatch: { // Assign to thread pool.
      shared_ptr<KmerTask> ktf(new KmerTask(pBWT.get(), &dbs, &sender, &hash, w, Strand::Forward, dbpool.get()));
      pool->run(ktf); // delete qtf after finish job.
      shared_ptr<KmerTask> ktr(new KmerTask(pBWT.get(), &dbs, &sender, &hash, w, Strand::RevComp, dbpool.get()));
      pool->run(ktr); // delete qtr after finish job.
      break;
    }

    case Request_RequestType_ExactMatch: {
      switch ( w->rt() ) {
      case Request_ReturnType_Count: { // This should be quick.
        shared_ptr<CountTask> ctf(new CountTask(pBWT.get(), &sender, w->q(), Strand::Forward, Reply::ExactMatch));
        pool->run(ctf); // delete qtf after finish job.
        shared_ptr<CountTask> ctr(new CountTask(pBWT.get(), &sender, w->q(), Strand::RevComp, Reply::ExactMatch));
        pool->run(ctr); // delete qtf after finish job.
        break;
      }
      default: // Assign to thread pool.
        shared_ptr<QueryTask> qtf(new QueryTask(pBWT.get(), &dbs, &sender, &hash, w, suffix, Strand::Forward, dbpool.get()));
        pool->run(qtf); // delete qtf after finish job.
        shared_ptr<QueryTask> qtr(new QueryTask(pBWT.get(), &dbs, &sender, &hash, w, suffix, Strand::RevComp, dbpool.get()));
        pool->run(qtr); // delete qtr after finish job.
        break;
      }

      break;
    }

    case Request_RequestType_CountReads: { // This should be quick.
      count_reads(pBWT.get(), &sender_count, w->q(), Strand::Forward, Reply::CountReads);
      count_reads(pBWT.get(), &sender_count, w->q(), Strand::RevComp, Reply::CountReads);
      break;
    }

    default:
      cout << "unrecognised request type." << endl;
      break;
    }    
  }

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}
