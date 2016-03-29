#include <string>
#include <iostream>
#include <memory>
#include <mutex>
#include <future>
#include <map>
#include <stdlib.h>
#include <pthread.h>

#include "mongoose.h"
#include "zmq.hpp"
#include "libconfig.h++"
#include "google/protobuf/io/coded_stream.h"
#include "readserver.pb.h"
#include "threadpool.h"

#include <chrono>
#include <thread>


#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;
using namespace libconfig;

size_t workers_count = 32;

class Response {
public:
  Response () 
    : m_ready(false),
      m_reply(make_shared<string>()),
      m_replies(make_shared<vector<shared_ptr<Reply> > >())
  {
    m_reply->reserve(200); // allocate enough memory here otherwise tcmalloc will have issue
    m_replies->reserve(workers_count);
  }

  ~Response () {}

  void push_back ( const shared_ptr<Reply>& reply ) {
    m_replies->push_back(reply);
  }

  shared_ptr<vector<shared_ptr<Reply> > > getReplies () const {
    return m_replies;
  }

  size_t size () const {
    return m_replies->size();
  }

  bool hasReply () const {
    return m_reply->size() > 0;
  }

  shared_ptr<string> getReply () const {
    return m_reply;
  }

  void append ( const string& s ) {
    m_reply->append(s);
  }

  bool isReady () const {
    return m_ready;
  }

  void setReady () {
    m_ready = true;
  }

private:
  bool m_ready;
  shared_ptr<string> m_reply;
  shared_ptr<vector<shared_ptr<Reply> > > m_replies;
};


void handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

map<string, Reply_ReplyType> msRplTypes ( {
    { "all", Reply_ReplyType_ReplyAll },
    { "count", Reply_ReplyType_ReplyCount },
    { "reads", Reply_ReplyType_ReplyReads },
    { "samples", Reply_ReplyType_ResultSamples }
} );

map<string, Request_ReturnType> msRtnTypes ( {
    { "all", Request_ReturnType_All },
    { "count", Request_ReturnType_Count },
    { "reads", Request_ReturnType_Reads },
    { "samples", Request_ReturnType_Samples }
} );

mutex mtx;

// Set up messaging channel
zmq::context_t context(1);
//  Socket to receive messages on
zmq::socket_t receiver_count(context, ZMQ_PULL);
//  Socket to receive messages on
zmq::socket_t receiver(context, ZMQ_PULL);
//  Socket to send messages to
zmq::socket_t sender_samples(context, ZMQ_PUB);
//  Socket to send messages to
zmq::socket_t sender(context, ZMQ_PUB);

size_t min_query_length = 30;
size_t max_query_length = 300;
size_t max_query_length_count_threshold = 95;
size_t max_match_reads = 100000;

const size_t worker_thread_size = 8;
const unique_ptr<ThreadPool::ThreadPool> pool(new ThreadPool::ThreadPool(worker_thread_size));
const unique_ptr<ThreadPool::ThreadPool> pool2(new ThreadPool::ThreadPool(worker_thread_size));

map<string, shared_ptr<Response> > responses;
map<string, size_t> requests;
map<string, shared_ptr<Response> > countread_responses;
map<string, shared_ptr<Response> > completed_responses;
map<string, shared_ptr<Response> > completed_requests;

void readinfo2json ( string& dst, const ReadInfo& ri ) {
  dst.append("{\"g\":\"");
  dst.append(ri.g());
  dst.append("\",\"c\":");
  dst.append(to_string(ri.c()));
  dst.append(",\"l\":");
  dst.append(to_string(ri.l()));
  dst.append("}");
}

void resultreads2json ( string& dst, const ResultReads& rr ) {
  dst.append("{\"r\":\"");
  dst.append(rr.r());
  dst.append("\"}");
}

void resultall2json ( string& dst, const ResultAll& ra ) {
  dst.append("{\"r\":\"");
  dst.append(ra.r());
  dst.append("\",\"s\":[");
  for ( int i=0; i<ra.s_size(); ++i ) {
    readinfo2json(dst, ra.s(i));
    if ( i+1 < ra.s_size() ) {
      dst.append(",");
    }
  }
  dst.append("]}");
}

