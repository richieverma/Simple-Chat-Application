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


//FIND CLIENT IN LIST BY IP
struct Client* findClient_ip(struct Client *allClients, char * register_ip){
  struct Client *temp = NULL;
  
  for(temp = allClients; temp != NULL; temp = temp->next){
    if (strcmp(register_ip, temp->ip) == 0){
      return temp;
    }
  }
  return temp;
}

//FIND CLIENT IN LIST BY SOCKET ID
struct Client* findClient_socket(struct Client *allClients, int socket){
  struct Client *temp = NULL;
  for(temp = allClients; temp != NULL; temp = temp->next){
    if (temp ->csocket == socket){
      return temp;
    }
  }
  return temp;
}


//REGISTER DETAILS OF NEW CLIENT
struct Client* registerClient(struct Client *allClients, struct Client *newClient){
  struct Client *it, *prev;
  
  //INSERT IN ORDER OF INCREASING PORT 
  //IF THERE ARE NO REGISTERED CLIENTS YET
  if (allClients == NULL){
    allClients = newClient;
    return allClients;
  }
  
  //FIRST CLIENT HAS A GREATER PORT THAN THE NEW CLIENT
  if (allClients->port >= newClient->port){
    newClient->next = allClients;
    allClients = newClient;
    return allClients;
  }
  
  for(it = allClients->next, prev = allClients; it != NULL; it = it->next, prev = prev->next){
    if (it->port >= newClient->port){
      break;
    }
  }  
  
  newClient->next = it;
  prev->next = newClient;
  
  return allClients;
}







