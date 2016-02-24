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
#include <stdbool.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "../include/logger.h"
#include "../include/global.h"
#include "../include/cs.h"

#define MAX_PENDING 5
#define MAX_LINE 256
#define STDIN 0

//check if IP is valid
bool checkIpPort(char *ip, int port){
  struct in_addr s;
  inet_aton(ip, &s);
  return ((gethostbyaddr(&s, sizeof(struct in_addr), AF_INET)!=NULL?true:false) &&(port > 0 && port <65535));
}

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

//FIND CLIENT IN LIST BY IP AND PORT
struct Client* findClient_ip_port(struct Client *allClients, char * register_ip, int port){
  struct Client *temp = NULL;
  
  for(temp = allClients; temp != NULL; temp = temp->next){
    if ((strcmp(register_ip, temp->ip) == 0) &&(temp->port == port)){
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

//REMOVE CLIENT FROM ALLCLIENTS LIST WHEN A CLIENT EXITS
struct Client* removeClient(struct Client *allClients, struct Client *c){
  struct Client *it, *prev;
  
  if (strcmp(allClients->ip, c->ip) == 0){
    return allClients->next;
  }
  
  for(it = allClients->next, prev = allClients; it != NULL; it = it->next, prev = prev->next){
    if (strcmp(it->ip, c->ip) == 0){
      prev->next = it->next;
      break;
    }
  }
  return allClients;
}

//FUNCTION TO CHECK IF SENDER IS BLOCKED BY RECEIVER
bool isSenderBlocked(char *sender_ip, struct Client *receiver){
  struct bClient *list = receiver->blocked, *it;
  
  for(it = list; it!=NULL; it= it->next){
    if (strcmp(it->ip, sender_ip) == 0){
      return true;
    }
  }
  return false;
}

//ADD MESSAGES TO BUFFER TO BE DELIVERED WHEN CLIENT COMES ONLINE
struct messages * addMsgToList(struct messages *mlist, struct messages *msg){
  //NO MESSAGES
  if (mlist == NULL){
    return msg;
  }
  
  //ONE MESSAGE IN LIST
  if (mlist->next == NULL){
    mlist->next = msg;
    return mlist;
  }
  
  //LIST OF MESSAGES
  else{
    struct messages *it = mlist;
    while(it->next != NULL){
      it = it->next;
    }
    it->next = msg;
  }
  return mlist;
}

//RELAY PENDING MESSAGES IF ANY WHEN CLIENT COMES ONLINE
void relayMessages(struct Client *c){
  while(c->mlist != NULL){ 
    send(c->csocket, c->mlist->send_msg, strlen(c->mlist->send_msg), 0);
    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
    cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n", c->mlist->sender, c->mlist->msg);
    cse4589_print_and_log("[RELAYED:END]\n");         
    c->mlist = c->mlist->next;
    ++c->msgs_recv;
  }
}

//SERVER BLOCKED COMMAND
void printBlocked(struct Client *allClients, char *ip){
  struct Client *c = findClient_ip(allClients, ip);
  struct bClient *it;
  if (c == NULL){
    cse4589_print_and_log("[BLOCKED:ERROR]\n");
    cse4589_print_and_log("[BLOCKED:END]\n");  
  }
  else{
    cse4589_print_and_log("[BLOCKED:SUCCESS]\n");
    int i = 1;
    for (it = c->blocked; it!=NULL; it = it->next, i++){
      cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", i, it->host, it->ip, it->port);
    }
    cse4589_print_and_log("[BLOCKED:END]\n");     
  }
}

//SERVER STATISTICS COMMAND
void printStatistics(struct Client *allClients){
  struct Client *it = allClients;
  cse4589_print_and_log("[STATISTICS:SUCCESS]\n");  
  int i =1;
  for(it = allClients; it != NULL; it = it->next){
    cse4589_print_and_log("%-5d%-35s%-8d%-8d%-8s\n", i++, it->host, it->msgs_sent, it->msgs_recv, (it->online?"online":"offline"));
  }
  cse4589_print_and_log("[STATISTICS:END]\n");    
}



//BLOCK A CLIENT
struct bClient* blockClient(struct Client *allClients, struct Client *sender, char *ip){
  struct Client *find_to_be_blocked = findClient_ip(allClients, ip);
  struct bClient *it, *to_be_blocked;
  
  //CHECK IF CLIENT HAS ALREADY BEEN BLOCKED BY SENDER
  if (isSenderBlocked(find_to_be_blocked->ip, sender)){
    send(sender->csocket, "<BLOCK ERROR>", strlen("<BLOCK ERROR>"), 0);                 
    return sender->blocked;    
  }
  
  //ADD CLIENT TO BLOCKED LIST
  to_be_blocked = (struct bClient*) malloc(sizeof (struct bClient));
  strcpy(to_be_blocked->ip, find_to_be_blocked->ip);
  strcpy(to_be_blocked->host, find_to_be_blocked->host);
  to_be_blocked->port = find_to_be_blocked->port;
  to_be_blocked->next = NULL;
  
  it = sender->blocked;
  if (it == NULL){
    send(sender->csocket, "<BLOCK SUCCESS>", strlen("<BLOCK SUCCESS>"), 0);             
    return to_be_blocked;
  }
  while(it->next != NULL){
    it = it->next;
  }
  send(sender->csocket, "<BLOCK SUCCESS>", strlen("<BLOCK SUCCESS>"), 0);
  it->next = to_be_blocked;
  return sender->blocked;  
  
}

//UNBLOCK A CLIENT
struct bClient* unblockClient(struct Client *allClients, struct Client *sender, char *ip){
  struct Client *to_be_unblocked = findClient_ip(allClients, ip);
  struct bClient *it, *prev;

  //CHECK IF CLIENT WAS NEVER BLOCKED
  if (!isSenderBlocked(to_be_unblocked->ip, sender)){
    send(sender->csocket, "<UNBLOCK ERROR>", strlen("<UNBLOCK ERROR>"), 0);           
    return sender->blocked;    
  }

  //REMOVE CLIENT FROM BLOCKED LIST
  it = sender->blocked;
  if (strcmp(to_be_unblocked->ip, it->ip)==0){
    send(sender->csocket, "<UNBLOCK SUCCESS>", strlen("<UNBLOCK SUCCESS>"), 0);       
    return it->next;
  }

  for(it = sender->blocked->next, prev = sender->blocked; it != NULL; it = it->next, prev = prev->next){
    if (strcmp(to_be_unblocked->ip, it->ip)==0){
      prev->next = it->next;
      break;
    }
  }
  send(sender->csocket, "<UNBLOCK SUCCESS>", strlen("<UNBLOCK SUCCESS>"), 0);  
  return sender->blocked;  
  
}

//SEND A LIST OF ONLINE CLIENTS
void sendOnlineClients(struct Client *allClients, int i){
  //Send List of all online Clients
  char send_list[256];
  struct Client *it;
  for(it = allClients; it != NULL; it = it->next){
    if (it->online == 1){
      memset (send_list, 0, sizeof send_list );                  
      snprintf(send_list, sizeof(send_list),"<LIST> %s %s %d", it->host, it->ip, it->port);
      send(i, send_list, strlen(send_list), 0);
      usleep(100000);                  
    }
  }   
}

//CREATE SOCKET TO LISTEN FOR INCOMING CONNECTIONS
int listenSocket(char port[]){
  struct addrinfo hints, *p, *res;
  int status;
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  
  //GETADDRINFO
  
  if ((status = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }
  
  void *addr = NULL;
  char *ipver = NULL;
  
  for(p = res;p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      // convert the IP to a string
      //inet_ntop(p->ai_family, addr, server_ip, sizeof server_ip);
      break;
    }
  }
  
  if (p == NULL){
    printf("Getaddrinfo Error\n");
    return -1;
  }
  
  //freeaddrinfo(res); // free the linked list
  
  //SOCKET
  int listener;
  if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
  {
    int errsv = errno;
    printf("Socket Error: %d\n", errsv);
    return -1;    
  }
  
  // set SO_REUSEADDR on a socket to true (1) to reuse the same address:
  int optval = 1;
  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  
  //BIND
  if (bind(listener, p->ai_addr, p->ai_addrlen)== -1){
    int errsv = errno;
    printf("Bind Error: %d\n", errsv);  
    return -1;
  }
  
  //LISTEN
  if (listen(listener, MAX_PENDING) == -1){
    int errsv = errno;
    printf("Listen Error: %d\n", errsv);  
    return -1;    
  }
  
  return listener;
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
  
  //FIND SERVER IP FROM HOSTNAME
  struct hostent *he;
  he = gethostbyname(host);
  struct in_addr **addr_list;
  addr_list = (struct in_addr **) he->h_addr_list;
  strcpy(server_ip , inet_ntoa(*addr_list[0]) );
  printf("Server_ip: %s Server port:%s\n", server_ip, server_port);   
  
  /*
  struct addrinfo hints, *p, *res;
  
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET; // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  
  //GETADDRINFO
  
  if ((status = getaddrinfo(NULL, server_port, &hints, &res)) != 0) {
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
      //inet_ntop(p->ai_family, addr, server_ip, sizeof server_ip);
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

  // set SO_REUSEADDR on a socket to true (1) to reuse the same address:
  int optval = 1;
  setsockopt(server_listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  
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
  */
  int server_listener = listenSocket(server_port);
  
  //SELECT
  fd_set master; // master file descriptor list
  fd_set read_fds; // temp file descriptor list for select()
  int fdmax; // maximum file descriptor number
  int newfd; // newly accepted socket descriptor
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;
  char buf[300]; // buffer for client data
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
        printf("We got one!\n");
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
          //BLOCKED
          else if (strcmp(command_str, "BLOCKED") == 0){
            char ip_blocked[16];
            scanf("%s", ip_blocked);
            printBlocked(allClients, ip_blocked);
          } 
          
          //STATISTICS
          else if (strcmp(command_str, "STATISTICS") == 0){
            printStatistics(allClients);
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
          printf("Incoming Connection\n");
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
              allClients = removeClient(allClients, findClient_socket(allClients, i));
              //TODO REMOVE CLIENT FROM ALLCLIENTS LIST
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
              
              socklen_t len;
              struct sockaddr_storage addr;
              char ipstr[INET6_ADDRSTRLEN];
              int port;
              
              len = sizeof addr;
              getpeername(i, (struct sockaddr*)&addr, &len);
              
              // GET IP OF CLIENT
              if (addr.ss_family == AF_INET) {
                struct sockaddr_in *s = (struct sockaddr_in *)&addr;
                port = ntohs(s->sin_port);
                inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
                printf("GETPEERIP: %s\n", ipstr);
              }
              
              //Populate new Client to register
              struct Client *c = (struct Client*)malloc(sizeof(struct Client));
              //memset(&c, 0, sizeof (c)); //DOES NOT WORK FOR SOME REASON - SEG FAULT
              char header[30], string_port[16];
             
              sscanf(buf, "%s %s %s %[^\r\n]", header, c->ip, c->host, string_port);
              strcpy(c->ip, ipstr);
              c->port = atoi(string_port);
              c->msgs_sent = 0;
              c->msgs_recv = 0;
              c->online = 1;
              c->csocket = i;
              c->blocked = NULL;
              c->mlist = NULL;
              
              //CHANGE STATUS TO ONLINE IF ALREADY REGISTERED
              if (findClient_ip_port(allClients, c->ip, c->port)){
                findClient_ip_port(allClients, c->ip, c->port)->online = 1;
                free(c);
                c = findClient_ip_port(allClients, c->ip, c->port);
              }
              //REGISTER NEW CLIENT
              else{
                allClients = registerClient(allClients, c);
              }
              
              //Send List of all online Clients
              sendOnlineClients(allClients, i);              
              
              //Relay all buffered messages
              if (c->mlist != NULL){
                relayMessages(c);
              }
            }
            
            //REFRESH RECEIVED FROM CLIENT
            else if (strcmp(token, "<REFRESH>") == 0){
              sendOnlineClients(allClients, i);
            } 
            
            //BLOCK RECEIVED FROM CLIENT
            else if (strcmp(token, "<BLOCK>") == 0){
              char ip_to_be_blocked[16], header[30];
              sscanf(buf, "%s %[^\r\n]", header, ip_to_be_blocked);  
              
              struct Client *sender = findClient_socket(allClients, i);
              sender->blocked = blockClient(allClients, sender, ip_to_be_blocked);
            }  
            
            //UNBLOCK RECEIVED FROM CLIENT
            else if (strcmp(token, "<UNBLOCK>") == 0){
              char ip_to_be_unblocked[16], header[30];
              sscanf(buf, "%s %[^\r\n]", header, ip_to_be_unblocked);  
              
              struct Client *sender = findClient_socket(allClients, i);
              sender->blocked = unblockClient(allClients, sender, ip_to_be_unblocked);
            }    
            
            //LOGOUT RECEIVED FROM CLIENT
            else if (strcmp(token, "<LOGOUT>") == 0){
              struct Client *sender = findClient_socket(allClients, i);
              sender->online = 0;
            } 
            
            
            //BROADCAST RECEIVED FROM CLIENT            
            else if (strcmp(token, "<BROADCAST>") == 0){
              char msg[256], send_msg[256], header[20];
              sscanf(buf, "%s %[^\r\n]", header, msg);
              
              printf("BROADCAST MESSAGE: %s\n", msg);
              
              struct Client *sender = findClient_socket(allClients, i), *it;
              ++sender->msgs_sent;
              
              //TODO CHECK IF sender is blocked by receiver, and if receiver is online
              snprintf(send_msg, sizeof(send_msg),"<SEND> %s %s", sender->ip, msg);
              
              for (it = allClients;it!= NULL; it = it->next){
                if ((it->csocket != sender->csocket) && (!isSenderBlocked(sender->ip, it))){
                  printf("Sender not blocked\n");
                  if (it->online == 1){
                    send(it->csocket, send_msg, strlen(send_msg), 0);
                    cse4589_print_and_log("[RELAYED:SUCCESS]\n");
                    cse4589_print_and_log("msg from:%s, to:255.255.255.255\n[msg]:%s\n", sender->ip, msg);
                    cse4589_print_and_log("[RELAYED:END]\n");     
                    it->msgs_recv = it->msgs_recv + 1;                    
                  }
                  else{
                    //Buffer it
                    printf("Buffer message for %s\n", it->ip); 
                    struct messages *m = (struct messages *)malloc (sizeof (struct messages));
                    strcpy(m->msg, msg);
                    strcpy(m->send_msg, send_msg);
                    m->next = NULL; 
                    strcpy(m->sender, sender->ip);
                    it->mlist = addMsgToList(it->mlist, m);
                  }
                } 
              }
            }            
            
            //SEND RECEIVED FROM CLIENT
            else if (strcmp(token, "<SEND>") == 0){
              char send_ip[16], msg[256], send_msg[300], header[20];
              sscanf(buf, "%s %s %[^\r\n]", header, send_ip, msg);
              
              printf("SEND MESSAGE: %s %s\n", send_ip, msg);
              
              struct Client *receiver = findClient_ip(allClients, send_ip);
              struct Client *sender = findClient_socket(allClients, i);
              sender->msgs_sent = sender->msgs_sent+1;
              //TODO CHECK IF sender is blocked by receiver, and if receiver is online
              snprintf(send_msg, sizeof(send_msg),"<SEND> %s %s", sender->ip, msg);
              
              if (!isSenderBlocked(sender->ip, receiver)){
                printf("Sender not blocked\n");
                if (receiver->online == 1){
                  send(receiver->csocket, send_msg, strlen(send_msg), 0);
                  cse4589_print_and_log("[RELAYED:SUCCESS]\n");
                  cse4589_print_and_log("msg from:%s, to:%s\n[msg]:%s\n", sender->ip, receiver->ip, msg);
                  cse4589_print_and_log("[RELAYED:END]\n");  
                  receiver->msgs_recv = receiver->msgs_recv + 1;
                }
                else{
                  //Buffer it
                  printf("Buffer message for %s\n", receiver->ip); 
                  struct messages *m = (struct messages *)malloc (sizeof (struct messages));
                  strcpy(m->msg, msg);
                  strcpy(m->send_msg, send_msg);
                  m->next = NULL; 
                  strcpy(m->sender, sender->ip);                    
                  receiver->mlist = addMsgToList(receiver->mlist, m);
                }
              }              
            }//Identifying headers in received data               
          }//If data was received or connection was closed
        }//If server_listener
      }//If to check if there is data available on any socket
    } // For Loop
  }//Infinite While loop
  close(server_listener);
  return 0;
  
}