void resultsamples2json ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  dst.append("\"s\":[");
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    const ResultSamples& rs = (*it)->s();
    for ( int i=0; i<rs.s_size(); ++i ) {
      readinfo2json(dst, rs.s(i));
      if ( i+1 < rs.s_size() ) { 
        dst.append(",");
      }
    }
  }
  dst.append("]");
}

void replycount2json ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  ::google::protobuf::int32 fmc = 0;
  ::google::protobuf::int32 rmc = 0;
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    fmc += (*it)->c().forward_matches().c();
    rmc += (*it)->c().revcomp_matches().c();
  }

  dst.append("\"forward_matches\":{\"c\":");
  dst.append(to_string(fmc));
  dst.append("},\"revcomp_matches\":{\"c\":");
  dst.append(to_string(rmc));
  dst.append("}");
}

void replyreads2json_fm ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  bool first = true;
  dst.append("\"forward_matches\":[");
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    const ReplyReads& rr = (*it)->r();
    for ( int i=0; i<rr.forward_matches_size(); ++i ) {
      if ( !first ) {
        dst.append(",");
      }
      first = false;
      resultreads2json(dst, rr.forward_matches(i));
    }
  }
  dst.append("]");
}

void replyreads2json_rm ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  bool first = true;
  dst.append("\"revcomp_matches\":[");
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    const ReplyReads& rr = (*it)->r();
    for ( int i=0; i<rr.revcomp_matches_size(); ++i ) {
      if ( !first ) {
        dst.append(",");
      }
      first = false;
      resultreads2json(dst, rr.revcomp_matches(i));
    }
  }
  dst.append("]");
}

void replyall2json_fm ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  bool first = true;
  dst.append("\"forward_matches\":[");
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    const ReplyAll& ra = (*it)->a();
    for ( int i=0; i<ra.forward_matches_size(); ++i ) {
      if ( !first ) {
        dst.append(",");
      }
      first = false;
      resultall2json(dst, ra.forward_matches(i));
    }
  }
  dst.append("]");
}

void replyall2json_rm ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  bool first = true;
  dst.append("\"revcomp_matches\":[");
  for ( vector<shared_ptr<Reply> >::const_iterator it=vr.begin(); it!=vr.end(); ++it ) {
    const ReplyAll& ra = (*it)->a();
    for ( int i=0; i<ra.revcomp_matches_size(); ++i ) {
      if ( !first ) {
        dst.append(",");
      }
      first = false;
      resultall2json(dst, ra.revcomp_matches(i));
    }
  }
  dst.append("]");
}

void jsonise ( string& dst, const vector<shared_ptr<Reply> >& vr ) {
  if ( vr.size() == 0 ) {
    return;
  }

  if ( vr[0]->has_q() ) {
    if ( vr[0]->has_s() ) {
      resultsamples2json(dst, vr);
    }
    else if ( vr[0]->has_c()) {
      replycount2json(dst, vr);
    }
  }
}

string serialise ( const map<string, string>& params ) {
  string rval;

  for ( map<string, string>::const_iterator it=params.begin(); it!=params.end(); ++it ) {
    rval.append("\"");
    rval.append(it->first);
    rval.append("\":\"");
    rval.append(it->second);
    rval.append("\",");
  }

  return rval;
}

string getKey ( const string& query, const Reply_RequestType& requestType, const Reply_ReplyType& replyType ) {
  string rval =  Reply_RequestType_Name(requestType);
  rval.append(query);
  rval.append(Reply_ReplyType_Name(replyType));

  return rval;
}

class JsoniseTask : public ThreadPool::Job {
protected:
  shared_ptr<string> ss;
  shared_ptr<vector<shared_ptr<Reply> > > sv;
  shared_ptr<promise<int> > p;

public:
  JsoniseTask ( const shared_ptr<string>& s, const shared_ptr<vector<shared_ptr<Reply> > >& r, const shared_ptr<promise<int> >& pp ) :
    ss(s),
    sv(r),
    p(pp)
 {}

  virtual void run ( void * ) {
    if ( (*sv)[0]->has_a() ) {
      replyall2json_rm(*ss, *sv);
    }
    else if ( (*sv)[0]->has_r() ) {
      replyreads2json_rm(*ss, *sv);
    }

    p->set_value(1);
  }
 };

