// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() {
  // init the root dir
  int i;
  put(1, "", i);
  pthread_mutex_init(&m, NULL);
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  int r = extent_protocol::OK;
  pthread_mutex_lock(&m);
  extent_entry &entry = extent_store[id];
  entry.buf = buf;
  time((time_t *)&entry.attr.mtime);
  time((time_t *)&entry.attr.ctime);
  pthread_mutex_unlock(&m);
  return r;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  int r = extent_protocol::NOENT;
  pthread_mutex_lock(&m);
  if (extent_store.find(id) != extent_store.end()) {
    extent_entry &entry = extent_store[id];
    buf = entry.buf;
    time((time_t *)&entry.attr.atime);
    r = extent_protocol::OK;
  }
  pthread_mutex_unlock(&m);
  return r;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  int r = extent_protocol::NOENT;

  pthread_mutex_lock(&m);
  if (extent_store.find(id) != extent_store.end()) {
    extent_entry &entry = extent_store[id];
    a.size = entry.buf.size();
    a.atime = entry.attr.atime;
    a.mtime = entry.attr.mtime;
    a.ctime = entry.attr.ctime;
    r = extent_protocol::OK;
  }
  pthread_mutex_unlock(&m);
  return r;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  pthread_mutex_lock(&m);
  int ret = extent_store.erase(id); 
  pthread_mutex_unlock(&m);
  return ret ? extent_protocol::OK : extent_protocol::NOENT;
}