//CONNECT TO SERVER
int loginClient(char *command_str, char server_host[], char server_port[]){
  struct addrinfo hints, *res, *p;
  void *addr = NULL;
  //char server_host[INET_ADDRSTRLEN], server_port[5];
  char ipstr[INET_ADDRSTRLEN];
  int status, sockfd;
  
  //scanf("%s %s", server_host, server_port);
  
  if (!checkIpPort(server_host, atoi(server_port))){
    cse4589_print_and_log("[%s:ERROR]\n", command_str);
    cse4589_print_and_log("[%s:END]\n", command_str);  
    return -1;
  }
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  //GETADDRINFO
  
  if ((status = getaddrinfo(server_host, server_port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    cse4589_print_and_log("[%s:ERROR]\n", command_str);
    cse4589_print_and_log("[%s:END]\n", command_str);               
    return -1;
  }
  
  for(p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET ) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      // convert the IP to a string and print it
      inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
      printf("IPSTR: %s\n", ipstr);
      break;
    }
    
  }
  
  //freeaddrinfo(res); // free the linked list
  
  //SOCKET
  
  if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
  {
    int errsv = errno;
    printf("Socket Error: %d\n", errsv);
    cse4589_print_and_log("[%s:ERROR]\n", command_str);
    cse4589_print_and_log("[%s:END]\n", command_str);               
    return -1;
    //return errsv;    
  }
  
  //CONNECT
  if (connect(sockfd, p->ai_addr, p->ai_addrlen)== -1){
    int errsv = errno;
    printf("Connect Error: %d\n", errsv);  
    close(sockfd);
    cse4589_print_and_log("[%s:ERROR]\n", command_str);
    cse4589_print_and_log("[%s:END]\n", command_str);               
    return -1;
    //return errsv;
  }
   
  return sockfd;
}