class ProcessTask : public ThreadPool::Job {
protected:
  shared_ptr<Response> spr;
  string storeKey;

public:
  ProcessTask ( const shared_ptr<Response>& r, const string& s ) :
    spr(r),
    storeKey(s)
 {}

  virtual void run ( void * ) {
    const shared_ptr<vector<shared_ptr<Reply> > >& sv = spr->getReplies();
    if ( sv->size() == 0 ) {
      return;
    }

    int bytesize = 0;
    for ( vector<shared_ptr<Reply> >::const_iterator it=sv->begin(); it!=sv->end(); ++it ) {
      bytesize += (*it)->ByteSize();
    }

    shared_ptr<promise<int> > pi = make_shared<promise<int> >();
    shared_ptr<string> s = make_shared<string>();
    s->reserve(2 * bytesize);
    shared_ptr<JsoniseTask>jtask(new JsoniseTask(s, sv, pi));
    pool2->run(jtask); // delete jtask after finish job

    const shared_ptr<string>& ss = spr->getReply();
    ss->reserve(2 * bytesize);
    if ( (*sv)[0]->has_a() ) {
      replyall2json_fm(*ss, *sv);
    }
    else if ( (*sv)[0]->has_r() ) {
      replyreads2json_fm(*ss, *sv);
    }
    
    ss->append(",");
    (pi->get_future()).get(); // wait for jtask to complete.
    ss->append(s->c_str());
    ss->append("}");

    sv->clear();
    spr->setReady(); // result is ready

    {
      lock_guard<mutex> lg(mtx);
      completed_responses[storeKey] = spr;
    }
  }
};

void * counter_routine ( void * arg ) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if ( arg ) {
    arg = NULL;
  }

  while ( 1 ) {
    // Receive a message
    zmq::message_t message;
    receiver_count.recv(&message);

    shared_ptr<Reply> r = make_shared<Reply>();
    r->ParseFromArray(message.data(), message.size());

    if ( r->has_q() && r->has_c() ) {
      const string& storeKey = getKey(r->q(), r->rt(), r->t());
      if ( countread_responses.find(storeKey) == countread_responses.end() ) {
        countread_responses[storeKey] = make_shared<Response>();
      }

      const shared_ptr<Response>& spr = countread_responses[storeKey];
      if ( spr ) { // make sure we got something pointed to
        spr->push_back(r);
        if ( spr->size() == workers_count ) { // all replies are here
          ::google::protobuf::int32 fmc = 0;
          ::google::protobuf::int32 rmc = 0;
          const shared_ptr<vector<shared_ptr<Reply> > >& replies = spr->getReplies();
          for ( vector<shared_ptr<Reply> >::const_iterator it=replies->begin(); it!=replies->end(); ++it ) {
            fmc += (*it)->c().forward_matches().c();
            rmc += (*it)->c().revcomp_matches().c();
          }

          const size_t count = fmc + rmc;
          if ( count > max_match_reads ) {
            spr->append("Bad Request: match too many reads. Please make you query more specific.");
          }
          spr->setReady();

          {
            lock_guard<mutex> lg(mtx);
            completed_responses[storeKey] = spr;
          }
          countread_responses.erase(storeKey);
        }
      }
    }
  }

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  pthread_exit(NULL);

  return NULL;
}