//SERVER
int server(char *port)
{
  struct Client *allClients = NULL;
  
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
  
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 500000;
  
  char command_str[20];
  
  while(1){
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
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
            cse4589_print_and_log("PORT:%s\n", server_port);
            cse4589_print_and_log("[%s:END]\n", command_str);            
          } 
          
          //LIST
          else if (strcmp(command_str, "LIST") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            struct Client *it;
            int j = 1;
            for(it = allClients; it != NULL; it = it->next){
              if (it->online == 1){
                cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", j++, it->host, it->ip, it->port);
              }
            }
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
            
            printf("selectserver: new connection from %s on socket %d\n",
                  inet_ntop(remoteaddr.ss_family,
                  get_in_addr((struct sockaddr*)&remoteaddr),
                  remoteIP, INET_ADDRSTRLEN),newfd);
          }
          
          //Send List of all online Clients
          char send_list[256];
          struct Client *it;
          for(it = allClients; it != NULL; it = it->next){
            memset ( send_list, 0, sizeof send_list );
            if (it->online == 1){
              snprintf(send_list, sizeof(send_list),"<LIST> %s %s %d", it->host, it->ip, it->port);
              printf("Send String: %s\n", send_list);
              send(newfd, send_list, strlen(send_list), 0);
            }
          }
        } 
        
        else {
          // handle data from a client
          memset ( buf, 0, sizeof buf );
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            // got error or connection closed by client
            if (nbytes == 0) {
              // connection closed
              printf("selectserver: socket %d hung up\n", i);
            }
            else {
              perror("recv");
            }
            
            if (findClient_socket(allClients, i)){
              findClient_socket(allClients, i)->online = 0;
            }
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          }
          else {
            // we got some data from a client 
            printf("Data from Client: %s\n", buf);
            char to_be_tokenized[256];
            strcpy(to_be_tokenized, buf);
            
            char *token;
            token = strtok(to_be_tokenized, " ");
            
            //REGISTER /LOGIN RECEIVED FROM CLIENT
            if (strcmp(token, "<REGISTER>") == 0){
              //Populate new Client to register
              struct Client *c = (struct Client*)malloc(sizeof(struct Client));
              strcpy(c->ip, strtok(NULL, " "));
              strcpy(c->host, strtok(NULL, " "));
              c->port = atoi(strtok(NULL, " "));
              c->msgs_sent = 0;
              c->msgs_recv = 0;
              c->online = 1;
              c->csocket = i;
              c->blocked = NULL;
              c->mlist = NULL;
              
              //CHANGE STATUS TO ONLINE IF ALREADY REGISTERED
              if (findClient_ip(allClients, c->ip)){
                findClient_ip(allClients, c->ip)->online = 1;
                free(c);
              }
              //REGISTER NEW CLIENT
              else{
                allClients = registerClient(allClients, c);
              }
            }
            
            //REFRESH RECEIVED FROM CLIENT
            else if (strcmp(token, "<REFRESH>") == 0){
              //Send List of all online Clients
              char send_list[256];
              struct Client *it;
              for(it = allClients; it != NULL; it = it->next){
                memset ( send_list, 0, sizeof send_list );
                if (it->online == 1){
                  snprintf(send_list, sizeof(send_list),"<LIST> %s %s %d", it->host, it->ip, it->port);
                  printf("Send String: %s\n", send_list);
                  printf("Send list to%s\n", findClient_socket(allClients, i)->ip);
                  send(i, send_list, strlen(send_list), 0);
                }
              } 
            }            
            
            //SEND RECEIVED FROM CLIENT
            else if (strcmp(token, "<SEND>") == 0){
              char send_ip[16], msg[256], send_msg[256], header[20];
              sscanf(buf, "%s %s %[^\r\n]", header, send_ip, msg);
              
              printf("SEND MESSAGE: %s %s\n", send_ip, msg);
              
              struct Client *receiver = findClient_ip(allClients, send_ip);
              struct Client *sender = findClient_socket(allClients, i);
              //TODO CHECK IF sender is blocked by receiver, and if receiver is online
              snprintf(send_msg, sizeof(send_msg),"<SEND> %s %s", sender->ip, msg);
              send(receiver->csocket, send_msg, strlen(send_msg), 0);
              cse4589_print_and_log("[RELAYED:SUCCESS]\n");
              cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender->ip, receiver->ip, msg);
              cse4589_print_and_log("[RELAYED:END]\n");              
              
            }//Identifying headers in received data               
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
  struct Client *onlineClients = NULL;
  char command_str[20], client_ip[INET_ADDRSTRLEN], client_port[6], host[64], buf[256];
  int sockfd, status, nbytes, logged_in_yet = 0;
  struct addrinfo hints;
  struct addrinfo *p, *res;
  void *addr = NULL;
  
  strcpy(client_port, port);
  gethostname(host, 64);
  printf("Hostname: %s\n", host);
  
  //FIND CLIENT IP AND PORT
  struct hostent *he;
  he = gethostbyname(host);
  struct in_addr **addr_list;
  addr_list = (struct in_addr **) he->h_addr_list;
  strcpy(client_ip , inet_ntoa(*addr_list[0]) );
  printf("Client_ip: %s Client port:%s\n", client_ip, client_port);  
  
  //SELECT
  fd_set master; // master file descriptor list
  fd_set read_fds; // temp file descriptor list for select()
  int fdmax = 0; // maximum file descriptor number
  
  FD_ZERO(&master); // clear the master and temp sets
  FD_ZERO(&read_fds);
  
  FD_SET(STDIN, &master);
  
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 500000;
  
  while (1){
    read_fds = master; // copy it
    if (select(fdmax+1, &read_fds, NULL, NULL, &tv) == -1) {
      int errsv = errno;
      printf("Select Error %d\n", errsv);
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
          
          //LOGIN
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
              if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                addr = &(ipv4->sin_addr);
                // convert the IP to a string and print it
                inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
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
            
            char register_string[256];
            snprintf(register_string, sizeof(register_string),"<REGISTER> %s %s %s", client_ip, host, client_port);
            printf("Register String: %s\n", register_string);
            send(sockfd, register_string, strlen(register_string), 0);
            logged_in_yet == 1;
          }
          
          //EXIT
          else if (strcmp(command_str, "EXIT") == 0){
            close(sockfd);
            logged_in_yet == 0;
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
            return 0;
          }
          
          //LOGOUT
          else if (strcmp(command_str, "LOGOUT") == 0 && logged_in_yet == 1){
            close(sockfd);
            FD_CLR(sockfd, &master);
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 
          
          //IP
          else if (strcmp(command_str, "IP") == 0 && logged_in_yet == 1){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", client_ip);            
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 
          
          //PORT
          else if (strcmp(command_str, "PORT") == 0 && logged_in_yet == 1){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", client_port);            
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 

          //BLOCK
          else if (strcmp(command_str, "BLOCK") == 0 && logged_in_yet == 1){
            char block_ip[16], block_string[256];
            
            scanf("%s", block_ip);
            snprintf(block_string, sizeof(block_string),"<BLOCK> %s", block_ip);
            printf("Send String: %s\n", block_string);            
            send(sockfd, block_string, strlen(block_string), 0);
            
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);          
            cse4589_print_and_log("[%s:END]\n", command_str);    
          }  
          
          //UNBLOCK
          else if (strcmp(command_str, "UNBLOCK") == 0 && logged_in_yet == 1){
            char unblock_ip[16], unblock_string[256];
            
            scanf("%s", unblock_ip);
            snprintf(unblock_string, sizeof(unblock_string),"<BLOCK> %s", unblock_ip);
            printf("Send String: %s\n", unblock_string);            
            send(sockfd, unblock_string, strlen(unblock_string), 0);
            
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);          
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 
          
          //LIST
          else if (strcmp(command_str, "LIST") == 0 && logged_in_yet == 1){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            struct Client *it;
            int j = 1;
            for(it = onlineClients; it != NULL; it = it->next){
                cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", j++, it->host, it->ip, it->port);
            }
            cse4589_print_and_log("[%s:END]\n", command_str);            
          }
          
          //REFRESH
          else if (strcmp(command_str, "REFRESH") == 0 && logged_in_yet == 1){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            onlineClients = NULL;
            send(sockfd, "<REFRESH>", strlen("<REFRESH>"), 0);            
            cse4589_print_and_log("[%s:END]\n", command_str);            
          }           
          
          //BROADCAST
          else if (strcmp(command_str, "BROADCAST") == 0 && logged_in_yet == 1){
            char broadcast_string[256], send_msg[256];
            fgets (send_msg, 256, stdin);
            
            snprintf(broadcast_string, sizeof(broadcast_string),"<BROADCAST> %s", send_msg);
            printf("Send String: %s\n", broadcast_string);
            send(sockfd, broadcast_string, strlen(broadcast_string), 0);
            
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);          
            cse4589_print_and_log("[%s:END]\n", command_str);    
          }          
          
          //SEND
          else if (strcmp(command_str, "SEND") == 0 && logged_in_yet == 1){
            char send_string[256], send_ip[16], send_msg[256];
            scanf("%s", send_ip);
            fgets (send_msg, 256, stdin);
            
            snprintf(send_string, sizeof(send_string),"<SEND> %s %s", send_ip, send_msg);
            printf("Send String: %s\n", send_string);
            send(sockfd, send_string, strlen(send_string), 0);
            
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
          }//If for commands from STDIN
        }//Check if input is from STDIN or other socket
        else {
          // handle data from server
          memset ( buf, 0, sizeof buf );
          if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
            // got error or connection closed by server
            if (nbytes == 0) {
              // connection closed
              printf("selectclient: server hung up\n");
            }
            else {
              perror("recv");
            }
            
            close(i); // bye!
            FD_CLR(i, &master); // remove from master set
          }
          else {
            // we got some data from server 
            printf("Data from Server: %s\n", buf);
            char to_be_tokenized[256];
            strcpy(to_be_tokenized, buf);
            
            char *token;
            token = strtok(to_be_tokenized, " ");
            
            if (strcmp(token, "<LIST>") == 0){
              //Populate list of online clients available to client
              struct Client *c = (struct Client*)malloc(sizeof(struct Client));
              char header[20], cport[6];
              sscanf(buf, "%s %s %s %[^\r\n]", header, c->host, c->ip, cport);
              c->port = atoi(cport);
              c->msgs_sent = 0;
              c->msgs_recv = 0;
              c->online = 1;
              c->blocked = NULL;
              c->mlist = NULL;
              
              //No need to list details of this client itself
              if (strcmp(c->ip, client_ip) == 0){
                free(c);
              }
              else{
                onlineClients = registerClient(onlineClients, c);
              }
            }
            
            //MESSAGE RECEIVED FROM ANOTHER CLIENT VIA SERVER
            else if (strcmp(token, "<SEND>") == 0){
              char header[20], sender_ip[16], msg[256];
              sscanf(buf, "%s %s %[^\r\n]", header, sender_ip, msg);
              cse4589_print_and_log("[RECEIVED:SUCCESS]\n");
              cse4589_print_and_log("msg from:%s\n[msg]:%s\n", sender_ip, msg);
              cse4589_print_and_log("[RECEIVED:END]\n");               
            }
          }
        }
      }//Check if FD is set
    }//For loop 
  }//Infinite while Loop
  return 0;
  
}


