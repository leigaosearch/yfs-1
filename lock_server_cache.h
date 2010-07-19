#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

class client_req {
 public:
  int clt;
  int seq;
};

class lock_server_cache {

 private:
  std::map<lock_protocol::lockid_t, int> locks;
  std::map<lock_protocol::lockid_t, std::list<int> > pending_requests;
  pthread_mutex_t m;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  lock_protocol::status acquire(int, int, lock_protocol::lockid_t,
      int &);
  lock_protocol::status release(int, int, lock_protocol::lockid_t,
      int &);
  void revoker();
  void retryer();
};

#endif