void * worker_routine ( void * arg ) {
  // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  if ( arg ) {
    arg = NULL;
  }

  while ( 1 ) {
    // Receive a message
    zmq::message_t message;
    receiver.recv(&message);

    shared_ptr<Reply> r = make_shared<Reply>();
    unique_ptr<::google::protobuf::io::CodedInputStream> coded_input(new ::google::protobuf::io::CodedInputStream(reinterpret_cast<unsigned char*>(message.data()), message.size()));
    coded_input->SetTotalBytesLimit(1073741824, 536870912); // total_bytes_limit=1024*1024*1024, warning_threshold=512*1024*1024
    r->ParseFromCodedStream(coded_input.get());

    if ( r->has_q() ) {
      const string& storeKey = getKey(r->q(), r->rt(), r->t());
      if ( responses.find(storeKey) == responses.end() ) {
        responses[storeKey] = make_shared<Response>();
      }

      const shared_ptr<Response>& spr = responses[storeKey];

      if ( !(spr.get()) ) {
        cerr << "storeKey is UNIQUE: " << storeKey << "\n";
      }
      else  {
        spr->push_back(r);
 
        if ( r->has_s() || (r->has_c() && spr->size()==workers_count) ) { // all replies are here
          const shared_ptr<string>& ss = spr->getReply();
          jsonise(*ss, *(spr->getReplies()));
          ss->append("}");
          spr->setReady();
          {
            lock_guard<mutex> lg(mtx);
            completed_responses[storeKey] = spr;
          }
          responses.erase(storeKey);
        }
        else if ( spr->size() == workers_count ) { // send it to thread pool to process
          shared_ptr<ProcessTask> pt(new ProcessTask(spr, storeKey));
          pool->run(pt); // delete pt after finish job.
          responses.erase(storeKey);
        }
      }
    }
    else {
      cerr << "Invalid message for Reply which does not have query string." << "\n";
    }
  }

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  pthread_exit(NULL);

  return NULL;
}

map<string, string> parse_query_string ( const string& query ) {
  map<string, string> rval;

  size_t start = 0;
  size_t pos_and = 0;
  size_t pos_equal = 0;
  while ( (pos_and = query.find("&", start)) != string::npos ) {
    pos_equal = query.find("=", start);
    string value = query.substr(pos_equal+1, pos_and-pos_equal-1);
    char vals[value.size()+1];
    mg_url_decode(value.c_str(), value.size(), vals, value.size()+1, 0);
    rval[query.substr(start, pos_equal-start)] = vals;
    start = pos_and + 1;
  }

  pos_equal = query.find("=", start);
  rval[query.substr(start, pos_equal-start)] = query.substr(pos_equal+1, query.size()-pos_equal-1);

  return rval;
}

int handle_get ( struct mg_connection *conn ) {
  const char* query_string = conn->query_string;
  if ( query_string == NULL ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: please provide query string.");
    return MG_TRUE;
  }

  const map<string, string>& query_params = parse_query_string(conn->query_string);
  map<string, string>::const_iterator query_iter = query_params.find("query");
  if ( query_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: please provide query string.");
    return MG_TRUE;
  }

  const string& query = query_iter->second;

  if ( query.size() > max_query_length ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, string("Bad Request: query string is too long (maximum: ").append(max_query_length + ").").c_str());
    return MG_TRUE;
  }

  string output("all");
  map<string, string>::const_iterator output_iter = query_params.find("output");
  if ( output_iter != query_params.end() ) {
    output = output_iter->second;
  }

  if ( msRplTypes.find(output) == msRplTypes.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: output is specified but not one of these: \"all\", \"count\", \"reads\", \"samples\".");
    return MG_TRUE;
  }

  if ( output == "samples" ) {
    if ( query.size() != 100 && query.size() != 73 ) {
      mg_send_status(conn, 400);
      mg_printf_data(conn, "Bad Request: query is not of length 73 or 100.");
      return MG_TRUE;
    }
  }

  if ( query.size() < max_query_length_count_threshold && output != "count" && output != "samples" ) {
    const string& storeKey = getKey(query, Reply_RequestType_CountReads, msRplTypes["count"]);

    if ( requests.find(storeKey) == requests.end() ) {
      requests[storeKey] = 1;

      Request r;
      r.set_t(Request_RequestType_CountReads);
      r.set_rt(msRtnTypes["count"]);
      r.set_q(query);

      zmq::message_t message(r.ByteSize());
      r.SerializeToArray(message.data(), r.ByteSize());
      sender.send(message);
    }
    else {
      ++requests[storeKey];
    }

    conn->connection_param = static_cast<void*>(new string(storeKey));
  }
  else {
    const string& storeKey = getKey(query, Reply_RequestType_ExactMatch, msRplTypes[output]);

    if ( requests.find(storeKey) == requests.end() ) {
      requests[storeKey] = 1;

      Request r;
      r.set_t(Request_RequestType_ExactMatch);
      r.set_rt(msRtnTypes[output]);
      r.set_q(query);

      zmq::message_t message(r.ByteSize());
      r.SerializeToArray(message.data(), r.ByteSize());

      if ( output != "samples" ) {
        sender.send(message);
      }
      else {
        string msg(query.rbegin(), query.rbegin()+3);
        msg.append(" ");
        msg.append(static_cast<char*>(message.data()), message.size());
        zmq::message_t newmsg(const_cast<void*>(reinterpret_cast<const void*>(msg.c_str())), msg.size(), NULL, NULL);

        if ( !(sender_samples.send(newmsg)) ) {
            cerr << "failed to send message for " << query << endl;
        }
      }
    }
    else {
      ++requests[storeKey];
    }

    conn->connection_param = static_cast<void*>(new string(storeKey));
  }

  return MG_MORE; 
}

