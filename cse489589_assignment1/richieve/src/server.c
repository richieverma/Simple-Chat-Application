#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

int main(int argc, char *argv[])
{
  int status;
  
  char *server_port = argv[1], ipstr6[INET6_ADDRSTRLEN], ipstr[INET_ADDRSTRLEN], host[64];
  gethostname(host, 64);
  printf("Hostname: %s\n", host);
  
  struct addrinfo hints;
  struct addrinfo *p, *res;
  
  memset(&hints, 0, sizeof hints);
  
  hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  if ((status = getaddrinfo(host, server_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  
  for(p = res;p != NULL; p = p->ai_next) {
    printf("Loop\n");
    void *addr;
    char *ipver;
    // get the pointer to the address itself,
    // different fields in IPv4 and IPv6:
    if (p->ai_family == AF_INET) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      ipver = "IPv4";
      // convert the IP to a string and print it
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf(" %s: %s\n", ipver, ipstr);
    }
    /*
     IPv6 not needed
     else { // IPv6
      struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
      addr = &(ipv6->sin6_addr);
      ipver = "IPv6";
      inet_ntop(p->ai_family, addr, ipstr6, sizeof ipstr6);
      printf(" %s: %s\n", ipver, ipstr6);
    }
    */
    
  }
  
  freeaddrinfo(res); // free the linked list
  return 0;
  
}