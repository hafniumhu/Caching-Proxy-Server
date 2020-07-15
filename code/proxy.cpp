#include <signal.h>
#include "proxy.h"

using namespace std;
//signal(SIGPIPE, SIG_IGN);
Logger writer;

void thread_initial(Proxy proxy, int socket_fd, int client_connection_fd, string ip, int id)
{
  proxy.initial(socket_fd, client_connection_fd, ip, id);
  proxy.initial_process();
}

int main()
{
  const char *hostname = NULL;
  const char *port = "12345";
  struct addrinfo host_info;
  struct addrinfo *host_info_list;
  int socket_fd = create_server(hostname, port, host_info, host_info_list);
  int id = 1;
  while (true) {
    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    int client_connection_fd = accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (client_connection_fd == -1)
    {
      writer.addMessage(id, "Failure accepting browser connection");
      continue;
    }
    string ip = inet_ntoa(((struct sockaddr_in *)&socket_addr)->sin_addr);
    // deal with broken pipe
    sigset_t signal_mask;
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGPIPE);
    int rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0)
    {
      printf("block sigpipe error\n");
    }
    Proxy proxy;
    try{
      thread thd = thread(thread_initial, proxy, socket_fd, client_connection_fd, ip, id);
      thd.detach();
    }
    catch(...) {
      cout << "some external exception happens" << id << endl;
    }
    id++;
  }
  close(socket_fd);
  return 0;
}