int handle_gt ( struct mg_connection *conn ) {
  const char* query_string = conn->query_string;
  if ( query_string == NULL ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: please provide query string.");
    return MG_TRUE;
  }

  const map<string, string>& query_params = parse_query_string(conn->query_string);
  map<string, string>::const_iterator query_iter = query_params.find("query");
  if ( query_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: query is not supplied.");
    return MG_TRUE;
  }
  map<string, string>::const_iterator kmer_iter = query_params.find("kmer");
  if ( kmer_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: kmer is not supplied.");
    return MG_TRUE;
  }
  map<string, string>::const_iterator skip_iter = query_params.find("skip");
  if ( skip_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: skip is not supplied.");
    return MG_TRUE;
  }
  map<string, string>::const_iterator pos_iter = query_params.find("pos");
  if ( pos_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: pos is not supplied.");
    return MG_TRUE;
  }

  string output("all");
  map<string, string>::const_iterator output_iter = query_params.find("output");
  if ( output_iter != query_params.end() ) {
    output = output_iter->second;
  }

  const string& query = query_iter->second;
  const string& kmer = kmer_iter->second;
  const string& skip = skip_iter->second;
  const string& pos = pos_iter->second;
  const string& storeKey = getKey(query, Reply_RequestType_SiteMatch, msRplTypes[output]);

  if ( requests.find(storeKey) == requests.end() ) {
    requests[storeKey] = 1;

    Request r;
    r.set_t(Request_RequestType_SiteMatch);
    r.set_rt(msRtnTypes[output]);
    r.set_q(query);
    r.set_k(stoi(kmer));
    r.set_s(stoi(skip));
    r.set_p(stoi(pos));

    zmq::message_t message(r.ByteSize());
    r.SerializeToArray(message.data(), r.ByteSize());
    sender.send(message);
  }
  else {
    ++requests[storeKey];
  }

  conn->connection_param = static_cast<void*>(new string(storeKey));
  
  return MG_MORE; 
}

int handle_kmermatch ( struct mg_connection *conn ) {
  const char* query_string = conn->query_string;
  if ( query_string == NULL ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: please provide query string.");
    return MG_TRUE;
  }

  const map<string, string>& query_params = parse_query_string(conn->query_string);
  map<string, string>::const_iterator query_iter = query_params.find("query");
  if ( query_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: query is not supplied.");
    return MG_TRUE;
  }
  map<string, string>::const_iterator kmer_iter = query_params.find("kmer");
  if ( kmer_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: kmer is not supplied.");
    return MG_TRUE;
  }
  map<string, string>::const_iterator skip_iter = query_params.find("skip");
  if ( skip_iter == query_params.end() ) {
    mg_send_status(conn, 400);
    mg_printf_data(conn, "Bad Request: skip is not supplied.");
    return MG_TRUE;
  }

  string output("all");
  map<string, string>::const_iterator output_iter = query_params.find("output");
  if ( output_iter != query_params.end() ) {
    output = output_iter->second;
  }

  const string& query = query_iter->second;
  const string& kmer = kmer_iter->second;
  const string& skip = skip_iter->second;
  const string& storeKey = getKey(query, Reply_RequestType_KmerMatch, msRplTypes[output]);

  if ( requests.find(storeKey) == requests.end() ) {
    requests[storeKey] = 1;

    Request r;
    r.set_t(Request_RequestType_KmerMatch);
    r.set_rt(msRtnTypes[output]);
    r.set_q(query);
    r.set_k(stoi(kmer));
    r.set_s(stoi(skip));

    zmq::message_t message(r.ByteSize());
    r.SerializeToArray(message.data(), r.ByteSize());
    sender.send(message);
  }
  else {
    ++requests[storeKey];
  }

  conn->connection_param = static_cast<void*>(new string(storeKey));
 
  return MG_MORE; 
}

