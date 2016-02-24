#include <stdbool.h>
#include <netinet/in.h>


//Struct to store messages in order received
struct messages {
  char msg[256], send_msg[300], sender[16];
  struct messages *next;
};

//Struct to store messages in order received
struct bClient {
  char ip[16], host[30];
  int port;
  struct bClient *next;
};

//Struct for storing the list and stats and pending messages
struct Client {
  char host[30], ip[16];
  int msgs_sent, msgs_recv, online, csocket, port;
  struct messages *mlist;
  struct Client *next;
  struct bClient *blocked;
};

//Functions specific to server and client
int server(char *port);
int client(char *port);

//check if IP is valid
bool checkIpPort(char *ip, int port);

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa);

//FIND CLIENT IN LIST BY IP
struct Client* findClient_ip(struct Client *allClients, char * register_ip);

//FIND CLIENT IN LIST BY IP AND PORT
struct Client* findClient_ip_port(struct Client *allClients, char * register_ip, int port);

//FIND CLIENT IN LIST BY SOCKET ID
struct Client* findClient_socket(struct Client *allClients, int socket);


//REGISTER DETAILS OF NEW CLIENT
struct Client* registerClient(struct Client *allClients, struct Client *newClient);

//REMOVE CLIENT FROM ALLCLIENTS LIST WHEN A CLIENT EXITS
struct Client* removeClient(struct Client *allClients, struct Client *c);

//FUNCTION TO CHECK IF SENDER IS BLOCKED BY RECEIVER
bool isSenderBlocked(char *sender_ip, struct Client *receiver);

//ADD MESSAGED TO BUFFER TO BE DELIVERED WHEN CLIENT COMES ONLINE
struct messages * addMsgToList(struct messages *mlist, struct messages *msg);

//RELAY PENDING MESSAGES IF ANY WHEN CLIENT COMES ONLINE
void relayMessages(struct Client *c);

//SERVER BLOCKED COMMAND
void printBlocked(struct Client *allClients, char *ip);

//SERVER STATISTICS COMMAND
void printStatistics(struct Client *allClients);

//BLOCK A CLIENT
struct bClient* blockClient(struct Client *allClients, struct Client *sender, char *ip);

//UNBLOCK A CLIENT
struct bClient* unblockClient(struct Client *allClients, struct Client *sender, char *ip);

//SEND A LIST OF ONLINE CLIENTS
void sendOnlineClients(struct Client *allClients, int i);

//CREATE SOCKET TO LISTEN FOR INCOMING CONNECTIONS
int listenSocket(char port[]);

//CONNECT
int loginClient(char *command_str, char server_host[], char server_port[]);