// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"

class extent_server {

 public:
   struct extent_entry {
     // buf is a \n-separated string, in which each line is of form:
     // entry_name:num
     // Note that this restricts usage of the colon symbol (:) in file
     // names.
     std::string buf;
     extent_protocol::attr attr;
   };
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);

 private:
  std::map<extent_protocol::extentid_t, extent_entry> extent_store;
  pthread_mutex_t m;

};

#endif 