int request_handler ( struct mg_connection *conn, enum mg_event ev ) {
  switch ( ev ) {
  case MG_AUTH:
    return MG_TRUE;
  case MG_CLOSE:
    if ( conn->connection_param ) {
      const string& storeKey = *(reinterpret_cast<string*>(conn->connection_param));
      if ( requests.find(storeKey) != requests.end() ) {
        --requests[storeKey];
        if ( requests[storeKey] == 0 ) {
          requests.erase(storeKey); // clean up store.
          cerr << "deleting storeKey because of MG_CLOSE: " << storeKey << endl;
        }
      }

      if ( conn->connection_param ) {
        delete reinterpret_cast<string*>(conn->connection_param);
        conn->connection_param = NULL;
      }
    }
    return MG_TRUE;
  case MG_REQUEST:
    {
      if ( strcmp ( conn->request_method, "GET" ) == 0  ) { // GET method
        if ( strcmp ( conn->uri, "/get" ) == 0 ) { // get uri
          return handle_get(conn);
        }
        else if ( strcmp ( conn->uri, "/kmermatch" ) == 0 ) { // kmermatch uri
          return handle_kmermatch(conn);
        }
        else if ( strcmp ( conn->uri, "/gt" ) == 0 ) { // gt uri
          return handle_gt(conn);
        }
        else {
          mg_send_status(conn, 400);
          mg_printf_data(conn, "Bad Request: invalid uri.");
        }
      }
      else {
        mg_send_status(conn, 400);
        mg_printf_data(conn, "Bad Request: only GET method is accepted.");
      }

      return MG_TRUE;
    }
  case MG_POLL:
    if ( conn->connection_param ) {
      const string& storeKey = *(reinterpret_cast<string*>(conn->connection_param));
      
      if ( completed_requests.find(storeKey) != completed_requests.end() && completed_requests[storeKey]->isReady() ) { // results ready
        const string prefix = Reply_RequestType_Name(Reply_RequestType_CountReads);
        if ( storeKey.size() > prefix.size() && storeKey.substr(0, prefix.size()) == prefix ) { // Count reads request
          if ( completed_requests[storeKey]->hasReply() ) { // contains error message
            mg_send_status(conn, 400);
            mg_printf_data(conn, "%s", completed_requests[storeKey]->getReply()->c_str());
          }
          else { // no error message (matched reads within limit)
            // first remove the entry for count reads request
            --requests[storeKey];
            if ( requests[storeKey] == 0 ) {
              requests.erase(storeKey);
            }
            delete reinterpret_cast<string*>(conn->connection_param);
            conn->connection_param = NULL;

            // second build new request (the orginal request)
            string output("all");
            const map<string, string>& query_params = parse_query_string(conn->query_string);
            map<string, string>::const_iterator output_iter = query_params.find("output");
            if ( output_iter != query_params.end() ) {
              output = output_iter->second;
            }
            map<string, string>::const_iterator query_iter = query_params.find("query");
            const string& query = query_iter->second;
            const string& newStoreKey = getKey(query, Reply_RequestType_ExactMatch, msRplTypes[output]);

            if ( requests.find(newStoreKey) == requests.end() ) {
              requests[newStoreKey] = 1;

              Request r;
              r.set_t(Request_RequestType_ExactMatch);
              r.set_rt(msRtnTypes[output]);
              r.set_q(query);
              zmq::message_t message(r.ByteSize());
              r.SerializeToArray(message.data(), r.ByteSize());
              sender.send(message);
            }
            else {
              ++requests[storeKey];
            }

            conn->connection_param = static_cast<void*>(new string(newStoreKey));

            return MG_MORE;
          }
        }
        else { // Normal request
          mg_printf_data(conn, "%s", string("{"+serialise(parse_query_string(conn->query_string))).c_str());
          mg_printf_data(conn, "%s", completed_requests[storeKey]->getReply()->c_str());
        }

        --requests[storeKey];
        if ( requests[storeKey] == 0 ) {
          requests.erase(storeKey);
          // we ought to erase storeKey from completed_requests, but due to the fact the this could be removed when we
          // fetch new set of completed results, we can leave this safely.
        }

        delete reinterpret_cast<string*>(conn->connection_param);
        conn->connection_param = NULL;

        return MG_TRUE;
      }
    }

    return MG_FALSE;
  default:
    return MG_FALSE;
  }
}

