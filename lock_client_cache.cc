// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>

cached_lock::cached_lock()
{
}

cached_lock::~cached_lock()
{
  pthread_mutex_destroy(&m);
  pthread_cond_destroy(&cv);
}

static void *
releasethread(void *x)
{
  lock_client_cache *cc = (lock_client_cache *) x;
  cc->releaser();
  return 0;
}

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu), last_seq(0)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // assert(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  /* register RPC handlers with rlsrpc */
  rlsrpc->reg(rlock_protocol::revoke, this, lock_client_cache::revoke);
  rlsrpc->reg(rlock_protocol::retry, this, lock_client_cache::retry);
  pthread_t th;
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  assert (r == 0);
}

lock_client_cache::~lock_client_cache()
{
  delete rlsrpc;
}

void
lock_client_cache::releaser()
{

  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.
  while (true) {
    cl->call(lock_protocol::release, last_seq);
  }

}


// this function blocks until the specified lock is successfully acquired
// or if an expected error occurs
lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  lock_protocol::status r;
  // check if this lock is locally present
  pthread_mutex_lock(&m);
  if (cached_locks.find(lid) != cached_locks.end()) {
    cached_lock &l = cached_locks[lid];
    pthread_mutex_lock(&l.m);
    pthread_mutex_unlock(&m);
    switch (l.status) {
      case cached_lock::FREE:
        r = lock_protocol::OK;
        pthread_mutex_unlock(&l.m);
        break;
      case cached_lock::LOCKED:
        if (l.owner == pthread_self()) {
          // the current thread has already obtained the lock
          r = lock_protocol::OK;
        } else {
          while (l.status == cached_lock::LOCKED) {
            pthread_cond_wait(&l.free_cv, &l.m);
          }
          if (l.status == cached_lock::FREE) {

          } else if (l.status == cached_lock::RELEASING) {

          }
          r = lock_protocol::OK;
          l.status = cached_lock::LOCKED;
          pthread_mutex_unlock(&l.m);
        }
        break;
      case cached_lock::ACQUIRING:
        // there is a pending acquire request. we just wait for its
        // completion
        pthread_cond_wait(&l.cv, &l.m);
        break;
      case cached_lock::RELEASING:
        break;
      default:
        break;
    }
  } else {
    // lock is not locally present, we have to ask the server for it
    int unused;
    while (r = cl->call(lock_protocol::acquire, cl->id(), last_seq, lid,
          unused) == lock_protocol::RETRY) {
      pthread_cond_wait(l.retry_cv, l.m);
    }
    if (r == lock_protocol::OK) {
      // cache this precious lock
      lock_status[lid] = true;
      lock_owners[lid] = pthread_self();
    }
  }
  pthread_mutex_unlock(&m);
  return r;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  pthread_mutex_lock(&m);
}

rlock_protocol::status
lock_client_cache::revoke(lock_protocol::lockid_t lid, int seq)
{
  // server asks me to release lock[lid] ASAP.
  // we append this revocation to the revocation list, which will
  // sooner be processed by the releaser thread.

  lock_protocol::status r = lock_protocol::OK;
  pthread_mutex_lock(&m);
  cached_lock &l = cached_locks[lid];
  l.status = cached_lock::RELEASING;
  pthread_mutex_unlock(&m);
  pthread_mutex_lock(&revoke_m);
  revoke_queue.push_back(lid);
  pthread_mutex_unlock(&revoke_m);
  return r;
}

rlock_protocol::status
lock_client_cache::retry(lock_protocol::lockid_t lid, int seq)
{
  // wake up the thread that is waiting for this lock
  pthread_cond_broadcast(&l.cv);
}

