//Functions specific to server and client
int server(char *port);
int client(char *port);


//Struct to store messages in order received
struct messagesList {
  char *msg;
  struct messagesList *next;
};

//Struct for storing the list and stats and pending messages
struct allClients {
  char *host, *ip, *port;
  int msgs_sent, msgs_recv, is_online;
  struct messagesList *mlist;
};
