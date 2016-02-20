#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
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

#define MAX_PENDING 5
#define MAX_LINE 256
#define STDIN 0

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int server(char *port)
{
  
  //Get hostname to use for getaddrinfo
  char *server_port = port, server_ip[INET_ADDRSTRLEN], remoteIP[INET_ADDRSTRLEN], host[64];
  int status;
  gethostname(host, 64);
  printf("Hostname: %s\n", host);
  
  struct addrinfo hints, *p, *res;
  
  
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
    if (p->ai_family == AF_INET) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      // convert the IP to a string
      inet_ntop(p->ai_family, addr, server_ip, sizeof server_ip);
      printf("SERVER IP: %s\n", server_ip);
      break;
    }
  }
  
  if (p == NULL){
    printf("Getaddrinfo Error\n");
    return 1;
  }
  
  //freeaddrinfo(res); // free the linked list
  
  //SOCKET
  int server_listener;
  if ((server_listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
  {
    int errsv = errno;
    printf("Socket Error: %d\n", errsv);
    return errsv;    
  }
  
  //BIND
  if (bind(server_listener, p->ai_addr, p->ai_addrlen)== -1){
    int errsv = errno;
    printf("Bind Error: %d\n", errsv);  
    return errsv;
  }
  
  //LISTEN
  if (listen(server_listener, MAX_PENDING) == -1){
    int errsv = errno;
    printf("Listen Error: %d\n", errsv);  
    return errsv;    
  }
  
  //SELECT
  fd_set master; // master file descriptor list
  fd_set read_fds; // temp file descriptor list for select()
  int fdmax; // maximum file descriptor number
  int newfd; // newly accepted socket descriptor
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;
  char buf[256]; // buffer for client data
  int nbytes;
  
  FD_ZERO(&master); // clear the master and temp sets
  FD_ZERO(&read_fds);
  
  FD_SET(server_listener, &master);
  FD_SET(STDIN, &master);
  
  fdmax = (server_listener>STDIN?server_listener:STDIN);
  
  char command_str[20];
  
  while(1){
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
      printf("Select Error\n");
      return 4;
    }
    
    // run through the existing connections looking for data to read
    for(int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) { // we got one!!
        if (i == STDIN){
          scanf("%s", command_str); // Read data from STDIN
          
          //AUTHOR
          if (strcmp(command_str, "AUTHOR") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("I, richieve, have read and understood the course academic integrity policy.\n");
            cse4589_print_and_log("[%s:END]\n", command_str);
          } 
          
          //IP
          else if (strcmp(command_str, "IP") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", server_ip);
            cse4589_print_and_log("[%s:END]\n", command_str);            
          }
          
          //PORT
          else if (strcmp(command_str, "PORT") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", server_port);
            cse4589_print_and_log("[%s:END]\n", command_str);            
          } 
        }
        
        // handle new connections
        else if (i == server_listener) {
          addrlen = sizeof remoteaddr;
          //ACCEPT
          newfd = accept(server_listener, (struct sockaddr *)&remoteaddr, &addrlen);
          if (newfd == -1) {
            printf("Accept Error");
            continue;
          }
          else {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax) { // keep track of the max
              fdmax = newfd;
            }
            printf("selectserver: new connection from %s on "
                   "socket %d\n",
                   inet_ntop(remoteaddr.ss_family,
                             get_in_addr((struct sockaddr*)&remoteaddr),
                             remoteIP, INET_ADDRSTRLEN),
                   newfd);
          }
        } 
        else {
          // handle data from a client
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
              // connection closed
              printf("selectserver: socket %d hung up\n", i);
            }
            else {
              perror("recv");
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          }
          else {
            // we got some data from a client 
            printf("Data from Client: %s\n", buf);
          }//If data was received or connection was closed
        }//If server_listener
      }//If to check if there is data available on any socket
    } // For Loop
  }//Infinite While loop
  close(server_listener);
  return 0;
  
}






//CLIENT
int client(char *port)
{  
  char command_str[20], *client_ip, *client_port, host[64];
  int sockfd, status;
  
  struct addrinfo hints;
  struct addrinfo *p, *res;
  void *addr = NULL;
  
  printf("ClentHello\n");
  gethostname(host, 64);
    printf("After\n");
  printf("Hostname: %s\n", host);
  
  //NEED TO FIND CLIENT IP AND PORT
  
  //SELECT
  fd_set master; // master file descriptor list
  fd_set read_fds; // temp file descriptor list for select()
  int fdmax = 0; // maximum file descriptor number
  
  FD_ZERO(&master); // clear the master and temp sets
  FD_ZERO(&read_fds);
  
  FD_SET(STDIN, &master);
  
  while (1){
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
      int errsv = errno;
      printf("Select Error %d\n", errsv);
      return 4;
    }
    
    // run through the existing connections looking for data to read
    for(int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) { // we got one!!
        if (i == STDIN){
          scanf("%s", command_str); // Read data from STDIN
          
          
          if (strcmp(command_str, "AUTHOR") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("I, richieve, have read and understood the course academic integrity policy.\n");
            cse4589_print_and_log("[%s:END]\n", command_str);
          }
          
          else if (strcmp(command_str, "LOGIN") == 0){
            char server_host[INET_ADDRSTRLEN];
            char server_port[5], ipstr[INET_ADDRSTRLEN];
            
            scanf("%s %s", server_host, server_port);
            
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
            hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
            hints.ai_flags = AI_PASSIVE; // fill in my IP for me
            
            //GETADDRINFO
            
            if ((status = getaddrinfo(server_host, server_port, &hints, &res)) != 0) {
              fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
              exit(1);
            }
            
            for(p = res;p != NULL; p = p->ai_next) {
              // get the pointer to the address itself,
              // different fields in IPv4 and IPv6:
              if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                addr = &(ipv4->sin_addr);
                // convert the IP to a string and print it
                inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
                printf(" %s: %s\n", "IPv4", ipstr);
                break;
              }
              
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
            FD_SET(sockfd, &master);
            fdmax = (fdmax < sockfd? sockfd: fdmax);
          }
          
          else if (strcmp(command_str, "EXIT") == 0){
            close(sockfd);
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
            return 0;
          }
          
          else if (strcmp(command_str, "LOGOUT") == 0){
            close(sockfd);
            FD_CLR(i, &master);
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
          }          
          
          else if (strcmp(command_str, "SEND") == 0){
            send(sockfd, "Hello", 5, 0); 
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
          }//If for commands from STDIN
        }//Check if input is from STDIN or other socket
        else {
          char server_reply[6000];
          int bytes_recv = recv(sockfd, server_reply, 6000, 0);
          printf("Received: %s\n", server_reply);          
        }
      }//Check if FD is set
    }//For loop 
  }//Infinite while Loop
  return 0;
  
}


