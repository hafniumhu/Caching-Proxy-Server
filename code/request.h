#ifndef __REQUEST_H__
#define __REQUEST_H__
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>
#include <fstream>
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <ostream>
#include <limits.h>
#include <time.h>
#include <unordered_map>
#include <cstdlib>
#include <string>
#include <vector>
#include <thread>
#define TAIL 4
#define MAX_PORT 65536
#define LEN_HOST 6
#define LEN_MAXAGE 8
#define LEN_CONTENT 16
#define LEN_EXPIRE 9
#define LEN_DATE 6
#define LEN_ETAG 6
#define LEN_MODIFY 15

using namespace std;
class Request
{
public:
  string server, host, http_method, head_line;
  time_t max_age;
  int port, content_len, header_len, total_len;
  bool no_store, no_cache;


  Request(vector<char> msg) {
    string request;
    for (char c: msg) {
      request += c;
    }
    head_line = request.substr(0, request.find("\r\n"));
    http_method = request.substr(0,request.find(" "));
    string host_tmp = "Host: ";
    string tmp = request.substr(request.find(host_tmp) +LEN_HOST);
    host = tmp.substr(0,tmp.find("\r\n")); 
    if (host.find(":") != string::npos) {
      server = host.substr(0, host.find(":"));
      string temp1 = host.substr(host.find(":") + 1);
      port = stoi(temp1.substr(0, temp1.find("\r\n")));
    }
    else {
      server = host;
      port = 80;
    }
  
    if (request.find("max-age") != string::npos) {
      string age_temp = request.substr(request.find("max-age=") + LEN_MAXAGE);
      max_age = stoi(age_temp.substr(0, age_temp.find("\r\n")));
    }
    else {
      max_age = -1;
    }
    if (request.find("no-store") != string::npos) {
      no_store = true;
    }
    if (request.find("no-cache") != string::npos) {
      no_cache = true;
    }

    if (request.find("Content-Length:") != string::npos) {
      string content_tmp = request.substr(request.find("Content-Length:") + LEN_CONTENT);
      content_len = stoi(content_tmp.substr(0, content_tmp.find("\r\n")));
    }
    else {
      content_len = -1;
    }
    string temp_len = request.substr(0, request.find("\r\n\r\n"));
    header_len = temp_len.size() + TAIL;
    total_len = content_len + header_len;
  }
};
#endif