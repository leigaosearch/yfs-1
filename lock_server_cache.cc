// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server_cache::lock_server_cache()
{
  pthread_mutex_init(&m);
}

lock_server_cache::~lock_server_cache()
{
  pthread_mutex_destroy(&m);
}

static void *
revokethread(void *x)
{
  lock_server_cache *sc = (lock_server_cache *) x;
  sc->revoker();
  return 0;
}

static void *
retrythread(void *x)
{
  lock_server_cache *sc = (lock_server_cache *) x;
  sc->retryer();
  return 0;
}

lock_server_cache::lock_server_cache()
{
  pthread_t th;
  int r = pthread_create(&th, NULL, &revokethread, (void *) this);
  assert (r == 0);
  r = pthread_create(&th, NULL, &retrythread, (void *) this);
  assert (r == 0);
}

lock_protocol::status
lock_server_cache::acquire(int clt, unsigned int seq,
    lock_protocol::lockid_t lid, int &unused)
{
  lock_protocol::status r;
  pthread_mutex_lock(&m);
  if (locks.find(lid) != locks.end()) {
    if (locks[lid] == -1 || locks[lid] == clt) {
      locks[lid] = clt;
      r = lock_protocol::OK;
    } else {
      r = lock_protocol::RETRY;
      retry_queue.push_back(acquire_request(clt, seq));
      revoke_queue.push_back(lid);
    }
  } else {
    locks[lid] = clt;
  }
  pthread_mutex_unlock(&m);
  return r;
}

lock_protocol::status
lock_server_cache::release(int clt, unsigned int seq,
    lock_protocol::lockid_t lid, int &unused)
{
  pthread_mutex_lock(&m);
  if (locks.find(lid) != locks.end() && locks[lid]) {
    locks[lid].status = false;
  }
  if (pending_requests.find(lid) != locks.end()) {
    std::list<int> &requests = pending_requests[lid];
  }
  pthread_mutex_unlock(&m);
  return r;
}

void
lock_server_cache::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock

  while (true) {

  }
}


void
lock_server_cache::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.

  while (true) {

  }
}