int main ( int argc, char **argv ) {
   // Verify that the version of the library that we linked against is
  // compatible with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  signal(SIGSEGV, handler); 

  if ( argc < 2 ) {
    cerr << "Require more arguments to run the programme." << "\n";
    return(EXIT_FAILURE);
  }

  // Get all the configs.
  Config cfg;
  // Read the file. If there is an error, report it and exit.
  try {
    cfg.readFile(argv[1]);
  }
  catch ( const FileIOException &fioex ) {
    cerr << "I/O error while reading file." << "\n";
    return ( EXIT_FAILURE );
  }
  catch ( const ParseException &pex ) {
    cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
         << " - " << pex.getError() << "\n";
    return(EXIT_FAILURE);
  }

  string port;
  string pull_socket_count;
  string pull_socket;
  string push_socket_samples;
  string push_socket;
  string min_query_length_cfg;
  string max_query_length_cfg;
  string max_query_length_count_threshold_cfg;
  string max_match_reads_cfg;
  string workers;
  
  try {
    port = cfg.lookup("port").c_str();
    pull_socket_count = cfg.lookup("pull_socket_count").c_str();
    pull_socket = cfg.lookup("pull_socket").c_str();
    push_socket_samples = cfg.lookup("push_socket_samples").c_str();
    push_socket = cfg.lookup("push_socket").c_str();
    min_query_length_cfg = cfg.lookup("min_query_length").c_str();
    max_query_length_cfg = cfg.lookup("max_query_length").c_str();
    max_query_length_count_threshold_cfg = cfg.lookup("max_query_length_count_threshold").c_str();
    max_match_reads_cfg = cfg.lookup("max_match_reads").c_str();
    workers = cfg.lookup("workers").c_str();
  }
  catch ( const SettingNotFoundException &nfex )
    {
      cerr << "No 'name' setting in configuration file." << "\n";
      return(EXIT_FAILURE);
    }

  workers_count = atoi(workers.c_str());
  min_query_length = atoi(min_query_length_cfg.c_str());
  max_query_length = atoi(max_query_length_cfg.c_str());
  max_query_length_count_threshold = atoi(max_query_length_count_threshold_cfg.c_str());
  max_match_reads = atoi(max_match_reads_cfg.c_str());

  try {
    receiver_count.bind(pull_socket_count.c_str());
    receiver.bind(pull_socket.c_str());
    int hwm = 0;
    sender_samples.setsockopt(ZMQ_SNDHWM, &hwm, sizeof(hwm));
    sender_samples.setsockopt(ZMQ_RCVHWM, &hwm, sizeof(hwm));
    sender_samples.bind(push_socket_samples.c_str());
    sender.bind(push_socket.c_str());
  }
  catch ( const zmq::error_t& er ) {
    cerr << "error in binding with nubmer " << er.num() << "\n";
  }

  std::this_thread::sleep_for(std::chrono::milliseconds( 1 * 6 * 1000));

  pthread_t worker;
  pthread_create(&worker, NULL, worker_routine, NULL);

  pthread_t counter;
  pthread_create(&counter, NULL, counter_routine, NULL);

  struct mg_server *server;

  // Create and configure the server
  server = mg_create_server(NULL, request_handler);
  mg_set_option(server, "listening_port", port.c_str());

  // Serve request. Hit Ctrl-C to terminate the program
  cerr << "start listening on port " << port << "\n";
  for (;;) {
    completed_requests.clear();
    {
      lock_guard<mutex> lg(mtx);
      completed_requests.swap(completed_responses);
    }
    mg_poll_server(server, 10);
  }

  // Cleanup, and free server instance
  mg_destroy_server(&server);

  // Optional:  Delete all global objects allocated by libprotobuf.
  google::protobuf::ShutdownProtobufLibrary();

  return 0;
}