//CLIENT
int client(char *port)
{  
  struct Client *onlineClients = NULL;
  char command_str[20], client_ip[INET_ADDRSTRLEN], client_port[6], host[64], buf[256], remoteIP[INET_ADDRSTRLEN];
  int sockfd, status, cstatus, nbytes, logged_in_yet = 0, cport = atoi(client_port);
  struct addrinfo hints, *p, *res;
  void *addr = NULL;
  int filefd; // newly accepted socket descriptor for file transfer
  struct sockaddr_storage remoteaddr; // client address
  socklen_t addrlen;
  
  strcpy(client_port, port);
  gethostname(host, 64);
  printf("Hostname: %s\n", host);
  
  //FIND CLIENT IP
  struct hostent *he;
  he = gethostbyname(host);
  struct in_addr **addr_list;
  addr_list = (struct in_addr **) he->h_addr_list;
  strcpy(client_ip , inet_ntoa(*addr_list[0]) ); 
  printf("Client_ip: %s Client port:%s\n", client_ip, client_port);  
    
  //File
  int fsize = 0, remain_data = 0;
  FILE * f_recv;
  char filename_recv[50], header[30];
  
  //MAKE CLIENT LISTEN
  /*
  struct addrinfo chints, *cp, *cres;
  
  memset(&chints, 0, sizeof (chints));
  chints.ai_family = AF_INET; // don't care IPv4 or IPv6
  chints.ai_socktype = SOCK_STREAM; // TCP stream sockets
  chints.ai_flags = AI_PASSIVE; // fill in my IP for me
  
  //GETADDRINFO
  
  if ((cstatus = getaddrinfo(NULL, client_port, &chints, &cres)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  
  for(cp = cres;cp != NULL; cp = cp->ai_next) {
    if (cp->ai_family == AF_INET) { // IPv4
      struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
      addr = &(ipv4->sin_addr);
      // convert the IP to a string
      //inet_ntop(p->ai_family, addr, server_ip, sizeof server_ip);
      break;
    }
  }
  
  if (cp == NULL){
    printf("Getaddrinfo Error\n");
    return 1;
  }
  
  //freeaddrinfo(res); // free the linked list
  //SOCKET
  int client_listener;
  if ((client_listener = socket(cp->ai_family, cp->ai_socktype, cp->ai_protocol)) == -1)
  {
    int errsv = errno;
    printf("Socket Error: %d\n", errsv);
    return errsv;    
  }
  
  // set SO_REUSEADDR on a socket to true (1) to reuse the same address:
  int optval = 1;
  setsockopt(client_listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
  
  //BIND
  if (bind(client_listener, cp->ai_addr, cp->ai_addrlen)== -1){
    int errsv = errno;
    printf("Bind Error: %d\n", errsv);  
    return errsv;
  }
  
  //LISTEN
  if (listen(client_listener, MAX_PENDING) == -1){
    int errsv = errno;
    printf("Listen Error: %d\n", errsv);  
    return errsv;    
  }
  */
  //END MAKE CLIENT LISTEN

  int client_listener = listenSocket(client_port);
  
    printf("Listening before select");
  //SELECT
  fd_set master; // master file descriptor list
  fd_set read_fds; // temp file descriptor list for select()
  int fdmax = client_listener; // maximum file descriptor number
  
  FD_ZERO(&master); // clear the master and temp sets
  FD_ZERO(&read_fds);
  
  FD_SET(STDIN, &master);
  FD_SET(client_listener, &master);
  
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 500000;
  
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
          
          //AUTHOR
          if (strcmp(command_str, "AUTHOR") == 0){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("I, richieve, have read and understood the course academic integrity policy.\n");
            cse4589_print_and_log("[%s:END]\n", command_str);
          }
          
          //LOGIN
          else if (strcmp(command_str, "LOGIN") == 0){
            /*
            char server_host[INET_ADDRSTRLEN];
            char server_port[5], ipstr[INET_ADDRSTRLEN];
            
            scanf("%s %s", server_host, server_port);
            
            if (!checkIpPort(server_host, atoi(server_port))){
              cse4589_print_and_log("[%ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            
            memset(&hints, 0, sizeof hints);
            hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
            hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
            hints.ai_flags = AI_PASSIVE; // fill in my IP for me
            
            //GETADDRINFO
            
            if ((status = getaddrinfo(server_host, server_port, &hints, &res)) != 0) {
              fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
              cse4589_print_and_log("[%ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
              continue;
            }
            
            for(p = res;p != NULL; p = p->ai_next) {
              if (p->ai_family == AF_INET ) { // IPv4
                struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                addr = &(ipv4->sin_addr);
                // convert the IP to a string and print it
                inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
                printf("IPSTR: %s\n", ipstr);
                break;
              }
              
            }
            
            //freeaddrinfo(res); // free the linked list
            
            //SOCKET
            
            if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            {
              int errsv = errno;
              printf("Socket Error: %d\n", errsv);
              cse4589_print_and_log("[%ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
              continue;
              //return errsv;    
            }
            
             
 
            if (p== NULL){
              printf("P is NULL");
            }
            
            //CONNECT
            if (connect(sockfd, p->ai_addr, p->ai_addrlen)== -1){
              int errsv = errno;
              printf("Connect Error: %d\n", errsv);  
              close(sockfd);
              cse4589_print_and_log("[%ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
              continue;
              //return errsv;
            }
            */
            char server_host[INET_ADDRSTRLEN];
            char server_port[5];
            scanf("%s %s", server_host, server_port);
            if ((sockfd = loginClient("LOGIN", server_host, server_port)) == -1){
              continue;
            } 

            
            FD_SET(sockfd, &master);
            fdmax = (fdmax < sockfd? sockfd: fdmax);
            
            //LOGIN SUCCESSFUL
            logged_in_yet = 1;
            
            char register_string[256];
            snprintf(register_string, sizeof(register_string),"<REGISTER> %s %s %s", client_ip, host, client_port);
            printf("Register String: %s\n", register_string);
            if (send(sockfd, register_string, strlen(register_string), 0) == -1){
              close(sockfd);
              cse4589_print_and_log("[%s:ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            
            else{
              cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            
          }
          
          //EXIT
          else if (strcmp(command_str, "EXIT") == 0){
            close(sockfd);
            FD_CLR(sockfd, &master);            
            logged_in_yet == 0;
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("[%s:END]\n", command_str);    
            return 0;
          }
          
          //LOGOUT
          else if ((strcmp(command_str, "LOGOUT") == 0) && (logged_in_yet == 1)){
            logged_in_yet == 0;
            if (send(sockfd, "<LOGOUT>", strlen("<LOGOUT>"), 0)!= -1){
              cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);    
            }
            else {
              cse4589_print_and_log("[%s:ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
          } 
          
          //IP
          else if ((strcmp(command_str, "IP") == 0) && (logged_in_yet == 1)){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", client_ip);            
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 
          
          //PORT
          else if ((strcmp(command_str, "PORT") == 0) && (logged_in_yet == 1)){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            cse4589_print_and_log("IP:%s\n", client_port);            
            cse4589_print_and_log("[%s:END]\n", command_str);    
          } 
          
          //BLOCK
          else if ((strcmp(command_str, "BLOCK") == 0) && (logged_in_yet == 1)){
            char block_ip[16], block_string[256];
            scanf("%s", block_ip);
            
            if (findClient_ip(onlineClients, block_ip) == NULL){
              cse4589_print_and_log("[%s:ERROR]\n", command_str);          
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            else{
              snprintf(block_string, sizeof(block_string),"<BLOCK> %s", block_ip);          
              if (send(sockfd, block_string, strlen(block_string), 0) == -1){
                cse4589_print_and_log("[%s:ERROR]\n", command_str);          
                cse4589_print_and_log("[%s:END]\n", command_str);                
              }
              //ELSE SERVER WILL CONFIRM IF BLOCK IS SUCCESSFUL
            }
          }  
          
          //UNBLOCK
          else if ((strcmp(command_str, "UNBLOCK") == 0) && (logged_in_yet == 1)){
            char unblock_ip[16], unblock_string[256];
            scanf("%s", unblock_ip);
            if (findClient_ip(onlineClients, unblock_ip) == NULL){
              cse4589_print_and_log("[%s:ERROR]\n", command_str);          
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            else{            
              snprintf(unblock_string, sizeof(unblock_string),"<UNBLOCK> %s", unblock_ip);
              printf("Send String: %s\n", unblock_string);            
              if (send(sockfd, unblock_string, strlen(unblock_string), 0) == -1){
                cse4589_print_and_log("[%s:ERROR]\n", command_str);          
                cse4589_print_and_log("[%s:END]\n", command_str);                  
              }
              //ELSE SERVER WILL CONFIRM IF BLOCK IS SUCCESSFUL
            }    
          } 
          
          //LIST
          else if ((strcmp(command_str, "LIST") == 0) && (logged_in_yet == 1)){
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
            struct Client *it;
            int j = 1;
            for(it = onlineClients; it != NULL; it = it->next){
              cse4589_print_and_log("%-5d%-35s%-20s%-8d\n", j++, it->host, it->ip, it->port);
            }
            cse4589_print_and_log("[%s:END]\n", command_str);            
          }
          
          //REFRESH
          else if ((strcmp(command_str, "REFRESH") == 0) && (logged_in_yet == 1)){
            
            onlineClients = NULL;
            if (send(sockfd, "<REFRESH>", strlen("<REFRESH>"), 0) != -1){
              cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);            
            }
            else{
              cse4589_print_and_log("[%s:ERROR]\n", command_str);
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
          }           
          
          //BROADCAST
          else if ((strcmp(command_str, "BROADCAST") == 0) && (logged_in_yet == 1)){
            char broadcast_string[256], send_msg[256];
            fgets (send_msg, 256, stdin);
            
            //REMOVE TRAILING NEWLINE CHAR
            if (send_msg[strlen(send_msg) - 1] == '\n'){
              send_msg[strlen(send_msg) - 1] = '\0';            
            }
            
            snprintf(broadcast_string, sizeof(broadcast_string),"<BROADCAST> %s", send_msg);
            printf("Send String: %s\n", broadcast_string);
            
            if (send(sockfd, broadcast_string, strlen(broadcast_string), 0) != -1){
              cse4589_print_and_log("[%s:SUCCESS]\n", command_str);   
              cse4589_print_and_log("msg from:%s\n[msg]:%s\n", client_ip, send_msg);              
              cse4589_print_and_log("[%s:END]\n", command_str);    
            }
            else{
              cse4589_print_and_log("[%s:ERROR]\n", command_str);          
              cse4589_print_and_log("[%s:END]\n", command_str);                
            }
          }          
          
          //SEND
          else if ((strcmp(command_str, "SEND") == 0) && (logged_in_yet == 1)){
            char send_string[256], send_ip[16], send_msg[256];
            scanf("%s", send_ip);
            fgets (send_msg, 256, stdin);
            
            //REMOVE TRAILING NEWLINE CHAR
            if (send_msg[strlen(send_msg) - 1] == '\n'){
              send_msg[strlen(send_msg) - 1] = '\0';            
            }            
            
            //EXCEPTION HANDLING
            if (findClient_ip(onlineClients, send_ip) == NULL){
              cse4589_print_and_log("[%s:ERROR]\n", command_str);          
              cse4589_print_and_log("[%s:END]\n", command_str);               
            }
            else{             
              snprintf(send_string, sizeof(send_string),"<SEND> %s %s", send_ip, send_msg);
              printf("Send String: %s\n", send_string);
              if (send(sockfd, send_string, strlen(send_string), 0) != -1){
                cse4589_print_and_log("[%s:SUCCESS]\n", command_str);
                cse4589_print_and_log("[%s:END]\n", command_str); 
              }
              else{
                cse4589_print_and_log("[%s:ERROR]\n", command_str);
                cse4589_print_and_log("[%s:END]\n", command_str);                
              }
            }
          }
          //SENDFILE
          else if ((strcmp(command_str, "SENDFILE") == 0) && (logged_in_yet == 1)){
            printf("Sendfile\n");
            char file_send_ip[16], filename_send[50], file_send_port[6];
            scanf("%s %s", file_send_ip, filename_send);

            struct Client *file_send_client = findClient_ip(onlineClients, file_send_ip);
            sprintf(file_send_port,"%d",file_send_client->port);
            
            int sockfilesend = loginClient("SENDFILE", file_send_client->ip, file_send_port);
            
            //TODO READ FILE AND SEND TO THIS SOCKET
            int f = open(filename_send, O_RDONLY);
            struct stat file_stat;
            fstat(f, &file_stat);
            char file_details[20];
            sprintf(file_details,"<FILE> %d %s",file_stat.st_size, filename_send);
            
            send(sockfilesend, file_details, strlen(file_details), 0);
            usleep(100000);
            
            int sent_bytes = 0;
            int remain_data = file_stat.st_size;
            off_t offset = 0;
            /* Sending file data */
            while (((sent_bytes = sendfile(sockfilesend, f, &offset, 256)) > 0) && (remain_data > 0))
            {
              fprintf(stdout, "1. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
              remain_data -= sent_bytes;
              fprintf(stdout, "2. Server sent %d bytes from file's data, offset is now : %d and remaining data = %d\n", sent_bytes, offset, remain_data);
            }
            
            cse4589_print_and_log("[%s:SUCCESS]\n", command_str);         
            cse4589_print_and_log("[%s:END]\n", command_str);  
            close(sockfilesend);
          }//If for commands from STDIN
          
        }//Check if input is from STDIN or other socket
        //Handle new connection for sending file
        else if (i == client_listener){
          printf("Incoming Connection\n");
          addrlen = sizeof remoteaddr;
          //ACCEPT
          filefd = accept(client_listener, (struct sockaddr *)&remoteaddr, &addrlen);
          if (filefd == -1) {
            printf("Accept Error");
            continue;
          }
          else {
            FD_SET(filefd, &master); // add to master set
            if (filefd > fdmax) { // keep track of the max
              fdmax = filefd;
            }
          }          
        }
        
        //To receive file
        else if (i == filefd){
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
          else{
            
            // we got some data for file transfer 
            printf("Data from Client for file transfer: %s\n", buf);
            char to_be_tokenized[256];
            strcpy(to_be_tokenized, buf);
            
            char token[30];
            //token = strtok(to_be_tokenized, " ");
            sscanf(buf, "%s ", token);
            printf("Token:%s\n", token);
            if (strcmp(token, "<FILE>") == 0){
              char sfsize[30];
              printf("Need to open file\n");
              sscanf(buf, "%s %s %[^\r\n]", header, sfsize, filename_recv);
              fsize = atoi(sfsize);
              printf("file to be opened: %s with size %d\n", filename_recv, fsize);
              f_recv = fopen(filename_recv, "w");
              if (f_recv == NULL)
              {
                printf("Failed to open file foo\n");
                
                continue;
              }              
              printf("file opened\n");
              remain_data = fsize;
            }
            else{
              //Received file contents  
              printf("Received file contents of size %d remaining: %s\n", remain_data, buf);
              
              if (remain_data > 0){
                fwrite(buf, sizeof(char), nbytes, f_recv);
                remain_data -= nbytes;
                printf("Remaining data: %d\n", remain_data);
                if (remain_data <= 0){
                  fclose(f_recv);
                }
              }
              else{
              //File Transfer complete
                fclose(f_recv);
                close(i); // bye!
                FD_CLR(i, &master); // remove from master set                
              }
            }
          }
        }
        
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
              
              //No need to add details of client already in list
              if (findClient_ip_port(onlineClients, c->ip, c->port) != NULL){
                printf("Client already present:%s %s\n", c->ip, c->port);
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
            
            else if (strcmp(buf, "<BLOCK ERROR>") == 0){            
              cse4589_print_and_log("[BLOCK:ERROR]\n");
              cse4589_print_and_log("[BLOCK:END]\n"); 
            }
            
            else if (strcmp(buf, "<BLOCK SUCCESS>") == 0){            
              cse4589_print_and_log("[BLOCK:SUCCESS]\n");
              cse4589_print_and_log("[BLOCK:END]\n"); 
            }  
            
            else if (strcmp(buf, "<UNBLOCK ERROR>") == 0){            
              cse4589_print_and_log("[UNBLOCK:ERROR]\n");
              cse4589_print_and_log("[UNBLOCK:END]\n"); 
            }
            
            else if (strcmp(buf, "<UNBLOCK SUCCESS>") == 0){            
              cse4589_print_and_log("[UNBLOCK:SUCCESS]\n");
              cse4589_print_and_log("[UNBLOCK:END]\n"); 
            }             
          }
        }
      }//Check if FD is set
    }//For loop 
  }//Infinite while Loop
  return 0;
  
}


