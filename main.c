#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define PORT 8080

void set_sock_opt(int sock, int level, int opt_name) {
  int opt = 1;
  if (setsockopt(sock, level, opt_name, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
}

int get_socket() {
  struct sockaddr_in address;
  
  int sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

  if (sock == -1) {
    perror("getting socket");
    exit(1);
  }

  // allows reusing of the same address and port
  set_sock_opt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT);
  // all writes should send packets right away
  set_sock_opt(sock, SOL_TCP, TCP_NODELAY);
  
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(sock, (struct sockaddr*) &address, sizeof(address)) == -1) {
    perror("bind failure");
    exit(EXIT_FAILURE);
  }
  
  return sock;
}

int main(int argc, char* argv[]) {
  printf("startup...\n");

  int sock = get_socket();
  
  
    
}
