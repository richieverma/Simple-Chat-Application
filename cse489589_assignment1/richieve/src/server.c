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
#include <errno.h>

#include "../include/logger.h"
#include "../include/global.h"
#include "../include/cs.h"

#define SERVER_PORT 5432
#define MAX_PENDING 5
#define MAX_LINE 256

int server(char *port)
{
  
  
  char *server_port = port, ipstr6[INET6_ADDRSTRLEN], ipstr[INET_ADDRSTRLEN], host[64];
  gethostname(host, 64);
  printf("Hostname: %s\n", host);
  
  int status;
  struct addrinfo hints;
  struct addrinfo *p, *res;
  
  memset(&hints, 0, sizeof hints);
  
  hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  //GETADDRINFO
  
  if ((status = getaddrinfo(host, server_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  
  void *addr = NULL;
  char *ipver = NULL;
  
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
      break;
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
  
  //freeaddrinfo(res); // free the linked list
  
  //SOCKET
  int sockfd;
  if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
  {
    int errsv = errno;
    printf("Socket Error: %d\n", errsv);
    return errsv;    
  }
  
  //BIND
  if (bind(sockfd, p->ai_addr, p->ai_addrlen)== -1){
    int errsv = errno;
    printf("Bind Error: %d\n", errsv);  
    return errsv;
  }
  
  //LISTEN
  if (listen(sockfd, MAX_PENDING) == -1){
    int errsv = errno;
    printf("Listen Error: %d\n", errsv);  
    return errsv;    
  }
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  addr_size = sizeof their_addr;
  int new_fd;
  
  //ACCEPT
  if ((new_fd= accept(sockfd, (struct sockaddr *)&their_addr, &addr_size))== -1){
    int errsv = errno;
    printf("Accept Error: %d\n", errsv);  
    return errsv;    
  }
  
  //SEND
  int bytes_sent = send(new_fd, "Hello", strlen("Hello"), 0);
  close(new_fd);
  close(sockfd);
  return 0;
  
}






//CLIENT
int client(char *port)
{  
  char command_str[20];
  int sockfd;
  
  while (1){
    scanf("%s", command_str);
    
    if (strcmp(command_str, "AUTHOR") == 0){
      cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
      cse4589_print_and_log("I, richieve, have read and understood the course academic integrity policy.\n");
      cse4589_print_and_log("[%s:END]\n", command_str);
    }
    
    else if (strcmp(command_str, "LOGIN") == 0){
      char host[INET_ADDRSTRLEN];
      char server_port[5], ipstr6[INET6_ADDRSTRLEN], ipstr[INET_ADDRSTRLEN];
      
      scanf("%s %s", host, server_port);
      printf("Hostname: %s\n", host);
      
      int status;
      
      struct addrinfo hints;
      struct addrinfo *p, *res;
      
      memset(&hints, 0, sizeof hints);
      
      hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
      hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
      hints.ai_flags = AI_PASSIVE; // fill in my IP for me
      
      //GETADDRINFO
      
      if ((status = getaddrinfo(host, server_port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
      }
      
      void *addr = NULL;
      char *ipver = NULL;
      
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
          break;
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
      
      //freeaddrinfo(res); // free the linked list
      
      //SOCKET
      
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
      {
        int errsv = errno;
        printf("Socket Error: %d\n", errsv);
        return errsv;    
      }
      
      //CONNECT
      if (connect(sockfd, p->ai_addr, p->ai_addrlen)== -1){
        int errsv = errno;
        printf("Connect Error: %d\n", errsv);  
        return errsv;
      }
      
      char server_reply[6000];
      int bytes_recv = recv(sockfd, server_reply, 6000, 0);
      printf("Received: %s", server_reply);
      close(sockfd);
      
      
    }
    else if (strcmp(command_str, "EXIT") == 0){
      cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
      cse4589_print_and_log("[%s:END]\n", command_str);    
      return 0;
    }
    
  }
  return 0;
  
}