#ifndef __PROXY_H__
#define __PROXY__

#include "cache.h"

using namespace std;

Cache cache;
int status,status1,status2,status3,status4,status5;

class Proxy
{
private:
  int socket_fd, client_connection_fd, port_num, ID;
  string ip;
  Logger writer;

public:
  Proxy(){}
  void initial(int _socket_fd, int _client_connection_fd, string _ip, int id) {
    socket_fd = _socket_fd;
    client_connection_fd = _client_connection_fd;
    ip = _ip;
    ID = id;
  }
  bool isBeforeMaxage(string s1, time_t maxage){
    struct tm tm1;
    if (strptime(s1.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm1) == NULL)
    {
      cerr << "strptime failed" << endl;
    }
    time_t t1 = mktime(&tm1);
    time_t now = time_t(&now);
    return now - t1 < maxage;
  }
  // time
  void time_startlog (Request P) {
    time_t rawtime;
    struct tm *ptminfo;
    time(&rawtime);
    ptminfo = localtime(&rawtime);
    writer.addMessage(ID, P.head_line + " from " + ip + " @ " + asctime(ptminfo));
  }

  // convert vector to string
  string vec2Str(vector<char> buffer) {
    string ans;
    for (char c : buffer) {
        ans += c;
    }
    return ans;
  }

  // initial
  void initial_process() {
    int length = 0;
    vector<char> buffer(MAX_PORT);
    vector<char>::iterator i = buffer.begin();
    char *ptr_buff = &*i;
    while (true) {
      length = recv(client_connection_fd, ptr_buff, 2048, 0);
      if (length < 0){
        writer.addMessage(ID, "Failure receiveing from browser");
        return;
      }

      if (vec2Str(buffer).find("\r\n\r\n") != string::npos){
        break;
      }

      buffer.resize(length + MAX_PORT);
      ptr_buff += length;
    }

    string buffer_str = vec2Str(buffer);

    if (!if_end(buffer_str,ID,client_connection_fd)) {
      writer.addMessage(ID, "Failure finding header");
      return;
    }
    executeMethod(buffer, buffer_str);
    close(client_connection_fd);
  }

  // helper for  "GET"
  bool getHelper(Request r, string buffer_str) {
    if (!cache.hasResponse(buffer_str) || r.no_store || r.no_cache) {
      writer.addMessage(ID, "not in cache");
      return false;
    }
    else if (cache.hasResponse(buffer_str) && r.max_age != -1 && !isBeforeMaxage(cache.respMap[buffer_str].date_time, r.max_age)) {
      return false;
    }
    else if (cache.hasResponse(buffer_str) && cache.respMap[buffer_str].revalidation) {
      writer.addMessage(ID, "in cache, requires validation");
      return false;
    }
    else {
      return true;
    }
  }
  
  // helper for methods
  void executeMethod(vector<char> buffer, string buffer_str) {
    Request P(buffer);
    cache.initial(buffer);
    time_startlog(P);
    
    if (P.http_method == "CONNECT"){
      http_connect(P);
    }
    else if(P.http_method == "GET") {
      if (!getHelper(P, buffer_str)) {
        getFromServer(P, buffer_str, false);
      }
      else {
        getFromCache(P,buffer_str);
      }
    }
    else if (P.http_method == "POST"){
      getFromServer(P, buffer_str, true);
    }
  }

  void http_connect(Request P){
    int status;
    string succ = "HTTP/1.1 200 OK\r\n\r\n";
    int socket_fd = connect_server(P,succ,client_connection_fd);
    if (socket_fd == -1) {
      return;
    }
    fd_set readfds;
    while (true)
    {
      FD_ZERO(&readfds);
      FD_SET(socket_fd, &readfds);
      FD_SET(client_connection_fd, &readfds);
      int select_fd = max(socket_fd,client_connection_fd) + 1;
      int result = select(select_fd, &readfds, NULL, NULL, NULL);
      if (result < 0)
      {
        writer.addMessage(ID, "Failure select");
        break;
      }
      if (FD_ISSET(client_connection_fd, &readfds))
      {
        if (!communicate(client_connection_fd,socket_fd)) {
          break;
        }
      }
      else if (FD_ISSET(socket_fd, &readfds))
      {
        if (!communicate(socket_fd,client_connection_fd)) {
          break;
        }
      }
    }
    close(socket_fd);
    writer.addMessage(ID, "Tunnel closed");
    return;
  }

  // check if header exist
  bool checkHeader(string str) {
    if (str.find("\r\n\r\n") == string::npos){
      writer.addMessage(ID, "Failure finding header");
      char msg[] = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
      send(client_connection_fd, msg, strlen(msg), 0);
      close(socket_fd);
      return false;
    }
    return true;
  }

  void resizeHelper(int &finish, int &sz, int &total, vector<char> &data) {
    Request request(data);
    finish = request.total_len;
    data.resize(finish);
    sz = finish - total;
  }

  bool receiveHelper(Request P, vector<char> &buffer, int size, bool getCache) {
    int tot = 0;
    int len = INT_MAX;
    int finish = 0;
    buffer.resize(size);
    while(len > 0) {
      char *head = buffer.data();
      len = recv(socket_fd, head + tot, size, 0);
      if (len < 0){
        return false;
      }
      tot += len;
      if (getCache &&!checkHeader(vec2Str(buffer))) {
        return false;
      }
      if (vec2Str(buffer).find("chunked") != string::npos){
        chunkHelper(buffer, socket_fd, len, P.server);
        return false;
      }
      resizeHelper(finish, size, tot, buffer);
      if (tot == finish){
        return true;
      }
    }
    return true;
  }

