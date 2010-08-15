// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

// TODO use rwlock to achieve better performance

extent_client::extent_t::extent_t()
  : dirty(false), removed(false)
{

}

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
	make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }

  pthread_mutex_init(&cache_m, NULL);
}

extent_client::~extent_client()
{
  pthread_mutex_destroy(&cache_m);
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end() && (ret = _fetch(eid)) !=
      extent_protocol::OK) {
    pthread_mutex_unlock(&cache_m);
    return ret;
  } else {
    extent_t &entry = cache[eid];
    if (entry.removed) {
      // the entry is removed, so it shouldn't be used anymore
      ret = extent_protocol::NOENT;
    } else {
      buf = entry.buf;
    }
  }
  pthread_mutex_unlock(&cache_m);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &a)
{
  extent_protocol::status ret = extent_protocol::OK;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end() && (ret = _fetch(eid)) !=
      extent_protocol::OK) {
    pthread_mutex_unlock(&cache_m);
    return ret;
  } else {
    extent_t &entry = cache[eid];
    if (entry.removed) {
      ret = extent_protocol::NOENT;
    } else {
      a = entry.attr;
      a.size  = entry.buf.size();
    }
  }
  pthread_mutex_unlock(&cache_m);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r;
  //ret = cl->call(extent_protocol::put, eid, buf, r);
  _put(eid, buf);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r;
  //ret = cl->call(extent_protocol::remove, eid, r);
  if (cache.find(eid) != cache.end()) {
    cache[eid].dirty = true;
    cache[eid].removed = true;
  }
  return ret;
}

extent_protocol::status
extent_client::pget(extent_protocol::extentid_t eid, off_t offset,
          size_t nbytes, std::string &buf)
{
  extent_protocol::status r;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end() && (r = _fetch(eid)) !=
      extent_protocol::OK) {
    pthread_mutex_unlock(&cache_m);
    return r;
  }
  extent_t &entry = cache[eid];
  if (entry.removed) {
    pthread_mutex_unlock(&cache_m);
    return extent_protocol::NOENT;
  }
  size_t len = entry.buf.size();
  if (offset < len) {
    size_t can_read = len - offset;
    size_t actual_read = can_read > nbytes ? nbytes : can_read;
    buf = entry.buf.substr(offset, actual_read);
    time((time_t *)&entry.attr.atime);
    entry.dirty = true;
    r = extent_protocol::OK;
  } else {
    r = extent_protocol::IOERR; 
  }
  pthread_mutex_unlock(&cache_m);
  return r;
}


extent_protocol::status
extent_client::update(extent_protocol::extentid_t eid, std::string &data,
    off_t offset, size_t &bytes_written)
{
  extent_protocol::status r = extent_protocol::OK;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end() && (r = _fetch(eid)) !=
      extent_protocol::OK) {
    pthread_mutex_unlock(&cache_m);
    return r;
  }
  extent_t &entry = cache[eid];
  size_t len = entry.buf.size();
  size_t nbytes = data.size();
  size_t end = offset + nbytes;
  if (end > len) {
    // we need to resize the string 
    entry.buf.resize(end);
  }
  entry.buf.replace(offset, nbytes, data);
  time((time_t *)&entry.attr.mtime);
  bytes_written = nbytes;
  entry.dirty = true;
  pthread_mutex_unlock(&cache_m);
  return r;
}

extent_protocol::status
extent_client::resize(extent_protocol::extentid_t eid, off_t new_size)
{
  int r;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end() && (r = _fetch(eid)) !=
      extent_protocol::OK) {
    pthread_mutex_unlock(&cache_m);
    return r;
  }
  extent_t &entry = cache[eid];
  entry.buf.resize(new_size);
  entry.attr.mtime = time(NULL);
  entry.dirty = true;
  pthread_mutex_unlock(&cache_m);
  return r;
}

extent_protocol::status
extent_client::poke(extent_protocol::extentid_t eid)
{
  extent_protocol::status r = extent_protocol::NOENT;
  pthread_mutex_lock(&cache_m);
  if (cache.find(eid) == cache.end()) {
    r = _fetch(eid);
  }
  pthread_mutex_unlock(&cache_m);
  return r;
}

// assume cache_m ownership
extent_protocol::status
extent_client::_fetch(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret;
  extent_t extent;
  if ((ret = cl->call(extent_protocol::get, eid, extent.buf)) ==
      extent_protocol::OK) {
    if ((ret = cl->call(extent_protocol::getattr, eid, extent.attr)) ==
        extent_protocol::OK) {
      extent.dirty = false;
      cache[eid] = extent;
    }
  }
  return ret;
}

void
extent_client::flush(void)
{
  pthread_mutex_lock(&cache_m);
  std::map<extent_protocol::extentid_t, extent_t>::iterator it; 
  for (it = cache.begin(); it != cache.end(); ++it) {
    int u;
    extent_protocol::extentid_t eid = it->first;
    extent_t &extent = it->second;
    if (extent.removed) {
      if (cl->call(extent_protocol::remove, eid, u) != extent_protocol::OK) {
        printf("unable to remove eid %llu\n", eid);
      }
    } else if (extent.dirty) {
      if (cl->call(extent_protocol::put, eid, extent.buf, u) !=
          extent_protocol::OK || cl->call(extent_protocol::setattr, eid,
            extent.attr, u) != extent_protocol::OK) {
        printf("unable to update extent server for %llu\n", eid);
      }
    }
  }
  for (it = cache.begin(); it != cache.end(); ++it) {
    cache.erase(it);
  }
  pthread_mutex_unlock(&cache_m);
}

void
extent_client::dorelease(lock_protocol::lockid_t lid)
{
  this->flush();
}

void
extent_client::_put(extent_protocol::extentid_t id,
    std::string &buf)
{
  pthread_mutex_lock(&cache_m);
  bool updating = cache.find(id) != cache.end();
  extent_t &entry = cache[id];
  entry.buf = buf;
  if (updating) {
    time((time_t *)&entry.attr.atime);
  } else {
    memset(&entry.attr, 0, sizeof(extent_protocol::attr));
  }
  time((time_t *)&entry.attr.mtime);
  time((time_t *)&entry.attr.ctime);
  entry.dirty = true;
  pthread_mutex_unlock(&cache_m);
}

void
extent_client::_put(extent_protocol::extentid_t id,
    const char *buf)
{
  std::string temp(buf);
  _put(id, temp);
}