  void forwardChunk(string chunk_str, int serverfd, int browserfd, int size) {
    int len_chunk = INT_MAX;
    vector<char> buffer;
    buffer.resize(size);
    while (len_chunk > 0){
      if (chunk_str.find("0\r\n\r\n") != string::npos  || len_chunk < 0){
        writer.addMessage(ID, "Failure receiving from server");
        return;
      }
      memset(buffer.data(), 0, size);
      len_chunk = recv(serverfd, buffer.data(), 2048, 0);
      if (send(browserfd, buffer.data(), len_chunk, 0) != -1){
        chunk_str.append(vec2Str(buffer));
      }
      else {
        writer.addMessage(ID, "Failure sending to browser");
        return;
      }
    }
  }

  void chunkHelper(vector<char> chunk_buffer, int fd, int len, string server){
    string chunk_str = vec2Str(chunk_buffer);
    string header = chunk_str.substr(0, chunk_str.find("\r\n"));
    writer.addMessage(ID, "Received " + header + " from " + server);
    writer.addMessage(ID, "Responding " + header);
    if (send(client_connection_fd, chunk_buffer.data(), len, 0) == -1){
      writer.addMessage(ID, "Failure sending to browser");
    }
    else {
      forwardChunk(chunk_str, fd, client_connection_fd, 2048);
    }
    close(fd);
  }

  void getFromCache(Request P, string request){
    int socket_fd = connect_server(P,request,-1);
    if (socket_fd == -1) {
      return;
    }
    vector<char> buffer;
    if (receiveHelper(P, buffer, 1024, true)) {
      if (!cache.find_cache(client_connection_fd, request, buffer, ID)){ 
        getFromServer(P, request, false);
      }
    }
    close(socket_fd);
  }

  void getFromServer(Request P, string request, bool method){
    writer.addMessage(ID, "Requesting " + P.head_line + " from " + P.server);
    int socket_fd = connect_server(P,request,-1);
    if (socket_fd == -1) {
      return;
    }

    vector<char> buffer;
    if (receiveHelper(P, buffer, 1024, false)) {
      string check = vec2Str(buffer);
      string header = check.substr(0, check.find("\r\n"));
      writer.addMessage(ID, "Received " + header + " from " + P.server);

      if (method == false){
        cache.add_cache(request, buffer.size(), buffer, ID);
      }
      const char *resp = buffer.data();
      if (send(client_connection_fd, resp, buffer.size(), 0) == -1){
        writer.addMessage(ID, "Failure sending to browser");
      }
      else {
        writer.addMessage(ID, "Responding " + header);
      }
    }
    close(socket_fd);
  }

  int connect_server (Request P, string info, int client_connection_fd) {
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    memset(&host_info, 0, sizeof(host_info));
    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    string server = P.server;
    int port_num = P.port;
    status = getaddrinfo(server.c_str(), to_string(port_num).c_str(), &host_info, &host_info_list);
    if (status != 0) {
      cerr << "Error: getaddrinfo()" << endl;
      return -1;
	  }
    socket_fd = socket(host_info_list->ai_family,
                       host_info_list->ai_socktype,
                       host_info_list->ai_protocol);
    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1){
      writer.addMessage(ID, "Failure connecting server");
      close(socket_fd);
      return -1;
    }
    const char *information = info.c_str();
    if (client_connection_fd == -1) {
      status = send(socket_fd, information, strlen(information), 0);
    }
    else {
      status = send(client_connection_fd, information, strlen(information), 0);
    }   
    if (status == -1){
      writer.addMessage(ID, "Failure sending to browser");
      close(socket_fd);
      return -1;
    }
    return socket_fd;
  }

  // check if it's end
  bool if_end (string info, int ID, int client_connection_fd) {
    if ((info.find("\r\n\r\n")) == string::npos) {
        char msg[] = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_connection_fd, msg, strlen(msg), 0);
        return false;
    }
    else {
      return true;
    }
  }

  bool communicate (int client_connection_fd, int socket_fd) {
    int recv_len;
    char tmp[MAX_PORT];
    memset(tmp, 0, MAX_PORT);
    recv_len = recv(client_connection_fd, tmp, MAX_PORT, 0);
    if (recv_len < 0){
      writer.addMessage(ID, "Failure receiving from server");
      return false;
    }
    else if (recv_len == 0){
      close(socket_fd);
      writer.addMessage(ID, "Tunnel closed");
      return false;
    }
    recv_len = send(socket_fd, tmp, recv_len, 0);
    if (recv_len < 0){
      writer.addMessage(ID, "Failure sending to browser");
      return false;
    }
    return true;
  }
};

int create_server(const char *hostname, const char *port, struct addrinfo host_info, struct addrinfo *host_info_list) {
  int socket_fd;
  memset(&host_info, 0, sizeof(host_info));
  host_info.ai_family = AF_UNSPEC;
  host_info.ai_socktype = SOCK_STREAM;
  host_info.ai_flags = AI_PASSIVE;
  status = getaddrinfo(hostname, port, &host_info, &host_info_list);
  if (status != 0) {
		cerr << "Error: getaddrinfo()" << endl;
		return EXIT_FAILURE;
	}
  socket_fd = socket(host_info_list->ai_family,
                     host_info_list->ai_socktype,
                     host_info_list->ai_protocol);
  int oviet = 1;
  status1 = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &oviet, sizeof(int));
  status2 = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
  status3 = listen(socket_fd, 1024);
  if (status1 == -1 || status2 == -1 || status3 == -1){
      cerr << "Error" << endl;
      return EXIT_FAILURE;
  }
  return socket_fd;
}

#endif
